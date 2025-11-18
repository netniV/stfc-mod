#pragma once
#include <string>

struct WindowTitle {
  // Get the window title
  static std::wstring Get();

  // Set the window title
  static bool Set(const std::wstring& title);
};

#include "titlewindows.h"
#include "titlelinux.h"
