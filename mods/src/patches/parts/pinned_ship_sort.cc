#include "config.h"
#include "errormsg.h"
#include "ship_name_match.h"

#include <prime/FleetPlayerData.h>
#include <prime/HullSpec.h>

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <cstdint>
#include <vector>

// Fleet management dock ship sort: hooks ShipManagementViewContext.Sort()
// and stable-partitions cfg.pinned_ships to the front of SortedIdleShips,
// keeping the active sort field's order within both the pinned group and
// everyone else. Only the concrete class is hooked, not ISortedFleetsContext,
// so HailingFrequenciesInventoryContext is unaffected. Matching is shared
// with the instant-warp filters (ship_name_match.h). If multiple ships share
// a pinned name, only the highest-level unclaimed one is pinned.

namespace {

struct PinnedShipSortState {
  IL2CppClassHelper* shipManagementViewContext = nullptr;
  IL2CppFieldHelper* sortedIdleShipsField      = nullptr;
  bool               valid                     = false;
};

PinnedShipSortState& State()
{
  static PinnedShipSortState s;
  return s;
}

bool InitializeState()
{
  auto& s = State();

  s.shipManagementViewContext = new IL2CppClassHelper(
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Ships", "ShipManagementViewContext"));
  if (!s.shipManagementViewContext->get_cls()) {
    ErrorMsg::MissingHelper("Digit.Prime.Ships", "ShipManagementViewContext");
    return false;
  }

  s.sortedIdleShipsField = new IL2CppFieldHelper(s.shipManagementViewContext->GetField("SortedIdleShips"));
  if (!s.sortedIdleShipsField->isValidHelper()) {
    spdlog::error("[PinnedShipSort] ShipManagementViewContext field layout changed");
    return false;
  }

  s.valid = true;
  return true;
}

struct ShipEntry {
  void*                                 item;
  std::vector<std::vector<std::string>> match_words; // word-sequences from HullSpec.Name and HullSpec.IdStr
  int64_t                               level;
  std::string                           debug_name;  // raw HullSpec.Name, for diagnostics
  std::string                           debug_idstr; // raw HullSpec.IdStr, for diagnostics
};

ShipEntry BuildShipEntry(void* item)
{
  ShipEntry entry{item, {}, 0, {}, {}};

  auto* ship = reinterpret_cast<FleetPlayerData*>(item);
  if (!ship) return entry;

  entry.level       = ship->Level;
  entry.match_words = ShipNameMatch::CandidateWords(ship, &entry.debug_name, &entry.debug_idstr);
  return entry;
}

// Stable-partitions `list` (a List<FleetPlayerData>) so that ships matching
// cfg.pinned_ships come first, while both the pinned group and everyone else
// keep their existing relative order (i.e. whatever sort field is active).
void ReorderPinnedShips(void* list)
{
  if (!list) return;

  const auto& cfg = Config::Get();
  if (cfg.pinned_ships.empty()) return;

  auto* listClass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(list));
  if (!listClass) return;

  auto* getCount = il2cpp_class_get_method_from_name(listClass, "get_Count", 0);
  auto* getItem  = il2cpp_class_get_method_from_name(listClass, "get_Item", 1);
  auto* clear    = il2cpp_class_get_method_from_name(listClass, "Clear", 0);
  auto* add      = il2cpp_class_get_method_from_name(listClass, "Add", 1);
  if (!getCount || !getItem || !clear || !add) {
    spdlog::warn("[PinnedShipSort] SortedIdleShips list is missing expected List<T> methods");
    return;
  }

  Il2CppException* exc = nullptr;
  auto* countObj = il2cpp_runtime_invoke(getCount, list, nullptr, &exc);
  if (exc || !countObj) return;
  const auto count = *reinterpret_cast<int32_t*>(il2cpp_object_unbox(countObj));
  if (count <= 1) return;
  if (count > 2000) {
    spdlog::warn("[PinnedShipSort] SortedIdleShips reported an implausible count ({}); skipping", count);
    return;
  }

  // cfg.pinned_ships entries are already normalized (see parse_ship_filter /
  // ShipNameMatch::NormalizeKey in config.cc); just split them into words.
  std::vector<std::vector<std::string>> pinned_words;
  pinned_words.reserve(cfg.pinned_ships.size());
  for (const auto& name : cfg.pinned_ships) pinned_words.push_back(ShipNameMatch::SplitWords(name));

  std::vector<ShipEntry> ships;
  ships.reserve(count);
  for (int32_t i = 0; i < count; ++i) {
    void* idxArgs[1] = {&i};
    exc              = nullptr;
    auto* item       = il2cpp_runtime_invoke(getItem, list, idxArgs, &exc);
    if (exc) item = nullptr;

    auto entry = BuildShipEntry(item);
    spdlog::debug("[PinnedShipSort] idle ship: name='{}' idstr='{}' level={}", entry.debug_name,
                 entry.debug_idstr, entry.level);
    ships.push_back(std::move(entry));
  }

  // Pinned ships get rank 0, everyone else rank 1; the stable_sort below then
  // preserves each group's existing (actively-sorted) relative order.
  constexpr int    kPinnedRank   = 0;
  constexpr int    kUnpinnedRank = 1;
  std::vector<int> ranks(count, kUnpinnedRank);

  bool any_pinned = false;
  for (size_t p = 0; p < pinned_words.size(); ++p) {
    if (pinned_words[p].empty()) continue;

    int     best_index = -1;
    int64_t best_level = -1;
    for (int32_t i = 0; i < count; ++i) {
      if (ranks[i] != kUnpinnedRank) continue; // already claimed by an earlier pinned_ships entry
      if (!ShipNameMatch::MatchesAny(ships[i].match_words, pinned_words[p])) continue;
      if (ships[i].level > best_level) {
        best_level = ships[i].level;
        best_index = i;
      }
    }

    if (best_index >= 0) {
      ranks[best_index] = kPinnedRank;
      any_pinned        = true;
    } else {
      spdlog::warn("[PinnedShipSort] pinned_ships entry '{}' matched no idle ship (check the debug log above "
                   "for each ship's actual name/idstr)",
                   cfg.pinned_ships[p]);
    }
  }

  if (!any_pinned) return;

  std::vector<void*> items;
  items.reserve(count);
  for (const auto& entry : ships) items.push_back(entry.item);

  std::vector<int> order(count);
  for (int32_t i = 0; i < count; ++i) order[i] = i;
  std::ranges::stable_sort(order, [&](int a, int b) { return ranks[a] < ranks[b]; });

  bool changed = false;
  for (int32_t i = 0; i < count; ++i) {
    if (order[i] != i) {
      changed = true;
      break;
    }
  }
  if (!changed) return;

  exc = nullptr;
  il2cpp_runtime_invoke(clear, list, nullptr, &exc);
  if (exc) {
    spdlog::warn("[PinnedShipSort] list.Clear raised an exception; aborting reorder");
    return;
  }

  for (int32_t i = 0; i < count; ++i) {
    void* addArgs[1] = {items[order[i]]};
    exc               = nullptr;
    il2cpp_runtime_invoke(add, list, addArgs, &exc);
    if (exc) {
      spdlog::warn("[PinnedShipSort] list.Add raised an exception while re-inserting an item");
    }
  }
}

void ShipManagementViewContext_Sort_Hook(auto original, void* _this)
{
  original(_this);

  auto& s = State();
  if (!s.valid) return;

  auto* list = *reinterpret_cast<void**>(reinterpret_cast<char*>(_this) + s.sortedIdleShipsField->offset());
  ReorderPinnedShips(list);
}

} // namespace

void InstallPinnedShipSortHooks()
{
  if (!InitializeState()) {
    spdlog::error("[PinnedShipSort] initialization failed; hooks not installed");
    return;
  }

  auto& s = State();

  auto sortMethod = s.shipManagementViewContext->GetMethod("Sort", 0);
  if (!sortMethod) {
    ErrorMsg::MissingMethod("ShipManagementViewContext", "Sort");
    return;
  }

  SPUD_STATIC_DETOUR(sortMethod, ShipManagementViewContext_Sort_Hook);
  spdlog::info("Pinned ship sort: hooks installed");
}
