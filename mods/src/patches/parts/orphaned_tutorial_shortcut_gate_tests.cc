#include "orphaned_tutorial_shortcut_gate.h"

namespace
{
// This branch has no unit-test target, so the pure decision matrix is enforced at compile time.
constexpr OrphanedTutorialShortcutGateState ExactM94OrphanState()
{
  OrphanedTutorialShortcutGateState state;
  state.repair_enabled           = true;
  state.native_shortcuts_enabled = true;
  state.evidence_complete        = true;
  state.actions_enabled          = true;
  state.interior_action_enabled  = true;
  state.galaxy_action_enabled    = true;
  state.tutorial_active          = true;
  state.has_tutorial_manager     = true;
  state.has_mission              = true;
  state.has_data                 = true;
  state.step_identity            = 0x1234;
  state.step_index               = 0;
  state.mission_id               = 1463528981;
  state.action_id                = -1401001831;
  state.next_action_id           = state.action_id;
  state.objective_being_cleared  = -1;
  state.target_section           = -1;
  state.step_type                = 0;
  return state;
}

constexpr bool LegitimateBlockersFailClosed()
{
  auto state = ExactM94OrphanState();

  state.video_playing = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                   = ExactM94OrphanState();
  state.tutorial_blocking = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                  = ExactM94OrphanState();
  state.tutorial_ui_open = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                  = ExactM94OrphanState();
  state.show_keybindings = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                     = ExactM94OrphanState();
  state.message_box_visible = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state               = ExactM94OrphanState();
  state.touch_blocked = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state               = ExactM94OrphanState();
  state.input_focused = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state               = ExactM94OrphanState();
  state.popup_visible = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                = ExactM94OrphanState();
  state.plc_offer_open = true;
  return !should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2);
}

constexpr bool WrongOrIncompleteSignaturesFailClosed()
{
  auto state = ExactM94OrphanState();

  state.repair_enabled = false;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                          = ExactM94OrphanState();
  state.native_shortcuts_enabled = false;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                   = ExactM94OrphanState();
  state.evidence_complete = false;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                   = ExactM94OrphanState();
  state.can_use_shortcuts = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                 = ExactM94OrphanState();
  state.actions_enabled = false;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state               = ExactM94OrphanState();
  state.has_objective = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                     = ExactM94OrphanState();
  state.has_objective_items = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state               = ExactM94OrphanState();
  state.has_component = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state              = ExactM94OrphanState();
  state.has_end_step = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state               = ExactM94OrphanState();
  state.has_next_step = true;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state            = ExactM94OrphanState();
  state.step_index = 1;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                = ExactM94OrphanState();
  state.target_section = 0;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state            = ExactM94OrphanState();
  state.mission_id = 1;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state           = ExactM94OrphanState();
  state.action_id = 1;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                = ExactM94OrphanState();
  state.next_action_id = 1;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state                         = ExactM94OrphanState();
  state.objective_being_cleared = 0;
  if (should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2))
    return false;
  state           = ExactM94OrphanState();
  state.step_type = 1;
  return !should_repair_orphaned_tutorial_shortcut_gate(state, state.step_identity, 2);
}

constexpr auto kExactState = ExactM94OrphanState();
static_assert(should_repair_orphaned_tutorial_shortcut_gate(kExactState, kExactState.step_identity, 2));
static_assert(!should_repair_orphaned_tutorial_shortcut_gate(kExactState, kExactState.step_identity, 1));
static_assert(!should_repair_orphaned_tutorial_shortcut_gate(kExactState, 0x5678, 2));
static_assert(LegitimateBlockersFailClosed());
static_assert(WrongOrIncompleteSignaturesFailClosed());
} // namespace
