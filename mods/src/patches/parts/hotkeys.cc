#include "config.h"

#include <spud/detour.h>

#include "patches/hotkey_router.h"

#include "prime/ChatMessageListLocalViewController.h"

void ScreenManager_Update_Hook(auto original, ScreenManager* _this);

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
  if (hotkey_router_init_actions()) {
    return original(_this);
  }
}

void OnDidBindContext_Hook(auto original, RewardsButtonWidget* _this)
{
  original(_this);
  hotkey_router_bind_context(_this);
}

void ShowWithFleet_Hook(auto original, PreScanTargetWidget* _this, void* a1)
{
  original(_this, a1);
  hotkey_router_show_fleet(_this);
}

void InstallHotkeyHooks()
{
  hotkey_router_init();

  auto shortcuts_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
  if (!shortcuts_manager_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("GameInput", "ShortcutsManager");
  } else {
    auto ptr_can_user_shortcuts = shortcuts_manager_helper.GetMethod("InitializeActions");
    if (ptr_can_user_shortcuts == nullptr) {
      ErrorMsg::MissingMethod("ShortcutsManager", "InitializeActions");
    } else {
      SPUD_STATIC_DETOUR(ptr_can_user_shortcuts, InitializeActions_Hook);
    }
  }

  auto screen_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ScreenManager");
  if (!screen_manager_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("UI", "ScreenManager");
  } else {
    auto ptr_update = screen_manager_helper.GetMethod("Update");
    if (ptr_update == nullptr) {
      ErrorMsg::MissingMethod("ScreenManager", "Update");
    } else {
      SPUD_STATIC_DETOUR(ptr_update, ScreenManager_Update_Hook);
    }
  }

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

void ScreenManager_Update_Hook(auto original, ScreenManager* _this)
{
  if (hotkey_router_screen_update(_this)) {
    return original(_this);
  }
}
