#pragma once

#include "FleetPlayerData.h"

#include <il2cpp/il2cpp_helper.h>

struct CourseData {
public:
  __declspec(property(get = __get_PlayerFleet)) FleetPlayerData* PlayerFleet;

  FleetPlayerData* __get_PlayerFleet()
  {
    static auto field = get_class_helper().GetProperty("PlayerFleet");
    return field.GetRaw<FleetPlayerData>(this);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "CourseData");
    return class_helper;
  }
};
