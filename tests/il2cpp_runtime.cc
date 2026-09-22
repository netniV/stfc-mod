// Production helpers with a controlled IL2CPP boundary, on Windows and macOS.
#include <il2cpp-config.h>
#if _WIN32
#undef IL2CPP_IMPORT
#define IL2CPP_IMPORT
#endif
#include "il2cpp/runtime.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
Il2CppClass  klass;
Il2CppType   type{};
Il2CppObject object{};
MethodInfo   method{};
int          stage = 3, calls = 0;
bool         fail = false, boxed = false, null_result = false;
void*        seen_target = nullptr;
void**       seen_args   = nullptr;
} // namespace
#if _WIN32
#define API(ret, name, params) extern "C" ret name params
#define END_API
#else
#define API(ret, name, params) name##_t name = +[] params->ret
#define END_API ;
#endif
API(Il2CppDomain*, il2cpp_domain_get, ())
{ return stage >= 0 ? reinterpret_cast<Il2CppDomain*>(&object) : nullptr; }
END_API
API(const Il2CppAssembly*, il2cpp_domain_assembly_open, (Il2CppDomain * domain, const char*))
{
  if (!domain)
    std::abort();
  return stage >= 1 ? reinterpret_cast<Il2CppAssembly*>(&object) : nullptr;
}
END_API
API(const Il2CppImage*, il2cpp_assembly_get_image, (const Il2CppAssembly* assembly))
{
  if (!assembly)
    std::abort();
  return stage >= 2 ? reinterpret_cast<Il2CppImage*>(&object) : nullptr;
}
END_API
API(Il2CppClass*, il2cpp_class_from_name, (const Il2CppImage* image, const char*, const char*))
{
  if (!image)
    std::abort();
  return stage >= 3 ? &klass : nullptr;
}
END_API
API(const MethodInfo*, il2cpp_class_get_method_from_name, (Il2CppClass * cls, const char*, int))
{
  if (!cls)
    std::abort();
  return &method;
}
END_API
API(const Il2CppType*, il2cpp_class_get_type, (Il2CppClass*))
{ return &type; }
END_API
API(void*, il2cpp_object_unbox, (Il2CppObject*))
{ return &boxed; }
END_API
API(Il2CppObject*, il2cpp_runtime_invoke, (const MethodInfo*, void* target, void** args, Il2CppException** error))
{
  ++calls;
  seen_target = target;
  seen_args   = args;
  if (fail)
    *error = reinterpret_cast<Il2CppException*>(&object);
  return null_result ? nullptr : &object;
}
END_API
void Require(bool condition)
{
  if (!condition)
    throw std::runtime_error("runtime helper regression");
}
int main()
{
  for (stage = -1; stage < 3; ++stage)
    Require(!Il2CppRuntime::Class("Assembly", "Namespace", "Class"));
  Require(Il2CppRuntime::Class("Assembly", "Namespace", "Class") == &klass);
  Require(!Il2CppRuntime::Method(nullptr, "Method", 0));
  Require(Il2CppRuntime::Method(&klass, "Method", 0) == &method);

  Il2CppObject* result = &object;
  Require(!Il2CppRuntime::TryInvoke(nullptr, nullptr, nullptr, &result) && calls == 0 && result == &object);
  bool  value  = true;
  void* args[] = {&value, &object, nullptr};
  Require(Il2CppRuntime::TryInvoke(&method, &object, args, &result));
  Require(seen_target == &object && seen_args == args && seen_args[0] == &value && seen_args[1] == &object);
  // Static calls, null reference/void returns and exceptions remain distinct.
  null_result = true;
  Require(Il2CppRuntime::TryInvoke(&method, nullptr, args, &result) && !result && !seen_target);
  result = &object;
  fail   = true;
  Require(!Il2CppRuntime::TryInvoke(&method, nullptr, args, &result) && result == &object);

  object.klass = &klass;
  type.type    = IL2CPP_TYPE_BOOLEAN;
  Require(Il2CppRuntime::TryBoolean(&object, value) && !value);
  boxed = true;
  Require(Il2CppRuntime::TryBoolean(&object, value) && value);
  type.type = IL2CPP_TYPE_I4;
  Require(!Il2CppRuntime::TryBoolean(&object, value) && value);
  Require(!Il2CppRuntime::TryBoolean(nullptr, value));
  type.type = IL2CPP_TYPE_CLASS;
  Require(Il2CppRuntime::Reference(&type));
  type.byref = true;
  Require(!Il2CppRuntime::Reference(&type));
  type.byref            = false;
  type.type             = IL2CPP_TYPE_VOID;
  method.methodPointer  = reinterpret_cast<Il2CppMethodPointer>(1);
  method.invoker_method = reinterpret_cast<InvokerMethod>(1);
  method.return_type    = &type;
  Require(Il2CppRuntime::Instance(&method, 0, IL2CPP_TYPE_VOID));
  method.flags = METHOD_ATTRIBUTE_STATIC;
  Require(!Il2CppRuntime::Instance(&method, 0, IL2CPP_TYPE_VOID));
  method.flags                              = 0;
  method.has_full_generic_sharing_signature = true;
  Require(!Il2CppRuntime::Instance(&method, 0, IL2CPP_TYPE_VOID));
  std::cout << "IL2CPP runtime helper regressions passed\n";
}
