#pragma once

#include "BattleTargetData.h"
#include "HullSpec.h"
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
  CannotLaunch = 56,
  Battling     = 64,
  WarpCharging = 128,
  Warping      = 256,
  CanRemove    = 384,
  CannotMove   = 504,
  Impulsing    = 512,
  CanManage    = 899,
  Capturing    = 1024,
  CanRecall    = 1541,
  CanEngage    = 1543,
  Deployed     = 1989,
  CanLocate    = 1991
};
    
struct FleetPlayerData {
public:
  __declspec(property(get = __get_CurrentState)) FleetState CurrentState;
  __declspec(property(get = __get_PreviousState)) FleetState PreviousState;
  __declspec(property(get = __get_Id)) uint64_t Id;
  __declspec(property(get = __get_Hull)) HullSpec* Hull;
  __declspec(property(get = __get_Address)) void* Address;
  __declspec(property(get = __get_RecallRequirements)) RecallRequirement* RecallRequirements;
  __declspec(property(get = __get_CanRepairRequirement)) CanRepairRequirement* CanRepairRequirements;

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
  void* __get_Address()
  {
    static auto field = get_class_helper().GetProperty("Address");
    return field.GetRaw<void>(this);
  }
  FleetState __get_CurrentState()
  {
    static auto field = get_class_helper().GetProperty("CurrentState");
    return *field.Get<FleetState>(this);
  }
  FleetState __get_PreviousState()
  {
    static auto field = get_class_helper().GetProperty("PreviousState");
    return *field.Get<FleetState>(this);
  }
  
  CanRepairRequirement* __get_CanRepairRequirement()
  {
    static auto field = get_class_helper().GetProperty("CanRepairRequirement");
    return field.GetRaw<CanRepairRequirement>(this);
  }


  RecallRequirement* __get_RecallRequirements()
  {
    static auto field = get_class_helper().GetProperty("RecallRequirement");
    return field.GetRaw<RecallRequirement>(this);
  }

  uint64_t __get_Id()
  {
    static auto field = get_class_helper().GetProperty("Id");
    return *field.Get<uint64_t>(this);
  }
};
