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

struct Cargo {
public:
  __declspec(property(get = __get_Count)) uint64_t Count;
  __declspec(property(get = __get_IsEmpty)) bool IsEmpty;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Cargo");
    return class_helper;
  }

public:
  uint64_t __get_Count()
  {
    static auto field = get_class_helper().GetProperty("Count");
    return *field.Get<uint64_t>(this);
  }
  bool __get_IsEmpty()
  {
    static auto field = get_class_helper().GetProperty("IsEmpty");
    return *field.Get<bool>(this);
  }
};

struct ProgressData {
public:
  __declspec(property(get = __get_CurrentValue)) double CurrentValue;
  __declspec(property(get = __get_MinValue)) double MinValue;
  __declspec(property(get = __get_MaxValue)) double MaxValue;
  __declspec(property(get = __get_NormalizedValue)) double NormalizedValue;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ProgressData");
    return class_helper;
  }

public:
  double __get_CurrentValue()
  {
    static auto field = get_class_helper().GetProperty("CurrentValue");
    return *field.Get<double>(this);
  }
  double __get_MinValue()
  {
    static auto field = get_class_helper().GetProperty("MinValue");
    return *field.Get<double>(this);
  }
  double __get_MaxValue()
  {
    static auto field = get_class_helper().GetProperty("MaxValue");
    return *field.Get<double>(this);
  }
  double __get_NormalizedValue()
  {
    static auto field = get_class_helper().GetProperty("NormalizedValue");
    return *field.Get<double>(this);
  }
};

struct CargoHoldData {
public:
  __declspec(property(get = __get_ProtectedCargoProgress)) ProgressData* ProtectedCargoProgress;
  __declspec(property(get = __get_UnprotectedCargoProgress)) ProgressData* UnprotectedCargoProgress;
  __declspec(property(get = __get_ProtectedCargoPercentage)) float ProtectedCargoPercentage;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "CargoHoldData");
    return class_helper;
  }

public:
  ProgressData* __get_ProtectedCargoProgress()
  {
    static auto field = get_class_helper().GetProperty("ProtectedCargoProgress");
    return field.GetRaw<ProgressData>(this);
  }
  ProgressData* __get_UnprotectedCargoProgress()
  {
    static auto field = get_class_helper().GetProperty("UnprotectedCargoProgress");
    return field.GetRaw<ProgressData>(this);
  }
  float __get_ProtectedCargoPercentage()
  {
    static auto field = get_class_helper().GetProperty("ProtectedCargoPercentage");
    return *field.Get<float>(this);
  }
};

struct Ship {
public:
  __declspec(property(get = __get_HullId)) long HullId;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "Ship");
    return class_helper;
  }

public:
  long __get_HullId()
  {
    static auto field = get_class_helper().GetProperty("HullId");
    return *field.Get<long>(this);
  }
};
    
struct FleetPlayerData {
public:
  __declspec(property(get = __get_CurrentState)) FleetState CurrentState;
  __declspec(property(get = __get_PreviousState)) FleetState PreviousState;
  __declspec(property(get = __get_Id)) uint64_t Id;
  __declspec(property(get = __get_IsHome)) bool IsHome;
  __declspec(property(get = __get_Cargo)) Cargo* CCargo;
  __declspec(property(get = __get_CargoHoldData)) CargoHoldData* CCargoHoldData;
  __declspec(property(get = __get_Hull)) HullSpec* Hull;
  __declspec(property(get = __get_HullId)) long HullId;
  __declspec(property(get = __get_Address)) void* Address;
  __declspec(property(get = __get_RecallRequirements)) RecallRequirement* RecallRequirements;
  __declspec(property(get = __get_CanRepairRequirement)) CanRepairRequirement* CanRepairRequirements;
  __declspec(property(get = __get_Ship)) Ship* SShip;

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
  long __get_HullId()
  {
    static auto field = get_class_helper().GetProperty("HullId");
    return *field.Get<long>(this);
  }
  Cargo* __get_Cargo()
  {
    static auto field = get_class_helper().GetProperty("Cargo");
    return field.GetRaw<Cargo>(this);
  }
  CargoHoldData* __get_CargoHoldData()
  {
    static auto field = get_class_helper().GetField("_cargoHoldData").offset();
    return *(CargoHoldData**)((uintptr_t)this + field);
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

  bool __get_IsHome()
  {
    static auto field = get_class_helper().GetProperty("IsHome");
    return *field.Get<bool>(this);
  }

  Ship* __get_Ship()
  {
    static auto field = get_class_helper().GetProperty("Ship");
    return field.GetRaw<Ship>(this);
  }
};
