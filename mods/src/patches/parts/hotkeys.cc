#include "config.h"

#include <spud/detour.h>

// Object Viewers
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
#include "prime/ArtifactHallDetailsViewController.h"
#include "prime/AssignShipsWidget.h"
#include "prime/BookmarksManager.h"
#include "prime/CanvasController.h"
#include "prime/ChatManager.h"
#include "prime/DeploymentManager.h"
#include "prime/ElementSelectorViewController.h"
#include "prime/FleetBarViewController.h"
#include "prime/FleetLocalViewController.h"
#include "prime/FleetsManager.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/Hub.h"
#include "prime/KeyCode.h"
#include "prime/LanguageManager.h"
#include "prime/NavigationInteractionUIViewController.h"
#include "prime/NavigationSectionManager.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/ScanEngageButtonsWidget.h"
#include "prime/ScreenManager.h"
#include "prime/SelectableList.h"
#include "prime/ShortcutsManager.h"

#include "patches/key.h"
#include "patches/mapkey.h"
#include "patches/parts/daily_faction_bulk_claim.h"
#include "patches/parts/focus_search.h"
#include "str_utils.h"

#include <il2cpp-tabledefs.h>
#include <il2cpp/il2cpp-functions.h>

#include "patches/screen_update_hook.h"

#ifdef _MODDBG
#include "patches/fleet_watch.h"
#endif

#include <EASTL/vector.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

#ifdef _WIN32
#include <Windows.h>
#endif

static bool reset_focus_next_frame = false;
static int  show_info_pending      = 0;

using GetShowKeybindingsFn     = bool(void*);
using SetShowKeybindingsFn     = void(void*, bool);
using CanUseShortcutsFn        = bool();
using ClearTextOverrideFn      = void(void*);
using UpdateShortcutHintTextFn = void(void*);
using GetInputActionFn         = void*(void*);
using GetInputActionNameFn     = Il2CppString*(void*);
using OverrideLocalizedTextFn  = void(void*, Il2CppString*);
using UpdateShortcutHintVisibilityFn = void(void*, bool);

static GetShowKeybindingsFn*     get_show_keybindings               = nullptr;
static SetShowKeybindingsFn*     set_show_keybindings               = nullptr;
static CanUseShortcutsFn*        can_use_shortcuts                  = nullptr;
static ClearTextOverrideFn*      clear_text_override                = nullptr;
static GetInputActionFn*         get_input_action                   = nullptr;
static GetInputActionNameFn*     get_input_action_name              = nullptr;
static OverrideLocalizedTextFn*  override_localized_text            = nullptr;
static UpdateShortcutHintTextFn* original_shortcut_hint_update_text = nullptr;
static ptrdiff_t                  shortcut_hint_input_action_offset   = 0;
static ptrdiff_t                  shortcut_hint_text_localizer_offset = 0;
static bool                       shortcut_hint_fields_ready          = false;
static bool                       initialize_actions_hook_ready       = false;
static bool                       shortcut_hints_ready                = false;

bool SetNativeShortcutHintsVisible(bool visible)
{
  if (!get_show_keybindings || !set_show_keybindings) {
    spdlog::warn("[Hotkeys] native shortcut hints are unavailable");
    return false;
  }

  auto* shortcuts_manager = ShortcutsManager::Instance();
  if (!shortcuts_manager) {
    spdlog::warn("[Hotkeys] native shortcut hints are unavailable");
    return false;
  }

  if (get_show_keybindings(shortcuts_manager) != visible)
    set_show_keybindings(shortcuts_manager, visible);

  return true;
}

bool ToggleNativeShortcutHints()
{
  if (!shortcut_hints_ready || !get_show_keybindings || !set_show_keybindings || !can_use_shortcuts) {
    spdlog::warn("[Hotkeys] native shortcut hints are unavailable");
    return false;
  }

  auto* shortcuts_manager = ShortcutsManager::Instance();
  if (!shortcuts_manager) {
    spdlog::warn("[Hotkeys] native shortcut hints are unavailable");
    return false;
  }

  // Match the native OnShowKeybindingsAction gate while avoiding a fabricated InputAction.CallbackContext.
  if (!can_use_shortcuts())
    return false;

  set_show_keybindings(shortcuts_manager, !get_show_keybindings(shortcuts_manager));

  return true;
}

struct NativeShortcutMapping {
  std::string_view action_name;
  GameFunction     primary;
  GameFunction     fallback = GameFunction::Max;
};

constexpr auto kNativeShortcutMappings = std::to_array<NativeShortcutMapping>({
    {"interior_view", GameFunction::ShowStationInterior},
    {"exterior_view", GameFunction::ShoWStationExterior},
    {"system_view", GameFunction::ShowSystem},
    {"galaxy_view", GameFunction::ShowGalaxy},
    {"events", GameFunction::ShowEvents},
    {"ship_a", GameFunction::SelectShip1},
    {"ship_b", GameFunction::SelectShip2},
    {"ship_c", GameFunction::SelectShip3},
    {"ship_d", GameFunction::SelectShip4},
    {"ship_e", GameFunction::SelectShip5},
    {"ship_f", GameFunction::SelectShip6},
    {"ship_g", GameFunction::SelectShip7},
    {"ship_h", GameFunction::SelectShip8},
    {"alliance", GameFunction::ShowAlliance},
    {"chat", GameFunction::ShowChat},
    {"side_chat", GameFunction::ShowChatSide1, GameFunction::ShowChatSide2},
    {"away_teams", GameFunction::ShowAwayTeam},
    {"missions", GameFunction::ShowMissions},
    {"daily_goals", GameFunction::ShowDaily},
    {"ship_locate", GameFunction::SelectCurrent},
    {"ship_manage", GameFunction::ShowShips},
    {"research", GameFunction::ShowResearch},
    {"consumables", GameFunction::ShowExoComp},
    {"ship_recall", GameFunction::ActionRecall},
    {"show_keybindings", GameFunction::ToggleShortcutHints},
    {"gifts", GameFunction::ShowGifts},
    {"help_alliance", GameFunction::ShowAllianceHelp},
    {"officers", GameFunction::ShowOfficers},
    {"factions", GameFunction::ShowFactions},
    {"items", GameFunction::ShowInventory},
    {"refinery", GameFunction::ShowRefinery},
    {"commanders", GameFunction::ShowCommander},
    {"challenges", GameFunction::ShowQTrials},
});

