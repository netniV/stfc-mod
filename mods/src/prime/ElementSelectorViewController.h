#pragma once

#include "Button.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

struct ElementSelectorViewController {
public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.UI", "ElementSelectorViewController");
    return class_helper;
  }

  // Field offsets from dump.cs
  Button* _decrementButton() { return *reinterpret_cast<Button**>(reinterpret_cast<uint8_t*>(this) + 0x38); }
  Button* _incrementButton() { return *reinterpret_cast<Button**>(reinterpret_cast<uint8_t*>(this) + 0x40); }

  bool isActiveAndEnabled()
  {
    static auto get_isActiveAndEnabled =
        il2cpp_resolve_icall_typed<bool(ElementSelectorViewController*)>("UnityEngine.Behaviour::get_isActiveAndEnabled()");
    if (get_isActiveAndEnabled) {
      return get_isActiveAndEnabled(this);
    }
    return true;
  }

  void Decrement()
  {
    static auto DecrementMethod =
        get_class_helper().GetMethod<void(ElementSelectorViewController*)>("Decrement");
    static auto DecrementWarn = true;
    if (DecrementMethod) {
      DecrementMethod(this);
    } else if (DecrementWarn) {
      DecrementWarn = false;
      ErrorMsg::MissingMethod("ElementSelectorViewController", "Decrement");
    }
  }

  void Increment()
  {
    static auto IncrementMethod =
        get_class_helper().GetMethod<void(ElementSelectorViewController*)>("Increment");
    static auto IncrementWarn = true;
    if (IncrementMethod) {
      IncrementMethod(this);
    } else if (IncrementWarn) {
      IncrementWarn = false;
      ErrorMsg::MissingMethod("ElementSelectorViewController", "Increment");
    }
  }

  void PressDecrement()
  {
    if (auto btn = _decrementButton()) {
      btn->Press();
    }
  }

  void PressIncrement()
  {
    if (auto btn = _incrementButton()) {
      btn->Press();
    }
  }
};
