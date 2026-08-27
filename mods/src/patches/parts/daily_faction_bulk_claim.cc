#include "config.h"
#include "errormsg.h"
#include "str_utils.h"

#include <prime/GameObject.h>
#include <prime/IList.h>
#include <prime/Transform.h>

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>

#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

namespace
{
constexpr std::string_view kFactionPointResourcePrefix = "Resource_FactionPoint_";

class ResourceSpec
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ResourceSpec");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  {
    return get_class_helper();
  }
};

class InventoryItem
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "InventoryItem");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  {
    return get_class_helper();
  }
};

class MissionViewController
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Missions", "MissionViewController");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  {
    return get_class_helper();
  }

  Il2CppObject* ClaimAllToggle()
  {
    static auto field = get_class_helper().GetField("_claimAllToggle");
    return field.isValidHelper() ? *(Il2CppObject**)((ptrdiff_t)this + field.offset()) : nullptr;
  }
};

Transform* GetTransformOf(Il2CppObject* component)
{
  if (component == nullptr || component->klass == nullptr) {
    return nullptr;
  }
  auto helper = IL2CppClassHelper{component->klass}.GetParent("Component");
  if (!helper.isValidHelper()) {
    return nullptr;
  }
  return helper.GetProperty("transform").GetRaw<Transform>(component);
}

Transform* FindChildByName(Transform* root, const char* name)
{
  if (root == nullptr) {
    return nullptr;
  }

  auto* go = root->gameObject;
  if (go != nullptr && go->Name() == name) {
    return root;
  }

  for (int32_t i = 0; i < root->childCount; ++i) {
    if (auto* found = FindChildByName(root->GetChild(i), name)) {
      return found;
    }
  }

  return nullptr;
}

struct FactionIconName {
  const char* faction;
  const char* game_object_name;
};

constexpr FactionIconName kFactionIconNames[] = {
    {"federation", "Fed_Icon"},
    {"romulan", "Rom_Icon"},
    {"klingon", "Klg_Icon"},
};

void ApplyFactionIconVisibility(Transform* toggle_tf)
{
  if (toggle_tf == nullptr) {
    return;
  }

  auto* parent = toggle_tf->parent;
  if (parent == nullptr) {
    spdlog::debug("[DailyFactionBulkClaim] ApplyFactionIconVisibility: toggle has no parent Transform");
    return;
  }

  const auto& configured = Config::Get().daily_bulk_claim_factions;

  for (const auto& entry : kFactionIconNames) {
    auto* icon_transform = FindChildByName(parent, entry.game_object_name);
    if (icon_transform == nullptr) {
      spdlog::debug("[DailyFactionBulkClaim] ApplyFactionIconVisibility: '{}' not found", entry.game_object_name);
      continue;
    }

    auto* go = icon_transform->gameObject;
    if (go == nullptr) {
      continue;
    }

    const bool should_show =
        configured.empty() || std::ranges::find(configured, std::string(entry.faction)) != configured.end();
    go->SetActive(should_show);
  }
}

class UnityEventSystem
{
public:
  static Il2CppObject* Current()
  {
    static auto class_helper =
        il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.EventSystems", "EventSystem");
    static auto get_current = class_helper.GetMethodInfo("get_current", 0);
    if (get_current == nullptr) {
      return nullptr;
    }
    Il2CppException* exception = nullptr;
    auto*            result    = il2cpp_runtime_invoke(get_current, nullptr, nullptr, &exception);
    return exception != nullptr ? nullptr : result;
  }
};

class UnityToggle
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Toggle");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  {
    return get_class_helper();
  }

  static bool IsOn(Il2CppObject* toggle)
  {
    if (toggle == nullptr) {
      return false;
    }
    static auto get_is_on = get_class_helper().GetMethodInfo("get_isOn", 0);
    if (get_is_on == nullptr) {
      return false;
    }
    Il2CppException* exception = nullptr;
    auto*            result    = il2cpp_runtime_invoke(get_is_on, toggle, nullptr, &exception);
    if (exception != nullptr || result == nullptr) {
      return false;
    }
    return *(bool*)il2cpp_object_unbox(result);
  }

  static void SimulateClick(Il2CppObject* toggle)
  {
    if (toggle == nullptr) {
      return;
    }

    static auto on_pointer_click = get_class_helper().GetMethodInfoSpecial("OnPointerClick", [](auto count, auto) {
      return count == 1;
    });
    if (on_pointer_click == nullptr) {
      spdlog::debug("[DailyFactionBulkClaim] UnityToggle::SimulateClick: OnPointerClick(PointerEventData) not "
                   "found");
      return;
    }

    static auto event_data_helper =
        il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.EventSystems", "PointerEventData");
    if (event_data_helper.get_cls() == nullptr) {
      spdlog::debug("[DailyFactionBulkClaim] UnityToggle::SimulateClick: PointerEventData class not found");
      return;
    }
    static auto ctor = event_data_helper.GetMethodInfoSpecial(".ctor", [](auto count, auto) { return count == 1; });
    if (ctor == nullptr) {
      spdlog::debug("[DailyFactionBulkClaim] UnityToggle::SimulateClick: PointerEventData(EventSystem) ctor not "
                   "found");
      return;
    }

    auto* event_data = il2cpp_object_new(event_data_helper.get_cls());
    if (event_data == nullptr) {
      spdlog::debug("[DailyFactionBulkClaim] UnityToggle::SimulateClick: PointerEventData allocation failed");
      return;
    }

    Il2CppException* exception    = nullptr;
    auto*            event_system = UnityEventSystem::Current();
    void*            ctor_args[1] = {event_system};
    il2cpp_runtime_invoke(ctor, event_data, ctor_args, &exception);
    if (exception != nullptr) {
      char buffer[256] = {};
      il2cpp_format_exception(exception, buffer, sizeof(buffer));
      spdlog::debug("[DailyFactionBulkClaim] UnityToggle::SimulateClick: PointerEventData ctor threw: {}", buffer);
      return;
    }

    exception       = nullptr;
    void* args[1]   = {event_data};
    il2cpp_runtime_invoke(on_pointer_click, toggle, args, &exception);
    if (exception != nullptr) {
      char buffer[256] = {};
      il2cpp_format_exception(exception, buffer, sizeof(buffer));
      spdlog::debug("[DailyFactionBulkClaim] UnityToggle::SimulateClick: OnPointerClick invoke threw: {}", buffer);
    }
  }
};

