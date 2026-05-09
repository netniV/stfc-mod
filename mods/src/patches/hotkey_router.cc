#include "config.h"

#include "patches/hotkey_router.h"

#include "patches/hotkey_dispatch.h"
#include "patches/key.h"
#include "patches/mapkey.h"

#include "prime/AllianceStarbaseObjectViewerWidget.h"
#include "prime/ArmadaObjectViewerWidget.h"
#include "prime/CelestialObjectViewerWidget.h"
#include "prime/EmbassyObjectViewer.h"
#include "prime/HousingObjectViewerWidget.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/MissionsObjectViewerWidget.h"
#include "prime/StarNodeObjectViewerWidget.h"

#include "prime/ActionQueueManager.h"
#include "prime/AnimatedRewardsScreenViewController.h"
#include "prime/BookmarksManager.h"
#include "prime/ChatManager.h"
#include "prime/DeploymentManager.h"
#include "prime/FleetBarViewController.h"
#include "prime/FleetLocalViewController.h"
#include "prime/FleetsManager.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/Hub.h"
#include "prime/KeyCode.h"
#include "prime/NavigationInteractionUIViewController.h"
#include "prime/NavigationSectionManager.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/ScanEngageButtonsWidget.h"
#include "prime/ScreenManager.h"

#include <Windows.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <span>
#include <string_view>

namespace {

int  show_info_pending             = 0;
bool force_space_action_next_frame = false;

void     ChangeNavigationSection(SectionID sectionID);
void     ExecuteSpaceAction(FleetBarViewController* fleet_bar);
bool     DidExecuteRecall(FleetBarViewController* fleet_bar);
bool     DidExecuteRepair(FleetBarViewController* fleet_bar);
HullType GetHullTypeFromBattleTarget(BattleTargetData* context);
void     GotoSection(SectionID sectionID, void* screen_data = nullptr);
bool     CanHideViewers();
bool     DidHideViewers();
bool     CheckShowCargo(RewardsButtonWidget* widget);

bool MoveOfficerCanvas(bool goLeft)
{
  auto const canvas = ScreenManager::GetTopCanvas(true);
  if (strcmp(((Il2CppObject*)(canvas))->klass->name, "OfficerShowcase_Canvas") == 0) {}

  return false;
}

template <typename T>
inline bool CanHideViewersOfType()
{
  for (auto widget : ObjectFinder<T>::GetAll()) {
    const auto visible = widget && widget->_visibilityController != NULL
                         && (widget->_visibilityController->_state == VisibilityState::Visible
                             || widget->_visibilityController->_state == VisibilityState::Show);
    if (visible) {
      return true;
    }
  }

  return false;
}

template <typename T>
inline bool DidHideViewersOfType()
{
  const auto objects = ObjectFinder<T>::GetAll();
  auto       didHide = false;
  for (auto widget : objects) {
    if (!widget) {
      continue;
    }
    auto visbility_controller = widget->_visibilityController;
    if (!visbility_controller) {
      continue;
    }
    const auto visible = (visbility_controller->_state == VisibilityState::Visible
                          || visbility_controller->_state == VisibilityState::Show);
    if (visible) {
      widget->HideAllViewers();
      didHide = true;
    }
  }

  return didHide;
}

} // namespace

void hotkey_router_init()
{
}

