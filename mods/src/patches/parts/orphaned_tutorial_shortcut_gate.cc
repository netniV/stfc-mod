#include "orphaned_tutorial_shortcut_gate.h"

#include "config.h"
#include "patches/key.h"

#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>

#include <cstring>

namespace
{
constexpr uint64_t kOrphanedTutorialShortcutGateSampleFrames = 60;

using InstanceBoolGetter = bool (*)(void*);
using FindActionMethod   = void* (*)(void*, Il2CppString*, bool);

struct OrphanedTutorialShortcutBindings {
  FieldInfo*        shortcuts_actions_field          = nullptr;
  FieldInfo*        shortcuts_show_keybindings_field = nullptr;
  const MethodInfo* get_can_use_shortcuts            = nullptr;

  const MethodInfo*  get_video_player_manager = nullptr;
  const MethodInfo*  get_shop_scene_manager   = nullptr;
  const MethodInfo*  get_tutorial_manager     = nullptr;
  InstanceBoolGetter get_is_video_playing     = nullptr;

  const MethodInfo* get_is_tutorial_active                 = nullptr;
  const MethodInfo* get_is_tutorial_blocking               = nullptr;
  FieldInfo*        tutorial_is_active_field               = nullptr;
  FieldInfo*        tutorial_ui_field                      = nullptr;
  FieldInfo*        tutorial_mission_field                 = nullptr;
  FieldInfo*        tutorial_objective_field               = nullptr;
  FieldInfo*        tutorial_data_field                    = nullptr;
  FieldInfo*        tutorial_component_field               = nullptr;
  FieldInfo*        tutorial_items_field                   = nullptr;
  FieldInfo*        tutorial_end_step_field                = nullptr;
  FieldInfo*        tutorial_next_step_field               = nullptr;
  FieldInfo*        tutorial_step_field                    = nullptr;
  FieldInfo*        tutorial_step_index_field              = nullptr;
  FieldInfo*        tutorial_next_action_id_field          = nullptr;
  FieldInfo*        tutorial_objective_being_cleared_field = nullptr;
  FieldInfo*        tutorial_target_section_field          = nullptr;

  const MethodInfo* get_is_message_box_visible = nullptr;
  FieldInfo*        block_touch_field          = nullptr;

  const MethodInfo*  ui_manager_instance_getter = nullptr;
  InstanceBoolGetter get_is_popup_visible       = nullptr;

  FieldInfo*         plc_offer_popup_field = nullptr;
  InstanceBoolGetter is_open               = nullptr;

  InstanceBoolGetter asset_enabled  = nullptr;
  FindActionMethod   find_action    = nullptr;
  InstanceBoolGetter action_enabled = nullptr;

  bool complete = false;
};

template <typename T> bool ReadInstanceField(void* instance, FieldInfo* field, T& value)
{
  if (!instance || !field) {
    return false;
  }

  const auto* address = reinterpret_cast<const char*>(instance) + il2cpp_field_get_offset(field);
  std::memcpy(&value, address, sizeof(T));
  return true;
}

template <typename T> bool ReadObjectField(void* object, const char* field_name, T& value)
{
  if (!object) {
    return false;
  }

  auto* klass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(object));
  auto* field = klass ? il2cpp_class_get_field_from_name(klass, field_name) : nullptr;
  return ReadInstanceField(object, field, value);
}

bool InvokeStaticObjectGetter(const MethodInfo* method, void*& value)
{
  value = nullptr;
  if (!method) {
    return false;
  }

  Il2CppException* exception = nullptr;
  value                      = il2cpp_runtime_invoke(method, nullptr, nullptr, &exception);
  return exception == nullptr;
}

bool InvokeStaticBoolGetter(const MethodInfo* method, bool& value)
{
  value = false;
  if (!method) {
    return false;
  }

  Il2CppException* exception = nullptr;
  auto*            result    = il2cpp_runtime_invoke(method, nullptr, nullptr, &exception);
  if (exception || !result) {
    return false;
  }

  auto* unboxed = il2cpp_object_unbox(result);
  if (!unboxed) {
    return false;
  }

  value = *reinterpret_cast<bool*>(unboxed);
  return true;
}

