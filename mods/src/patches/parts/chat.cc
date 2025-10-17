#include "prime/ChatPreviewController.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/GenericButtonContext.h"

#include "config.h"
#include "errormsg.h"

#include <spud/detour.h>
#include <tuple>

auto GetChatTabIndices()
{
  if (const auto chat_manager = ChatManager::Instance(); chat_manager) {
    const auto hasCadetChat    = chat_manager->CanSeeNewbieChat();
    const auto hasVeilChat     = chat_manager->CanSeeRegionalChat();

    const auto galaxyChatIndex = 0 + (hasCadetChat ? 1 : 0);
    const auto veilChatIndex   = galaxyChatIndex + 1;
    const auto allianceChatIndex = (hasCadetChat ? 1 : 0) + 1 + (hasVeilChat ? 1 : 0);

    return std::make_tuple(hasCadetChat ? 0 : -1, galaxyChatIndex, hasVeilChat ? veilChatIndex : -1, allianceChatIndex);
  }

  return std::make_tuple(-1, 0, -1, 1);
}

void DisableButtons(FullScreenChatViewController* _this)
{
  const auto disableGalaxyChat = Config::Get().disable_galaxy_chat;
  const auto disableVeilChat = Config::Get().disable_veil_chat;

  if (!(disableGalaxyChat || disableVeilChat))
    return;

  const auto [cadetChatIdx, galaxyChatIdx, veilChatIdx, allianceChatIdx] = GetChatTabIndices();

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

  if (disableGalaxyChat) {
    if (const auto elm = listData->Get(galaxyChatIdx)) {
      const auto button = reinterpret_cast<GenericButtonContext*>(elm);
      button->Interactable = false;
    }
  }

  if (disableVeilChat && veilChatIdx != -1) {
    if (const auto elm = listData->Get(veilChatIdx)) {
      const auto button = reinterpret_cast<GenericButtonContext*>(elm);
      button->Interactable = false;
    }
  }
}

void FullScreenChatViewController_AboutToShow(auto original, FullScreenChatViewController* _this)
{
  original(_this);
  DisableButtons(_this);
}

void FullScreenChatViewController_OnDidChangeSelectedTab(auto original, FullScreenChatViewController* _this, int32_t tabIdx, void* tab)
{
  const auto [cadetChatIdx, galaxyChatIdx, veilChatIdx, allianceChatIdx] = GetChatTabIndices();
  if ((tabIdx == galaxyChatIdx && Config::Get().disable_galaxy_chat) || (tabIdx == veilChatIdx && Config::Get().disable_veil_chat)) {
    // don't show disabled chats if the associated tab was selected
    return;
  }

  original(_this, tabIdx, tab);
}

void ChatPreviewController_AboutToShow(auto original, ChatPreviewController* _this)
{
  original(_this);

  if (Config::Get().disable_galaxy_chat || Config::Get().disable_veil_chat) {
    const auto allianceChatIdx = std::get<3>(GetChatTabIndices());
    _this->_focusedPanel       = ChatChannelCategory::Alliance;

    if (_this->_swipeScroller->_currentContentIndex != allianceChatIdx) {
      _this->_swipeScroller->FocusOnInstantly(allianceChatIdx);
    }
  }
}

void ChatPreviewController_OnPanelFocused(auto original, ChatPreviewController* _this, int32_t index)
{
  static const auto disableGalaxyChat = Config::Get().disable_galaxy_chat;
  static const auto disableVeilChat   = Config::Get().disable_veil_chat;

  if (!(disableGalaxyChat || disableVeilChat)) {
    original(_this, index);
    return;
  }

  const auto [cadetChatIdx, galaxyChatIdx, veilChatIdx, allianceChatIdx] = GetChatTabIndices();
  if (disableGalaxyChat || (veilChatIdx != -1 && disableVeilChat)) {
    _this->_focusedPanel = ChatChannelCategory::Alliance;
    original(_this, allianceChatIdx);

    if (_this->_swipeScroller->_currentContentIndex != allianceChatIdx) {
      _this->_swipeScroller->FocusOnInstantly(allianceChatIdx);
    }

    return;
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
      SPUD_STATIC_DETOUR(ptr, FullScreenChatViewController_AboutToShow);
    }

    if (const auto ptr = fullscreen_controller.GetMethod("OnDidChangeSelectedTab"); ptr == nullptr) {
      ErrorMsg::MissingMethod("FullScreenChatViewController", "OnDidChangeSelectedTab");
    } else {
      SPUD_STATIC_DETOUR(ptr, FullScreenChatViewController_OnDidChangeSelectedTab);
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
