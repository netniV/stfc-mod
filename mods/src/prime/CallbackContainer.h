#pragma once

#include <il2cpp/il2cpp_helper.h>

struct __declspec(align(8)) Delegate__Fields {
  void*                method_ptr;
  void*                invoke_impl;
  struct Object*       m_target;
  void*                method;
  void*                delegate_trampoline;
  void*                method_code;
  struct MethodInfo_1* method_info;
  struct MethodInfo_1* original_method_info;
  struct DelegateData* data;
};

struct Delegate {
  struct Delegate__Class* klass;
  MonitorData*            monitor;
  struct Delegate__Fields fields;
};

struct MulticastDelegate__Fields {
  struct Delegate__Fields   _;
  struct MulticastDelegate* prev;
  struct MulticastDelegate* kpm_next;
};

struct OnSuccess_1_System_Int32___Fields {
  struct MulticastDelegate__Fields _;
};

struct OnSuccess_1_System_Int32_ {
  struct OnSuccess_1_System_Int32___Class* klass;
  MonitorData*                             monitor;
  struct OnSuccess_1_System_Int32___Fields fields;
};

struct OnError__Fields {
  struct MulticastDelegate__Fields _;
};

struct OnError {
  struct OnError__Class* klass;
  MonitorData*           monitor;
  struct OnError__Fields fields;
};

struct __declspec(align(8)) CallbackContainer_1_System_Int32___Fields {
  struct OnSuccess_1_System_Int32_* _onSuccess;
  struct OnError*                   _onError;
  struct OnRetry*                   _onRetry;
#if defined(_CPLUSPLUS_)
  CallbackErrorHandling__Enum _callbackErrorHandling;
#else
  int32_t _callbackErrorHandling;
#endif
};

struct CallbackContainer_1_System_Int32_ {
  struct CallbackContainer_1_System_Int32___Class* klass;
  MonitorData*                                     monitor;
  struct CallbackContainer_1_System_Int32___Fields fields;
};

class CallbackContainer
{
public:
  CallbackContainer() {}

  static CallbackContainer_1_System_Int32_* Create()
  {
    auto n             = get_class_helper().New<CallbackContainer_1_System_Int32_>();
    n                  = n;
    n->fields._onRetry = nullptr;

    n->fields._callbackErrorHandling = 0;
    n->fields._onSuccess             = (OnSuccess_1_System_Int32_*)n;
    n->fields._onError               = (OnError*)n;
    return n;
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Engine.Utilities.Runtime", "Digit.Networking.Core", "CallbackContainer`1");
    return class_helper;
  }
};