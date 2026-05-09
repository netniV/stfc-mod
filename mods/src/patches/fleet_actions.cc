#include "errormsg.h"
#include "config.h"

#include "patches/fleet_actions.h"
#include "patches/viewer_mgmt.h"

#include "prime/ActionQueueManager.h"
#include "prime/ArmadaObjectViewerWidget.h"
#include "prime/DeploymentManager.h"
#include "prime/FleetBarViewController.h"
#include "prime/FleetLocalViewController.h"
#include "prime/FleetsManager.h"
#include "prime/Hub.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/NavigationInteractionUIViewController.h"
#include "prime/NavigationSectionManager.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/ScanEngageButtonsWidget.h"
#include "prime/StarNodeObjectViewerWidget.h"

#include "patches/key.h"
#include "patches/mapkey.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <span>
#include <string_view>

bool force_space_action_next_frame = false;

namespace {

std::chrono::time_point<std::chrono::steady_clock> select_clock = std::chrono::steady_clock::now();
int32_t                                            last_ship_select_request = -1;

#define FleetAction_Format "Fleet {} ({}) #{} - State: {}, previous {} - canAction {}, canState {} - didAction: {}"

template <typename T>
inline bool DidExecuteFleetAction(std::string_view action_text, ActionType action_type,
                                  FleetBarViewController*       fleet_bar,
                                  const std::span<const FleetState> wanted_states,
                                  FleetState                        help_state = FleetState::Unknown)
{
  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet            = fleet_bar->_fleetPanelController->fleet;
  auto fleet_state      = fleet->CurrentState;

  auto       fleet_id   = fleet->Id;
  auto       prev_state = fleet->PreviousState;
  auto       can_action = true;
  FleetState can_state  = FleetState::Unknown;
  auto       did_action = false;

  if (std::find(std::begin(wanted_states), std::end(wanted_states), fleet_state) != std::end(wanted_states)) {
    can_state = fleet_state;
  }

  spdlog::trace(FleetAction_Format, action_text, (int)action_type, (int)fleet_id, (int)fleet_state,
                (int)prev_state, can_action, (int)can_state, "[start]");

  if (can_state != FleetState::Unknown && can_action) {
    if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
      NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
    }

    did_action = fleet_controller->RequestAction(fleet, action_type, 0, ActionBehaviour::Default);
  }

  if (help_state != FleetState::Unknown && (did_action || help_state == fleet->CurrentState)) {
    did_action = did_action || fleet_controller->RequestAction(fleet, action_type, 0, ActionBehaviour::AskHelp);
  }

  spdlog::trace(FleetAction_Format, action_text, (int)action_type, (int)fleet_id, (int)fleet_state,
                (int)prev_state, can_action, (int)can_state, did_action);

  return did_action;
}

} // namespace

bool HandleShipSelection(int ship_select_request)
{
  if (ship_select_request == -1 || Key::IsInputFocused()) {
    return false;
  }

  auto config = &Config::Get();

  if (Key::HasShift()) {
    FleetPlayerData* found_disco = nullptr;
    for (int disco_idx = 0; disco_idx < 10; ++disco_idx) {
      auto fleet_player_data = FleetsManager::Instance()->GetFleetPlayerData(disco_idx);
      if (fleet_player_data && fleet_player_data->Hull && fleet_player_data->Hull->Id == 1307832955) {
        found_disco = fleet_player_data;
        break;
      }
    }

    if (found_disco) {
      auto towed_fleet_id = FleetsManager::Instance()->GetFleetPlayerData(ship_select_request)->Id;
      auto planned_course = DeploymentManger::Instance()->PlanCourse(
          FleetsManager::Instance()->GetFleetPlayerData(ship_select_request), found_disco->Address, Vector3::zero(),
          nullptr, nullptr, nullptr);
      while (planned_course->MoveNext()) {
        ;
      }
      DeploymentManger::Instance()->SetTowRequest(towed_fleet_id, found_disco->Id);
    }
  } else {
    auto fleet_bar  = ObjectFinder<FleetBarViewController>::Get();
    auto can_locate = !config->disable_preview_locate || !CanHideViewers();
    if (fleet_bar) {
      auto select_now  = std::chrono::steady_clock::now();
      auto select_diff = std::chrono::duration_cast<std::chrono::milliseconds>(select_now - select_clock);
      spdlog::debug("select_diff was {}ms", select_diff.count());
        if (can_locate && ship_select_request == last_ship_select_request
          && fleet_bar->IsIndexSelected(ship_select_request)
          && select_diff < std::chrono::milliseconds((int)Config::Get().select_timer)) {
        auto fleet = fleet_bar->_fleetPanelController->fleet;
        if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
          NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
        }
        FleetsManager::Instance()->RequestViewFleet(fleet, true);
      } else {
        fleet_bar->RequestSelect(ship_select_request);
      }

  last_ship_select_request = ship_select_request;
      select_clock = select_now;
      return true;
    }
  }

  return false;
}

