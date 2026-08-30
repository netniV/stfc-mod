#pragma once

#include <il2cpp/il2cpp_helper.h>

struct ProgressData {
public:
  __declspec(property(get = __get_CurrentValue)) double CurrentValue;

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
    static auto property = get_class_helper().GetProperty("CurrentValue");
    auto*       value    = property.Get<double>(this);
    return value ? *value : 0.0;
  }
};
