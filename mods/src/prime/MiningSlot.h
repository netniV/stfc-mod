#pragma once

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

struct MiningSlot {
public:
  __declspec(property(get = __get_ResourceId)) int64_t ResourceId;

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
};
