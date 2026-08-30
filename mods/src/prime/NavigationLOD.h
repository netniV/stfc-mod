#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "ZoomLevels.h"

struct NavigationLOD {
  void OnZoomChanged(ZoomLevels level)
  {
    static auto on_zoom_changed = get_class_helper().GetMethod<void(NavigationLOD*, ZoomLevels)>("OnZoomChanged");
    if (on_zoom_changed != nullptr) {
      on_zoom_changed(this, level);
    }
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationLOD");
    return class_helper;
  }
};
