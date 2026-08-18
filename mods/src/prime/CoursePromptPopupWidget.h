#pragma once

#include "GenericButtonWidget.h"
#include "Widget.h"

#include <il2cpp/il2cpp_helper.h>

class CoursePromptPopupDataContext
{
public:
  __declspec(property(get = __get_HasInstantWarp)) bool HasInstantWarp;

  bool __get_HasInstantWarp()
  {
    static auto field = get_class_helper().GetField("HasInstantWarp");
    return *(bool*)((char*)this + field.offset());
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "CoursePromptPopupDataContext");
    return class_helper;
  }
};

struct CoursePromptPopupWidget : public Widget<CoursePromptPopupDataContext, CoursePromptPopupWidget> {
public:
  __declspec(property(get = __get_LeftButton)) GenericButtonWidget* LeftButton;
  __declspec(property(get = __get_RightButton)) GenericButtonWidget* RightButton;

  GenericButtonWidget* __get_LeftButton()
  {
    static auto field = get_class_helper().GetField("_leftButton");
    return *(GenericButtonWidget**)((char*)this + field.offset());
  }

  GenericButtonWidget* __get_RightButton()
  {
    static auto field = get_class_helper().GetField("_rightButton");
    return *(GenericButtonWidget**)((char*)this + field.offset());
  }

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "CoursePromptPopupWidget");
    return class_helper;
  }

private:
  friend struct Widget<CoursePromptPopupDataContext, CoursePromptPopupWidget>;
};

struct CoursePromptPopupViewController {
public:
  __declspec(property(get = __get_PopupWidget)) CoursePromptPopupWidget* PopupWidget;

  CoursePromptPopupWidget* __get_PopupWidget()
  {
    static auto field = get_class_helper().GetField("_coursePromptPopupWidget");
    return *(CoursePromptPopupWidget**)((char*)this + field.offset());
  }

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.ObjectViewer", "CoursePromptPopupViewController");
    return class_helper;
  }
};
