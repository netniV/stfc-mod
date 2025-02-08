#pragma once

#include <il2cpp/il2cpp_helper.h>
#include "HullSpec.h"

enum class DeployedFleetType {
  Nonexistent,
  Player,
  Marauder,
  NpcInstantiated,
  Sentinel,
  Alliance,
};

struct FleetDeployedData {
public:
  __declspec(property(get = __get_ID)) long ID;
  __declspec(property(get = __get_Hull)) HullSpec* Hull;
  __declspec(property(get = __get_FleetType)) DeployedFleetType FleetType;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetDeployedData");
    return class_helper;
  }

public:
  long __get_ID()
  {
    static auto field = get_class_helper().GetProperty("ID");
    return *field.Get<long>(this);
  }

  HullSpec* __get_Hull()
  {
    static auto field = get_class_helper().GetProperty("Hull");
    return field.GetRaw<HullSpec>(this);
  }

  DeployedFleetType __get_FleetType()
  {
    static auto field = get_class_helper().GetProperty("FleetType");
    return *field.Get<DeployedFleetType>(this);
  }
};
