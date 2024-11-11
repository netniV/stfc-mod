#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "FleetPlayerData.h"
#include "MonoSingleton.h"

struct ActionQueueManager : MonoSingleton<ActionQueueManager> {
  friend struct MonoSingleton<ActionQueueManager>;

public:
  bool IsQueueFull(FleetPlayerData* playerData)
  {
    static auto IsQueueFullMethod =
        get_class_helper().GetMethod<bool(ActionQueueManager*, FleetPlayerData*)>("IsQueueFull");
    return IsQueueFullMethod(this, playerData);
  }

  bool IsFleetInQueue(FleetPlayerData* playerData)
  {
    static auto IsFleetInQueueMethod =
        get_class_helper().GetMethod<bool(ActionQueueManager*, FleetPlayerData*)>("IsFleetInQueue");

    return IsFleetInQueueMethod(this, playerData);
  }

  bool IsQueueUnlocked()
  {
    static auto IsQueueUnlockedMethod = get_class_helper().GetMethod<bool(ActionQueueManager*)>("IsQueueUnlocked");
    return IsQueueUnlockedMethod(this);
  }

  bool CanAddToQueue(FleetPlayerData* playerData)
  {
    if (IsQueueUnlocked()) {
      if (!IsQueueFull(playerData)) {
        if (!IsFleetInQueue(playerData)) {
          return true;
        }
      }
    }

    return false;
  }

  void AddToQueue(long targetId)
  {
    static auto AddToQueueMethod = get_class_helper().GetMethod<void(ActionQueueManager*, long)>("AddActionToQueue");
    if (AddToQueueMethod != nullptr) {
      AddToQueueMethod(this, targetId);
    }
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
    return class_helper;
  }
};
