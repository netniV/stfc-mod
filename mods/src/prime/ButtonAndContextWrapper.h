#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "GenericButtonWidget.h"

struct ButtonAndContextWrapper {
public:
  __declspec(property(get = __get_Widget)) GenericButtonWidget* Widget;

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ButtonAndContextWrapper");
    return class_helper;
  }

  GenericButtonWidget* __get_Widget()
  {
    static auto field = get_class_helper().GetField("Widget").offset();
    return *(GenericButtonWidget**)((char*)this + field);
  }
};
