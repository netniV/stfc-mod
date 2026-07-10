#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "ChatManager.h"
#include "TMP_InputField.h"

struct ChatSectionContext {
public:
  __declspec(property(get = __get__category)) ChatChannelCategory _category;
  ;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Chat", "ChatSectionContext");
    return class_helper;
  }

public:
  ChatChannelCategory __get__category()
  {
    static auto field = get_class_helper().GetField("_category").offset();
    return *(ChatChannelCategory*)((uintptr_t)this + field);
  }
};

struct ChatMessageListLocalViewController {
public:
  __declspec(property(get = __get__inputField)) TMP_InputField* _inputField;
  __declspec(property(get = __get__inputFieldSelected)) bool _inputFieldSelected;
  __declspec(property(get = __get_CanvasContext)) ChatSectionContext* CanvasContext;

private:
  friend class ObjectFinder<ChatMessageListLocalViewController>;

public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Chat", "ChatMessageListLocalViewController");
    return class_helper;
  }

  TMP_InputField* __get__inputField()
  {
    static auto field = get_class_helper().GetField("_inputField").offset();
    return *(TMP_InputField**)((uintptr_t)this + field);
  }
  bool __get__inputFieldSelected()
  {
    static auto field = get_class_helper().GetField("_inputFieldSelected").offset();
    return *(bool*)((uintptr_t)this + field);
  }
  ChatSectionContext* __get_CanvasContext()
  {
    static auto field = get_class_helper().GetProperty("CanvasContext");
    return field.Get<ChatSectionContext>(this);
  }
};
