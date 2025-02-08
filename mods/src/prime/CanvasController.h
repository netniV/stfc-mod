#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "Canvas.h"
#include "Transform.h"

struct CanvasController {
public:
  __declspec(property(get = __get_Transform)) Transform* transform;
  __declspec(property(get = __get_Name)) Il2CppString* name;
  __declspec(property(get = __get_m_canvas)) Canvas* m_canvas;

  void ResetFocus()
  {
    static auto ResetFocusMethod = get_class_helper().GetMethod<void(CanvasController*)>("ResetFocus");
    static auto ResetFocusWarn   = true;

    if (ResetFocusMethod) {
      ResetFocusMethod(this);
    } else if (ResetFocusWarn) {
      ResetFocusWarn = false;
      ErrorMsg::MissingMethod("CanvasController", "ResetFocus");
    }
  }

  bool Visible()
  {
    static auto field = get_class_helper().GetProperty("Visible");
    return field.Get<bool>(this);
  }

  bool get_enabled()
  {
    static auto field = get_class_helper().GetProperty("enabled");
    return field.Get<bool>(this);
  }

  bool m_Visible()
  {
    static auto field = get_class_helper().GetField("m_visible").offset();
    return *(bool*)((ptrdiff_t)this + field);
  }

  Il2CppString* __get_Name()
  {
    static auto field = get_class_helper().GetProperty("name");
    return field.GetRaw<Il2CppString>(this);
  }

  Transform* __get_Transform()
  {
    static auto field = get_class_helper().GetProperty("transform");
    return field.GetRaw<Transform>(this);
  }

  Canvas* __get_m_canvas()
  {
    static auto field = get_class_helper().GetField("m_canvas").offset();
    return *(Canvas**)((ptrdiff_t)this + field);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "CanvasController");
    return class_helper;
  }
};
