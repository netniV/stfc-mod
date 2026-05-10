#include "patches/notification_service.h"
#include "patches/battle_notify_parser.h"

#include "config.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/LanguageManager.h>
#include <prime/Toast.h>

#include <spdlog/spdlog.h>

#include <string>

#if _WIN32
#include <windows.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>
#endif

// ---------------------------------------------------------------------------
// IL2CPP method cache
// ---------------------------------------------------------------------------
static const MethodInfo* s_localize_ltc = nullptr;
static bool              s_notification_initialized = false;

// ---------------------------------------------------------------------------
// Toast state → human-readable title
// ---------------------------------------------------------------------------
static const char* toast_state_title(int state)
{
  switch (state) {
    case Victory:                   return "Victory!";
    case Defeat:                    return "Defeat";
    case PartialVictory:            return "Partial Victory";
    case StationVictory:            return "Station Victory!";
    case StationDefeat:             return "Station Defeat";
    case StationBattle:             return "Station Under Attack!";
    case IncomingAttack:            return "Incoming Attack!";
    case IncomingAttackFaction:     return "Incoming Faction Attack!";
    case FleetBattle:               return "Fleet Battle";
    case ArmadaBattleWon:           return "Armada Victory!";
    case ArmadaBattleLost:          return "Armada Defeated";
    case ArmadaCreated:             return "Armada Created";
    case ArmadaCanceled:            return "Armada Canceled";
    case ArmadaIncomingAttack:      return "Armada Under Attack!";
    case AssaultVictory:            return "Assault Victory!";
    case AssaultDefeat:             return "Assault Defeat";
    case Tournament:                return "Event Progress";
    case ChainedEventScored:        return "Event Progress";
    case Achievement:               return "Achievement";
    case ChallengeComplete:         return "Challenge Complete";
    case ChallengeFailed:           return "Challenge Failed";
    case TakeoverVictory:           return "Takeover Victory!";
    case TakeoverDefeat:            return "Takeover Defeat";
    case TreasuryProgress:          return "Treasury Progress";
    case TreasuryFull:              return "Treasury Full";
    case WarchestProgress:          return "Warchest Progress";
    case WarchestFull:              return "Warchest Full";
    case FactionLevelUp:            return "Faction Level Up";
    case FactionLevelDown:          return "Faction Level Down";
    case FactionDiscovered:         return "Faction Discovered";
    case FactionWarning:            return "Faction Warning";
    case DiplomacyUpdated:          return "Diplomacy Updated";
    case StrikeHit:                 return "Strike Hit";
    case StrikeDefeat:              return "Strike Defeat";
    case SurgeWarmUpEnded:          return "Surge Started";
    case SurgeHostileGroupDefeated: return "Surge Hostiles Defeated";
    case SurgeTimeLeft:             return "Surge Time Warning";
    case ArenaTimeLeft:             return "Arena Time Warning";
    case FleetPresetApplied:        return "Fleet Preset Applied";
    default:                        return nullptr;
  }
}

// ---------------------------------------------------------------------------
// Platform notification delivery
// ---------------------------------------------------------------------------
#if _WIN32
static void show_system_notification(const char* title, const char* body)
{
  try {
    using namespace winrt::Windows::UI::Notifications;
    using namespace winrt::Windows::Data::Xml::Dom;

    auto xml   = ToastNotificationManager::GetTemplateContent(ToastTemplateType::ToastText02);
    auto nodes = xml.GetElementsByTagName(L"text");
    nodes.Item(0).InnerText(winrt::to_hstring(title));
    nodes.Item(1).InnerText(winrt::to_hstring(body));

    auto notification = ToastNotification(xml);
    auto notifier     = ToastNotificationManager::CreateToastNotifier(L"Star Trek Fleet Command");
    notifier.Show(notification);
  } catch (const winrt::hresult_error& e) {
    spdlog::warn("[Notify] WinRT notification failed: {}", winrt::to_string(e.message()));
  } catch (...) {
    spdlog::warn("[Notify] WinRT notification failed (unknown error)");
  }
}
#endif

// ---------------------------------------------------------------------------
// Resolve basic localized text from a Toast's TextLocaleTextContext
// ---------------------------------------------------------------------------
static std::string resolve_toast_text(Toast* toast)
{
  if (!s_localize_ltc) return {};

  auto* ltc = toast->get_TextLocaleTextContext();
  if (!ltc) return {};

  auto* langMgr = LanguageManager::Instance();
  if (!langMgr) return {};

  Il2CppString*  resolved = nullptr;
  void*          params[2] = { &resolved, ltc };
  Il2CppException* exc = nullptr;
  il2cpp_runtime_invoke(s_localize_ltc, langMgr, params, &exc);

  if (exc || !resolved) return {};
  return to_string(resolved);
}

// ---------------------------------------------------------------------------
// Strip Unity rich text tags (e.g. <color=#FF0000>, <b>, </size>)
// ---------------------------------------------------------------------------
static std::string strip_unity_rich_text(const std::string& s)
{
  std::string result;
  result.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    if (s[i] == '<') {
      auto end = s.find('>', i);
      if (end != std::string::npos) { i = end + 1; continue; }
    }
    result += s[i++];
  }
  return result;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void notification_init()
{
  if (s_notification_initialized) {
    return;
  }

  // Resolve LanguageManager::Localize(out string, LocaleTextContext) — the
  // 2-parameter overload that takes an LTC and returns a localized string.
  auto lm_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Localization", "LanguageManager");
  if (lm_helper.isValidHelper()) {
    auto* cls = lm_helper.get_cls();
    if (cls) {
      void* iter = nullptr;
      while (auto* method = il2cpp_class_get_methods(cls, &iter)) {
        auto name = std::string_view(il2cpp_method_get_name(method));
        auto pc   = il2cpp_method_get_param_count(method);
        if (name == "Localize" && pc == 2) {
          s_localize_ltc = method;
          spdlog::info("[Notify] Resolved LanguageManager::Localize(out, LTC) at {:p}", (const void*)method);
          break;
        }
      }
    }
  }

  if (!s_localize_ltc) {
    spdlog::warn("[Notify] Could not resolve LanguageManager::Localize — notifications will show titles only");
  }

#if _WIN32
  try { winrt::init_apartment(); } catch (...) {}
  spdlog::info("[Notify] Windows notification service initialized");
#else
  spdlog::info("[Notify] Notification service: platform not supported (no-op)");
#endif

  s_notification_initialized = true;
}

void notification_show(const char* title, const char* body)
{
#if _WIN32
  show_system_notification(title, body);
#endif
}

void notification_handle_toast(Toast* toast)
{
#if !_WIN32
  return; // No notification delivery on non-Windows platforms yet
#else
  auto state = toast->get_State();

  // Check if this toast type is in the user's notify list
  const auto& notify_types = Config::Get().notify_banner_types;
  if (std::ranges::find(notify_types, state) == notify_types.end()) {
    return;
  }

  auto* title = toast_state_title(state);
  if (!title) {
    spdlog::debug("[Notify] No title mapping for toast state {}, skipping", state);
    return;
  }

  auto body = battle_notify_parse(toast);
  if (body.empty()) {
    body = strip_unity_rich_text(resolve_toast_text(toast));
  }
  if (body.empty()) {
    body = "(no details available)";
  }

  spdlog::info("[Notify] {} — {}", title, body);
  show_system_notification(title, body.c_str());
#endif
}
