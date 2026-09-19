#include "fc_confirmation_reset.h"
#include "settings/boolean_view.h"

// Validate native entries against the loaded platform image before installing hooks.
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
#include "settings/native_boolean_callback.h"
#include "patches/native_hook_extent.h"
#if _WIN32
#include <Windows.h>
#endif
#include <array>
#include <cstring>
#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#include <stdexcept>
#include <thread>

namespace
{
using namespace mod_settings;
constexpr const char*      CategoryKey = "game_settings_category_7";
constexpr std::size_t      ViewLimit   = 8;
bool                       active      = false;
bool                       installing  = false;
bool                       warned      = false;
std::thread::id            uiThread;
NativeCallback<bool>       getter;
NativeCallback<void, bool> setter;
NativeCallback<int>        query;

void Warn()
{
  if (!warned) {
    warned = true;
    spdlog::warn("[ModSettings] Native confirmation UI unavailable; no mod control added");
  }
}

struct Root {
  Il2CppGCHandle handle = nullptr;
  explicit Root(Il2CppObject* object, bool weak = false)
  {
    if (object)
      handle = weak ? il2cpp_gchandle_new_weakref(object, false) : il2cpp_gchandle_new(object, false);
    if (object && !handle)
      throw std::runtime_error("settings root");
  }
  ~Root()
  {
    if (handle)
      il2cpp_gchandle_free(handle);
  }
  Root(const Root&) = delete;
  Il2CppObject* get() const
  { return handle ? il2cpp_gchandle_get_target(handle) : nullptr; }
};

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
Il2CppObject* Invoke(const MethodInfo* method, Il2CppObject* object, void** args = nullptr)
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
Il2CppObject* Call(Il2CppObject* object, const char* name, int count = 0, void** args = nullptr)
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

struct Metadata {
  IL2CppClassHelper director =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameSettings", "SettingsSectionDirector");
  IL2CppClassHelper widget =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameSettings", "ToggleOptionWidget");
  IL2CppClassHelper context = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameSettings", "SettingsContext");
  IL2CppClassHelper row = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameSettings", "ToggleOptionContext");
  IL2CppClassHelper prefs =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.PersistentPrefs", "PersistentPrefsManager");
  const MethodInfo* addGeneral  = director.GetMethodInfo("AddGeneralSettings", 1);
  const MethodInfo* addToggle   = context.GetMethodInfo("AddToggle", 4);
  const MethodInfo* refresh     = widget.GetMethodInfo("SetWidgetData", 0);
  const MethodInfo* changed     = widget.GetMethodInfo("OnToggleValueChanged", 1);
  const MethodInfo* release     = widget.GetMethodInfo("OnAboutToReleaseContext", 0);
  const MethodInfo* reload      = prefs.GetMethodInfo("RegisterEvents", 0);
  const MethodInfo* session     = prefs.GetMethodInfo("GameSessionStartedEventHandler", 0);
  const MethodInfo* load        = prefs.GetMethodInfo("LoadPersistentPrefsFromCloud", 0);
  const MethodInfo* getContext  = widget.GetMethodInfo("get_Context", 0);
  const MethodInfo* querySetter = row.GetMethodInfo("set_QueryOptionState", 1);
  FieldInfo*        queryField  = Field(row.get_cls(), "<QueryOptionState>k__BackingField");
  FieldInfo*        labelField  = Field(widget.get_cls(), "_label");
  FieldInfo*        toggleField = Field(widget.get_cls(), "_toggle");
  FieldInfo*        stateField  = Field(widget.get_cls(), "_toggleStateAnimator");
};
Metadata& Meta()
{
  static Metadata metadata;
  return metadata;
}

// Fixed weak records: no page, context, delegate, or account is retained by UI
// bookkeeping. Records are released on native unbind and reclaimed on next bind
// if Unity destroys a widget without sending that notification.
struct View {
  Il2CppGCHandle                widget = nullptr, context = nullptr, label = nullptr;
  std::array<Il2CppGCHandle, 2> indicators{};
  std::array<bool, 2>           activeBefore{};
  bool                          overridden          = false;
  bool                          hidden              = false;
  bool                          rendering           = false;
  bool                          preserveNextRefresh = false;
  BooleanView                   state{FleetCommanderConfirmationSetting()};
};
std::array<View, ViewLimit>& Views()
{
  static std::array<View, ViewLimit> views;
  return views;
}
Il2CppObject* Target(Il2CppGCHandle handle)
{ return handle ? il2cpp_gchandle_get_target(handle) : nullptr; }
void Free(Il2CppGCHandle& handle)
{
  if (handle)
    il2cpp_gchandle_free(handle);
  handle = nullptr;
}
void SetActive(Il2CppObject* object, bool value)
{
  void* args[] = {&value};
  Call(object, "SetActive", 1, args);
}
void Restore(View& view)
{
  if (view.overridden) {
    if (auto* label = Target(view.label))
      Call(label, "ClearTextOverride");
    view.overridden = false;
  }
  if (view.hidden) {
    for (std::size_t i = 0; i < view.indicators.size(); ++i)
      if (auto* object = Target(view.indicators[i]))
        SetActive(object, view.activeBefore[i]);
    view.hidden = false;
  }
}
void Clear(View& view)
{
  try {
    Restore(view);
  } catch (...) {
    Warn();
  }
  Free(view.widget);
  Free(view.context);
  Free(view.label);
  for (auto& handle : view.indicators)
    Free(handle);
  view.state.Unbind();
  view.overridden = view.hidden = view.preserveNextRefresh = false;
}
View* Find(Il2CppObject* widget)
{
  for (auto& view : Views())
    if (Target(view.widget) == widget)
      return &view;
  return nullptr;
}
bool Owned(Il2CppObject* context)
{
  if (!context || context->klass != Meta().row.get_cls())
    return false;
  auto* callback = reinterpret_cast<Il2CppDelegate*>(ReadField(context, Meta().queryField));
  return callback && callback->method == query.method() && callback->method_ptr == query.method()->methodPointer;
}
bool ChildOf(Il2CppObject* transform, Il2CppObject* parent)
{
  void* args[] = {parent};
  return Boolean(Call(transform, "IsChildOf", 1, args));
}
View& Track(Il2CppObject* widget, Il2CppObject* context)
{
  for (auto& view : Views()) {
    if (Target(view.widget))
      continue;
    Clear(view);
    Root                         label(ReadField(widget, Meta().labelField));
    Root                         widgetTransform(Call(widget, "get_transform"));
    Root                         labelTransform(Call(label.get(), "get_transform"));
    std::array<Il2CppObject*, 2> indicators{ReadField(widget, Meta().toggleField),
                                            ReadField(widget, Meta().stateField)};
    Root                         first(Call(indicators[0], "get_gameObject"));
    Root                         second(Call(indicators[1], "get_gameObject"));
    indicators = {first.get(), second.get()};
    // Never hide the row or an ancestor of its label to suppress unknown ON/OFF.
    // An unsupported prefab is rejected before any override is applied.
    for (auto* indicator : indicators) {
      Root transform(Call(indicator, "get_transform"));
      if (transform.get() == widgetTransform.get() || !ChildOf(transform.get(), widgetTransform.get())
          || ChildOf(labelTransform.get(), transform.get()))
        throw std::runtime_error("settings indicator hierarchy");
    }
    try {
      auto weak = [](Il2CppObject* object) {
        auto handle = il2cpp_gchandle_new_weakref(object, false);
        if (!handle)
          throw std::runtime_error("settings weak root");
        return handle;
      };
      view.widget  = weak(widget);
      view.context = weak(context);
      view.label   = weak(label.get());
      for (std::size_t i = 0; i < indicators.size(); ++i)
        view.indicators[i] = weak(indicators[i]);
    } catch (...) {
      Clear(view);
      throw;
    }
    return view;
  }
  throw std::runtime_error("settings view capacity");
}

View* renderingView  = nullptr;
View* requestingView = nullptr;
bool  GetEnabled(Il2CppObject*, const MethodInfo*)
{
  // Native bool signatures cannot express unknown. Only the owned render scope
  // consumes this placeholder; its indicators are suppressed when value is empty.
  return renderingView ? renderingView->state.value().value_or(false) : false;
}
void SetEnabled(Il2CppObject*, bool, const MethodInfo*)
{
  // Deliberately inert. Only OnToggleValueChanged with a live view snapshot can
  // authorize a write; rendering and reflection cannot mutate game preferences.
}
int QueryState(Il2CppObject*, const MethodInfo*)
{ return 0; }

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
  if (count < 0 || count > 128)
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
Il2CppObject* Category(Il2CppObject* container, int depth, int& remaining)
{
  if (!container || depth > 3 || --remaining < 0)
    return nullptr;
  if (HasLabel(container, CategoryKey))
    return container;
  if (!il2cpp_class_get_method_from_name(container->klass, "get_Children", 0))
    return nullptr;
  Root children(Call(container, "get_Children"));
  for (int i = 0, count = Count(children.get()); i < count && remaining > 0; ++i)
    if (auto* found = Category(Item(children.get(), i), depth + 1, remaining))
      return found;
  return nullptr;
}
void AddRow(Il2CppObject* director, Il2CppObject* context)
{
  auto& setting = FleetCommanderConfirmationSetting();
  if (setting.Observe().state.availability == Availability::Unsupported)
    return;
  Root root(Call(context, "get_RootOption"));
  int  remaining = 128;
  Root category(Category(root.get(), 0, remaining));
  if (!category.get())
    throw std::runtime_error("settings confirmation category");
  Root      children(Call(category.get(), "get_Children"));
  const int before = Count(children.get());
  for (int i = 0; i < before; ++i)
    if (Owned(Item(children.get(), i)) || HasLabel(Item(children.get(), i), setting.id().c_str()))
      return;
  if (before == 128)
    throw std::runtime_error("settings category full");
  auto& m = Meta();
  Root  get(MakeDelegate(il2cpp_class_from_type(m.addToggle->parameters[2]), director, getter.method()));
  Root  set(MakeDelegate(il2cpp_class_from_type(m.addToggle->parameters[3]), director, setter.method()));
  Root  state(MakeDelegate(il2cpp_class_from_type(m.querySetter->parameters[0]), director, query.method()));
  Root  label(reinterpret_cast<Il2CppObject*>(il2cpp_string_new(setting.id().c_str())));
  void* args[] = {category.get(), label.get(), get.get(), set.get()};
  Invoke(m.addToggle, context, args);
  if (Count(children.get()) != before + 1)
    throw std::runtime_error("settings row insertion");
  Root row(Item(children.get(), before));
  try {
    if (row.get()->klass != m.row.get_cls() || !HasLabel(row.get(), setting.id().c_str()))
      throw std::runtime_error("settings row identity");
    void* stateArgs[] = {state.get()};
    Invoke(m.querySetter, row.get(), stateArgs);
  } catch (...) {
    void* removeArgs[] = {row.get()};
    Call(category.get(), "RemoveChild", 1, removeArgs);
    throw;
  }
}

void Render(View& view, auto original, Il2CppObject* widget)
{
  if (view.rendering)
    return;
  struct Scope {
    View&                       view;
    View*                       previous;
    BooleanSetting::RenderScope suppress;
    Scope(View& view)
        : view(view)
        , previous(renderingView)
        , suppress(view.state.setting())
    {
      view.rendering = true;
      renderingView  = &view;
    }
    ~Scope()
    {
      renderingView  = previous;
      view.rendering = false;
    }
  } scope(view);
  Restore(view);
  original(widget);
  Root        label(Target(view.label));
  std::string text = view.state.setting().label();
  // The native row has limited label width: "Change not applied; try again" was
  // visibly truncated after "; tr" alongside the FC label. Keep these suffixes
  // short; recheck the full label at supported UI scales when changing wording.
  if (!view.state.value())
    text += " — Reopen to retry";
  else if (view.state.failed())
    text += " — Retry";
  Root  message(reinterpret_cast<Il2CppObject*>(il2cpp_string_new(text.c_str())));
  void* args[] = {message.get()};
  Call(label.get(), "OverrideLocalizedText", 1, args);
  view.overridden = true;
  if (!view.state.value()) {
    // Capture all native values first (the two components may share a node).
    for (std::size_t i = 0; i < view.indicators.size(); ++i)
      view.activeBefore[i] = Boolean(Call(Target(view.indicators[i]), "get_activeSelf"));
    view.hidden = true;
    for (auto handle : view.indicators)
      SetActive(Target(handle), false);
  }
}
void HideUnsupported(Il2CppObject* widget)
{
  try {
    Root object(Call(widget, "get_gameObject"));
    SetActive(object.get(), false);
  } catch (...) {
  }
  Warn();
}
bool OnThread()
{ return active && std::this_thread::get_id() == uiThread; }

void AddGeneralHook(auto original, Il2CppObject* director, Il2CppObject* context)
{
  original(director, context);
  if (!OnThread())
    return;
  try {
    AddRow(director, context);
  } catch (...) {
    Warn();
  }
}
void RefreshHook(auto original, Il2CppObject* widget)
{
  if (!OnThread()) {
    original(widget);
    return;
  }
  bool owned = false;
  try {
    Root context(Invoke(Meta().getContext, widget));
    owned      = Owned(context.get());
    auto* view = Find(widget);
    if (view && view->rendering)
      return;
    if (view && Target(view->context) != context.get()) {
      Clear(*view);
      view = nullptr;
    }
    if (!owned) {
      if (view)
        Clear(*view);
    } else {
      if (!view)
        view = &Track(widget, context.get());
      if (!view->preserveNextRefresh)
        view->state.Bind();
      view->preserveNextRefresh = false;
      Render(*view, original, widget);
      return;
    }
  } catch (...) {
    if (owned) {
      HideUnsupported(widget);
      return;
    }
    Warn();
  }
  original(widget);
}
void RefreshViews()
{
  if (!OnThread())
    return;
  for (auto& view : Views()) {
    if (&view == requestingView || view.rendering)
      continue;
    Root widget(Target(view.widget));
    if (!widget.get())
      continue;
    try {
      Root context(Invoke(Meta().getContext, widget.get()));
      if (Target(view.context) != context.get() || !Owned(context.get()))
        continue;
      Invoke(Meta().refresh, widget.get());
    } catch (...) {
      HideUnsupported(widget.get());
    }
  }
}
void ChangedHook(auto original, Il2CppObject* widget, bool desired)
{
  if (!OnThread()) {
    original(widget, desired);
    return;
  }
  bool owned = false;
  try {
    Root context(Invoke(Meta().getContext, widget));
    owned = Owned(context.get());
    if (owned) {
      auto* view = Find(widget);
      if (!view || view->rendering || Target(view->context) != context.get())
        return;
      struct RequestScope {
        View* previous = requestingView;
        explicit RequestScope(View* view)
        { requestingView = view; }
        ~RequestScope()
        { requestingView = previous; }
      } requestScope(view);
      auto result = view->state.Request(desired);
      if (result.outcome == Outcome::Suppressed || result.outcome == Outcome::Busy)
        return;
      // Refresh through the hook once, preserving the write result. A fresh Bind
      // here would erase an unverified outcome merely because a later read works.
      view->preserveNextRefresh = true;
      Invoke(Meta().refresh, widget);
      return;
    }
  } catch (...) {
    if (owned) {
      HideUnsupported(widget);
      return;
    }
    Warn();
  }
  original(widget, desired);
}
void ReleaseHook(auto original, Il2CppObject* widget)
{
  if (OnThread()) {
    try {
      if (auto* view = Find(widget))
        Clear(*view);
    } catch (...) {
      Warn();
    }
  }
  original(widget);
}
void Invalidate()
{
  InvalidateFleetCommanderConfirmationSession();
  for (auto& view : Views()) {
    view.state.Invalidate();
    // Re-rendering during a native account transition can read the old account.
    // Hide the indicators immediately; next explicit bind may establish readiness.
    if (auto* widget = Target(view.widget)) {
      try {
        Render(view, [](Il2CppObject*) {}, widget);
      } catch (...) {
        HideUnsupported(widget);
      }
    }
  }
}
void SessionBoundary(auto original, Il2CppObject* owner)
{
  if (OnThread()) {
    try {
      Invalidate();
    } catch (...) {
      Warn();
    }
  }
  original(owner);
}
void ReloadHook(auto original, Il2CppObject* owner)
{ SessionBoundary(original, owner); }
void SessionHook(auto original, Il2CppObject* owner)
{ SessionBoundary(original, owner); }
void LoadHook(auto original, Il2CppObject* owner)
{ SessionBoundary(original, owner); }
bool Extent(const MethodInfo* method)
{
  if (!method || !method->methodPointer)
    return false;
#if __APPLE__
  return native_hooks::MacHookFits(reinterpret_cast<const void*>(method->methodPointer));
#else
  DWORD64    base    = 0;
  const auto address = reinterpret_cast<DWORD64>(method->methodPointer);
  auto*      entry   = RtlLookupFunctionEntry(address, &base, nullptr);
  // Bundled x64 SPUD reserves 24 bytes; the 64-byte minimum and exact entry reject
  // shared tiny accessors/thunks. The Mac path separately decodes its full overwrite.
  return entry && base + entry->BeginAddress == address && entry->EndAddress - entry->BeginAddress >= 64;
#endif
}
} // namespace