bool          g_applied_default_toggle_on      = false;
int           g_default_toggle_attempts        = 0;
Il2CppObject* g_active_mission_view_controller = nullptr;

constexpr int kMaxDefaultToggleAttempts = 300;

void ApplyDefaultToggleOnOnce(MissionViewController* controller)
{
  auto* toggle = controller != nullptr ? controller->ClaimAllToggle() : nullptr;
  if (g_applied_default_toggle_on || !Config::Get().daily_bulk_claim_toggle_default_on || toggle == nullptr) {
    return;
  }

  if (UnityToggle::IsOn(toggle)) {
    g_applied_default_toggle_on = true;
    return;
  }

  if (++g_default_toggle_attempts > kMaxDefaultToggleAttempts) {
    spdlog::warn("[DailyFactionBulkClaim] daily_bulk_claim_toggle_default_on: giving up, toggle never became "
                "interactable");
    g_applied_default_toggle_on = true;
    return;
  }

  UnityToggle::SimulateClick(toggle);

  if (UnityToggle::IsOn(toggle)) {
    spdlog::debug("[DailyFactionBulkClaim] daily_bulk_claim_toggle_default_on: forced Claim All toggle on "
                 "(attempt {})",
                 g_default_toggle_attempts);
    g_applied_default_toggle_on = true;
  }
}

bool FactionMatches(const std::string& faction_name, const std::vector<std::string>& configured)
{
  if (faction_name.empty() || configured.empty()) {
    return false;
  }
  return std::ranges::find(configured, AsciiStrToLower(faction_name)) != configured.end();
}

std::string FactionNameOfTournament(Il2CppObject* tournament)
{
  if (tournament == nullptr || tournament->klass == nullptr) {
    return {};
  }

  auto list_field = IL2CppClassHelper{tournament->klass}.GetField("_factionPointRewards");
  if (!list_field.isValidHelper()) {
    return {};
  }

  auto* list_obj = *(Il2CppObject**)((char*)tournament + list_field.offset());
  if (list_obj == nullptr) {
    return {};
  }

  auto* list = (IList*)list_obj;
  for (int32_t i = 0; i < list->Count; ++i) {
    auto* item = list->Get(i);
    if (item == nullptr || item->klass == nullptr) {
      continue;
    }

    auto spec_field = IL2CppClassHelper{item->klass}.GetField("_resourceSpec");
    if (!spec_field.isValidHelper()) {
      continue;
    }

    auto* spec = *(Il2CppObject**)((char*)item + spec_field.offset());
    if (spec == nullptr || spec->klass == nullptr) {
      continue;
    }

    auto id_field = IL2CppClassHelper{spec->klass}.GetField("resourceId_");
    if (!id_field.isValidHelper()) {
      continue;
    }

    auto* id_str = *(Il2CppString**)((char*)spec + id_field.offset());
    if (id_str == nullptr) {
      continue;
    }

    auto resource_id = to_string(id_str);
    if (resource_id.starts_with(kFactionPointResourcePrefix)) {
      return resource_id.substr(kFactionPointResourcePrefix.size());
    }
  }

  return {};
}