OrphanedTutorialShortcutBindings ResolveBindings()
{
  OrphanedTutorialShortcutBindings bindings;

  auto shortcuts = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
  if (auto* klass = shortcuts.get_cls(); klass) {
    bindings.shortcuts_actions_field          = il2cpp_class_get_field_from_name(klass, "_actions");
    bindings.shortcuts_show_keybindings_field = il2cpp_class_get_field_from_name(klass, "_showKeybindings");
    bindings.get_can_use_shortcuts            = shortcuts.GetMethodInfo("get_CanUseShortcuts");
  }

  auto hub                          = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "Hub");
  bindings.get_video_player_manager = hub.GetMethodInfo("get_VideoPlayerManager");
  bindings.get_shop_scene_manager   = hub.GetMethodInfo("get_ShopSceneManager");
  bindings.get_tutorial_manager     = hub.GetMethodInfo("get_TutorialManager");

  auto video_player_manager = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Videos", "VideoPlayerManager");
  bindings.get_is_video_playing = video_player_manager.GetMethod<bool(void*)>("get_IsVideoPlaying", 0);

  auto tutorial = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Tutorial", "TutorialManager");
  bindings.get_is_tutorial_active   = tutorial.GetMethodInfo("get_IsActive");
  bindings.get_is_tutorial_blocking = tutorial.GetMethodInfo("get_IsBlockingInput");
  if (auto* klass = tutorial.get_cls(); klass) {
    bindings.tutorial_is_active_field      = il2cpp_class_get_field_from_name(klass, "_isActive");
    bindings.tutorial_ui_field             = il2cpp_class_get_field_from_name(klass, "_loadAndShow");
    bindings.tutorial_mission_field        = il2cpp_class_get_field_from_name(klass, "_currentMission");
    bindings.tutorial_objective_field      = il2cpp_class_get_field_from_name(klass, "_currentTutorialObjective");
    bindings.tutorial_data_field           = il2cpp_class_get_field_from_name(klass, "_currentTutorialData");
    bindings.tutorial_component_field      = il2cpp_class_get_field_from_name(klass, "_currentComponent");
    bindings.tutorial_items_field          = il2cpp_class_get_field_from_name(klass, "_currentObjectiveTutorialItems");
    bindings.tutorial_end_step_field       = il2cpp_class_get_field_from_name(klass, "_endMissionTutorialStep");
    bindings.tutorial_next_step_field      = il2cpp_class_get_field_from_name(klass, "_nextTutorialStep");
    bindings.tutorial_step_field           = il2cpp_class_get_field_from_name(klass, "_currentTutorialStep");
    bindings.tutorial_step_index_field     = il2cpp_class_get_field_from_name(klass, "_currentTutorialStepIndex");
    bindings.tutorial_next_action_id_field = il2cpp_class_get_field_from_name(klass, "_nextTutorialActionID");
    bindings.tutorial_objective_being_cleared_field =
        il2cpp_class_get_field_from_name(klass, "_currentObjectiveBeingCleared");
    bindings.tutorial_target_section_field = il2cpp_class_get_field_from_name(klass, "_currentTargetSection");
  }

  auto message_box                    = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "MessageBox");
  bindings.get_is_message_box_visible = message_box.GetMethodInfo("get_IsMessageBoxVisible");

  auto touch_kit = il2cpp_get_class_helper("TouchKit", "", "TouchKit");
  if (auto* klass = touch_kit.get_cls(); klass) {
    bindings.block_touch_field = il2cpp_class_get_field_from_name(klass, "BlockTouch");
  }

  auto ui_manager               = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.SharedFeatures", "UIManager");
  bindings.get_is_popup_visible = ui_manager.GetMethod<bool(void*)>("get_IsPopupVisible", 0);
  if (auto* singleton = ui_manager.GetParent("MonoSingleton`1").get_cls(); singleton) {
    auto* property = il2cpp_class_get_property_from_name(singleton, "Instance");
    bindings.ui_manager_instance_getter =
        property ? il2cpp_property_get_get_method(const_cast<PropertyInfo*>(property)) : nullptr;
  }

  auto shop_scene_manager = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopSceneManager");
  if (auto* klass = shop_scene_manager.get_cls(); klass) {
    bindings.plc_offer_popup_field = il2cpp_class_get_field_from_name(klass, "_plcOfferPopupLoadAndShow");
  }

  auto generic_load_and_show = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "GenericLoadAndShowUI");
  bindings.is_open           = generic_load_and_show.GetMethod<bool(void*)>("IsOpen", 0);

  auto input_action_asset = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputActionAsset");
  auto input_action       = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputAction");
  bindings.asset_enabled  = input_action_asset.GetMethod<bool(void*)>("get_enabled", 0);
  bindings.find_action    = input_action_asset.GetMethod<void*(void*, Il2CppString*, bool)>("FindAction", 2);
  bindings.action_enabled = input_action.GetMethod<bool(void*)>("get_enabled", 0);

  bindings.complete =
      bindings.shortcuts_actions_field && bindings.shortcuts_show_keybindings_field && bindings.get_can_use_shortcuts
      && bindings.get_video_player_manager && bindings.get_shop_scene_manager && bindings.get_tutorial_manager
      && bindings.get_is_video_playing && bindings.get_is_tutorial_active && bindings.get_is_tutorial_blocking
      && bindings.tutorial_is_active_field && bindings.tutorial_ui_field && bindings.tutorial_mission_field
      && bindings.tutorial_objective_field && bindings.tutorial_data_field && bindings.tutorial_component_field
      && bindings.tutorial_items_field && bindings.tutorial_end_step_field && bindings.tutorial_next_step_field
      && bindings.tutorial_step_field && bindings.tutorial_step_index_field && bindings.tutorial_next_action_id_field
      && bindings.tutorial_objective_being_cleared_field && bindings.tutorial_target_section_field
      && bindings.get_is_message_box_visible && bindings.block_touch_field && bindings.ui_manager_instance_getter
      && bindings.get_is_popup_visible && bindings.plc_offer_popup_field && bindings.is_open && bindings.asset_enabled
      && bindings.find_action && bindings.action_enabled;
  return bindings;
}

