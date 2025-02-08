#pragma once

#include <il2cpp/il2cpp_helper.h>

enum class HullType {
  Any          = -1,
  Destroyer    = 0,
  Survey       = 1,
  Explorer     = 2,
  Battleship   = 3,
  Defense      = 4,
  ArmadaTarget = 5
};

struct HullSpec {
public:
  __declspec(property(get = __get_Id)) long Id;
  __declspec(property(get = __get_Type)) HullType Type;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "HullSpec");
    return class_helper;
  }

public:
  long __get_Id()
  {
    static auto field = get_class_helper().GetField("id_").offset();
    return *(long*)((char*)this + field);
  }

  HullType __get_Type()
  {
    static auto field = get_class_helper().GetProperty("Type");
    return *field.Get<HullType>(this);
  }
};