bool hotkey_router_screen_update(ScreenManager* screen_manager)
{
  screen_manager = screen_manager;

  static std::chrono::time_point<std::chrono::steady_clock> select_clock = std::chrono::steady_clock::now();
  static int32_t last_ship_select_request = -1;

  Key::ResetCache();

  if (MapKey::IsDown(GameFunction::DisableHotKeys)) {
    Config::Get().hotkeys_enabled = false;
    spdlog::warn("Setting hotkeys to DISABLED");
    return false;
  } else if (MapKey::IsDown(GameFunction::EnableHotKeys)) {
    Config::Get().hotkeys_enabled = true;
    spdlog::warn("Setting hotkeys to ENABLED");
    return false;
  }

  if (Config::Get().use_scopely_hotkeys && Config::Get().hotkeys_enabled) {
    return true;
  }

  if (!Config::Get().hotkeys_enabled) {
    return false;
  }

  const auto is_in_chat = Hub::IsInChat();
  const auto config     = &Config::Get();

  if (MapKey::IsDown(GameFunction::Quit)) {
    TerminateProcess(GetCurrentProcess(), 1);
  }

  int32_t ship_select_request = -1;
  if (MapKey::IsDown(GameFunction::SelectShip1)) {
    ship_select_request = 0;
  } else if (MapKey::IsDown(GameFunction::SelectShip2)) {
    ship_select_request = 1;
  } else if (MapKey::IsDown(GameFunction::SelectShip3)) {
    ship_select_request = 2;
  } else if (MapKey::IsDown(GameFunction::SelectShip4)) {
    ship_select_request = 3;
  } else if (MapKey::IsDown(GameFunction::SelectShip5)) {
    ship_select_request = 4;
  } else if (MapKey::IsDown(GameFunction::SelectShip6)) {
    ship_select_request = 5;
  } else if (MapKey::IsDown(GameFunction::SelectShip7)) {
    ship_select_request = 6;
  } else if (MapKey::IsDown(GameFunction::SelectShip8)) {
    ship_select_request = 7;
  }

  if (ship_select_request != -1 && !Key::IsInputFocused()) {
    if (Key::HasShift()) {
      FleetPlayerData* foundDisco = nullptr;
      for (int discoIdx = 0; discoIdx < 10; ++discoIdx) {
        auto fleetPlayerData = FleetsManager::Instance()->GetFleetPlayerData(discoIdx);
        if (fleetPlayerData && fleetPlayerData->Hull && fleetPlayerData->Hull->Id == 1307832955) {
          foundDisco = fleetPlayerData;
          break;
        }
      }

      if (foundDisco) {
        auto towedFleetId = FleetsManager::Instance()->GetFleetPlayerData(ship_select_request)->Id;
        auto plannedCourse = DeploymentManger::Instance()->PlanCourse(
            FleetsManager::Instance()->GetFleetPlayerData(ship_select_request), foundDisco->Address, Vector3::zero(),
            nullptr, nullptr, nullptr);
        while (plannedCourse->MoveNext()) {
          ;
        }
        DeploymentManger::Instance()->SetTowRequest(towedFleetId, foundDisco->Id);
      }
    } else {
      auto fleet_bar  = ObjectFinder<FleetBarViewController>::Get();
      auto can_locate = !config->disable_preview_locate || !CanHideViewers();
      if (fleet_bar) {
        std::chrono::time_point<std::chrono::steady_clock> select_now = std::chrono::steady_clock::now();
        std::chrono::milliseconds                          select_diff =
            std::chrono::duration_cast<std::chrono::milliseconds>(select_now - select_clock);
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
        return false;
      }
    }
  }

  if (Key::Pressed(KeyCode::Escape) && (Key::IsInputFocused() || Hub::IsInChat())) {
    Key::ClearInputFocus();
    return false;
  }

  if (!is_in_chat) {
    if (!Key::IsInputFocused()) {
      if (MapKey::IsDown(GameFunction::SelectCurrent)) {
        auto fleet_bar = ObjectFinder<FleetBarViewController>::Get();
        if (fleet_bar) {
          auto fleet = fleet_bar->_fleetPanelController->fleet;
          if (fleet) {
            if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
              NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
            }
            FleetsManager::Instance()->RequestViewFleet(fleet, true);
            return false;
          }
        }
      }

      if ((MapKey::IsDown(GameFunction::ToggleQueue))) {
        config->queue_enabled = !config->queue_enabled;
        return false;
      }

      if ((MapKey::IsDown(GameFunction::ShowChat) || MapKey::IsDown(GameFunction::ShowChatSide1)
           || MapKey::IsDown(GameFunction::ShowChatSide2))) {
        if (auto chat_manager = ChatManager::Instance(); chat_manager) {
          if (chat_manager->IsSideChatOpen) {
            if (auto view_controller = ObjectFinder<FullScreenChatViewController>::Get(); view_controller) {
              if (auto message_list = view_controller->_messageList; message_list) {
                if (auto message_field = message_list->_inputField; message_field) {
                  message_field->ActivateInputField();
                }
              }
            }
          } else if (MapKey::IsDown(GameFunction::ShowChatSide1) || MapKey::IsDown(GameFunction::ShowChatSide2)) {
            chat_manager->OpenChannel(ChatChannelCategory::Alliance, ChatViewMode::Side);
          } else {
            chat_manager->OpenChannel(ChatChannelCategory::Alliance, ChatViewMode::Fullscreen);
          }
        }
      }

      if (MapKey::IsDown(GameFunction::MoveLeft)) {
        auto const result = MoveOfficerCanvas(true);
        if (result) {
          return false;
        }
      }

      if (MapKey::IsDown(GameFunction::MoveRight)) {
        auto const result = MoveOfficerCanvas(false);
        if (result) {
          return false;
        }
      }

      for (const auto& entry : GetHotkeyDispatchTable()) {
        const auto active =
            (entry.input_mode == InputMode::Pressed) ? MapKey::IsPressed(entry.game_function)
                                                     : MapKey::IsDown(entry.game_function);
        if (!active) {
          continue;
        }

        auto decision = entry.handler();
        if (decision == DispatchDecision::HandledStop) {
          return false;
        }
        break;
      }
    }
  } else {
    if (auto chat_manager = ChatManager::Instance(); chat_manager) {
      if (MapKey::IsDown(GameFunction::SelectChatGlobal)) {
        chat_manager->OpenChannel(ChatChannelCategory::Global);
        return false;
      } else if (MapKey::IsDown(GameFunction::SelectChatAlliance)) {
        chat_manager->OpenChannel(ChatChannelCategory::Alliance);
        return false;
      } else if (MapKey::IsDown(GameFunction::SelectChatPrivate)) {
        chat_manager->OpenChannel(ChatChannelCategory::Private);
        return false;
      }
    }
  }

  if (!Key::IsInputFocused()) {
    if (Key::Pressed(KeyCode::Escape) && DidHideViewers()) {
      return false;
    }

    if (MapKey::IsDown(GameFunction::ActionPrimary) || Key::Pressed(KeyCode::Escape)) {
      if (auto reward_controller = ObjectFinder<AnimatedRewardsScreenViewController>::Get(); reward_controller) {
        if (reward_controller->IsActive()) {
          reward_controller->GoBackToLastSection();
          return false;
        }
      }
    }

    if (MapKey::IsDown(GameFunction::ActionPrimary) || MapKey::IsDown(GameFunction::ActionSecondary)
        || MapKey::IsDown(GameFunction::ActionRecall) || MapKey::IsDown(GameFunction::ActionRepair)
        || MapKey::IsDown(GameFunction::ActionQueue) || MapKey::IsDown(GameFunction::ActionQueueClear)
        || force_space_action_next_frame) {
      if (Hub::IsInSystemOrGalaxyOrStarbase() && !Hub::IsInChat() && !Key::IsInputFocused()) {
        auto fleet_bar = ObjectFinder<FleetBarViewController>::Get();
        if (fleet_bar) {
          bool was_forced = force_space_action_next_frame;
          ExecuteSpaceAction(fleet_bar);
          if (was_forced) {
            force_space_action_next_frame = false;
          }
        }
      }
    }

    if (MapKey::IsDown(GameFunction::ActionView)) {
      auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAll();

      for (auto& pre_scan_widget : all_pre_scan_widgets) {
        if (pre_scan_widget
            && (pre_scan_widget->_visibilityController->_state == VisibilityState::Visible
                || pre_scan_widget->_visibilityController->_state == VisibilityState::Show)) {
          auto rewardsWidget = pre_scan_widget->_rewardsButtonWidget;
          if (rewardsWidget->_rewardsController->_state != VisibilityState::Visible
              && rewardsWidget->_rewardsController->_state != VisibilityState::Show) {
            show_info_pending = 5;
          } else {
            rewardsWidget->_rewardsController->Hide();
          }
        }
      }
    }

    if (show_info_pending > 0) {
      auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAll();

      for (auto& pre_scan_widget : all_pre_scan_widgets) {
        const auto pre_scan_visible = pre_scan_widget
                                      && (pre_scan_widget->_visibilityController->_state == VisibilityState::Visible
                                          || pre_scan_widget->_visibilityController->_state == VisibilityState::Show);
        if (pre_scan_visible) {
          auto       rewardsWidget          = pre_scan_widget->_rewardsButtonWidget;
          const auto rewards_widget_visible = rewardsWidget->_rewardsController->_state == VisibilityState::Visible
                                              || rewardsWidget->_rewardsController->_state == VisibilityState::Show;
          if (!rewards_widget_visible) {
            rewardsWidget->_rewardsController->Show(true);
          }
        }
      }
      show_info_pending -= 1;
    }
  }

  if (config->disable_escape_exit && Key::Pressed(KeyCode::Escape)) {
    return false;
  }

  return true;
}

