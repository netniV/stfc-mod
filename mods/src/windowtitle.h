#pragma once
#include <string>

struct IWindowTitle {
  // Get the window title
  static std::wstring Get();

  // Set the window title
  static bool Set(const std::wstring& title);
};
