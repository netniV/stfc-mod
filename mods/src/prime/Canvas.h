#pragma once

#include "il2cpp/il2cpp_helper.h"

struct Canvas {
public:
  __declspec(property(get = __get_scaleFactor, put = __set_scaleFactor)) float scaleFactor;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("UnityEngine.UIModule", "UnityEngine", "Canvas");
    return class_helper;
  }

public:
  float __get_scaleFactor()
  {
    static auto field = get_class_helper().GetProperty("scaleFactor");
    return *field.Get<float>(this);
  }
  void __set_scaleFactor(float v)
  {
    static auto field = get_class_helper().GetProperty("scaleFactor");
    field.SetRaw<float>(this, v);
  }
};
