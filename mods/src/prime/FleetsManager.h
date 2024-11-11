#pragma once

#include "CallbackContainer.h"
#include "FleetDeployedData.h"
#include "FleetPlayerData.h"
#include "HullSpec.h"
#include "IEnumerator.h"
#include "MonoSingleton.h"
#include "Vector3.h"

#include <il2cpp/il2cpp_helper.h>

struct FleetsManager : MonoSingleton<FleetsManager> {
  friend struct MonoSingleton<FleetsManager>;

public:
  __declspec(property(get = __get_TargetFleetData)) FleetDeployedData* targetFleetData;

public:
  class IEnumerator_Tow
  {
  public:
    bool MoveNext()
    {
      static auto MoveNext = get_class_helper().GetMethodSpecial<bool(IEnumerator_Tow*)>("MoveNext");
      return MoveNext(this);
    }

  private:
    static IL2CppClassHelper& get_class_helper()
    {
      static auto class_helper = FleetsManager::get_class_helper().GetNestedType("<Tow>d__170");
      return class_helper;
    }
  };

  void RequestViewFleet(FleetPlayerData* fleetData, bool showSystemInfo = false)
  {
    static auto RequestViewFleet =
        get_class_helper().GetMethod<void(FleetsManager*, FleetPlayerData*, bool)>("RequestViewFleet");
    RequestViewFleet(this, fleetData, showSystemInfo);
  }
  void RecallFleet(long fleetId)
  {
    static auto RecallFleet = get_class_helper().GetMethod<void(FleetsManager*, long, void*)>("RecallFleet");
    auto        ptr         = CallbackContainer::Create();
    RecallFleet(this, fleetId, ptr);
  }

  IEnumerator_Tow* Tow(long towedFleetId, long towingFleetId, Vector3* targetPosition)
  {
    static auto Tow =
        get_class_helper().GetMethod<IEnumerator_Tow*(FleetsManager*, long, long, void*, Vector3*, void*)>("Tow");
    auto ptr = CallbackContainer::Create();
    return Tow(this, towedFleetId, towingFleetId, nullptr, targetPosition, ptr);
  }

  FleetPlayerData* GetFleetPlayerData(int idx)
  {
    static auto GetFleetPlayerData =
        get_class_helper().GetMethod<FleetPlayerData*(FleetsManager*, int)>("GetFleetPlayerData");
    return GetFleetPlayerData(this, idx);
  }

  FleetDeployedData* __get_TargetFleetData()
  {
    static auto field = get_class_helper().GetField("_targetFleetData").offset();
    return *(FleetDeployedData**)((char*)this + field);
  }

private:

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.FleetManagement", "FleetsManager");
    return class_helper;
  }
};
