#pragma once

#include <il2cpp/il2cpp_helper.h>

#include <cstring>

#include "FleetDeployedData.h"
#include "NavigationLOD.h"

struct NavigationFleetWidget {
  __declspec(property(get = __get__lod)) NavigationLOD* _lod;
  __declspec(property(get = __get_Context)) FleetDeployedData* Context;

  static FieldInfo* ContextField()
  {
    static auto* field = [] {
      for (auto* cls = get_class_helper().get_cls(); cls != nullptr; cls = il2cpp_class_get_parent(cls)) {
        void* iterator = nullptr;
        while (auto* candidate = il2cpp_class_get_fields(cls, &iterator)) {
          const auto* name = il2cpp_field_get_name(candidate);
          if (name != nullptr && std::strcmp(name, "m_context") == 0) {
            return candidate;
          }
        }
      }
      return static_cast<FieldInfo*>(nullptr);
    }();
    return field;
  }

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

  FleetDeployedData* __get_Context()
  {
    auto* field = ContextField();
    return field != nullptr ? *(FleetDeployedData**)((ptrdiff_t)this + field->offset) : nullptr;
  }
};