consteval bool NativeShortcutActionsAreUnique()
{
  for (size_t index = 0; index < kNativeShortcutMappings.size(); ++index) {
    for (size_t other = index + 1; other < kNativeShortcutMappings.size(); ++other) {
      if (kNativeShortcutMappings[index].action_name == kNativeShortcutMappings[other].action_name) {
        return false;
      }
    }
  }
  return true;
}

static_assert(NativeShortcutActionsAreUnique());

constexpr const NativeShortcutMapping* NativeShortcutForAction(std::string_view action_name)
{
  for (const auto& mapping : kNativeShortcutMappings) {
    if (mapping.action_name == action_name) {
      return &mapping;
    }
  }
  return nullptr;
}

static_assert(NativeShortcutForAction("interior_view")->primary == GameFunction::ShowStationInterior);
static_assert(NativeShortcutForAction("side_chat")->fallback == GameFunction::ShowChatSide2);
static_assert(NativeShortcutForAction("unknown") == nullptr);

bool IsInstanceClassField(const FieldInfo* field, std::string_view expected_namespace, std::string_view expected_name)
{
  if (!field || !field->type || field->type->byref || field->type->type != IL2CPP_TYPE_CLASS
      || (il2cpp_field_get_flags(const_cast<FieldInfo*>(field)) & FIELD_ATTRIBUTE_STATIC) != 0
      || field->offset < static_cast<int32_t>(sizeof(Il2CppObject))) {
    return false;
  }

  auto* field_class = il2cpp_class_from_type(field->type);
  return field_class && field_class->namespaze && field_class->name && field_class->namespaze == expected_namespace
         && field_class->name == expected_name;
}

void ShortcutKeybindHint_UpdateText_Hook(auto original, void* _this)
{
  if (!_this || !shortcut_hints_ready) {
    return original(_this);
  }

  auto* text_localizer =
      *reinterpret_cast<void**>(reinterpret_cast<char*>(_this) + shortcut_hint_text_localizer_offset);
  if (!text_localizer) {
    return original(_this);
  }

  // UpdateText does not clear an existing localization override. Clear ours first so native, disabled, and unknown
  // actions cannot retain a stale mod badge.
  clear_text_override(text_localizer);
  original(_this);

  const auto& config = Config::Get();
  if (!config.hotkeys_enabled || config.use_scopely_hotkeys) {
    return;
  }

  auto* input_action_reference =
      *reinterpret_cast<void**>(reinterpret_cast<char*>(_this) + shortcut_hint_input_action_offset);
  if (!input_action_reference) {
    return;
  }

  auto* action = get_input_action(input_action_reference);
  auto* name   = action ? get_input_action_name(action) : nullptr;
  if (!name) {
    return;
  }

  const auto  action_name = to_string(name);
  const auto* mapping     = NativeShortcutForAction(action_name);
  if (!mapping) {
    override_localized_text(text_localizer, il2cpp_string_new("-"));
    return;
  }

  auto shortcut = MapKey::GetShortcutHint(mapping->primary);
  if (shortcut.empty() && mapping->fallback != GameFunction::Max) {
    shortcut = MapKey::GetShortcutHint(mapping->fallback);
  }
  if (shortcut.empty()) {
    shortcut = "-";
  }
  override_localized_text(text_localizer, il2cpp_string_new(shortcut.c_str()));
}

void ShortcutKeybindHint_UpdateVisibility_Hook(auto original, void* _this, bool visible)
{
  original(_this, visible);
  if (visible && shortcut_hints_ready && original_shortcut_hint_update_text) {
    ShortcutKeybindHint_UpdateText_Hook(original_shortcut_hint_update_text, _this);
  }
}

static const MethodInfo* on_events_action = nullptr;
static const MethodInfo* on_galaxy_action = nullptr;

struct InputActionCallbackContext {
  void*   state;
  int32_t action_index;
};

static_assert(sizeof(InputActionCallbackContext) == 16);

bool InvokeNativeShortcut(const MethodInfo* method, const char* action_name)
{
  auto* shortcuts_manager = ShortcutsManager::Instance();
  if (!shortcuts_manager || !method) {
    spdlog::warn("[Hotkeys] native {} shortcut is unavailable", action_name);
    return false;
  }

  // These callbacks do not inspect CallbackContext, but runtime_invoke still requires storage for the value-type
  // argument. Using the managed invoker also avoids platform-specific by-value ABI assumptions.
  InputActionCallbackContext context{};
  void*                      args[]{&context};
  Il2CppException*           exception = nullptr;
  il2cpp_runtime_invoke(method, shortcuts_manager, args, &exception);
  if (exception) {
    spdlog::warn("[Hotkeys] native {} shortcut raised exception={}", action_name, static_cast<void*>(exception));
    return false;
  }

  return true;
}

bool force_space_action_next_frame = false;

void     ChangeNavigationSection(SectionID sectionID);
void     ExecuteSpaceAction(FleetBarViewController* fleet_bar);
bool     DidExecuteRecall(FleetBarViewController* fleet_bar);
bool     DidExecuteRepair(FleetBarViewController* fleet_bar);
HullType GetHullTypeFromBattleTarget(BattleTargetData* context);
void     GotoSection(SectionID sectionID, void* screen_data = nullptr);
bool     CanHideViewers();
bool     DidHideViewers();

