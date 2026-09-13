#if defined(_WIN32) && defined(_M_X64)
#include "interop.h"
#include "settings/page_catalog.h"
#include <Windows.h>
#include <cstring>
#include <spdlog/spdlog.h>

namespace mod_settings::native
{
namespace
{
  bool warned = false;
}
void Warn(const char* reason)
{
  if (!warned) {
    warned = true;
    spdlog::warn("[ModSettings] {}", reason);
  }
}
bool Type(const Il2CppType* type, int expected)
{ return type && !type->byref && type->type == expected; }
bool Instance(const MethodInfo* method, int count, int result)
{
  return method && method->methodPointer && method->invoker_method && !(method->flags & METHOD_ATTRIBUTE_STATIC)
         && method->parameters_count == count && Type(method->return_type, result)
         && !method->has_full_generic_sharing_signature;
}
bool Reference(const Il2CppType* type)
{
  return Type(type, IL2CPP_TYPE_CLASS) || Type(type, IL2CPP_TYPE_GENERICINST) || Type(type, IL2CPP_TYPE_OBJECT)
         || Type(type, IL2CPP_TYPE_STRING);
}
FieldInfo* Field(Il2CppClass* cls, const char* name)
{
  auto* field = cls ? il2cpp_class_get_field_from_name(cls, name) : nullptr;
  if (!field || !Reference(field->type) || (field->type->attrs & FIELD_ATTRIBUTE_STATIC))
    throw std::runtime_error("settings reference field");
  return field;
}
Il2CppObject* ReadField(Il2CppObject* object, FieldInfo* field)
{
  Il2CppObject* value = nullptr;
  if (object)
    il2cpp_field_get_value(object, field, &value);
  return value;
}
Il2CppObject* Invoke(const MethodInfo* method, Il2CppObject* object, void** args)
{
  if (!method || !object)
    throw std::runtime_error("settings invocation");
  Il2CppException* error  = nullptr;
  auto*            result = il2cpp_runtime_invoke(method, object, args, &error);
  if (error)
    throw std::runtime_error("settings managed exception");
  return result;
}
// Bounded discovery helpers used only while opening a page or binding a row.
Il2CppObject* Call(Il2CppObject* object, const char* name, int count, void** args)
{ return Invoke(object ? il2cpp_class_get_method_from_name(object->klass, name, count) : nullptr, object, args); }
bool Boolean(Il2CppObject* boxed)
{
  if (!boxed || !Type(il2cpp_class_get_type(boxed->klass), IL2CPP_TYPE_BOOLEAN))
    throw std::runtime_error("settings boolean result");
  return *static_cast<bool*>(il2cpp_object_unbox(boxed));
}
bool Equals(Il2CppObject* value, const char* ascii)
{
  if (!value || !Type(il2cpp_class_get_type(value->klass), IL2CPP_TYPE_STRING))
    return false;
  auto*      text   = reinterpret_cast<Il2CppString*>(value);
  const auto length = std::strlen(ascii);
  if (il2cpp_string_length(text) != length)
    return false;
  auto* chars = il2cpp_string_chars(text);
  for (std::size_t i = 0; i < length; ++i)
    if (chars[i] != static_cast<unsigned char>(ascii[i]))
      return false;
  return true;
}
Il2CppObject* Target(Il2CppGCHandle handle)
{ return handle ? il2cpp_gchandle_get_target(handle) : nullptr; }
void Free(Il2CppGCHandle& handle)
{
  if (handle)
    il2cpp_gchandle_free(handle);
  handle = nullptr;
}

// Cosmetic changes belong to the bound row. Restore before native refresh or
void SetActive(Il2CppObject* object, bool value)
{
  void* args[] = {&value};
  Call(object, "SetActive", 1, args);
}
Il2CppObject* MakeDelegate(Il2CppClass* cls, Il2CppObject* director, const MethodInfo* method)
{
  const auto* ctor = cls ? il2cpp_class_get_method_from_name(cls, ".ctor", 2) : nullptr;
  if (!Instance(ctor, 2, IL2CPP_TYPE_VOID) || !Reference(ctor->parameters[0])
      || !Type(ctor->parameters[1], IL2CPP_TYPE_I) || !method)
    throw std::runtime_error("settings delegate constructor");
  const auto* invoke = il2cpp_class_get_method_from_name(cls, "Invoke", method->parameters_count);
  auto*       parent = il2cpp_class_get_parent(cls);
  if (!parent || std::strcmp(il2cpp_class_get_name(parent), "MulticastDelegate") != 0
      || std::strcmp(il2cpp_class_get_namespace(parent), "System") != 0 || !invoke || invoke->return_type->byref
      || (invoke->flags & METHOD_ATTRIBUTE_STATIC)
      || il2cpp_class_from_type(invoke->return_type) != il2cpp_class_from_type(method->return_type)
      || !il2cpp_class_is_assignable_from(method->klass, director->klass))
    throw std::runtime_error("settings delegate signature");
  for (int i = 0; i < method->parameters_count; ++i)
    if (invoke->parameters[i]->byref
        || il2cpp_class_from_type(invoke->parameters[i]) != il2cpp_class_from_type(method->parameters[i]))
      throw std::runtime_error("settings delegate parameter");
  Root  object(il2cpp_object_new(cls));
  void* args[] = {director, &method};
  Invoke(ctor, object.get(), args);
  auto* delegate = reinterpret_cast<Il2CppDelegate*>(object.get());
  if (delegate->target != director || delegate->invoke_impl_this != director || delegate->method != method)
    throw std::runtime_error("settings closed delegate");
  delegate->method_ptr  = method->methodPointer;
  delegate->invoke_impl = method->methodPointer;
  return object.get();
}

int Count(Il2CppObject* list)
{
  Root value(Call(list, "get_Count"));
  if (!value.get() || !Type(il2cpp_class_get_type(value.get()->klass), IL2CPP_TYPE_I4))
    throw std::runtime_error("settings child count");
  const int count = *static_cast<int*>(il2cpp_object_unbox(value.get()));
  if (count < 0 || count > PageCatalog::NativeChildLimit)
    throw std::runtime_error("settings child bound");
  return count;
}
Il2CppObject* Item(Il2CppObject* list, int index)
{
  void* args[] = {&index};
  return Call(list, "get_Item", 1, args);
}
bool HasLabel(Il2CppObject* row, const char* id)
{
  Root label(Call(row, "get_LabelContext"));
  return label.get() && Equals(Call(label.get(), "get_Identifier"), id);
}
bool Extent(const MethodInfo* method)
{
  if (!method || !method->methodPointer)
    return false;
  DWORD64    base    = 0;
  const auto address = reinterpret_cast<DWORD64>(method->methodPointer);
  auto*      entry   = RtlLookupFunctionEntry(address, &base, nullptr);
  // Bundled x64 SPUD reserves 24 bytes; the 64-byte minimum and exact entry reject
  // shared tiny accessors/thunks. Only Windows x64 is enabled by this adapter.
  return entry && base + entry->BeginAddress == address && entry->EndAddress - entry->BeginAddress >= 64;
}

} // namespace mod_settings::native
#endif