bool hotkey_router_init_actions()
{
  return Config::Get().use_scopely_hotkeys;
}

void hotkey_router_bind_context(RewardsButtonWidget* widget)
{
  if (CheckShowCargo(widget)) {
    widget->_rewardsController->Show(true);
    show_info_pending = 1;
  }
}

void hotkey_router_show_fleet(PreScanTargetWidget* widget)
{
  auto rewards_button_widget = widget->_rewardsButtonWidget;
  if (CheckShowCargo(rewards_button_widget)) {
    rewards_button_widget->_rewardsController->Show(true);
    show_info_pending = 1;
  }
}

namespace {

bool CanHideViewers()
{
  return (CanHideViewersOfType<AllianceStarbaseObjectViewerWidget>() || CanHideViewersOfType<ArmadaObjectViewerWidget>()
          || CanHideViewersOfType<CelestialObjectViewerWidget>() || CanHideViewersOfType<EmbassyObjectViewer>()
          || CanHideViewersOfType<HousingObjectViewerWidget>() || CanHideViewersOfType<MiningObjectViewerWidget>()
          || CanHideViewersOfType<MissionsObjectViewerWidget>() || CanHideViewersOfType<PreScanTargetWidget>()
          || CanHideViewersOfType<HousingObjectViewerWidget>());
}

bool DidHideViewers()
{
  return DidHideViewersOfType<AllianceStarbaseObjectViewerWidget>() || DidHideViewersOfType<ArmadaObjectViewerWidget>()
         || DidHideViewersOfType<CelestialObjectViewerWidget>() || DidHideViewersOfType<EmbassyObjectViewer>()
         || DidHideViewersOfType<HousingObjectViewerWidget>() || DidHideViewersOfType<MiningObjectViewerWidget>()
         || DidHideViewersOfType<MissionsObjectViewerWidget>() || DidHideViewersOfType<PreScanTargetWidget>()
         || DidHideViewersOfType<HousingObjectViewerWidget>();
}

void GotoSection(SectionID sectionID, void* section_data)
{
  Hub::get_SectionManager()->TriggerSectionChange(sectionID, section_data, false, false, true);
}

void ChangeNavigationSection(SectionID sectionID)
{
  const auto section_data = Hub::get_SectionManager()->_sectionStorage->GetState(sectionID);

  if (section_data) {
    GotoSection(sectionID, section_data);
  } else {
    NavigationSectionManager::ChangeNavigationSection(sectionID);
  }
}

#define FleetAction_Format "Fleet {} ({}) #{} - State: {}, previous {} - canAction {}, canState {} - didAction: {}"

template <typename T>
inline bool DidExecuteFleetAction(std::string_view actionText, ActionType actionType,
                                  FleetBarViewController*       fleet_bar,
                                  const std::span<const FleetState> wantedStates,
                                  FleetState                        helpState = FleetState::Unknown)
{
  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet            = fleet_bar->_fleetPanelController->fleet;
  auto fleet_state      = fleet->CurrentState;

  auto       fleet_id   = fleet->Id;
  auto       prev_state = fleet->PreviousState;
  auto       canAction  = true;
  FleetState canState   = FleetState::Unknown;
  auto       didAction  = false;

  if (std::find(std::begin(wantedStates), std::end(wantedStates), fleet_state) != std::end(wantedStates)) {
    canState = fleet_state;
  }

  spdlog::trace(FleetAction_Format, actionText, (int)actionType, (int)fleet_id, (int)fleet_state, (int)prev_state,
                canAction, (int)canState, "[start]");

  if (canState != FleetState::Unknown && canAction) {
    if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
      NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
    }

    didAction = fleet_controller->RequestAction(fleet, actionType, 0, ActionBehaviour::Default);
  }

