#pragma once

#include "HullSpec.h"

#include <cstdint>
#include <il2cpp/il2cpp_helper.h>

enum class DeployedFleetType {
  Nonexistent,
  Player,
  Marauder,
  NpcInstantiated,
  Sentinel,
  Alliance,
  Challenge,
};

struct FleetDeployedData {
public:
  __declspec(property(get = __get_CurrentlyBattling)) bool      CurrentlyBattling;
  __declspec(property(get = __get_CurrentState)) int            CurrentState;
  __declspec(property(get = __get_ID)) std::int64_t             ID;
  __declspec(property(get = __get_IsDestroyed)) bool            IsDestroyed;
  __declspec(property(get = __get_Hull)) HullSpec* Hull;
  __declspec(property(get = __get_FleetType)) DeployedFleetType FleetType;
  __declspec(property(get = __get_PreviousState)) int           PreviousState;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetDeployedData");
    return class_helper;
  }

public:
  bool __get_CurrentlyBattling()
  {
    static auto prop  = get_class_helper().GetProperty("CurrentlyBattling");
    auto*       value = prop.Get<bool>(this);
    return value ? *value : false;
  }

  int __get_CurrentState()
  {
    static auto prop  = get_class_helper().GetProperty("CurrentState");
    auto*       value = prop.Get<int>(this);
    return value ? *value : -1;
  }

  std::int64_t __get_ID()
  {
    static auto prop  = get_class_helper().GetProperty("ID");
    auto*       value = prop.Get<std::int64_t>(this);
    return value ? *value : 0;
  }

  bool __get_IsDestroyed()
  {
    static auto prop  = get_class_helper().GetProperty("IsDestroyed");
    auto*       value = prop.Get<bool>(this);
    return value ? *value : false;
  }

  HullSpec* __get_Hull()
  {
    static auto field = get_class_helper().GetProperty("Hull");
    return field.GetRaw<HullSpec>(this);
  }

  DeployedFleetType __get_FleetType()
  {
    static auto prop  = get_class_helper().GetProperty("FleetType");
    auto*       value = prop.Get<DeployedFleetType>(this);
    return value ? *value : DeployedFleetType::Nonexistent;
  }

  int __get_PreviousState()
  {
    static auto prop  = get_class_helper().GetProperty("PreviousState");
    auto*       value = prop.Get<int>(this);
    return value ? *value : -1;
  }
};
