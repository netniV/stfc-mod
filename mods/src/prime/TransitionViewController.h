#pragma once

#include <il2cpp/il2cpp_helper.h>

struct TransitionViewController {
public:
  __declspec(property(get = __get__staticOverride)) void* _staticOverride;
  __declspec(property(get = __get__animator)) void* _animator;
  __declspec(property(get = __get__scroller)) void* _scroller;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.LoadingScreen", "TransitionViewController");
    return class_helper;
  }

public:
  void* __get__staticOverride()
  {
    if (!this) return nullptr;
    static auto field = get_class_helper().GetField("_staticOverride");
    if (!field.isValidHelper()) return nullptr;
    return *(void**)((ptrdiff_t)this + field.offset());
  }

  void* __get__animator()
  {
    static auto field = get_class_helper().GetField("_animator");
    return *(void**)((ptrdiff_t)this + field.offset());
  }

  void* __get__scroller()
  {
    static auto field = get_class_helper().GetField("_scroller");
    return *(void**)((ptrdiff_t)this + field.offset());
  }
};