void CycleAutoConfirmInstantWarp(Config& config)
{
  const char* state = nullptr;
  switch (config.auto_confirm_instant_warp) {
    case InstantWarpConfirmation::None:
      config.auto_confirm_instant_warp = InstantWarpConfirmation::Warp;
      state                            = "warp";
      break;
    case InstantWarpConfirmation::Warp:
      config.auto_confirm_instant_warp = InstantWarpConfirmation::Jump;
      state                            = "jump";
      break;
    case InstantWarpConfirmation::Jump:
      config.auto_confirm_instant_warp = InstantWarpConfirmation::None;
      state                            = "none";
      break;
  }

  spdlog::info("Auto-confirm instant warp set to {}", state);
}

bool MoveOfficerCanvas(bool goLeft)
{
  auto selectors = ObjectFinder<ElementSelectorViewController>::GetAll();
  if (selectors.empty()) {
    return false;
  }

  bool acted = false;
  for (auto selector : selectors) {
    if (!selector || !selector->isActiveAndEnabled()) {
      continue;
    }
    if (goLeft) {
      selector->PressDecrement();
    } else {
      selector->PressIncrement();
    }
    acted = true;
  }

  return acted;
}

bool MoveArtifactCanvas(bool goLeft)
{
  bool acted = false;
  for (auto controller : ObjectFinder<ArtifactHallDetailsViewController>::GetAll()) {
    if (!controller) {
      spdlog::trace("MoveArtifactCanvas({}) - No controller", (int)goLeft);
      continue;
    }

    if (!controller->isActiveAndEnabled()) {
      spdlog::trace("MoveArtifactCanvas({}) - Controller not active", (int)goLeft);
      continue;
    }

    if (goLeft) {
      spdlog::debug("MoveArtifactCanvas({}) - Pressing Left Arrow", (int)goLeft);
      controller->PressLeftArrow();
    } else {
      spdlog::debug("MoveArtifactCanvas({}) - Pressing Right Arrow", (int)goLeft);
      controller->PressRightArrow();
    }
    acted = true;
  }

  return acted;
}

bool IsRealShipIndex(SelectableList* list, int32_t index)
{
  auto* item = list->DataItem(index);
  if (!item) return false;
  return reinterpret_cast<FleetPlayerData*>(item)->HasShip;
}

bool MoveShipSelectionInDock(bool goLeft)
{
  if (!Config::Get().arrow_keys_to_select_ship) {
    return false;
  }

  bool acted = false;
  for (auto widget : ObjectFinder<AssignShipsWidget>::GetAll()) {
    if (!widget) continue;

    auto canvas = GetCanvasControllerFromComponent(widget);
    if (!canvas || !canvas->Visible() || !widget->isActiveAndEnabled) {
      spdlog::trace("MoveShipSelectionInDock({}) - widget={} not visible/active", (int)goLeft, (void*)widget);
      continue;
    }

    auto* list = widget->_selectableList;
    if (!list) {
      spdlog::trace("MoveShipSelectionInDock({}) - widget={} has no _selectableList", (int)goLeft, (void*)widget);
      continue;
    }

    const auto count = list->Count;
    if (count <= 0) {
      spdlog::trace("MoveShipSelectionInDock({}) - list={} count={}", (int)goLeft, (void*)list, count);
      continue;
    }

    const auto step = goLeft ? -1 : 1;
    auto       newIndex = list->SelectedIndex + step;
    while (newIndex >= 0 && newIndex < count && !IsRealShipIndex(list, newIndex)) {
      newIndex += step;
    }
    if (newIndex < 0 || newIndex >= count) {
      spdlog::trace("MoveShipSelectionInDock({}) - no further real ship in that direction", (int)goLeft);
      continue;
    }

    spdlog::debug("MoveShipSelectionInDock({}) - selecting index {} of {}", (int)goLeft, newIndex, count);
    acted = list->RequestSelect(newIndex) || acted;
  }

  return acted;
}

