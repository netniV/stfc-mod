#pragma once

#include <il2cpp/il2cpp_helper.h>
#include "HullSpec.h"

struct SpecService {
public:
  HullSpec* GetHull(long hullId)
  {
    static auto method =
        get_class_helper().GetMethod<HullSpec*(SpecService*, long)>("GetHull");
    return method(this, hullId);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "SpecService");
    return class_helper;
  }
};
