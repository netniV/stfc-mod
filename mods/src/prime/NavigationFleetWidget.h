#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "NavigationLOD.h"

struct NavigationFleetWidget {
  __declspec(property(get = __get__lod)) NavigationLOD* _lod;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationFleetWidget");
    return class_helper;
  }

public:
  NavigationLOD* __get__lod()
  {
    static auto* field = [] {
      auto* cls = get_class_helper().get_cls();
      return cls != nullptr ? il2cpp_class_get_field_from_name(cls, "_lod") : nullptr;
    }();
    return field != nullptr ? *(NavigationLOD**)((ptrdiff_t)this + field->offset) : nullptr;
  }
};
