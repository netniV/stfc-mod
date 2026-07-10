#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "HttpRequest.h"
#include "UnityWebRequest.h"

enum class HttpJobState {
  None                 = 0,
  Queued               = 1,
  RequestSending       = 2,
  ResponseReceiving    = 3,
  ResponseReceived     = 4,
  ResponseBuilt        = 5,
  WaitForAsyncCallback = 6,
  Complete             = 7
};

struct HttpJob {
public:
  __declspec(property(get = __get_Request)) HttpRequest* Request;
  __declspec(property(get = __get_State, put = __set_State)) HttpJobState State;
  __declspec(property(get = __get_UnityRequest)) UnityWebRequest* UnityRequest;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Engine.HttpClient.Runtime", "Digit.Engine.HttpClient", "HttpJob");
    return class_helper;
  }

public:
  HttpRequest* __get_Request()
  {
    static auto field = get_class_helper().GetField("Request");
    return *(HttpRequest**)((ptrdiff_t)this + field.offset());
  }

  void __set_State(HttpJobState state)
  {
    static auto field                                  = get_class_helper().GetField("State");
    *(HttpJobState*)((ptrdiff_t)this + field.offset()) = state;
  }

  HttpJobState __get_State()
  {
    static auto field = get_class_helper().GetField("State");
    return *(HttpJobState*)((ptrdiff_t)this + field.offset());
  }

  UnityWebRequest* __get_UnityRequest()
  {
    static auto field = get_class_helper().GetField("UnityRequest");
    return *(UnityWebRequest**)((ptrdiff_t)this + field.offset());
  }
};
