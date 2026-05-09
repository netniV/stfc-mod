#include "errormsg.h"
#include "config.h"

#include "patches/hotkey_router.h"

#include "patches/cargo_display.h"
#include "patches/fleet_actions.h"
#include "patches/hotkey_dispatch.h"
#include "patches/key.h"
#include "patches/mapkey.h"
#include "patches/navigation.h"
#include "patches/viewer_mgmt.h"

#include "prime/BookmarksManager.h"
#include "prime/ChatManager.h"
#include "prime/FleetBarViewController.h"
#include "prime/FleetsManager.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/Hub.h"
#include "prime/KeyCode.h"
#include "prime/NavigationSectionManager.h"
#include "prime/ObjectViewerBaseWidget.h"
#include "prime/ScanEngageButtonsWidget.h"
#include "prime/PreScanTargetWidget.h"

#include <Windows.h>

#include <spdlog/spdlog.h>

namespace {

} // namespace

void hotkey_router_init()
{
}

bool hotkey_router_screen_update(ScreenManager* screen_manager)
{
  screen_manager = screen_manager;

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

  if (HandleShipSelection(ship_select_request)) {
    return false;
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
      if (TryDismissRewardsScreen()) {
        return false;
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
      HandleActionView();
    }

    TickInfoPending();
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
  HandleCargoBindContext(widget);
}

void hotkey_router_show_fleet(PreScanTargetWidget* widget)
{
  HandleCargoShowFleet(widget->_rewardsButtonWidget);
}

namespace {

} // namespace