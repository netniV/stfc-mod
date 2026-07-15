#pragma once

#include <il2cpp/il2cpp_helper.h>

enum class VisibilityState // TypeDefIndex: 13743
{
  Unknown = 0,
  Show    = 1,
  Hide    = 2,
  Hidden  = 3,
  Visible = 4
};

struct VisibilityController {
public:
  __declspec(property(get = __get__state)) VisibilityState _state;
  __declspec(property(get = __get_State)) VisibilityState State;
  
  void Show(bool instant = false)
  {
    auto Show = get_class_helper().GetMethodSpecial<void(VisibilityController*, bool)>(
        "Show", [](int param_count, const Il2CppType** param) -> bool {
          if (param_count != 1) {
            return false;
          }
          return param[0]->type == IL2CPP_TYPE_BOOLEAN;
        });
    return Show(this, instant);
  }

  void Hide(bool instant = false)
  {
    auto Hide = get_class_helper().GetMethodSpecial<void(VisibilityController*, bool)>(
        "Hide", [](int param_count, const Il2CppType** param) -> bool {
          if (param_count != 1) {
            return false;
          }
          return param[0]->type == IL2CPP_TYPE_BOOLEAN;
        });
    return Hide(this, instant);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "VisibilityController");
    return class_helper;
  }

public:
  VisibilityState __get__state()
  {
    static auto field = get_class_helper().GetField("_state");
    if (field.isValidHelper()) {
      return *(VisibilityState*)((ptrdiff_t)this + field.offset());
    }

    return VisibilityState::Unknown;
  }

  VisibilityState __get_State()
  {
    static auto prop = get_class_helper().GetProperty("VisibilityState");
    if (prop.isValidHelper()) {
      return *prop.Get<VisibilityState>(this);
    }

    return VisibilityState::Unknown;
  }
};
