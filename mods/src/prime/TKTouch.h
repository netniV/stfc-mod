#pragma once

#include <il2cpp/il2cpp_helper.h>

enum class TouchPhase : int32_t {
  Began      = 0,
  Moved      = 1,
  Stationary = 2,
  Ended      = 3,
  Canceled   = 4,
};

struct TKTouch {
public:
  __declspec(property(get = __get_phase, put = __set_phase)) TouchPhase phase;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("TouchKit", "", "TKTouch");
    return class_helper;
  }

public:
  TouchPhase __get_phase()
  {
    static auto field = get_class_helper().GetField("phase");
    return *reinterpret_cast<TouchPhase*>(reinterpret_cast<ptrdiff_t>(this) + field.offset());
  }

  void __set_phase(TouchPhase phase)
  {
    static auto field = get_class_helper().GetField("phase");
    *reinterpret_cast<TouchPhase*>(reinterpret_cast<ptrdiff_t>(this) + field.offset()) = phase;
  }
};
