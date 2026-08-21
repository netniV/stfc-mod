#pragma once

#include <cstdint>

struct OrphanedTutorialShortcutGateState {
  bool      repair_enabled           = false;
  bool      native_shortcuts_enabled = false;
  bool      evidence_complete        = false;
  bool      can_use_shortcuts        = false;
  bool      actions_enabled          = false;
  bool      interior_action_enabled  = false;
  bool      galaxy_action_enabled    = false;
  bool      video_playing            = false;
  bool      tutorial_active          = false;
  bool      tutorial_blocking        = false;
  bool      tutorial_ui_open         = false;
  bool      show_keybindings         = false;
  bool      message_box_visible      = false;
  bool      touch_blocked            = false;
  bool      input_focused            = false;
  bool      popup_visible            = false;
  bool      plc_offer_open           = false;
  bool      has_tutorial_manager     = false;
  bool      has_mission              = false;
  bool      has_objective            = false;
  bool      has_data                 = false;
  bool      has_component            = false;
  bool      has_objective_items      = false;
  bool      has_end_step             = false;
  bool      has_next_step            = false;
  uintptr_t step_identity            = 0;
  int       step_index               = -1;
  int64_t   mission_id               = 0;
  int64_t   action_id                = 0;
  int64_t   next_action_id           = 0;
  int64_t   objective_being_cleared  = 0;
  int64_t   target_section           = 0;
  int64_t   step_type                = -1;
};

inline constexpr bool should_repair_orphaned_tutorial_shortcut_gate(const OrphanedTutorialShortcutGateState& state,
                                                                    const uintptr_t candidate_step_identity,
                                                                    const int       consecutive_samples)
{
  constexpr int64_t kM94OrphanedTutorialMissionId = 1463528981;
  constexpr int64_t kM94OrphanedTutorialActionId  = -1401001831;

  return state.repair_enabled && state.native_shortcuts_enabled && state.evidence_complete && !state.can_use_shortcuts
         && state.actions_enabled && state.interior_action_enabled && state.galaxy_action_enabled
         && !state.video_playing && state.tutorial_active && !state.tutorial_blocking && !state.tutorial_ui_open
         && !state.show_keybindings && !state.message_box_visible && !state.touch_blocked && !state.input_focused
         && !state.popup_visible && !state.plc_offer_open && state.has_tutorial_manager && state.has_mission
         && !state.has_objective && state.has_data && !state.has_component && !state.has_objective_items
         && !state.has_end_step && !state.has_next_step && state.step_identity != 0
         && state.step_identity == candidate_step_identity && state.step_index == 0 && state.target_section == -1
         && state.mission_id == kM94OrphanedTutorialMissionId && state.action_id == kM94OrphanedTutorialActionId
         && state.next_action_id == state.action_id && state.objective_being_cleared == -1 && state.step_type == 0
         && consecutive_samples >= 2;
}

void RepairOrphanedTutorialShortcutGateIfNeeded(void* shortcuts_manager);
