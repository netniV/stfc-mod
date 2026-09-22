#pragma once

#include "il2cpp_helper.h"
#include <il2cpp-tabledefs.h>

// Mechanics shared by optional runtime features. Signature/overload selection,
// argument storage and feature-specific failure policy remain with the caller.
namespace Il2CppRuntime
{
inline Il2CppClass* Class(const char* assembly, const char* ns, const char* name)
{ return il2cpp_get_class_helper(assembly, ns, name).get_cls(); }

inline const MethodInfo* Method(Il2CppClass* cls, const char* name, int count)
{ return IL2CppClassHelper(cls).GetMethodInfo(name, count); }

inline bool Type(const Il2CppType* type, int expected)
{ return type && !type->byref && type->type == expected; }

inline bool Reference(const Il2CppType* type)
{
  return Type(type, IL2CPP_TYPE_CLASS) || Type(type, IL2CPP_TYPE_GENERICINST) || Type(type, IL2CPP_TYPE_OBJECT)
         || Type(type, IL2CPP_TYPE_STRING);
}

inline bool Instance(const MethodInfo* method, int count, int result)
{
  return method && method->methodPointer && method->invoker_method && !(method->flags & METHOD_ATTRIBUTE_STATIC)
         && method->parameters_count == count && Type(method->return_type, result)
         && !method->has_full_generic_sharing_signature;
}

// The method and argument ABI must already be established by the caller.
// IL2CPP takes references directly, but value/byref arguments as addresses.
// A null target is valid for static methods. Success is independent of whether
// the return value is null (including void methods). Outputs change on success only.
inline bool TryInvoke(const MethodInfo* method, void* target, void** args, Il2CppObject** result = nullptr)
{
  if (!method)
    return false;
  Il2CppException* exception = nullptr;
  auto*            value     = il2cpp_runtime_invoke(method, target, args, &exception);
  if (exception)
    return false;
  if (result)
    *result = value;
  return true;
}

inline bool TryBoolean(Il2CppObject* boxed, bool& value)
{
  if (!boxed || !boxed->klass || !Type(il2cpp_class_get_type(boxed->klass), IL2CPP_TYPE_BOOLEAN))
    return false;
  auto* data = static_cast<bool*>(il2cpp_object_unbox(boxed));
  if (!data)
    return false;
  value = *data;
  return true;
}
} // namespace Il2CppRuntime
