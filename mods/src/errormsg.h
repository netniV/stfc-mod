#pragma once

#include "str_utils.h"

#include <spdlog/spdlog.h>

#if _WIN32
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.Headers.h>
#endif

class ErrorMsg
{
public:
  static void MissingMethod(const char* classname, const char* methodname)
  {
    spdlog::error("Unable to find method '{}->{}'", classname, methodname);
  }

  static void MissingStaticMethod(const char* classname, const char* methodname)
  {
    spdlog::error("Unable to find method '{}::{}'", classname, methodname);
  }

  static void MissingHelper(const char* namespacename, const char* classname)
  {
    spdlog::error("Unable to find helper '{}.{}'", namespacename, classname);
  }

  static void SyncMsg(const char* section, const std::wstring& msg)
  {
    spdlog::error("Failed to send {} sync data: {}", section, to_string(msg).c_str());
  }

  static void SyncRuntime(const char* section, const std::runtime_error& e)
  {
    spdlog::error("Runtime error sending {} sync data: {}", section, e.what());
  }

#if _WIN32
  static void SyncWinRT(const char* section, winrt::hresult_error const& ex)
  {
    spdlog::error("WINRT Error sending {} sync data: {}", section, winrt::to_string(ex.message()).c_str());
  }
#endif
};
