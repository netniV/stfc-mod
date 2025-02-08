#pragma once

#include <il2cpp/il2cpp_helper.h>
#include "FleetDeployedData.h"

struct BattleTargetData {
public:
  __declspec(property(get = __get_TargetFleetDeployedData)) FleetDeployedData* TargetFleetDeployedData;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BattleTargetData");
    return class_helper;
  }

public:
  FleetDeployedData* __get_TargetFleetDeployedData()
  {
    static auto field = get_class_helper().GetField("TargetFleetDeployedData").offset();
    return *(FleetDeployedData**)((char*)this + field);
  }
};
