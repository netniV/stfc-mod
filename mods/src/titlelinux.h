#pragma once
#ifdef _WIN32
#elif defined(__APPLE__)
#else

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <string>

inline std::wstring WindowTitle::Get()
{
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy)
    return L"";

  Window         root = DefaultRootWindow(dpy);
  Atom           prop = XInternAtom(dpy, "_NET_WM_NAME", False);
  Atom           type;
  int            format;
  unsigned long  nitems, bytes_after;
  unsigned char* prop_ret = nullptr;
  std::wstring   title;

  if (XGetWindowProperty(dpy, root, prop, 0, 1024, False, AnyPropertyType, &type, &format, &nitems, &bytes_after,
                         &prop_ret)
      == Success) {
    if (prop_ret) {
      std::string s(reinterpret_cast<char*>(prop_ret), nitems);
      title.assign(s.begin(), s.end()); // simple UTF-8 -> wstring conversion
      XFree(prop_ret);
    }
  }

  XCloseDisplay(dpy);
  return title;
}

inline bool WindowTitle::Set(const std::wstring& title)
{
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy)
    return false;

  Window      root = DefaultRootWindow(dpy);
  std::string s(title.begin(), title.end()); // simple conversion to UTF-8

  int result = XStoreName(dpy, root, s.c_str());
  XFlush(dpy);
  XCloseDisplay(dpy);

  return result != 0; // returns true if successful
}

#endif
