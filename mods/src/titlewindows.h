#pragma once

#ifdef _WIN32

#include "config.h"
#include <Windows.h>
#include <string>

inline std::wstring WindowTitle::Get()
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

inline bool WindowTitle::Set(const std::wstring& title)
{
  HWND hwnd = Config::WindowHandle();
  if (!hwnd)
    return false;

  return SetWindowTextW(hwnd, title.c_str()) != 0;
}

#elif defined(__APPLE__)
#else
#endif
