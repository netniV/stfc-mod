#pragma once

#include <cstdint>

struct TimeSpan {
public:
  __declspec(property(get = __get_Ticks)) int64_t Ticks;

  int64_t __get_Ticks() const
  {
    return ticks;
  }

  double TotalSeconds() const
  {
    return static_cast<double>(ticks) / 10000000.0;
  }

private:
  int64_t ticks = 0;
};