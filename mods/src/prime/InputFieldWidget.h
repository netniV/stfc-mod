#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "TMP_InputField.h"

struct InputFieldWidget {
public:
  __declspec(property(get = __get__input)) TMP_InputField* _input;
  __declspec(property(get = __get_isActiveAndEnabled)) bool isActiveAndEnabled;

  void Focus()
  {
    static auto focusMethod = get_class_helper().GetMethod<void(InputFieldWidget*)>("Focus");
    if (focusMethod) {
      focusMethod(this);
    }

    // Fallback in case Focus method isn't present or doesn't activate properly
    if (auto input = this->_input; input) {
      input->ActivateInputField();
    }
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "InputFieldWidget");
    return class_helper;
  }

public:
  TMP_InputField* __get__input()
  {
    static auto field = get_class_helper().GetField("_input").offset();
    return *(TMP_InputField**)((uintptr_t)this + field);
  }

  bool __get_isActiveAndEnabled()
  {
    static auto field = get_class_helper().GetProperty("isActiveAndEnabled");
    return field.Get<bool>(this);
  }
};
