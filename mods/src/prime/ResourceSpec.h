#pragma once

#include <il2cpp/il2cpp_helper.h>

struct ResourceSpec {
public:
  __declspec(property(get = __get_Name)) Il2CppString* Name;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ResourceSpec");
    return class_helper;
  }

public:
  Il2CppString* __get_Name()
  {
    static auto field = get_class_helper().GetProperty("Name");
    return field.GetRaw<Il2CppString>(this);
  }
};