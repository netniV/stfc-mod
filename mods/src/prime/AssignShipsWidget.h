#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "InputFieldWidget.h"

struct AssignShipsWidget {
public:
  __declspec(property(get = __get__inputField)) InputFieldWidget* _inputField;
  __declspec(property(get = __get_isActiveAndEnabled)) bool        isActiveAndEnabled;

private:
  friend class ObjectFinder<AssignShipsWidget>;

public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Ships", "AssignShipsWidget");
    return class_helper;
  }

  InputFieldWidget* __get__inputField()
  {
    static auto field = get_class_helper().GetField("_inputField").offset();
    return *(InputFieldWidget**)((uintptr_t)this + field);
  }

  bool __get_isActiveAndEnabled()
  {
    static auto field = get_class_helper().GetProperty("isActiveAndEnabled");
    return field.Get<bool>(this);
  }
};
