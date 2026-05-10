#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "NavigationInteractionUIContext.h"
#include "ViewController.h"

struct NavigationInteractionUIViewController
    : public ViewController<NavigationInteractionUIContext, NavigationInteractionUIViewController> {
public:
  void OnSetCourseButtonClick()
  {
    static auto OnSetCourseButtonClick =
        get_class_helper().GetMethod<void(NavigationInteractionUIViewController*)>("OnSetCourseButtonClick");
    OnSetCourseButtonClick(this);
  }

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIViewController");
    return class_helper;
  }

private:
  friend struct ViewController<NavigationInteractionUIContext, NavigationInteractionUIViewController>;
  friend class ObjectFinder<NavigationInteractionUIViewController>;
};
