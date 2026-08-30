#pragma once

#include "MonoSingleton.h"
#include "ResourceSpec.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

struct SpecManager : MonoSingleton<SpecManager> {
  friend struct MonoSingleton<SpecManager>;

  ResourceSpec* GetResourceSpec(int64_t id)
  {
    static auto method = get_class_helper().GetMethod<ResourceSpec*(SpecManager*, int64_t)>("GetResourceSpec");
    static bool warn   = true;
    if (method) {
      return method(this, id);
    }
    if (warn) {
      warn = false;
      ErrorMsg::MissingMethod("SpecManager", "GetResourceSpec");
    }
    return nullptr;
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation.Managers", "SpecManager");
    return class_helper;
  }
};
