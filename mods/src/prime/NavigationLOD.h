#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "ZoomLevels.h"

struct NavigationLOD {
  void UpdateLOD(ZoomLevels level)
  {
    static auto update_lod = get_class_helper().GetMethod<void(NavigationLOD*, ZoomLevels)>("UpdateLOD");
    if (update_lod != nullptr) {
      update_lod(this, level);
    }
  }

  void SetTargetLevel(ZoomLevels level)
  {
    static auto field                                = get_class_helper().GetField("<TargetLevel>k__BackingField");
    *(ZoomLevels*)((ptrdiff_t)this + field.offset()) = level;
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationLOD");
    return class_helper;
  }
};
