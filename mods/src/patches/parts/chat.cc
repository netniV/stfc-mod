#include "prime/ChatPreviewController.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/GenericButtonContext.h"

#include "config.h"
#include "errormsg.h"

#include <spud/detour.h>

bool FullScreenChatViewController_AboutToShow(auto original, FullScreenChatViewController* _this3)
{
  original(_this3);
  if (Config::Get().disable_galaxy_chat) {
    auto viewController = _this3->_categoriesTabBarViewController;
    if (!viewController) {
      return false;
    }
    auto tabBar = viewController->_tabBar;
    if (!tabBar) {
      return false;
    }
    auto list = tabBar->_listContainer;
    if (!list) {
      return false;
    }
    auto listData = tabBar->_data;
    if (!listData) {
      return false;
    }
    auto elm = listData->Get(0);
    if (!elm) {
      return false;
    }
    ((GenericButtonContext*)elm)->Interactable = false;
  }
  return false;
}

bool ChatPreviewController_AboutToShow(auto original, ChatPreviewController* _this, const MethodInfo* method)
{
  original(_this, method);
  if (Config::Get().disable_galaxy_chat) {
    _this->_focusedPanel = PanelState::Alliance;
    if (_this->_swipeScroller->_currentContentIndex != PanelState::Alliance) {
      _this->_swipeScroller->FocusOnInstantly(PanelState::Alliance);
    }
  }
  return false;
}

void ChatPreviewController_OnPanel_Focused(auto original, ChatPreviewController* _this, int32_t index)
{
  if (Config::Get().disable_galaxy_chat) {
    _this->_focusedPanel = PanelState::Alliance;
    original(_this, PanelState::Alliance);
    if (_this->_swipeScroller->_currentContentIndex != PanelState::Alliance) {
      _this->_swipeScroller->FocusOnInstantly(PanelState::Alliance);
    }
  } else {
    original(_this, index);
  }
}

void ChatPreviewController_OnGlobalMessageReceived(auto original, ChatPreviewController* _this, void* message)
{
  if (Config::Get().disable_galaxy_chat) {
    return;
  } else {
    original(_this, message);
  }
}

void InstallChatPatches()
{
  static auto fullscreen_controller =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Chat", "FullScreenChatViewController");

  if (!fullscreen_controller.HasClass()) {
    ErrorMsg::MissingHelper("Chat", "FullScreenChatViewController");
  } else {
    auto ptr = fullscreen_controller.GetMethod("AboutToShow");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("FullScreenChatViewController", "AboutToShow");
    } else {
      SPUD_STATIC_DETOUR(ptr, FullScreenChatViewController_AboutToShow);
    }
  }

  static auto preview_controller =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Chat", "ChatPreviewController");

  if (!preview_controller.HasClass()) {
    ErrorMsg::MissingHelper("Chat", "ChatPreviewController");
  } else {
    auto ptr = preview_controller.GetMethod("AboutToShow");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("ChatPreviewController", "AboutToShow");
    } else {
      SPUD_STATIC_DETOUR(ptr, ChatPreviewController_AboutToShow);
    }

    ptr = preview_controller.GetMethod("OnPanel_Focused");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("ChatPreviewController", "OnPanel_Focused");
    } else {
      SPUD_STATIC_DETOUR(ptr, ChatPreviewController_OnPanel_Focused);
    }

    ptr = preview_controller.GetMethod("OnGlobalMessageReceived");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("ChatPreviewController", "OnGlobalMessageReceived");
    } else {
      SPUD_STATIC_DETOUR(ptr, ChatPreviewController_OnGlobalMessageReceived);
    }
  }
}
