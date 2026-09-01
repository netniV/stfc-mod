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
      static auto MoveWarn = true;

      if (MoveNext) {
        return MoveNext(this);
      } else if (MoveWarn) {
        MoveWarn = false;
        ErrorMsg::MissingMethod("IEnumerator_Tow", "MoveNext");
      }

      return false;
    }

  private:
    static IL2CppClassHelper& get_class_helper()
    {
      static auto class_helper =
          il2cpp_get_class_helper("Assembly-CSharp", "", "FleetsManager.<Tow>d__192");
      return class_helper;
    }
  };

  void RequestViewFleet(FleetPlayerData* fleetData, bool showSystemInfo = false)
  {
    static auto RequestViewFleet =
        get_class_helper().GetMethod<void(FleetsManager*, FleetPlayerData*, bool)>("RequestViewFleet");
    static auto RequestViewWarn = true;
    if (RequestViewFleet) {
       RequestViewFleet(this, fleetData, showSystemInfo);
    } else if (RequestViewWarn) {
      RequestViewWarn = false;
      ErrorMsg::MissingMethod("FleetsManager", "RequestViewFleet");
    }
  }

  void RecallFleet(long fleetId)
  {
    static auto RecallFleet = get_class_helper().GetMethod<void(FleetsManager*, long, void*)>("RecallFleet");
    static auto RecallWarn  = true;

    if (RecallFleet) {
      auto ptr = CallbackContainer::Create();
      RecallFleet(this, fleetId, ptr);
    } else if (RecallWarn) {
      RecallWarn = true;
      ErrorMsg::MissingMethod("FleetsManager", "RecallFleet");
    }
  }

  IEnumerator_Tow* Tow(long towedFleetId, long towingFleetId, Vector3* targetPosition)
  {
    static auto TowMethod =
        get_class_helper().GetMethod<IEnumerator_Tow*(FleetsManager*, long, long, void*, Vector3*, void*)>("Tow");
    static auto TowWarn = true;

    if (TowMethod) {
      auto ptr = CallbackContainer::Create();
      return TowMethod(this, towedFleetId, towingFleetId, nullptr, targetPosition, ptr);
    } else if (TowWarn) {
      TowWarn = false;
      ErrorMsg::MissingMethod("FleetsManager", "Tow");
    }

    return nullptr;
  }

  FleetPlayerData* GetFleetPlayerData(int idx)
  {
    static auto GetFleetPlayerDataMethod =
        get_class_helper().GetMethod<FleetPlayerData*(FleetsManager*, int)>("GetFleetPlayerData");
    static auto GetFleetPlayerDataWarn = true;

    if (GetFleetPlayerDataMethod) {
      return GetFleetPlayerDataMethod(this, idx);
    } else if (GetFleetPlayerDataWarn) {
      GetFleetPlayerDataWarn = false;
      ErrorMsg::MissingMethod("FleetPlayerData", "GetFleetPlayerData");
    }

    return nullptr;
  }

  bool HasFleetService()
  {
    static auto* fleet_service_cache =
        il2cpp_class_get_field_from_name(get_class_helper().get_cls(), "_fleetServiceCache");
    static auto FleetServiceCacheWarn = true;
    if (!fleet_service_cache) {
      if (FleetServiceCacheWarn) {
        FleetServiceCacheWarn = false;
        spdlog::error("Unable to find field 'FleetsManager->_fleetServiceCache'");
      }
      return false;
    }

    auto* cache = reinterpret_cast<void**>(reinterpret_cast<char*>(this) + fleet_service_cache->offset);
    if (*cache) {
      return true;
    }

    static auto* HasServiceMethod = [] {
      auto* cache_class = il2cpp_class_from_type(il2cpp_field_get_type(fleet_service_cache));
      return cache_class ? il2cpp_class_get_method_from_name(cache_class, "get_HasService", 0) : nullptr;
    }();
    static auto HasServiceWarn = true;
    if (!HasServiceMethod) {
      if (HasServiceWarn) {
        HasServiceWarn = false;
        ErrorMsg::MissingMethod("CachedService<FleetService>", "get_HasService");
      }
      return false;
    }

    Il2CppException* exception = nullptr;
    auto* result = il2cpp_runtime_invoke(HasServiceMethod, cache, nullptr, &exception);
    return !exception && result && *static_cast<bool*>(il2cpp_object_unbox(result));
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
