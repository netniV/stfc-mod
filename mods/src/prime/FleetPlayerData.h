#pragma once

#include "BattleTargetData.h"
#include "CargoHoldData.h"
#include "HullSpec.h"
#include "MiningSlot.h"
#include "RecallRequirement.h"
#include "CanRepairRequirement.h"

#include <cstdint>

enum class FleetState {
  Unknown      = 0,
  IdleInSpace  = 1,
  Docked       = 2,
  Mining       = 4,
  Destroyed    = 8,
  TieringUp    = 16,
  Repairing    = 32,
  CanReplaceOfficers = 18,
  Battling     = 64,
  WarpCharging = 128,
  Warping      = 256,
  Impulsing    = 512,
  Capturing    = 1024,
  AutoHunting  = 2048,
  Outposting   = 4096,
  CannotLaunch = 56,
  CannotMove   = 2552,
  CanRecall    = 5637,
  CanRemove    = 384,
  CanManage    = 2947,
  CanLocate    = 8135,
  Deployed     = 8133,
  CanEngage    = 3591,
  CanActivateAbility     = 513,
  CanBeTargetedByAbility = 3589,
  CanDisco               = 515
};
    
struct FleetPlayerData {
public:
  __declspec(property(get = __get_CurrentState)) FleetState CurrentState;
  __declspec(property(get = __get_PreviousState)) FleetState PreviousState;
  __declspec(property(get = __get_Id)) uint64_t Id;
  __declspec(property(get = __get_Hull)) HullSpec* Hull;
  __declspec(property(get = __get_Index)) int Index;
  __declspec(property(get = __get_MiningData)) MiningSlot* MiningData;
  __declspec(property(get = __get_CargoHoldData)) ::CargoHoldData* CargoHoldData;
  __declspec(property(get = __get_CargoResourceFillLevel)) float CargoResourceFillLevel;
  __declspec(property(get = __get_Address)) void* Address;
  __declspec(property(get = __get_Level)) int64_t Level;
  __declspec(property(get = __get_HasShip)) bool HasShip;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetPlayerData");
    return class_helper;
  }

public:
  HullSpec* __get_Hull()
  {
    static auto field = get_class_helper().GetProperty("Hull");
    return field.GetRaw<HullSpec>(this);
  }
  int __get_Index()
  {
    static auto property = get_class_helper().GetProperty("Index");
    auto*       value    = property.Get<int>(this);
    return value ? *value : -1;
  }
  MiningSlot* __get_MiningData()
  {
    static auto property = get_class_helper().GetProperty("MiningData");
    return property.GetRaw<MiningSlot>(this);
  }
  ::CargoHoldData* __get_CargoHoldData()
  {
    static auto property = get_class_helper().GetProperty("CargoHoldData");
    return property.GetRaw<struct CargoHoldData>(this);
  }
  float __get_CargoResourceFillLevel()
  {
    static auto property = get_class_helper().GetProperty("CargoResourceFillLevel");
    auto*       value    = property.Get<float>(this);
    return value ? *value : -1.0f;
  }
  void* __get_Address()
  {
    static auto field = get_class_helper().GetProperty("Address");
    return field.GetRaw<void>(this);
  }
  FleetState __get_CurrentState()
  {
    static auto field = get_class_helper().GetProperty("CurrentState");
    auto*      value  = field.Get<FleetState>(this);
    return value ? *value : FleetState::Unknown;
  }
  FleetState __get_PreviousState()
  {
    static auto field = get_class_helper().GetProperty("PreviousState");
    auto*      value  = field.Get<FleetState>(this);
    return value ? *value : FleetState::Unknown;
  }
  
  uint64_t __get_Id()
  {
    static auto field = get_class_helper().GetProperty("Id");
    auto*      value  = field.Get<uint64_t>(this);
    return value ? *value : 0;
  }
  int64_t __get_Level()
  {
    static auto field = get_class_helper().GetProperty("Level");
    auto*      value  = field.Get<int64_t>(this);
    return value ? *value : 0;
  }

  bool __get_HasShip()
  {
    static auto field = get_class_helper().GetProperty("HasShip");
    auto*      value  = field.Get<bool>(this);
    return value ? *value : false;
  }
};
