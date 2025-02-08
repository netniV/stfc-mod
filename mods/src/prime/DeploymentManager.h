#pragma once

#include "CallbackContainer.h"
#include "MonoSingleton.h"
#include "Vector3.h"
#include "FleetPlayerData.h"
#include "FleetDeployedData.h"

#include <il2cpp/il2cpp_helper.h>

class DeploymentService
{
public:
  class IEnumerator_PlanCourse
  {
  public:
    bool MoveNext()
    {
      static auto MoveNext = get_class_helper().GetMethodSpecial<bool(IEnumerator_PlanCourse*)>("MoveNext");
      static auto MoveWarn = true;

      if (MoveNext) {
        return MoveNext(this);
      } else if (MoveWarn) {
        MoveWarn = false;
        ErrorMsg::MissingMethod("IEnumerator_PlanCourse", "MoveNext");
      }

      return false;
    }

  private:
    static IL2CppClassHelper& get_class_helper()
    {
      static auto class_helper = DeploymentService::get_class_helper().GetNestedType("<PlanCourse>d__134");
      return class_helper;
    }
  };

private:
  friend class IEnumerator_PlanCourse;

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "DeploymentService");
    return class_helper;
  }
};

struct DeploymentManger : MonoSingleton<DeploymentManger> {
  friend struct MonoSingleton<DeploymentManger>;

public:
  void SetTowRequest(uint64_t towedFleetId, uint64_t towingFleetId)
  {
    static auto SetTowRequestMethod =
        get_class_helper().GetMethod<void(DeploymentManger*, uint64_t, uint64_t, void*)>("SetTowRequest");
    static auto SetTowRequestWarn = true;

    if (SetTowRequestMethod) {
      auto ptr = CallbackContainer::Create();
      SetTowRequestMethod(this, towedFleetId, towingFleetId, ptr);
    } else if (SetTowRequestWarn) {
      SetTowRequestWarn = false;
      ErrorMsg::MissingMethod("DeploymentManager", "SetTowRequest");
    }
  }

  DeploymentService::IEnumerator_PlanCourse* PlanCourse(FleetPlayerData* selectedFleet, void* targetAddress,
                                                        Vector3 targetPosition, FleetDeployedData* targetDeployedFleet,
                                                        void* starbaseData, void* allianceStarbaseData)
  {
    static auto PlanCourseMethod =
        get_class_helper()
            .GetMethod<DeploymentService::IEnumerator_PlanCourse*(
                DeploymentManger*, FleetPlayerData * selectedFleet, void* targetAddress, Vector3* targetPosition,
                FleetDeployedData* targetDeployedFleet, void* starbaseData, void* allianceStarbaseData)>("PlanCourse");
    static auto PlanCourseWarn = true;

    if (PlanCourseMethod) {
      return PlanCourseMethod(this, selectedFleet, targetAddress, &targetPosition, targetDeployedFleet, starbaseData,
                              allianceStarbaseData);
    } else if (PlanCourseWarn) {
      PlanCourseWarn = false;
      ErrorMsg::MissingMethod("DeploymentService", "PlanCourse");
    }

    return nullptr;
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "", "DeploymentManager");
    return class_helper;
  }
};
