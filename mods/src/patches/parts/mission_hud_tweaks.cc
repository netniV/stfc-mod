#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace
{
struct MissionHudButtonDefinition {
  const char*          canonical_name;
  const char*          field_name;
  MissionHudVisibility visibility   = MissionHudVisibility::Auto;
  ptrdiff_t            field_offset = 0;
  bool                 field_valid  = false;
};

using ComponentGetGameObjectFn = void* (*)(void*);
using GameObjectSetActiveFn    = void (*)(void*, bool);

std::array<MissionHudButtonDefinition, 4> g_button_definitions{{
    {"q_trials", "_challengesButton"},
    {"field_training", "_achievementsButton"},
    {"outposts", "_outpostsButton"},
    {"missions", "_missionsButton"},
}};

std::vector<MissionHudButtonDefinition*> g_configured_buttons;
ComponentGetGameObjectFn                 g_get_game_object = nullptr;
GameObjectSetActiveFn                    g_set_active      = nullptr;

std::string_view to_string(MissionHudVisibility visibility)
{
  switch (visibility) {
    case MissionHudVisibility::Always:
      return "always";
    case MissionHudVisibility::Never:
      return "never";
    case MissionHudVisibility::Auto:
    default:
      return "auto";
  }
}

std::vector<MissionHudButtonDefinition*> LoadConfiguredButtons()
{
  std::vector<MissionHudButtonDefinition*> configured_buttons;
  for (auto& definition : g_button_definitions) {
    definition.visibility = Config::Get().MissionHudButtonVisibility(definition.canonical_name);
    if (definition.visibility == MissionHudVisibility::Auto) {
      continue;
    }

    configured_buttons.emplace_back(&definition);
  }
  return configured_buttons;
}

std::string ConfiguredButtonModes()
{
  std::string modes;
  for (const auto* button : g_configured_buttons) {
    if (!modes.empty()) {
      modes.append(", ");
    }
    modes.append(button->canonical_name);
    modes.append("=");
    modes.append(to_string(button->visibility));
  }
  return modes;
}

bool ResolveButtonFields(IL2CppClassHelper& controller_helper)
{
  auto valid_count = 0;
  for (auto* button : g_configured_buttons) {
    auto field = controller_helper.GetField(button->field_name);
    if (!field.isValidHelper()) {
      spdlog::error("MissionHudTweaks: unable to find MissionsHudViewController field '{}'", button->field_name);
      continue;
    }

    button->field_offset = field.offset();
    button->field_valid  = true;
    valid_count++;
    spdlog::info("MissionHudTweaks: mapped {} -> {}", button->canonical_name, button->field_name);
  }

  return valid_count > 0;
}

void ApplyButtonVisibility(void* controller, const MissionHudButtonDefinition& button)
{
  if (!controller || !button.field_valid || !g_get_game_object || !g_set_active) {
    return;
  }

  auto* component = *reinterpret_cast<void**>(reinterpret_cast<char*>(controller) + button.field_offset);
  if (!component) {
    return;
  }

  if (auto* game_object = g_get_game_object(component)) {
    g_set_active(game_object, button.visibility == MissionHudVisibility::Always);
  }
}
} // namespace

// Visibility is now refreshed through several independent paths. Keep the game's
// setup/notification work, then apply only explicitly configured overrides.
void ApplyConfiguredButtonVisibility(void* controller)
{
  for (const auto* button : g_configured_buttons) {
    ApplyButtonVisibility(controller, *button);
  }
}

void MissionsHudViewController_OnEnable_Hook(auto original, void* controller)
{
  original(controller);
  ApplyConfiguredButtonVisibility(controller);
}

void MissionsHudViewController_SetupAchievementsButton_Hook(auto original, void* controller)
{
  original(controller);
  ApplyConfiguredButtonVisibility(controller);
}

void MissionsHudViewController_SetupChallengesButton_Hook(auto original, void* controller, bool replace_with_outposts)
{
  original(controller, replace_with_outposts);
  ApplyConfiguredButtonVisibility(controller);
}

void MissionsHudViewController_SetupOutpostsButton_Hook(auto original, void* controller, bool show_outposts)
{
  original(controller, show_outposts);
  ApplyConfiguredButtonVisibility(controller);
}

void MissionsHudViewController_HandleOutpostsAndChallengesHUD_Hook(auto original, void* controller)
{
  // In planetary view this method hides challenges directly, bypassing its setup
  // method. Reapply after the whole operation, including the planetary branch.
  original(controller);
  ApplyConfiguredButtonVisibility(controller);
}

void InstallMissionHudTweaksHooks()
{
  g_configured_buttons = LoadConfiguredButtons();
  if (g_configured_buttons.empty()) {
    spdlog::warn("MissionHudTweaks: no mission HUD button overrides are configured");
    return;
  }

  auto controller_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "MissionsHudViewController");
  if (!controller_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.HUD", "MissionsHudViewController");
    return;
  }

  if (!ResolveButtonFields(controller_helper)) {
    return;
  }

  auto component_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
  if (!component_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("UnityEngine", "Component");
    return;
  }

  g_get_game_object = reinterpret_cast<ComponentGetGameObjectFn>(component_helper.GetMethod("get_gameObject"));
  if (!g_get_game_object) {
    ErrorMsg::MissingMethod("Component", "get_gameObject");
    return;
  }

  auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  if (!game_object_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("UnityEngine", "GameObject");
    return;
  }

  g_set_active = reinterpret_cast<GameObjectSetActiveFn>(game_object_helper.GetMethod("SetActive", 1));
  if (!g_set_active) {
    ErrorMsg::MissingMethod("GameObject", "SetActive");
    return;
  }

  // Resolve the complete surface before installing anything. Avoid the tiny
  // planetary/outpost event wrappers: these substantive methods own the work.
  auto on_enable = controller_helper.GetMethod("OnEnable", 0);
  auto achievements = controller_helper.GetMethod("SetupAchievementsButton", 0);
  auto challenges = controller_helper.GetMethod("SetupChallengesButton", 1);
  auto outposts = controller_helper.GetMethod("SetupOutpostsButton", 1);
  auto combined = controller_helper.GetMethod("HandleOutpostsAndChallengesHUD", 0);
  if (!on_enable || !achievements || !challenges || !outposts || !combined) {
    spdlog::error("MissionHudTweaks: current HUD lifecycle/setup methods are missing; overrides disabled");
    return;
  }

  spdlog::info("MissionHudTweaks: applying {}", ConfiguredButtonModes());
  const bool enabled = SPUD_STATIC_DETOUR(on_enable, MissionsHudViewController_OnEnable_Hook);
  const bool achievement_hook = SPUD_STATIC_DETOUR(achievements, MissionsHudViewController_SetupAchievementsButton_Hook);
  const bool challenge_hook = SPUD_STATIC_DETOUR(challenges, MissionsHudViewController_SetupChallengesButton_Hook);
  const bool outpost_hook = SPUD_STATIC_DETOUR(outposts, MissionsHudViewController_SetupOutpostsButton_Hook);
  const bool combined_hook = SPUD_STATIC_DETOUR(combined, MissionsHudViewController_HandleOutpostsAndChallengesHUD_Hook);
  if (enabled && achievement_hook && challenge_hook && outpost_hook && combined_hook) {
    spdlog::info("MissionHudTweaks: installed current HUD lifecycle/setup hooks");
  } else {
    // Successfully installed detours still call through, but must not partially
    // enforce preferences when another refresh path could undo them.
    g_configured_buttons.clear();
    spdlog::error("MissionHudTweaks: hook installation incomplete; overrides disabled");
  }
}
