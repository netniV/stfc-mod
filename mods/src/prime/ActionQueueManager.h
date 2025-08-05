#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "FleetPlayerData.h"
#include "MonoSingleton.h"
#include "errormsg.h"

struct ActionQueueManager : MonoSingleton<ActionQueueManager> {
  friend struct MonoSingleton<ActionQueueManager>;

public:
  void ClearQueue(FleetPlayerData* playerData)
  {
    static auto ClearQueueMethod =
        get_class_helper().GetMethod<void(ActionQueueManager*, FleetPlayerData*)>("ClearQueue");
    static auto CLearQueueWarn = true;

    if (ClearQueueMethod) {
      ClearQueueMethod(this, playerData);
    } else if (CLearQueueWarn) {
      CLearQueueWarn = false;
      ErrorMsg::MissingMethod("ActionQueueManager", "ClearQueue");
    }
  }
  bool IsQueueFull(FleetPlayerData* playerData)
  {
    static auto IsQueueFullMethod =
        get_class_helper().GetMethod<bool(ActionQueueManager*, FleetPlayerData*)>("IsQueueFull");
    static auto IsQueueFullWarn = true;

    if (IsQueueFullMethod) {
      return IsQueueFullMethod(this, playerData);
    } else if (IsQueueFullWarn) {
      IsQueueFullWarn = false;
      ErrorMsg::MissingMethod("ActionQueueManager", "IsQueueFull");
    }

    return false;
  }

  bool IsFleetInQueue(FleetPlayerData* playerData)
  {
    static auto IsFleetInQueueMethod =
        get_class_helper().GetMethod<bool(ActionQueueManager*, FleetPlayerData*)>("IsFleetInQueue");
    static auto IsFleetInQueueWarn = true;

    if (IsFleetInQueueMethod) {
      return IsFleetInQueueMethod(this, playerData);
    } else if (IsFleetInQueueWarn) {
      IsFleetInQueueWarn = false;
      ErrorMsg::MissingMethod("ActionQueueManager", "IFleetInQueue");
    }

    return false;
  }

  bool IsQueueUnlocked()
  {
    static auto IsQueueUnlockedMethod = get_class_helper().GetMethod<bool(ActionQueueManager*)>("IsQueueUnlocked");
    static auto IsQueueUnlockedWarn   = true;
    if (IsQueueUnlockedMethod) {
      return IsQueueUnlockedMethod(this);
    } else if (IsQueueUnlockedWarn) {
      IsQueueUnlockedWarn = false;
      ErrorMsg::MissingMethod("ActionQueueManager", "IsQueueUnlocked");
    }

    return false;
  }

  bool CanAddToQueue(FleetPlayerData* playerData)
  {
    if (IsQueueUnlocked()) {
      if (!IsQueueFull(playerData)) {
        return true;
      }
    }

    return false;
  }

  void AddToQueue(long targetId)
  {
    static auto AddToQueueMethod = get_class_helper().GetMethod<void(ActionQueueManager*, long)>("AddActionToQueue");
    static auto AddToQueueWarn   = true;

    if (AddToQueueMethod != nullptr) {
      spdlog::warn("AddToQueue({})", targetId);
      AddToQueueMethod(this, targetId);
    } else if (AddToQueueWarn) {
      AddToQueueWarn = false;
      ErrorMsg::MissingMethod("ActionQueueManager", "AddActionToQueue");
    }
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
    static auto class_warn   = true;
    if (!class_helper.isValidHelper()) {
      if (class_warn) {
        class_warn = false;
        ErrorMsg::MissingHelper("ActionQueue", "ActionQueueManager");
      }
    }
    return class_helper;
  }
};