const OrphanedTutorialShortcutBindings& GetBindings()
{
  static const auto bindings = ResolveBindings();
  return bindings;
}

bool NativeScopelyShortcutsPermitted()
{
  const auto& config = Config::Get();
  return config.hotkeys_enabled && config.use_scopely_hotkeys;
}

struct OrphanedTutorialShortcutRuntimeEvidence {
  OrphanedTutorialShortcutGateState       state;
  const OrphanedTutorialShortcutBindings* bindings = nullptr;
  void*                                   mission  = nullptr;
  void*                                   step     = nullptr;
};

OrphanedTutorialShortcutRuntimeEvidence CollectEvidence(void* shortcuts_manager)
{
  OrphanedTutorialShortcutRuntimeEvidence evidence;
  auto&                                   state = evidence.state;
  state.repair_enabled                          = Config::Get().repair_orphaned_tutorial_shortcut_gate;
  state.native_shortcuts_enabled                = NativeScopelyShortcutsPermitted();

  if (!shortcuts_manager || !state.repair_enabled || !state.native_shortcuts_enabled) {
    return evidence;
  }

  const auto& bindings = GetBindings();
  if (!bindings.complete) {
    static bool warned = false;
    if (!warned) {
      spdlog::warn("[Hotkeys] orphaned tutorial shortcut repair disabled: required M94 IL2CPP bindings are missing");
      warned = true;
    }
    return evidence;
  }
  evidence.bindings = &bindings;

  auto reads_complete = InvokeStaticBoolGetter(bindings.get_can_use_shortcuts, state.can_use_shortcuts);
  reads_complete = InvokeStaticBoolGetter(bindings.get_is_tutorial_active, state.tutorial_active) && reads_complete;
  reads_complete = InvokeStaticBoolGetter(bindings.get_is_tutorial_blocking, state.tutorial_blocking) && reads_complete;
  reads_complete =
      InvokeStaticBoolGetter(bindings.get_is_message_box_visible, state.message_box_visible) && reads_complete;
  state.input_focused = Key::IsInputFocused();
  il2cpp_field_static_get_value(bindings.block_touch_field, &state.touch_blocked);

  void* actions  = nullptr;
  reads_complete = ReadInstanceField(shortcuts_manager, bindings.shortcuts_actions_field, actions) && reads_complete;
  reads_complete =
      ReadInstanceField(shortcuts_manager, bindings.shortcuts_show_keybindings_field, state.show_keybindings)
      && reads_complete;
  if (actions) {
    state.actions_enabled         = bindings.asset_enabled(actions);
    auto* interior_action         = bindings.find_action(actions, il2cpp_string_new("General/interior_view"), false);
    auto* galaxy_action           = bindings.find_action(actions, il2cpp_string_new("General/galaxy_view"), false);
    state.interior_action_enabled = interior_action && bindings.action_enabled(interior_action);
    state.galaxy_action_enabled   = galaxy_action && bindings.action_enabled(galaxy_action);
  }

  void* video_player_manager = nullptr;
  reads_complete = InvokeStaticObjectGetter(bindings.get_video_player_manager, video_player_manager) && reads_complete;
  if (video_player_manager) {
    state.video_playing = bindings.get_is_video_playing(video_player_manager);
  }

  void* ui_manager = nullptr;
  reads_complete   = InvokeStaticObjectGetter(bindings.ui_manager_instance_getter, ui_manager) && reads_complete;
  if (ui_manager) {
    state.popup_visible = bindings.get_is_popup_visible(ui_manager);
  }

  void* shop_scene_manager = nullptr;
  reads_complete = InvokeStaticObjectGetter(bindings.get_shop_scene_manager, shop_scene_manager) && reads_complete;
  if (shop_scene_manager) {
    void* plc_offer_popup = nullptr;
    reads_complete =
        ReadInstanceField(shop_scene_manager, bindings.plc_offer_popup_field, plc_offer_popup) && reads_complete;
    if (plc_offer_popup) {
      state.plc_offer_open = bindings.is_open(plc_offer_popup);
    }
  }

  void* tutorial_manager = nullptr;
  reads_complete         = InvokeStaticObjectGetter(bindings.get_tutorial_manager, tutorial_manager) && reads_complete;
  state.has_tutorial_manager = tutorial_manager != nullptr;
  if (!tutorial_manager) {
    return evidence;
  }

  void* tutorial_ui     = nullptr;
  void* objective       = nullptr;
  void* data            = nullptr;
  void* component       = nullptr;
  void* objective_items = nullptr;
  void* end_step        = nullptr;
  void* next_step       = nullptr;
  reads_complete = ReadInstanceField(tutorial_manager, bindings.tutorial_ui_field, tutorial_ui) && reads_complete;
  reads_complete =
      ReadInstanceField(tutorial_manager, bindings.tutorial_mission_field, evidence.mission) && reads_complete;
  reads_complete = ReadInstanceField(tutorial_manager, bindings.tutorial_objective_field, objective) && reads_complete;
  reads_complete = ReadInstanceField(tutorial_manager, bindings.tutorial_data_field, data) && reads_complete;
  reads_complete = ReadInstanceField(tutorial_manager, bindings.tutorial_component_field, component) && reads_complete;
  reads_complete =
      ReadInstanceField(tutorial_manager, bindings.tutorial_items_field, objective_items) && reads_complete;
  reads_complete = ReadInstanceField(tutorial_manager, bindings.tutorial_end_step_field, end_step) && reads_complete;
  reads_complete = ReadInstanceField(tutorial_manager, bindings.tutorial_next_step_field, next_step) && reads_complete;
  reads_complete = ReadInstanceField(tutorial_manager, bindings.tutorial_step_field, evidence.step) && reads_complete;
  reads_complete =
      ReadInstanceField(tutorial_manager, bindings.tutorial_step_index_field, state.step_index) && reads_complete;
  reads_complete = ReadInstanceField(tutorial_manager, bindings.tutorial_next_action_id_field, state.next_action_id)
                   && reads_complete;
  reads_complete = ReadInstanceField(tutorial_manager, bindings.tutorial_objective_being_cleared_field,
                                     state.objective_being_cleared)
                   && reads_complete;
  reads_complete = ReadInstanceField(tutorial_manager, bindings.tutorial_target_section_field, state.target_section)
                   && reads_complete;

  state.tutorial_ui_open    = tutorial_ui && bindings.is_open(tutorial_ui);
  state.has_mission         = evidence.mission != nullptr;
  state.has_objective       = objective != nullptr;
  state.has_data            = data != nullptr;
  state.has_component       = component != nullptr;
  state.has_objective_items = objective_items != nullptr;
  state.has_end_step        = end_step != nullptr;
  state.has_next_step       = next_step != nullptr;
  state.step_identity       = reinterpret_cast<uintptr_t>(evidence.step);
  reads_complete            = ReadObjectField(evidence.step, "type_", state.step_type) && reads_complete;
  reads_complete            = ReadObjectField(evidence.mission, "missionId_", state.mission_id) && reads_complete;
  reads_complete            = ReadObjectField(evidence.step, "actionId_", state.action_id) && reads_complete;

  state.evidence_complete = reads_complete;
  return evidence;
}
} // namespace

