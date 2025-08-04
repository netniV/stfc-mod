#pragma once

#include <il2cpp/il2cpp_helper.h>

class Button
{
public:
  void Press()
  {
    auto PressMethod = get_class_helper().GetMethod<void(Button*)>("Press");
    PressMethod(this);
  };

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Button");
    return class_helper;
  };
};
