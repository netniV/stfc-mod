#pragma once

#include "ProgressData.h"

#include <il2cpp/il2cpp_helper.h>

struct CargoHoldData {
public:
  __declspec(property(get = __get_UnprotectedCargoProgress)) ProgressData* UnprotectedCargoProgress;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "CargoHoldData");
    return class_helper;
  }

public:
  ProgressData* __get_UnprotectedCargoProgress()
  {
    static auto property = get_class_helper().GetProperty("UnprotectedCargoProgress");
    return property.GetRaw<ProgressData>(this);
  }
};
