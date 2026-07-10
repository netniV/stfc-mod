#pragma once

#if _WIN32

#include <il2cpp/il2cpp_helper.h>

struct AspectRatioConstraintHandler {
public:
  static WNDPROC _unityWndProc()
  {
    return get_class_helper().GetStaticField("_unityWndProc").Get<WNDPROC>();
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Utils", "AspectRatioConstraintHandler");
    return class_helper;
  }
};

#endif
