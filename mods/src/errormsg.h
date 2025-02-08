#pragma once

#include <spdlog/spdlog.h>

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
};
