#pragma once

#include <il2cpp/il2cpp_helper.h>

struct SystemViewScaler {
public:
  __declspec(property(get = __get__baseSystemRadius, put = __set__baseSystemRadius)) float _baseSystemRadius;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "SystemViewScaler");
    return class_helper;
  }

public:
  float __get__baseSystemRadius()
  {
    static auto field = get_class_helper().GetField("_baseSystemRadius");
    return *(float*)((ptrdiff_t)this + field.offset());
  }

  void __set__baseSystemRadius(float v)
  {
    static auto field                          = get_class_helper().GetField("_baseSystemRadius");
    *(float*)((ptrdiff_t)this + field.offset()) = v;
  }
};
