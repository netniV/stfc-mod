#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "Camera.h"

struct OrbitFrameProvider {
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "OrbitFrameProvider");
    return class_helper;
  }

  float& RotationAngleDelta()
  {
    static auto field = get_class_helper().GetField("_rotationAngleDelta");
    return *(float*)((ptrdiff_t)this + field.offset());
  }
};
