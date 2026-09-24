#include "keyboard_layout_notifications.h"

// Use the tested Windows x64 delegate ABI.
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
#include "il2cpp/il2cpp_helper.h"

namespace keyboard_layout::notifications
{
namespace
{
  struct State {
    MethodInfo             native_method{}; // Private copy, never patch the game's metadata.
    const MethodInfo *     add = nullptr, *remove = nullptr;
    Il2CppGCHandle         root    = 0;
    RefreshState*          refresh = nullptr;
    std::atomic<bool>      accepting{false};
    bool                   attempted = false, subscribed = false, active = false;
  };

  State& Get()
  {
    // Retain metadata even if removal fails. There is no supported live DLL unload.
    static auto* state = new State;
    return *state;
  }

  void Changed(void*, int32_t change, const MethodInfo*) noexcept
  {
    auto& state = Get();
    if (!state.accepting.load())
      return;
    // Added/Removed/Disconnected/Reconnected/Enabled/Disabled (0..5),
    // or ConfigurationChanged (7). Any device can change which keyboard is current;
    // conservatively invalidate without polling or touching Unity in the callback.
    if ((change >= 0 && change <= 5) || change == 7)
      state.refresh->Invalidate();
  }

  bool Invoke(const MethodInfo* method, void* self, void** args)
  {
    Il2CppException* exception = nullptr;
    il2cpp_runtime_invoke(method, self, args, &exception);
    return exception == nullptr;
  }

  bool Create()
  {
    auto& state           = Get();
    auto  input           = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputSystem");
    auto  actions         = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputActionState");
    state.add             = input.GetMethodInfo("add_onDeviceChange", 1);
    state.remove          = input.GetMethodInfo("remove_onDeviceChange", 1);
    const auto* signature = actions.GetMethodInfo("OnDeviceChange", 2);
    if (!state.add || !state.remove || !signature)
      return false;
    const auto* delegate_type = il2cpp_method_get_param(state.add, 0);
    if (!delegate_type || il2cpp_type_is_byref(delegate_type)
        || !il2cpp_type_equals(delegate_type, il2cpp_method_get_param(state.remove, 0)))
      return false;
    auto* delegate_class = il2cpp_class_from_type(delegate_type);
    if (!delegate_class)
      return false;
    const auto* ctor   = il2cpp_class_get_method_from_name(delegate_class, ".ctor", 2);
    const auto* invoke = il2cpp_class_get_method_from_name(delegate_class, "Invoke", 2);
    if (!ctor || !ctor->methodPointer || !ctor->invoker_method || !invoke || !(signature->flags & 0x0010)
        || signature->is_generic || signature->is_inflated || signature->parameters_count != 2
        || il2cpp_type_get_type(signature->return_type) != IL2CPP_TYPE_VOID
        || il2cpp_type_get_type(invoke->return_type) != IL2CPP_TYPE_VOID)
      return false;
    // Validate complete by-value shapes as well as class identity before borrowing metadata.
    for (unsigned i = 0; i < 2; ++i) {
      const auto* param  = il2cpp_method_get_param(signature, i);
      const auto* target = il2cpp_method_get_param(invoke, i);
      if (!param || !target || il2cpp_type_is_byref(param) || il2cpp_type_is_byref(target)
          || !il2cpp_type_equals(param, target)
          || il2cpp_type_get_type(param) != (i == 0 ? IL2CPP_TYPE_CLASS : IL2CPP_TYPE_VALUETYPE))
        return false;
      if (i == 1) {
        auto* enum_class = il2cpp_class_from_type(param);
        if (!enum_class || !il2cpp_class_is_enum(enum_class))
          return false;
        const auto* base = il2cpp_class_enum_basetype(enum_class);
        if (!base || il2cpp_type_get_type(base) != IL2CPP_TYPE_I4)
          return false;
      }
    }
    auto* delegate = il2cpp_object_new(delegate_class);
    if (!delegate)
      return false;
    state.root = il2cpp_gchandle_new(delegate, true);
    if (!state.root)
      return false;
    state.native_method                      = *signature;
    state.native_method.methodPointer        = reinterpret_cast<Il2CppMethodPointer>(&Changed);
    state.native_method.virtualMethodPointer = state.native_method.methodPointer;
    const MethodInfo* entry                  = &state.native_method;
    void*             ctor_args[]{nullptr, &entry};
    // Follow libil2cpp Type::InvokeDelegateConstructor: the client's generated
    // invoker handles its platform's constructor ABI. Do not cast the constructor
    // to a Windows-specific native signature. runtime_invoke's special delegate
    // constructor path rejects the static null target on the researched client.
    try {
      ctor->invoker_method(ctor->methodPointer, ctor, delegate, ctor_args, nullptr);
    } catch (...) {
      return false;
    }
    auto* typed = reinterpret_cast<Il2CppDelegate*>(delegate);
    if (typed->method != entry || typed->method_ptr != state.native_method.methodPointer)
      return false;
    state.accepting  = true;
    state.subscribed = true; // An exception need not mean the event was untouched.
    void* args[]{delegate};
    return Invoke(state.add, nullptr, args);
  }
} // namespace

Result Start(RefreshState& refresh)
{
  auto& state = Get();
  if (!state.attempted) {
    state.attempted = true;
    state.refresh   = &refresh;
    state.active    = Create();
    if (!state.active)
      Stop();
  }
  return state.active ? Result::Started : Result::Failed;
}

void Stop()
{
  auto& state     = Get();
  state.accepting = false;
  state.active    = false;
  if (state.subscribed) {
    auto* delegate = il2cpp_gchandle_get_target(state.root);
    void* args[]{delegate};
    if (!delegate || !Invoke(state.remove, nullptr, args))
      return; // Preserve the root and metadata if listener removal is uncertain.
    state.subscribed = false;
  }
  if (state.root)
    il2cpp_gchandle_free(state.root);
  state.root = 0;
}
} // namespace keyboard_layout::notifications
#else
namespace keyboard_layout::notifications
{
Result Start(RefreshState&)
{ return Result::Unsupported; }
void Stop() {}
} // namespace keyboard_layout::notifications
#endif