bool DidExecuteRecall(FleetBarViewController* fleet_bar)
{
  static constexpr FleetState states[] = {FleetState::IdleInSpace, FleetState::Impulsing, FleetState::Mining,
                                          FleetState::Capturing};

  return DidExecuteFleetAction<RecallRequirement>("Recall", ActionType::Recall, fleet_bar, states);
}

bool DidExecuteRepair(FleetBarViewController* fleet_bar)
{
  static constexpr FleetState states[] = {FleetState::Docked, FleetState::Destroyed};

  return DidExecuteFleetAction<CanRepairRequirement>("Repair", ActionType::Repair, fleet_bar, states,
                                                     FleetState::Repairing);
}

void ExecuteSpaceAction(FleetBarViewController* fleet_bar)
{
  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet            = fleet_controller->fleet;

  auto action_queue = ActionQueueManager::Instance();

  auto has_primary       = MapKey::IsDown(GameFunction::ActionPrimary) || force_space_action_next_frame;
  auto has_repair        = MapKey::IsDown(GameFunction::ActionRepair);
  auto has_recall_cancel = MapKey::IsDown(GameFunction::ActionRecallCancel);
  auto has_secondary     = MapKey::IsDown(GameFunction::ActionSecondary);
  auto has_queue         = MapKey::IsDown(GameFunction::ActionQueue);
  auto has_queue_clear   = MapKey::IsDown(GameFunction::ActionQueueClear);
  auto has_recall =
      MapKey::IsDown(GameFunction::ActionRecall) && (!Config::Get().disable_preview_recall || !CanHideViewers());

  if (has_queue_clear) {
    action_queue->ClearQueue(fleet);
  } else if (has_recall_cancel
             && (fleet->CurrentState == FleetState::WarpCharging || fleet->CurrentState == FleetState::Warping)) {
    fleet_controller->CancelButtonClicked();
  } else {
    auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAll();
    for (auto pre_scan_widget : all_pre_scan_widgets) {
      if (pre_scan_widget
          && (pre_scan_widget->_visibilityController->_state == VisibilityState::Visible
              || pre_scan_widget->_visibilityController->_state == VisibilityState::Show)) {

        if (auto mine_object_viewer_widget = ObjectFinder<MiningObjectViewerWidget>::Get();
            mine_object_viewer_widget
            && (mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Visible
                || mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Show)) {
          if (has_secondary) {
            return pre_scan_widget->_scanEngageButtonsWidget->OnScanButtonClicked();
          } else if (has_primary) {
            return mine_object_viewer_widget->MineClicked();
          }
        }

        if (has_queue && action_queue->IsQueueUnlocked() && pre_scan_widget->_addToQueueButtonWidget
            && pre_scan_widget->_scanEngageButtonsWidget) {
          auto context = pre_scan_widget->_scanEngageButtonsWidget->Context;
          auto type    = GetHullTypeFromBattleTarget(context);

          if (type != HullType::ArmadaTarget && (type != HullType::Any || force_space_action_next_frame)) {
            if (pre_scan_widget->_addToQueueButtonWidget->isActiveAndEnabled) {
              auto listener = pre_scan_widget->_addToQueueButtonWidget->SemaphoreListener;
              if (listener && !action_queue->IsQueueFull(fleet)) {
                auto button = listener->TheButton;
                if (button) {
                  button->Press();
                  DidHideViewers();
                }
              }
              return;
            }

            if (type == HullType::Any) {
              force_space_action_next_frame = true;
              return;
            }
          }
        }

        if (has_secondary) {
          return pre_scan_widget->_scanEngageButtonsWidget->OnScanButtonClicked();
        }

        if (has_primary && pre_scan_widget->_scanEngageButtonsWidget
            && pre_scan_widget->_scanEngageButtonsWidget->enabled) {
          auto context = pre_scan_widget->_scanEngageButtonsWidget->Context;
          auto type    = GetHullTypeFromBattleTarget(context);

          auto armada_widget = ObjectFinder<ArmadaObjectViewerWidget>::Get();
          auto armada_state  = VisibilityState::Unknown;

          if (armada_widget) {
            if (armada_widget->_visibilityController) {
              armada_state = armada_widget->_visibilityController->State;
            } else {
              spdlog::warn("ArmadaWidget has no visibility controller, using default Visible state");
              armada_state = VisibilityState::Visible;
            }
          }

          auto can_action_primary = type != HullType::Any;
          if (type == HullType::ArmadaTarget
              && (armada_state == VisibilityState::Visible || armada_state == VisibilityState::Show)) {
            can_action_primary = false;
          } else if (force_space_action_next_frame) {
            can_action_primary = true;
          }

          if (can_action_primary) {
            if (type == HullType::ArmadaTarget) {
              if (pre_scan_widget->_armadaAttackButton && pre_scan_widget->_armadaAttackButton->isActiveAndEnabled) {
                auto listener = pre_scan_widget->_armadaAttackButton->SemaphoreListener;
                if (listener) {
                  auto button = listener->TheButton;
                  if (button) {
                    button->Press();
                  }
                }
                return;
              }
              pre_scan_widget->_scanEngageButtonsWidget->OnArmadaButtonClicked();
            } else {
              pre_scan_widget->_scanEngageButtonsWidget->OnEngageButtonClicked();
            }
            return;
          } else if (type == HullType::Any) {
            force_space_action_next_frame = true;
            return;
          }
        }
      }
    }

    if (auto mine_object_viewer_widget = ObjectFinder<MiningObjectViewerWidget>::Get();
        mine_object_viewer_widget
        && (mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Visible
            || mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Show)) {
      if (has_secondary) {
        if (mine_object_viewer_widget->_scanEngageButtonsWidget->Context) {
          return mine_object_viewer_widget->_scanEngageButtonsWidget->OnScanButtonClicked();
        }
      } else if (has_primary) {
        return mine_object_viewer_widget->MineClicked();
      }
    } else if (auto star_node_object_viewer_widget = ObjectFinder<StarNodeObjectViewerWidget>::Get();
               star_node_object_viewer_widget && star_node_object_viewer_widget->Context) {
      if (has_secondary) {
        star_node_object_viewer_widget->OnViewButtonActivation();
      } else if (has_primary) {
        star_node_object_viewer_widget->InitiateWarp();
      }
    } else if (auto navigation_ui_controller = ObjectFinder<NavigationInteractionUIViewController>::Get();
               navigation_ui_controller && has_primary) {
      auto armada_widget = ObjectFinder<ArmadaObjectViewerWidget>::Get();
      auto armada_state  = VisibilityState::Unknown;

      if (armada_widget) {
        if (armada_widget->_visibilityController) {
          armada_state = armada_widget->_visibilityController->State;
        } else {
          spdlog::warn("ArmadaWidget has no visibility controller, using default Visible state");
          armada_state = VisibilityState::Visible;
        }
      }

      spdlog::info("have armada? {}, State {}", (armada_widget ? "Yes" : "No"), (int)armada_state);
      if (armada_widget && (armada_state == VisibilityState::Visible || armada_state == VisibilityState::Show)) {
        auto button = armada_widget->__get__joinContext();
        if (button && button->Interactable) {
          armada_widget->ValidateThenJoinArmada();
          return;
        }
      } else {
        navigation_ui_controller->OnSetCourseButtonClick();
        return;
      }
    } else if (has_recall && DidExecuteRecall(fleet_bar)) {
      return;
    } else if (has_repair && DidExecuteRepair(fleet_bar)) {
      return;
    }
  }
}

HullType GetHullTypeFromBattleTarget(BattleTargetData* context)
{
  if (!context) {
    return HullType::Any;
  }
  auto deployed_data = context->TargetFleetDeployedData;
  if (!deployed_data) {
    return HullType::Any;
  }
  auto hull_spec = deployed_data->Hull;
  if (!hull_spec) {
    return HullType::Any;
  }
  return hull_spec->Type;
}