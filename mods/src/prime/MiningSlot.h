#pragma once

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>
#include <limits>

struct MiningSlot {
public:
  __declspec(property(get = __get_ResourceId)) int64_t  ResourceId;
  __declspec(property(get = __get_MiningSpeed)) float   MiningSpeed;
  __declspec(property(get = __get_CurrentValue)) double CurrentValue;
  __declspec(property(get = __get_MaxValue)) double     MaxValue;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "MiningSlot");
    return class_helper;
  }

public:
  int64_t __get_ResourceId()
  {
    static auto property = get_class_helper().GetProperty("ResourceId");
    auto*       value    = property.Get<int64_t>(this);
    return value ? *value : 0;
  }

  float __get_MiningSpeed()
  {
    static auto property = get_class_helper().GetProperty("MiningSpeed");
    auto*       value    = property.Get<float>(this);
    return value ? *value : 0;
  }

  double __get_CurrentValue()
  {
    static auto property = get_class_helper().GetProperty("CurrentValue");
    auto*       value    = property.Get<double>(this);
    return value ? *value : std::numeric_limits<double>::quiet_NaN();
  }

  double __get_MaxValue()
  {
    static auto property = get_class_helper().GetProperty("MaxValue");
    auto*       value    = property.Get<double>(this);
    return value ? *value : std::numeric_limits<double>::quiet_NaN();
  }
};
