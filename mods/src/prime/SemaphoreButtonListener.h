#pragma once

#include "Button.h"
#include "SemaphoreListenerBase.h"
#include <il2cpp/il2cpp_helper.h>

class SemaphoreButtonListener : SemaphoreListenerBase
{
public:
  __declspec(property(get = __get_Button)) Button*                              TheButton;
  __declspec(property(get = __get_Interactable, put = __set_Interactable)) bool Interactable;

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "SemaphoreButtonListener");
    return class_helper;
  }

  Button* __get_Button()
  {
    static auto field = get_class_helper().GetField("_button").offset();
    return *(Button**)((char*)this + field);
  }

  bool __get_Interactable()
  {
    static auto field = get_class_helper().GetProperty("Interactable");
    if (field.isValidHelper()) {
      return *field.Get<bool>(this);
    }

    return false;
  }

  void __set_Interactable(bool v)
  {
    static auto field = get_class_helper().GetProperty("Interactable");
    if (field.isValidHelper()) {
      field.SetRaw<bool>(this, v);
    }
  }
};
