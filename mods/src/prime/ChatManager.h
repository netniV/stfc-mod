#pragma once

#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

#include "MonoSingleton.h"

enum class ChatChannelCategory // TypeDefIndex: 11600
{
  None            = -1,
  Global          = 0,
  Alliance        = 1,
  Private         = 2,
  Private_Message = 3,
  Block           = 4
};

enum class ChatViewMode {
  Fullscreen,
  Side,
};

struct ChatManager : MonoSingleton<ChatManager> {
  friend struct MonoSingleton<ChatManager>;

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

public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Chat", "ChatManager");
    return class_helper;
  }

  //
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