void InstallModConfirmationSettings()
{
  if (active || installing)
    return;
  installing = true;
  try {
    auto&            m = Meta();
    const std::array hooks{m.addGeneral, m.refresh, m.changed, m.release, m.reload, m.session, m.load};
    for (std::size_t i = 0; i < hooks.size(); ++i) {
      if (!Instance(hooks[i], i == 0 || i == 2 ? 1 : 0, IL2CPP_TYPE_VOID) || !Extent(hooks[i]))
        throw std::runtime_error("settings hook metadata/extent");
      for (std::size_t j = 0; j < i; ++j)
        if (hooks[i]->methodPointer == hooks[j]->methodPointer)
          throw std::runtime_error("settings shared hook");
    }
    const auto* getSchema   = m.director.GetMethodInfo("IsBorgCubeCuttingBeamConfirmationOn", 0);
    const auto* setSchema   = m.director.GetMethodInfo("ToggleBorgCubeCuttingBeamConfirmation", 1);
    const auto* querySchema = m.director.GetMethodInfo("QueryShouldShowGenericPcSetting", 0);
    if (!Instance(getSchema, 0, IL2CPP_TYPE_BOOLEAN) || !Instance(setSchema, 1, IL2CPP_TYPE_VOID)
        || !Type(setSchema->parameters[0], IL2CPP_TYPE_BOOLEAN) || !querySchema
        || !il2cpp_class_is_enum(il2cpp_class_from_type(querySchema->return_type))
        || !Type(il2cpp_class_enum_basetype(il2cpp_class_from_type(querySchema->return_type)), IL2CPP_TYPE_I4)
        || !Instance(querySchema, 0, IL2CPP_TYPE_VALUETYPE) || !Instance(m.addToggle, 4, IL2CPP_TYPE_VOID)
        || !Type(m.addToggle->parameters[1], IL2CPP_TYPE_STRING) || !Reference(m.addToggle->parameters[0])
        || !Reference(m.addToggle->parameters[2]) || !Reference(m.addToggle->parameters[3])
        || !Type(m.changed->parameters[0], IL2CPP_TYPE_BOOLEAN) || !Reference(m.addGeneral->parameters[0])
        || !m.getContext || !Reference(m.getContext->return_type) || !Instance(m.querySetter, 1, IL2CPP_TYPE_VOID)
        || !Reference(m.querySetter->parameters[0]) || !getter.Initialize(getSchema, GetEnabled)
        || !setter.Initialize(setSchema, SetEnabled) || !query.Initialize(querySchema, QueryState))
      throw std::runtime_error("settings callback schema");
    uiThread = std::this_thread::get_id();
    if (!FleetCommanderConfirmationSetting().SetChangeObserver(RefreshViews))
      throw std::runtime_error("settings observer ownership");
    // A rejected target need not throw. Keep any installed hooks on their native
    // path until the complete adapter is ready; do not retry a partial install.
    if (!SPUD_STATIC_DETOUR(m.refresh->methodPointer, RefreshHook)
        || !SPUD_STATIC_DETOUR(m.changed->methodPointer, ChangedHook)
        || !SPUD_STATIC_DETOUR(m.release->methodPointer, ReleaseHook)
        || !SPUD_STATIC_DETOUR(m.reload->methodPointer, ReloadHook)
        || !SPUD_STATIC_DETOUR(m.session->methodPointer, SessionHook)
        || !SPUD_STATIC_DETOUR(m.load->methodPointer, LoadHook)
        || !SPUD_STATIC_DETOUR(m.addGeneral->methodPointer, AddGeneralHook))
      throw std::runtime_error("settings hook installation");
    active = true;
    spdlog::info("[ModSettings] Native FC confirmation adapter installed (Windows x64)");
  } catch (...) {
    Warn();
  }
}
#else
void InstallModConfirmationSettings() {}
#endif
