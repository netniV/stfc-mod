#pragma once
#include "windowtitle.h"
#include <Windows.h>
#include <config.h>

struct TitleWindows : public IWindowTitle {
  static std::wstring Get()
  {
    HWND hwnd = Config::WindowHandle();
    if (!hwnd)
      return L"";

    int length = GetWindowTextLengthW(hwnd);
    if (length == 0)
      return L"";

    std::wstring title(length + 1, L'\0');

    int written = GetWindowTextW(hwnd, title.data(), length + 1);
    title.resize(written);
    return title;
  }

  static bool Set(const std::wstring& title)
  {
    HWND hwnd = Config::WindowHandle();
    if (hwnd)
      return SetWindowTextW(hwnd, title.c_str());
  }
};
