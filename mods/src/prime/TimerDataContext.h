#pragma once

#include "TimeSpan.h"

#include <il2cpp/il2cpp_helper.h>

struct TimerDataContext {
public:
  __declspec(property(get = __get_RemainingTime)) TimeSpan RemainingTime;
  __declspec(property(get = __get_TimerTypeValue)) int TimerTypeValue;
  __declspec(property(get = __get_TimerStateValue)) int TimerStateValue;
  __declspec(property(get = __get_ShowTimerLabel)) bool ShowTimerLabel;

  TimeSpan __get_RemainingTime()
  {
    auto helper = IL2CppClassHelper{((Il2CppObject*)this)->klass};
    auto field  = helper.GetProperty("RemainingTime");
    auto* value = field.Get<TimeSpan>(this);
    return value ? *value : TimeSpan{};
  }

  int __get_TimerTypeValue()
  {
    auto helper = IL2CppClassHelper{((Il2CppObject*)this)->klass};
    auto field  = helper.GetProperty("TimerType");
    auto* value = field.Get<int>(this);
    return value ? *value : -1;
  }

  int __get_TimerStateValue()
  {
    auto helper = IL2CppClassHelper{((Il2CppObject*)this)->klass};
    auto field  = helper.GetProperty("TimerState");
    auto* value = field.Get<int>(this);
    return value ? *value : -1;
  }

  bool __get_ShowTimerLabel()
  {
    auto helper = IL2CppClassHelper{((Il2CppObject*)this)->klass};
    auto field  = helper.GetProperty("ShowTimerLabel");
    auto* value = field.Get<bool>(this);
    return value ? *value : false;
  }
};