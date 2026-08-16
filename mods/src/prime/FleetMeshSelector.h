#pragma once

#include <il2cpp/il2cpp_helper.h>

struct FleetMeshSelector {
public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "FleetMeshSelector");
    return class_helper;
  }

  Il2CppObject* LoadedObject()
  {
    static auto loaded_object = get_class_helper().GetProperty("LoadedObject");
    if (!loaded_object.isValidHelper()) {
      return nullptr;
    }
    return loaded_object.GetRaw<Il2CppObject>(this);
  }
};