void ScreenManager_Update_Hook(auto original, ScreenManager* _this)
{
  dispatch_screen_manager_update_callbacks();
  if (!Config::Get().installHotkeyHooks) {
    return original(_this);
  }

  // This function is called every frame to update the screen manager.
  // Create a global clock to detect time elapsed
  static std::chrono::time_point<std::chrono::steady_clock> select_clock             = std::chrono::steady_clock::now();
  static int32_t                                            last_ship_select_request = -1;

  Key::ResetCache();

  if (MapKey::IsDown(GameFunction::DisableHotKeys)) {
    if (shortcut_hints_ready) {
      SetNativeShortcutHintsVisible(false);
    }
    Config::Get().hotkeys_enabled = false;
    spdlog::warn("Setting hotkeys to DISABLED");
    return;
  } else if (MapKey::IsDown(GameFunction::EnableHotKeys)) {
    Config::Get().hotkeys_enabled = true;
    spdlog::warn("Setting hotkeys to ENABLED");
    return;
  }

  if (Config::Get().use_scopely_hotkeys && Config::Get().hotkeys_enabled) {
    return original(_this);
  }

  if (!Config::Get().hotkeys_enabled) {
    return;
  }

  if (shortcut_hints_ready && MapKey::IsDown(GameFunction::ToggleShortcutHints)) {
    ToggleNativeShortcutHints();
    return;
  }

  static auto GetDeltaTime = il2cpp_resolve_icall_typed<float()>("UnityEngine.Time::get_deltaTime()");

  const auto is_in_chat = Hub::IsInChat();
  const auto config     = &Config::Get();

  if (MapKey::IsDown(GameFunction::Restart)) {
    spdlog::info("Clearing localisation cache and restarting");
    LanguageManager::ClearCache();
    Hub::get_App()->Reload();
    return;
  }

#ifdef _WIN32
  if (MapKey::IsDown(GameFunction::Quit)) {
    TerminateProcess(GetCurrentProcess(), 1);
  }
#elif defined(__APPLE__)
  if (MapKey::IsDown(GameFunction::Quit)) {
    Hub::get_App()->Quit();
    return;
  }
#endif

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
        auto plannedCourse =
            DeploymentManger::Instance()->PlanCourse(FleetsManager::Instance()->GetFleetPlayerData(ship_select_request),
                                                     foundDisco->Address, Vector3::zero(), nullptr, nullptr, nullptr);
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
          auto fleet_controller = fleet_bar->_fleetPanelController;
          auto fleet            = fleet_controller ? fleet_controller->fleet : nullptr;
          if (!fleet) {
            return;
          }
          if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
            NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
          }
          FleetsManager::Instance()->RequestViewFleet(fleet, true);
        } else {
          fleet_bar->RequestSelect(ship_select_request);
        }

        last_ship_select_request = ship_select_request;
        select_clock             = select_now;
        return;
      }
    }
  }

  if (Key::Pressed(KeyCode::Escape) && (Key::IsInputFocused() || Hub::IsInChat())) {
    // This fixes issues with detecting when an input is selected
    // As the game usually doesn't clear this when using Escape, only when
    // pressing the back button with the mouse...
    return Key::ClearInputFocus();
  }

  if (!is_in_chat) {
    if (!Key::IsInputFocused()) {
      if (MapKey::IsDown(GameFunction::SelectCurrent)) {
        auto fleet_bar = ObjectFinder<FleetBarViewController>::Get();
        if (fleet_bar) {
          auto fleet_controller = fleet_bar->_fleetPanelController;
          auto fleet            = fleet_controller ? fleet_controller->fleet : nullptr;
          if (fleet) {
            if (NavigationSectionManager::Instance() && NavigationSectionManager::Instance()->SNavigationManager) {
              NavigationSectionManager::Instance()->SNavigationManager->HideInteraction();
            }
            FleetsManager::Instance()->RequestViewFleet(fleet, true);
            return;
          }
        }
      }

      if ((MapKey::IsDown(GameFunction::ToggleQueue))) {
        config->queue_enabled = !config->queue_enabled;
        return;
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
        auto const result = MoveShipSelectionInDock(true) || MoveArtifactCanvas(true) || MoveOfficerCanvas(true);
        if (result) {
          return;
        }
      }

      if (MapKey::IsDown(GameFunction::MoveRight)) {
        auto const result = MoveShipSelectionInDock(false) || MoveArtifactCanvas(false) || MoveOfficerCanvas(false);
        if (result) {
          return;
        }
      }

      if (Config::Get().installFocusSearchHooks && MapKey::IsDown(GameFunction::FocusSearch)) {
        if (FocusSearchBox()) {
          return;
        }
      }

      if (MapKey::IsDown(GameFunction::ShowQTrials)) {
        return GotoSection(SectionID::ChallengeSelection);
      } else if (MapKey::IsDown(GameFunction::ShowBookmarks)) {
        auto bookmark_manager = BookmarksManager::Instance();
        if (bookmark_manager) {
          return bookmark_manager->ViewBookmarks();
        }
        return GotoSection(SectionID::Bookmarks_Main);
      } else if (MapKey::IsDown(GameFunction::ShowLookup)) {
        auto bookmark_manager = BookmarksManager::Instance();
        if (bookmark_manager) {
          bookmark_manager->ViewCoordinateSearch();
          return;
        }
        spdlog::warn("[ShowLookup] BookmarksManager instance not available, falling back to main bookmarks");
        GotoSection(SectionID::Bookmarks_Main);
        return;
      } else if (MapKey::IsDown(GameFunction::ShowRefinery)) {
        return GotoSection(SectionID::Shop_Refining_List);
      } else if (MapKey::IsDown(GameFunction::ShowFactions)) {
        return GotoSection(SectionID::Shop_MainFactions);
      } else if (MapKey::IsDown(GameFunction::ShoWStationExterior)) {
        return GotoSection(SectionID::Starbase_Exterior);
      } else if (MapKey::IsDown(GameFunction::NativeShortcutGalaxy)) {
        InvokeNativeShortcut(on_galaxy_action, "Galaxy");
        return;
      } else if (MapKey::IsDown(GameFunction::ShowGalaxy)) {
        return ChangeNavigationSection(SectionID::Navigation_Galaxy);
      } else if (MapKey::IsDown(GameFunction::ShowStationInterior)) {
        return GotoSection(SectionID::Starbase_Interior);
      } else if (MapKey::IsDown(GameFunction::ShowSystem)) {
        return ChangeNavigationSection(SectionID::Navigation_System);
      } else if (MapKey::IsDown(GameFunction::ShowArtifacts)) {
        return GotoSection(SectionID::ArtifactHall_Inventory);
      } else if (MapKey::IsDown(GameFunction::ShowInventory)) {
        return GotoSection(SectionID::InventoryList);
      } else if (MapKey::IsDown(GameFunction::ShowMissions)) {
        return GotoSection(SectionID::Missions_AcceptedList);
      } else if (MapKey::IsDown(GameFunction::ShowResearch)) {
        return GotoSection(SectionID::Research_LandingPage);
      } else if (MapKey::IsDown(GameFunction::ShowScrapYard)) {
        return GotoSection(SectionID::ShipScrapping_List);
      } else if (MapKey::IsDown(GameFunction::ShowOfficers)) {
        return GotoSection(SectionID::OfficerInventory);
      } else if (MapKey::IsDown(GameFunction::ShowCommander)) {
        // TODO: Does not work properly, defaults to first FleetCommander (spock, rather than selected fleet
        // commander)
        return GotoSection(SectionID::FleetCommander_Management);
      } else if (MapKey::IsDown(GameFunction::ShowAwayTeam)) {
        return GotoSection(SectionID::Missions_AwayTeamsList);
      } else if (MapKey::IsDown(GameFunction::NativeShortcutEvents)) {
        InvokeNativeShortcut(on_events_action, "Events");
        return;
      } else if (MapKey::IsDown(GameFunction::ShowEvents)) {
        return GotoSection(SectionID::Tournament_Group_Selection);
      } else if (MapKey::IsDown(GameFunction::ShowExoComp)) {
        return GotoSection(SectionID::Consumables);
      } else if (MapKey::IsDown(GameFunction::ShowDaily)) {
        return GotoSection(SectionID::Missions_DailyGoals);
      } else if (MapKey::IsDown(GameFunction::ShowGifts)) {
        return GotoSection(SectionID::Shop_List);
      } else if (MapKey::IsDown(GameFunction::ShowAlliance)) {
        return GotoSection(SectionID::Alliance_Main);
      } else if (MapKey::IsDown(GameFunction::ShowAllianceHelp)) {
        return GotoSection(SectionID::Alliance_Help);
      } else if (MapKey::IsDown(GameFunction::ShowAllianceArmada)) {
        return GotoSection(SectionID::Alliance_Armadas);
      } else if (MapKey::IsDown(GameFunction::ShowSettings)) {
        return GotoSection(SectionID::GameSettings);
      } else if (MapKey::IsPressed(GameFunction::UiScaleUp)) {
        config->AdjustUiScale(true);
      } else if (MapKey::IsPressed(GameFunction::UiScaleDown)) {
        config->AdjustUiScale(false);
      } else if (MapKey::IsPressed(GameFunction::UiShipScaleUp)) {
        config->AdjustUiShipScale(true);
      } else if (MapKey::IsPressed(GameFunction::UiShipScaleDown)) {
        config->AdjustUiShipScale(false);
      } else if (MapKey::IsPressed(GameFunction::UiViewerScaleUp)) {
        config->AdjustUiViewerScale(true);
      } else if (MapKey::IsPressed(GameFunction::UiViewerScaleDown)) {
        config->AdjustUiViewerScale(false);
      } else if (MapKey::IsDown(GameFunction::ToggleAutoConfirmInstantWarp)) {
        CycleAutoConfirmInstantWarp(*config);
      } else if (MapKey::IsDown(GameFunction::TogglePreviewLocate)) {
        config->disable_preview_locate = !config->disable_preview_locate;
      } else if (MapKey::IsDown(GameFunction::TogglePreviewRecall)) {
        config->disable_preview_recall = !config->disable_preview_recall;
      } else if (MapKey::IsDown(GameFunction::ToggleCargoDefault)) {
        config->show_cargo_default = !config->show_cargo_default;
      } else if (MapKey::IsDown(GameFunction::ToggleCargoPlayer)) {
        config->show_player_cargo = !config->show_player_cargo;
      } else if (MapKey::IsDown(GameFunction::ToggleCargoStation)) {
        config->show_station_cargo = !config->show_station_cargo;
      } else if (MapKey::IsDown(GameFunction::ToggleCargoHostile)) {
        config->show_hostile_cargo = !config->show_hostile_cargo;
      } else if (MapKey::IsDown(GameFunction::ToggleCargoArmada)) {
        config->show_armada_cargo = !config->show_armada_cargo;
      } else if (MapKey::IsDown(GameFunction::LogLevelOff)) {
        // spdlog::log("Setting log level to OFF");
        spdlog::set_level(spdlog::level::off);
        spdlog::flush_on(spdlog::level::off);
      } else if (MapKey::IsDown(GameFunction::LogLevelError)) {
        spdlog::set_level(spdlog::level::err);
        spdlog::flush_on(spdlog::level::err);
        // spdlog::log("Setting log level to ERROR");
      } else if (MapKey::IsDown(GameFunction::LogLevelWarn)) {
        spdlog::set_level(spdlog::level::warn);
        spdlog::flush_on(spdlog::level::warn);
        // spdlog::log("Setting log level to WARN");
      } else if (MapKey::IsDown(GameFunction::LogLevelInfo)) {
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
        // spdlog::log("Setting log level to INFO");
      } else if (MapKey::IsDown(GameFunction::LogLevelDebug)) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
        // spdlog::log("Setting log level to DEBUG");
      } else if (MapKey::IsDown(GameFunction::LogLevelTrace)) {
        spdlog::set_level(spdlog::level::trace);
        spdlog::flush_on(spdlog::level::trace);
        // spdlog::log("Setting log level to TRACE");
      } else if (MapKey::IsDown(GameFunction::ShowShips)) {
        auto fleet_bar        = ObjectFinder<FleetBarViewController>::Get();
        auto fleet_controller = fleet_bar ? fleet_bar->_fleetPanelController : nullptr;
        auto fleet            = fleet_controller ? fleet_controller->fleet : nullptr;
        if (fleet) {
          fleet_controller->RequestAction(fleet, ActionType::Manage, 0, ActionBehaviour::Default);
        }
      }
    }
  } else {
    if (auto chat_manager = ChatManager::Instance(); chat_manager) {
      if (MapKey::IsDown(GameFunction::SelectChatGlobal)) {
        return chat_manager->OpenChannel(ChatChannelCategory::Global);
      } else if (MapKey::IsDown(GameFunction::SelectChatAlliance)) {
        return chat_manager->OpenChannel(ChatChannelCategory::Alliance);
      } else if (MapKey::IsDown(GameFunction::SelectChatPrivate)) {
        return chat_manager->OpenChannel(ChatChannelCategory::Private);
      }
    }
  }

  if (!Key::IsInputFocused()) {
    // Lets try to remove the pre-scan because we hit escape and it's visible
    if (Key::Pressed(KeyCode::Escape) && DidHideViewers()) {
      return;
    }

    // Dismiss the golden rewards screen when escape or space is pressed.
    if (MapKey::IsDown(GameFunction::ActionPrimary) || Key::Pressed(KeyCode::Escape)) {
      if (auto reward_controller = ObjectFinder<AnimatedRewardsScreenViewController>::Get(); reward_controller) {
        if (reward_controller->IsActive()) {
          return reward_controller->GoBackToLastSection();
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

    DailyFactionBulkClaimUpdate();

    if (MapKey::IsDown(GameFunction::ActionView)) {
      auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAll();

      for (auto& pre_scan_widget : all_pre_scan_widgets) {
        auto visibility_controller = pre_scan_widget ? pre_scan_widget->_visibilityController : nullptr;
        auto rewardsWidget         = pre_scan_widget ? pre_scan_widget->_rewardsButtonWidget : nullptr;
        auto rewards_controller    = rewardsWidget ? rewardsWidget->_rewardsController : nullptr;
        if (visibility_controller
            && (visibility_controller->_state == VisibilityState::Visible
                || visibility_controller->_state == VisibilityState::Show)
            && rewards_controller) {
          if (rewards_controller->_state != VisibilityState::Visible
              && rewards_controller->_state != VisibilityState::Show) {
            show_info_pending = 5;
          } else {
            rewards_controller->Hide();
          }
        }
      }
    }

    // Did we not find a rewards widget in the previous frame?
    if (show_info_pending > 0) {
      auto all_pre_scan_widgets = ObjectFinder<PreScanTargetWidget>::GetAll();

      for (auto& pre_scan_widget : all_pre_scan_widgets) {
        auto       visibility_controller = pre_scan_widget ? pre_scan_widget->_visibilityController : nullptr;
        auto       rewardsWidget         = pre_scan_widget ? pre_scan_widget->_rewardsButtonWidget : nullptr;
        auto       rewards_controller    = rewardsWidget ? rewardsWidget->_rewardsController : nullptr;
        const auto pre_scan_visible      = visibility_controller
                                           && (visibility_controller->_state == VisibilityState::Visible
                                               || visibility_controller->_state == VisibilityState::Show);
        if (pre_scan_visible && rewards_controller) {
          const auto rewards_widget_visible = rewards_controller->_state == VisibilityState::Visible
                                              || rewards_controller->_state == VisibilityState::Show;
          if (!rewards_widget_visible) {
            rewards_controller->Show(true);
          }
        }
      }
      show_info_pending -= 1;
    }
  }

  if (config->disable_escape_exit && Key::Pressed(KeyCode::Escape)) {
    // Keep suppressing a held key. Only distinct key-down edges participate in
    // the double-tap window.
    if (config->disable_escape_exit_timer <= 0 || !Key::Down(KeyCode::Escape)) {
      return;
    }

    static auto previous_escape_down = std::chrono::steady_clock::time_point{};
    const auto  escape_now           = std::chrono::steady_clock::now();
    const auto  escape_diff =
        std::chrono::duration_cast<std::chrono::milliseconds>(escape_now - previous_escape_down);

    if (previous_escape_down == std::chrono::steady_clock::time_point{}
        || escape_diff > std::chrono::milliseconds(config->disable_escape_exit_timer)) {
      previous_escape_down = escape_now;
      return;
    }

    // Consume the completed pair so a rapid third press starts a new one.
    previous_escape_down = {};
  }

  // config->Load();

  return original(_this);
}

// NOTE: If you change this loop functionality, also change DoHideViewersOfType template
template <typename T> inline bool CanHideViewersOfType()
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

bool CanHideViewers()
{
  return (CanHideViewersOfType<AllianceStarbaseObjectViewerWidget>() || CanHideViewersOfType<ArmadaObjectViewerWidget>()
          || CanHideViewersOfType<CelestialObjectViewerWidget>() || CanHideViewersOfType<EmbassyObjectViewer>()
          || CanHideViewersOfType<HousingObjectViewerWidget>() || CanHideViewersOfType<MiningObjectViewerWidget>()
          || CanHideViewersOfType<MissionsObjectViewerWidget>() || CanHideViewersOfType<PreScanTargetWidget>()
          || CanHideViewersOfType<HousingObjectViewerWidget>());
}

// NOTE: If you change this loop functionality, also change CanideViewersOfType template
template <typename T> inline bool DidHideViewersOfType()
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
inline bool DidExecuteFleetAction(std::string_view actionText, ActionType actionType, FleetBarViewController* fleet_bar,
                                  const std::span<const FleetState> wantedStates,
                                  FleetState                        helpState = FleetState::Unknown)
{
  if (!fleet_bar) {
    return false;
  }

  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet            = fleet_controller ? fleet_controller->fleet : nullptr;
  if (!fleet) {
    return false;
  }
  auto fleet_state = fleet->CurrentState;

  auto       fleet_id   = fleet->Id;
  auto       prev_state = fleet->PreviousState;
  auto       canAction  = true; // actionRequired->CheckIsMet();
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

  auto fleet_controller = fleet_bar->_fleetPanelController;

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
  if (!fleet_bar) {
    return;
  }

  auto fleet_controller = fleet_bar->_fleetPanelController;
  auto fleet            = fleet_controller ? fleet_controller->fleet : nullptr;
  if (!fleet) {
    return;
  }

  auto action_queue = ActionQueueManager::Instance();
  if (!action_queue) {
    return;
  }

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
      auto visibility_controller = pre_scan_widget ? pre_scan_widget->_visibilityController : nullptr;
      if (visibility_controller
          && (visibility_controller->_state == VisibilityState::Visible
              || visibility_controller->_state == VisibilityState::Show)) {

        if (auto mine_object_viewer_widget = ObjectFinder<MiningObjectViewerWidget>::Get();
            mine_object_viewer_widget && mine_object_viewer_widget->_visibilityController
            && (mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Visible
                || mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Show)) {
          if (has_secondary && pre_scan_widget->_scanEngageButtonsWidget) {
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

        if (has_secondary && pre_scan_widget->_scanEngageButtonsWidget) {
          return pre_scan_widget->_scanEngageButtonsWidget->OnScanButtonClicked();
        }

        if (has_primary && pre_scan_widget->_scanEngageButtonsWidget
            && pre_scan_widget->_scanEngageButtonsWidget->enabled) {
          auto context = pre_scan_widget->_scanEngageButtonsWidget->Context;
          auto type    = GetHullTypeFromBattleTarget(context);

          // Try once more in X frames if we get ANY
          // in-case of failed to navgitate error?
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

          // Try once more in X frames if we get ANY
          // in-case of failed to navgitate error?
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
        mine_object_viewer_widget && mine_object_viewer_widget->_visibilityController
        && (mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Visible
            || mine_object_viewer_widget->_visibilityController->_state == VisibilityState::Show)) {
      if (has_secondary && mine_object_viewer_widget->_scanEngageButtonsWidget) {
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
        return;
      } else if (has_primary) {
        star_node_object_viewer_widget->InitiateWarp();
        return;
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
    }

    if (has_recall && DidExecuteRecall(fleet_bar)) {
      force_space_action_next_frame = false;
      return;
    }
    if (has_repair && DidExecuteRepair(fleet_bar)) {
      force_space_action_next_frame = false;
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

void ChatMessageListLocalViewController_AboutToShow_Hook(ChatMessageListLocalViewController* _this);
decltype(ChatMessageListLocalViewController_AboutToShow_Hook)* oChatMessageListLocalViewController_AboutToShow =
    nullptr;
void ChatMessageListLocalViewController_AboutToShow_Hook(ChatMessageListLocalViewController* _this)
{
  oChatMessageListLocalViewController_AboutToShow(_this);
  if (_this->_inputField) {
    _this->_inputField->SendOnFocus();
  }
}

void InitializeActions_Hook(auto original, void* _this)
{
  if (Config::Get().use_scopely_hotkeys) {
    return original(_this);
  }
}

bool CheckShowCargo(RewardsButtonWidget* widget)
{
  if (!widget || !Config::Get().show_cargo_default) {
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

void OnDidBindContext_Hook(auto original, RewardsButtonWidget* _this)
{
  if (!_this) {
    return original(_this);
  }

  auto rewards_controller = _this->_rewardsController;
  auto pre_state          = rewards_controller ? rewards_controller->_state : VisibilityState::Unknown;
  pre_state               = pre_state;
  original(_this);

  rewards_controller = _this->_rewardsController;
  auto post_state    = rewards_controller ? rewards_controller->_state : VisibilityState::Unknown;
  post_state         = post_state;
  if (rewards_controller && CheckShowCargo(_this)) {
    rewards_controller->Show(true);
    show_info_pending = 1;
  }
}

void ShowWithFleet_Hook(auto original, PreScanTargetWidget* _this, void* a1)
{
  original(_this, a1);
  if (!_this) {
    return;
  }

  auto rewards_button_widget = _this->_rewardsButtonWidget;
  auto rewards_controller    = rewards_button_widget ? rewards_button_widget->_rewardsController : nullptr;
  if (rewards_controller && CheckShowCargo(rewards_button_widget)) {
    rewards_controller->Show(true);
    show_info_pending = 1;
  }
}

bool install_screen_manager_update_hook()
{
  static bool installed = false;
  if (installed) {
    return true;
  }

  auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ScreenManager");
  if (!helper.isValidHelper()) {
    ErrorMsg::MissingHelper("UI", "ScreenManager");
  } else if (auto update = helper.GetMethod("Update"); update) {
    SPUD_STATIC_DETOUR(update, ScreenManager_Update_Hook);
    installed = true;
    return true;
  } else {
    ErrorMsg::MissingMethod("ScreenManager", "Update");
  }
  return false;
}

void InstallShortcutHintHooks()
{
  // Configuration is fully parsed before hook installation. NONE, empty, and invalid bindings opt out.
  if (!MapKey::HasBinding(GameFunction::ToggleShortcutHints)) {
    return;
  }

  auto shortcuts_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
  if (!shortcuts_manager_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("GameInput", "ShortcutsManager");
  } else {
    get_show_keybindings = shortcuts_manager_helper.GetMethod<bool(void*)>("get_ShowKeybindings", 0);
    set_show_keybindings = shortcuts_manager_helper.GetMethod<void(void*, bool)>("set_ShowKeybindings", 1);
    can_use_shortcuts     = shortcuts_manager_helper.GetMethod<bool()>("get_CanUseShortcuts", 0);
    if (!get_show_keybindings)
      ErrorMsg::MissingMethod("ShortcutsManager", "get_ShowKeybindings");
    if (!set_show_keybindings)
      ErrorMsg::MissingMethod("ShortcutsManager", "set_ShowKeybindings");
    if (!can_use_shortcuts)
      ErrorMsg::MissingMethod("ShortcutsManager", "get_CanUseShortcuts");
  }

  auto text_localizer_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "TextLocalizer");
  if (!text_localizer_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Client.UI", "TextLocalizer");
  } else {
    clear_text_override = text_localizer_helper.GetMethod<void(void*)>("ClearTextOverride", 0);
    override_localized_text =
        text_localizer_helper.GetMethod<void(void*, Il2CppString*)>("OverrideLocalizedText", 1);
    if (!clear_text_override)
      ErrorMsg::MissingMethod("TextLocalizer", "ClearTextOverride");
    if (!override_localized_text)
      ErrorMsg::MissingMethod("TextLocalizer", "OverrideLocalizedText");
  }

  UpdateShortcutHintTextFn* shortcut_hint_update_text = nullptr;
  UpdateShortcutHintVisibilityFn* shortcut_hint_update_visibility = nullptr;
  auto shortcut_hint_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutKeybindHint");
  if (!shortcut_hint_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("GameInput", "ShortcutKeybindHint");
  } else {
    auto* input_action_field   = il2cpp_class_get_field_from_name(shortcut_hint_helper.get_cls(), "_inputAction");
    auto* text_localizer_field = il2cpp_class_get_field_from_name(shortcut_hint_helper.get_cls(), "_keyTextLocalizer");
    const bool input_action_field_valid =
        IsInstanceClassField(input_action_field, "UnityEngine.InputSystem", "InputActionReference");
    const bool text_localizer_field_valid =
        IsInstanceClassField(text_localizer_field, "Digit.Client.UI", "TextLocalizer");
    if (!input_action_field_valid) {
      spdlog::error("Unable to resolve field 'ShortcutKeybindHint->_inputAction' as InputActionReference");
    }
    if (!text_localizer_field_valid) {
      spdlog::error("Unable to resolve field 'ShortcutKeybindHint->_keyTextLocalizer' as TextLocalizer");
    }
    if (input_action_field_valid && text_localizer_field_valid) {
      shortcut_hint_input_action_offset   = input_action_field->offset;
      shortcut_hint_text_localizer_offset = text_localizer_field->offset;
      shortcut_hint_fields_ready          = true;
    }

    shortcut_hint_update_text = shortcut_hint_helper.GetMethod<void(void*)>("UpdateText", 0);
    shortcut_hint_update_visibility =
        shortcut_hint_helper.GetMethod<void(void*, bool)>("UpdateVisibility", 1);
    if (!shortcut_hint_update_text) {
      ErrorMsg::MissingMethod("ShortcutKeybindHint", "UpdateText");
    }
    if (!shortcut_hint_update_visibility)
      ErrorMsg::MissingMethod("ShortcutKeybindHint", "UpdateVisibility");
  }

  auto input_action_reference_helper =
      il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputActionReference");
  auto input_action_helper =
      il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputAction");
  if (!input_action_reference_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("UnityEngine.InputSystem", "InputActionReference");
  } else {
    get_input_action = input_action_reference_helper.GetMethod<void*(void*)>("get_action", 0);
    if (!get_input_action)
      ErrorMsg::MissingMethod("InputActionReference", "get_action");
  }
  if (!input_action_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("UnityEngine.InputSystem", "InputAction");
  } else {
    get_input_action_name = input_action_helper.GetMethod<Il2CppString*(void*)>("get_name", 0);
    if (!get_input_action_name)
      ErrorMsg::MissingMethod("InputAction", "get_name");
  }

  if (get_show_keybindings && set_show_keybindings && can_use_shortcuts && initialize_actions_hook_ready &&
      clear_text_override && override_localized_text && shortcut_hint_fields_ready && shortcut_hint_update_text &&
      shortcut_hint_update_visibility && get_input_action && get_input_action_name) {
    MapKey::CacheShortcutHints();
    original_shortcut_hint_update_text =
        SPUD_STATIC_DETOUR(shortcut_hint_update_text, ShortcutKeybindHint_UpdateText_Hook);
    SPUD_STATIC_DETOUR(shortcut_hint_update_visibility, ShortcutKeybindHint_UpdateVisibility_Hook);
    shortcut_hints_ready = true;
  }
}

void InstallHotkeyHooks()
{
  auto shortcuts_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
  if (!shortcuts_manager_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("GameInput", "ShortcutsManager");
  } else {
    on_events_action = shortcuts_manager_helper.GetMethodInfo("OnEventsAction", 1);
    if (on_events_action == nullptr) {
      ErrorMsg::MissingMethod("ShortcutsManager", "OnEventsAction");
    }

    on_galaxy_action = shortcuts_manager_helper.GetMethodInfo("OnGalaxyAction", 1);
    if (on_galaxy_action == nullptr) {
      ErrorMsg::MissingMethod("ShortcutsManager", "OnGalaxyAction");
    }

    auto ptr_can_user_shortcuts = shortcuts_manager_helper.GetMethod("InitializeActions");
    if (ptr_can_user_shortcuts == nullptr) {
      ErrorMsg::MissingMethod("ShortcutsManager", "InitializeActions");
    } else {
      SPUD_STATIC_DETOUR(ptr_can_user_shortcuts, InitializeActions_Hook);
      initialize_actions_hook_ready = true;
    }
  }

  InstallShortcutHintHooks();

  install_screen_manager_update_hook();
#ifdef _MODDBG
  fleet_watch::InstallRuntimeProbe();
#endif

  static auto rewards_button_widget =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Combat", "RewardsButtonWidget");
  if (!rewards_button_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Combat", "RewardsButtonWidget");
  } else {
    auto on_did_bind_context_ptr = rewards_button_widget.GetMethod("OnDidBindContext");
    on_did_bind_context_ptr      = on_did_bind_context_ptr;
    if (on_did_bind_context_ptr == nullptr) {
      ErrorMsg::MissingMethod("RewardsButtonWidget", "OnDidBindContext");
    } else {
      SPUD_STATIC_DETOUR(on_did_bind_context_ptr, OnDidBindContext_Hook);
    }
  }

  static auto pre_scan_target_widget =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Combat", "PreScanTargetWidget");
  if (!pre_scan_target_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Combat", "PreScanTargetWidget");
  } else {
    auto show_with_fleet_ptr = pre_scan_target_widget.GetMethod("ShowWithFleet");
    show_with_fleet_ptr      = show_with_fleet_ptr;
    if (show_with_fleet_ptr == nullptr) {
      ErrorMsg::MissingMethod("PreScanTargetWidget", "ShowWithFleet");
    } else {
      SPUD_STATIC_DETOUR(show_with_fleet_ptr, ShowWithFleet_Hook);
    }
  }
}
