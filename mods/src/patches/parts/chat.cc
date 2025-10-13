#include "prime/ChatPreviewController.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/GenericButtonContext.h"

#include "config.h"
#include "errormsg.h"

#include <spud/detour.h>

void FullScreenChatViewController_AboutToShow(auto original, FullScreenChatViewController* _this)
{
  original(_this);

  const auto disable_server = Config::Get().disable_galaxy_chat;
  const auto disable_regional = Config::Get().disable_veil_chat;

  if (!(disable_server || disable_regional))
    return;

  if (const auto chat_manager = ChatManager::Instance(); chat_manager) {
    const auto hasCadetChat    = chat_manager->CanSeeNewbieChat();
    const auto hasVeilChat     = chat_manager->CanSeeRegionalChat();
    const auto galaxyChatIndex = 0 + (hasCadetChat ? 1 : 0);
    const auto veilChatIndex   = galaxyChatIndex + 1;

    const auto viewController = _this->_categoriesTabBarViewController;
    if (!viewController) {
      return;
    }

    const auto tabBar = viewController->_tabBar;
    if (!tabBar) {
      return;
    }

    if (const auto list = tabBar->_listContainer; !list) {
      return;
    }

    const auto listData = tabBar->_data;
    if (!listData) {
      return;
    }

    if (disable_server) {
      if (const auto elm = listData->Get(galaxyChatIndex)) {
        reinterpret_cast<GenericButtonContext*>(elm)->Interactable = false;
      }
    }

    if (disable_regional && hasVeilChat) {
      if (const auto elm = listData->Get(veilChatIndex)) {
        reinterpret_cast<GenericButtonContext*>(elm)->Interactable = false;
      }
    }
  }
}

void ChatPreviewController_AboutToShow(auto original, ChatPreviewController* _this)
{
  original(_this);

  if (Config::Get().disable_galaxy_chat || Config::Get().disable_veil_chat) {
    if (const auto chat_manager = ChatManager::Instance(); chat_manager) {
      const auto hasCadetChat      = chat_manager->CanSeeNewbieChat();
      const auto hasVeilChat       = chat_manager->CanSeeRegionalChat();
      const auto allianceChatIndex = (hasCadetChat ? 1 : 0) + 1 + (hasVeilChat ? 1 : 0);

      _this->_focusedPanel = ChatChannelCategory::Alliance;
      if (_this->_swipeScroller->_currentContentIndex != allianceChatIndex) {
        _this->_swipeScroller->FocusOnInstantly(allianceChatIndex);
      }
    }
  }
}

void ChatPreviewController_OnPanelFocused(auto original, ChatPreviewController* _this, int32_t index)
{
  const auto disable_server = Config::Get().disable_galaxy_chat;
  const auto disable_regional = Config::Get().disable_veil_chat;

  if (!(disable_server || disable_regional)) {
    original(_this, index);
    return;
  }

  if (const auto chat_manager = ChatManager::Instance(); chat_manager) {
    const auto hasCadetChat      = chat_manager->CanSeeNewbieChat();
    const auto hasVeilChat       = chat_manager->CanSeeRegionalChat();
    const auto allianceChatIndex = (hasCadetChat ? 1 : 0) + 1 + (hasVeilChat ? 1 : 0);

    if (disable_server || hasVeilChat && disable_regional) {
      _this->_focusedPanel = ChatChannelCategory::Alliance;
      original(_this, allianceChatIndex);

      if (_this->_swipeScroller->_currentContentIndex != allianceChatIndex) {
        _this->_swipeScroller->FocusOnInstantly(allianceChatIndex);
      }

      return;
    }
  }

  original(_this, index);
}

void ChatPreviewController_OnGlobalMessageReceived(auto original, ChatPreviewController* _this, void* message)
{
  if (Config::Get().disable_galaxy_chat)
    return;

  original(_this, message);
}

void ChatPreviewController_OnRegionalMessageReceived(auto original, ChatPreviewController* _this, void* message)
{
  if (Config::Get().disable_veil_chat)
    return;

  original(_this, message);
}

void InstallChatPatches()
{
  if (auto fullscreen_controller =
          il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Chat", "FullScreenChatViewController");
      !fullscreen_controller.isValidHelper()) {
    ErrorMsg::MissingHelper("Chat", "FullScreenChatViewController");
  } else {
    if (const auto ptr = fullscreen_controller.GetMethod("AboutToShow"); ptr == nullptr) {
      ErrorMsg::MissingMethod("FullScreenChatViewController", "AboutToShow");
    } else {
      // this is fully broken, detour doesn't work
      SPUD_STATIC_DETOUR(ptr, FullScreenChatViewController_AboutToShow);
    }
  }

  if (auto preview_controller = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Chat", "ChatPreviewController");
      !preview_controller.isValidHelper()) {
    ErrorMsg::MissingHelper("Chat", "ChatPreviewController");
  } else {
    if (const auto ptr = preview_controller.GetMethod("AboutToShow"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ChatPreviewController", "AboutToShow");
    } else {
      SPUD_STATIC_DETOUR(ptr, ChatPreviewController_AboutToShow);
    }

    if (const auto ptr = preview_controller.GetMethod("OnPanelFocused"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ChatPreviewController", "OnPanelFocused");
    } else {
      SPUD_STATIC_DETOUR(ptr, ChatPreviewController_OnPanelFocused);
    }

    if (const auto ptr = preview_controller.GetMethod("OnGlobalMessageReceived"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ChatPreviewController", "OnGlobalMessageReceived");
    } else {
      SPUD_STATIC_DETOUR(ptr, ChatPreviewController_OnGlobalMessageReceived);
    }

    if (const auto ptr = preview_controller.GetMethod("OnRegionalMessageReceived"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ChatPreviewController", "OnRegionalMessageReceived");
    } else {
      SPUD_STATIC_DETOUR(ptr, ChatPreviewController_OnRegionalMessageReceived);
    }
  }
}