void FilterClaimableDailiesByFaction(IList* list, const std::vector<std::string>& configured)
{
  if (list == nullptr) {
    spdlog::debug("[DailyFactionBulkClaim] GetClaimableDailiesList returned null; nothing to filter");
    return;
  }

  const auto total = list->Count;
  spdlog::debug("[DailyFactionBulkClaim] filtering {} claimable daily(ies) against configured factions [{}]", total,
               fmt::join(configured, ", "));

  int kept = 0;
  for (int32_t i = total - 1; i >= 0; --i) {
    auto* tournament   = list->Get(i);
    auto  faction_name = FactionNameOfTournament(tournament);
    auto  matches      = FactionMatches(faction_name, configured);

    spdlog::debug("[DailyFactionBulkClaim]   [{}] faction: '{}' -> {}", i,
                 faction_name.empty() ? "<none>" : faction_name,
                 faction_name.empty() ? "keep (not a faction daily)" : (matches ? "keep (claim)" : "remove (skip)"));

    if (!faction_name.empty() && !matches) {
      list->RemoveAt(i);
    } else {
      ++kept;
    }
  }

  spdlog::debug("[DailyFactionBulkClaim] kept {}/{} daily(ies) for claiming", kept, total);
}

IList* MissionViewController_GetClaimableDailiesList(auto original, Il2CppObject* _this)
{
  auto* list = (IList*)original(_this);

  auto* controller = (MissionViewController*)_this;
  ApplyFactionIconVisibility(GetTransformOf(controller->ClaimAllToggle()));

  const auto& configured = Config::Get().daily_bulk_claim_factions;
  if (configured.empty()) {
    return list;
  }

  FilterClaimableDailiesByFaction(list, configured);
  return list;
}

void MissionViewController_AboutToShow(auto original, Il2CppObject* _this)
{
  original(_this);

  auto* controller = (MissionViewController*)_this;
  auto* toggle     = controller->ClaimAllToggle();
  auto* toggle_tf  = GetTransformOf(toggle);

  ApplyFactionIconVisibility(toggle_tf);

  g_active_mission_view_controller = _this;
}

void MissionViewController_AboutToHide(auto original, Il2CppObject* _this)
{
  auto* controller = (MissionViewController*)_this;
  auto* toggle_tf  = GetTransformOf(controller->ClaimAllToggle());
  auto* parent     = toggle_tf != nullptr ? toggle_tf->parent : nullptr;

  if (parent != nullptr) {
    for (const auto& entry : kFactionIconNames) {
      if (auto* icon_transform = FindChildByName(parent, entry.game_object_name)) {
        if (auto* go = icon_transform->gameObject) {
          go->SetActive(true);
        }
      }
    }
  }

  if (g_active_mission_view_controller == _this) {
    g_active_mission_view_controller = nullptr;
  }

  original(_this);
}
} // namespace

void DailyFactionBulkClaimUpdate()
{
  if (g_active_mission_view_controller != nullptr) {
    ApplyDefaultToggleOnOnce((MissionViewController*)g_active_mission_view_controller);
  }
}

void InstallDailyFactionBulkClaimHooks()
{
  auto resource_spec = ResourceSpec::ClassHelper();
  if (!resource_spec.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.PrimeServer.Models", "ResourceSpec");
  } else if (!resource_spec.GetField("resourceId_").isValidHelper()) {
    ErrorMsg::MissingMethod("ResourceSpec", "resourceId_");
  }

  auto inventory_item = InventoryItem::ClassHelper();
  if (!inventory_item.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.PrimeServer.Models", "InventoryItem");
  } else if (!inventory_item.GetField("_resourceSpec").isValidHelper()) {
    ErrorMsg::MissingMethod("InventoryItem", "_resourceSpec");
  }

  auto unity_toggle = UnityToggle::ClassHelper();
  if (!unity_toggle.isValidHelper()) {
    ErrorMsg::MissingHelper("UnityEngine.UI", "Toggle");
  } else {
    if (unity_toggle.GetMethodInfo("get_isOn", 0) == nullptr) {
      ErrorMsg::MissingMethod("Toggle", "get_isOn");
    }
    if (unity_toggle.GetMethodInfoSpecial("OnPointerClick", [](auto count, auto) { return count == 1; })
        == nullptr) {
      ErrorMsg::MissingMethod("Toggle", "OnPointerClick");
    }
  }

  if (il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.EventSystems", "PointerEventData").get_cls()
      == nullptr) {
    ErrorMsg::MissingHelper("UnityEngine.EventSystems", "PointerEventData");
  }

  auto mission_view_controller = MissionViewController::ClassHelper();
  if (!mission_view_controller.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Missions", "MissionViewController");
    return;
  }

  if (auto ptr = mission_view_controller.GetMethod("GetClaimableDailiesList", 0); ptr == nullptr) {
    ErrorMsg::MissingMethod("MissionViewController", "GetClaimableDailiesList");
  } else {
    SPUD_STATIC_DETOUR(ptr, MissionViewController_GetClaimableDailiesList);
  }

  if (auto ptr = mission_view_controller.GetMethod("AboutToShow", 0); ptr == nullptr) {
    ErrorMsg::MissingMethod("MissionViewController", "AboutToShow");
  } else {
    SPUD_STATIC_DETOUR(ptr, MissionViewController_AboutToShow);
  }

  if (auto ptr = mission_view_controller.GetMethod("AboutToHide", 0); ptr == nullptr) {
    ErrorMsg::MissingMethod("MissionViewController", "AboutToHide");
  } else {
    SPUD_STATIC_DETOUR(ptr, MissionViewController_AboutToHide);
  }
}
