#pragma once

#include <il2cpp/il2cpp_helper.h>

struct BlurController {
public:
  __declspec(property(get = __get__blurTime, put = __set__blurTime)) float _blurTime;
  __declspec(property(get = __get__waitFrames, put = __set__waitFrames)) float _waitFrames;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.LoadingScreen", "BlurController");
    return class_helper;
  }

public:
  float __get__blurTime()
  {
    static auto field = get_class_helper().GetField("_blurTime");
    return *(float*)((ptrdiff_t)this + field.offset());
  }
  void __set__blurTime(float v)
  {
    static auto field                           = get_class_helper().GetField("_blurTime");
    *(float*)((ptrdiff_t)this + field.offset()) = v;
  }

  float __get__waitFrames()
  {
    static auto field = get_class_helper().GetField("_waitFrames");
    return *(float*)((ptrdiff_t)this + field.offset());
  }
  void __set__waitFrames(float v)
  {
    static auto field                           = get_class_helper().GetField("_waitFrames");
    *(float*)((ptrdiff_t)this + field.offset()) = v;
  }

  void ForceCompletion()
  {
    static auto method = get_class_helper().GetMethod("ForceCompletion");
    if (method)
      reinterpret_cast<void (*)(BlurController*)>(method)(this);
  }
};