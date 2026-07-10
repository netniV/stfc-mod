#pragma once

#include <il2cpp/il2cpp_helper.h>

struct HttpRequest {
public:
  const Il2CppChar* get_URL()
  {
    static auto prop = get_class_helper().GetProperty("URL");
    auto        s    = prop.Get<Il2CppString>((void*)this);
    return il2cpp_string_chars(s);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Engine.HttpClient.Runtime", "Digit.Engine.HttpClient", "HttpRequest");
    return class_helper;
  }
};
