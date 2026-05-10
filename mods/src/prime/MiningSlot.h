#pragma once

#include "TimeSpan.h"

#include <il2cpp/il2cpp_helper.h>

struct MiningNodeParams {
public:
  __declspec(property(get = __get_Amount)) int64_t Amount;
  __declspec(property(get = __get_ResourceID)) int64_t ResourceID;
  __declspec(property(get = __get_Rate)) int64_t Rate;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "MiningNodeParams");
    return class_helper;
  }

public:
  int64_t __get_Amount()
  {
    static auto field = get_class_helper().GetProperty("Amount");
    auto* value = field.Get<int64_t>(this);
    return value ? *value : 0;
  }

  int64_t __get_ResourceID()
  {
    static auto field = get_class_helper().GetProperty("ResourceID");
    auto* value = field.Get<int64_t>(this);
    return value ? *value : 0;
  }

  int64_t __get_Rate()
  {
    static auto field = get_class_helper().GetProperty("Rate");
    auto* value = field.Get<int64_t>(this);
    return value ? *value : 0;
  }
};

struct MiningSlot {
public:
  __declspec(property(get = __get_PointData)) MiningNodeParams* PointData;
  __declspec(property(get = __get_ResourceId)) int64_t ResourceId;
  __declspec(property(get = __get_PerHourRate)) int64_t PerHourRate;
  __declspec(property(get = __get_RemainingTime)) TimeSpan RemainingTime;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "MiningSlot");
    return class_helper;
  }

public:
  MiningNodeParams* __get_PointData()
  {
    static auto field = get_class_helper().GetProperty("PointData");
    return field.GetRaw<MiningNodeParams>(this);
  }

  int64_t __get_ResourceId()
  {
    static auto field = get_class_helper().GetProperty("ResourceId");
    auto* value = field.Get<int64_t>(this);
    return value ? *value : 0;
  }

  int64_t __get_PerHourRate()
  {
    static auto field = get_class_helper().GetProperty("PerHourRate");
    auto* value = field.Get<int64_t>(this);
    return value ? *value : 0;
  }

  TimeSpan __get_RemainingTime()
  {
    static auto field = get_class_helper().GetProperty("RemainingTime");
    auto* value = field.Get<TimeSpan>(this);
    return value ? *value : TimeSpan{};
  }
};