void RepairOrphanedTutorialShortcutGateIfNeeded(void* shortcuts_manager)
{
  static uint64_t  frame                   = 0;
  static uintptr_t candidate_step_identity = 0;
  static int       consecutive_samples     = 0;

  if (!Config::Get().repair_orphaned_tutorial_shortcut_gate || !NativeScopelyShortcutsPermitted()) {
    frame                   = 0;
    candidate_step_identity = 0;
    consecutive_samples     = 0;
    return;
  }

  ++frame;
  if (frame != 1 && frame % kOrphanedTutorialShortcutGateSampleFrames != 0) {
    return;
  }

  auto  evidence = CollectEvidence(shortcuts_manager);
  auto& state    = evidence.state;
  if (!should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2)) {
    candidate_step_identity = 0;
    consecutive_samples     = 0;
    return;
  }

  if (candidate_step_identity != state.step_identity) {
    candidate_step_identity = state.step_identity;
    consecutive_samples     = 1;
  } else {
    ++consecutive_samples;
  }

  if (!should_repair_orphaned_tutorial_shortcut_gate(state, candidate_step_identity, consecutive_samples)) {
    return;
  }

  bool inactive = false;
  il2cpp_field_static_set_value(evidence.bindings->tutorial_is_active_field, &inactive);
  bool       tutorial_active_after   = true;
  bool       can_use_shortcuts_after = false;
  const auto verification_complete =
      InvokeStaticBoolGetter(evidence.bindings->get_is_tutorial_active, tutorial_active_after)
      && InvokeStaticBoolGetter(evidence.bindings->get_can_use_shortcuts, can_use_shortcuts_after);
  if (!verification_complete || tutorial_active_after || !can_use_shortcuts_after) {
    bool active = true;
    il2cpp_field_static_set_value(evidence.bindings->tutorial_is_active_field, &active);
    spdlog::error("[Hotkeys] orphaned tutorial shortcut repair rolled back mission_id={} action_id={} "
                  "tutorial_active_after={} can_use_shortcuts_after={}",
                  state.mission_id, state.action_id, tutorial_active_after, can_use_shortcuts_after);
  } else {
    spdlog::warn("[Hotkeys] repaired orphaned tutorial shortcut gate mission_id={} action_id={} "
                 "tutorial_active_after={} can_use_shortcuts_after={}",
                 state.mission_id, state.action_id, tutorial_active_after, can_use_shortcuts_after);
  }

  candidate_step_identity = 0;
  consecutive_samples     = 0;
}
