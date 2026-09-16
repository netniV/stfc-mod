#pragma once

#include "GameObject.h"

class TMP_InputField
{
public:
  __declspec(property(get = __get_isFocused)) bool isFocused;

  void ActivateInputField()
  {
    static auto ActivateInputField = get_class_helper().GetMethod<void(TMP_InputField*)>("ActivateInputField");
    ActivateInputField(this);
  }

  void DeactivateInputField()
  {
    static auto DeactivateInputField = get_class_helper().GetMethod<void(TMP_InputField*)>("DeactivateInputField");
    DeactivateInputField(this);
  }

  void SendOnFocusLost()
  {
    static auto SendOnFocusLost = get_class_helper().GetMethod<void(TMP_InputField*)>("SendOnFocusLost");
    SendOnFocusLost(this);
  }

  void SendOnFocus()
  {
    static auto SendOnFocus = get_class_helper().GetMethod<void(TMP_InputField*)>("SendOnFocus");
    SendOnFocus(this);
  }

private:
  friend class ObjectFinder<TMP_InputField>;
  friend struct GameObject;
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TMP_InputField");
    return class_helper;
  }

public:
  bool __get_isFocused()
  {
    static auto field = get_class_helper().GetProperty("isFocused");
    const auto* focused = field.Get<bool>(this);
    return focused && *focused;
  }
};
