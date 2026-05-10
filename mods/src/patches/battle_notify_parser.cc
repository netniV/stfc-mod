#include "patches/battle_notify_parser.h"

#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/BattleResultHeader.h>
#include <prime/HullSpec.h>
#include <prime/SpecService.h>
#include <prime/Toast.h>
#include <prime/UserProfile.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

#include <string>

#if _WIN32
#include <windows.h>
#endif

// ---------------------------------------------------------------------------
// SEH wrapper — catches access violations from bad IL2CPP pointers
// ---------------------------------------------------------------------------
template <typename Fn>
static bool seh_call(Fn fn)
{
#if _WIN32
  __try {
    fn();
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
#else
  fn();
  return true;
#endif
}

// ---------------------------------------------------------------------------
// Hull name key → human-readable name
//   "Hull_L30_Destroyer_Klingon_LIVE" → "Lv.30 Destroyer Klingon"
// ---------------------------------------------------------------------------
static std::string parse_hull_key(const std::string& key)
{
  auto s = key;

  if (s.size() > 5 && s.ends_with("_LIVE"))
    s = s.substr(0, s.size() - 5);

  if (s.starts_with("Hull_"))
    s = s.substr(5);

  for (auto& c : s)
    if (c == '_') c = ' ';

  if (s.size() >= 2 && s[0] == 'L' && std::isdigit(s[1])) {
    auto space = s.find(' ');
    auto lvl   = s.substr(1, space == std::string::npos ? std::string::npos : space - 1);
    auto rest  = space == std::string::npos ? "" : s.substr(space);
    s = "Lv." + lvl + rest;
  }

  return s;
}

// ---------------------------------------------------------------------------
// Resolve hull ID → display name via SpecService
// ---------------------------------------------------------------------------
static std::string resolve_hull_name(BattleResultHeader* brh, long hullId)
{
  if (hullId == 0) return "";

  auto* specSvc = reinterpret_cast<SpecService*>(brh->get_SpecService());
  if (!specSvc) return fmt::format("Hull#{}", hullId);

  auto* hull = specSvc->GetHull(hullId);
  if (!hull) return fmt::format("Hull#{}", hullId);

  auto* nameStr = hull->Name;
  auto nameKey  = nameStr ? to_string(nameStr) : std::string{};
  if (!nameKey.empty()) return parse_hull_key(nameKey);

  return fmt::format("Hull#{}", hullId);
}

// ---------------------------------------------------------------------------
// Format "Name (Ship) vs Name (Ship)"
// ---------------------------------------------------------------------------
struct BattleSummaryData {
  std::string playerName;
  std::string enemyName;
  std::string playerShip;
  std::string enemyShip;

  /** @brief Format the summary as "Player (Ship) vs Enemy (Ship)".
   *  For NPCs (empty name), uses the ship hull name as the identifier. */
  std::string format_body() const
  {
    auto format_side = [](const std::string& name, const std::string& ship) -> std::string {
      if (!name.empty() && !ship.empty()) return fmt::format("{} ({})", name, ship);
      if (!name.empty()) return name;
      if (!ship.empty()) return ship;
      return "";
    };

    auto left  = format_side(playerName, playerShip);
    auto right = format_side(enemyName, enemyShip);
    if (left.empty() && right.empty()) return "";
    if (left.empty()) return right;
    if (right.empty()) return left;
    return left + " vs " + right;
  }
};

// ---------------------------------------------------------------------------
// Extract player/enemy names + ship hulls from BattleResultHeader
// ---------------------------------------------------------------------------
static BattleSummaryData build_battle_data(Il2CppObject* data)
{
  BattleSummaryData result;
  if (!data) return result;

  auto* brh = reinterpret_cast<BattleResultHeader*>(data);

  if (!seh_call([&] {
        auto* p       = brh->get_PlayerUserProfile();
        auto* profile = reinterpret_cast<UserProfile*>(p);
        if (profile) {
          auto* nameStr = profile->Name;
          if (nameStr) result.playerName = to_string(nameStr);
          // NPC profiles have empty names — leave blank, hull name used instead
        }
      }))
    spdlog::warn("[Notify] SEH: get_PlayerUserProfile crashed");

  if (!seh_call([&] {
        auto* e       = brh->get_EnemyUserProfile();
        auto* profile = reinterpret_cast<UserProfile*>(e);
        if (profile) {
          auto* nameStr = profile->Name;
          if (nameStr) result.enemyName = to_string(nameStr);
          // NPC profiles have empty names — leave blank, hull name used instead
        }
      }))
    spdlog::warn("[Notify] SEH: get_EnemyUserProfile crashed");

  if (!seh_call([&] {
        auto hid          = brh->PlayerShipHullId;
        result.playerShip = resolve_hull_name(brh, hid);
      }))
    spdlog::warn("[Notify] SEH: PlayerShipHullId crashed");

  if (!seh_call([&] {
        auto hid         = brh->EnemyShipHullId;
        result.enemyShip = resolve_hull_name(brh, hid);
      }))
    spdlog::warn("[Notify] SEH: EnemyShipHullId crashed");

  spdlog::info("[Notify] Battle: {} ({}) vs {} ({})", result.playerName, result.playerShip,
               result.enemyName, result.enemyShip);
  return result;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
std::string battle_notify_parse(Toast* toast)
{
  auto state = toast->get_State();

  switch (state) {
    case Victory:
    case Defeat:
    case PartialVictory:
    case StationVictory:
    case StationDefeat:
    case StationBattle:
    case IncomingAttack:
    case FleetBattle:
    case ArmadaBattleWon:
    case ArmadaBattleLost:
    case AssaultVictory:
    case AssaultDefeat:
      break;
    default:
      return {};
  }

  auto* data = toast->get_Data();
  if (!data) return {};

  auto bsd = build_battle_data(data);
  return bsd.format_body();
}
