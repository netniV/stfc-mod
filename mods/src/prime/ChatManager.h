#pragma once

#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

#include "MonoSingleton.h"

enum class ChatChannelCategory : int32_t {
  None            = -1,
  Newbie          = 0,
  Global          = 1,
  Alliance        = 2,
  Private         = 3,
  Private_Message = 4,
  Block           = 5,
  Regional        = 6,
};

enum class ChatViewMode : int32_t {
  Fullscreen = 0,
  Side       = 1,
};

struct ChatManager : MonoSingleton<ChatManager> {
  friend struct MonoSingleton;

public:
  __declspec(property(get = __get_IsSideChatAllowed)) bool IsSideChatAllowed;
  __declspec(property(get = __get_IsSideChatOpen)) bool IsSideChatOpen;

  void OpenChannel(ChatChannelCategory category = ChatChannelCategory::Alliance)
  {
    static auto OpenChannelMethod =
        get_class_helper().GetMethod<void(ChatManager*, ChatChannelCategory, void*, void*)>("OpenChannel", 2);
    static auto  OpenChannelWarn = true;
    static void* params            = il2cpp_string_new("");
    if (OpenChannelMethod) {
      OpenChannelMethod(this, category, params, nullptr);
    } else if (OpenChannelWarn) {
      OpenChannelWarn = false;
      ErrorMsg::MissingMethod("ChatManager", "OpenChannel");
    }
  }

  void OpenChannel(ChatChannelCategory category, ChatViewMode viewMode)
  {
    this->__set_ViewMode(viewMode);
    this->OpenChannel(category);
  }

  bool CanSeeNewbieChat()
  {
    static auto CanSeeNewbieChatMethod = get_class_helper().GetMethod<bool(ChatManager*)>("CanSeeNewbieChat");
    static auto CanSeeNewbieChatWarn = true;

    if (CanSeeNewbieChatMethod != nullptr) {
      return CanSeeNewbieChatMethod(this);
    }

    if (CanSeeNewbieChatWarn) {
      CanSeeNewbieChatWarn = false;
      ErrorMsg::MissingMethod("ChatManager", "CanSeeNewbieChat");
    }

    return false;
  }

  bool CanSeeRegionalChat()
  {
    static auto CanSeeRegionalChatMethod = get_class_helper().GetMethod<bool(ChatManager*)>("CanSeeRegionalChat");
    static auto CanSeeRegionalChatWarn = true;

    if (CanSeeRegionalChatMethod != nullptr) {
      return CanSeeRegionalChatMethod(this);
    }

    if (CanSeeRegionalChatWarn) {
      CanSeeRegionalChatWarn = false;
      ErrorMsg::MissingMethod("ChatManager", "CanSeeRegionalChat");
    }

    return false;
  }

public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Chat", "ChatManager");
    return class_helper;
  }

  bool __get_IsSideChatAllowed()
  {
    static auto field = get_class_helper().GetProperty("IsSideChatAllowed");
    return *field.Get<bool>(this);
  }

  bool __get_IsSideChatOpen()
  {
    static auto field = get_class_helper().GetProperty("IsSideChatOpen");
    return *field.Get<bool>(this);
  }

  ChatViewMode __get_ViewMode()
  {
    static auto field = get_class_helper().GetProperty("ViewMode");
    return *field.Get<ChatViewMode>(this);
  }

  void __set_ViewMode(ChatViewMode v)
  {
    static auto field = get_class_helper().GetProperty("ViewMode");
    field.SetRaw<ChatViewMode>(this, v);
  }
};
