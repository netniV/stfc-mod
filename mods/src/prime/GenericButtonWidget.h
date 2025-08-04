#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "GenericButtonContext.h"
#include "SemaphoreButtonListener.h"
#include "Widget.h"

struct GenericButtonWidget : public Widget<GenericButtonContext, GenericButtonWidget> {
public:
  __declspec(property(get = __get_isActiveAndEnabled)) bool                          isActiveAndEnabled;
  __declspec(property(get = __get_SemaphoreButtonListener)) SemaphoreButtonListener* SemaphoreListener;

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "GenericButtonWidget");
    return class_helper;
  }

private:
  friend class ObjectFinder<GenericButtonWidget>;
  friend struct Widget<GenericButtonContext, GenericButtonWidget>;

public:
  SemaphoreButtonListener* __get_SemaphoreButtonListener()
  {
    static auto field = get_class_helper().GetField("_semaphoreButtonListener").offset();
    return *(SemaphoreButtonListener**)((char*)this + field);
  }

  bool __get_isActiveAndEnabled()
  {
    static auto field = get_class_helper().GetProperty("isActiveAndEnabled");
    return *field.Get<bool>(this);
  }
};