  if (helpState != FleetState::Unknown && (didAction || helpState == fleet->CurrentState)) {
    didAction = didAction || fleet_controller->RequestAction(fleet, actionType, 0, ActionBehaviour::AskHelp);
  }

  spdlog::trace(FleetAction_Format, actionText, (int)actionType, (int)fleet_id, (int)fleet_state, (int)prev_state,
                canAction, (int)canState, didAction);

  return didAction;
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

          auto canActionPrimary = type != HullType::Any;
          if (type == HullType::ArmadaTarget
              && (armada_state == VisibilityState::Visible || armada_state == VisibilityState::Show)) {
            canActionPrimary = false;
          } else if (force_space_action_next_frame) {
            canActionPrimary = true;
          }

          if (canActionPrimary) {
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

bool CheckShowCargo(RewardsButtonWidget* widget)
{
  if (!Config::Get().show_cargo_default) {
    return false;
  }

  if (!widget->Context) {
    return false;
  }

  const auto target_fleet_deployed = widget->Context->TargetFleetDeployedData;

  if (!target_fleet_deployed) {
    return Config::Get().show_station_cargo;
  }
  auto fleet_type = target_fleet_deployed->FleetType;
  if (fleet_type == DeployedFleetType::Player) {
    return Config::Get().show_player_cargo;
  } else if (fleet_type == DeployedFleetType::Marauder) {
    if (auto hull = target_fleet_deployed->Hull; hull && hull->Type == HullType::ArmadaTarget) {
      return Config::Get().show_armada_cargo;
    } else {
      return Config::Get().show_hostile_cargo;
    }
  }

  return false;
}

} // namespace