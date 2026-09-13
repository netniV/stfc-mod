#if defined(_WIN32) && defined(_M_X64)
#include "page_navigation.h"
#include "patches/parts/fc_confirmation_reset.h"
#include "settings/forbidden_tech.h"
#include "settings/native_boolean_callback.h"
#include "value_widget_record.h"
#include <cstdlib>
#include <cstring>
#include <deque>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#include <thread>

namespace mod_settings::native
{
namespace
{
  constexpr const char*       CategoryKey = "game_settings_category_7";
  bool                        active = false, installing = false;
  std::thread::id             uiThread;
  NativeCallback<bool>        getter;
  NativeCallback<void, bool>  setter;
  NativeCallback<int>         query, selectionGetter;
  NativeCallback<void, int>   selectionSetter;
  NativeCallback<float>       sliderGetter;
  NativeCallback<void, float> sliderSetter;
  bool                        selectionActive = false, sliderActive = false;
} // namespace
bool SelectionActive()
{ return selectionActive; }
bool SliderActive()
{ return sliderActive; }
const MethodInfo* QueryMethod()
{ return query.method(); }
BooleanSetting* SettingFor(Il2CppObject* context)
{
  auto& fc = FleetCommanderConfirmationSetting();
  if (HasLabel(context, fc.id().c_str()))
    return &fc;
  auto& ft = ForbiddenTechConfirmationSetting();
  if (HasLabel(context, ft.id().c_str()))
    return &ft;
  for (const auto& page : Pages())
    for (auto* setting : page.Controls<BooleanSetting>())
      if (HasLabel(context, setting->id().c_str()))
        return setting;
  return nullptr;
}
ValueWidgetMetadata& ToggleMeta()
{
  static ValueWidgetMetadata metadata;
  return metadata;
}
ValueWidgetMetadata& SelectionMeta()
{
  static ValueWidgetMetadata metadata(true);
  return metadata;
}
ValueWidgetMetadata& SliderMeta()
{
  static ValueWidgetMetadata metadata(false, true);
  return metadata;
}
ValueWidgetMetadata& WidgetMeta(Il2CppObject* widget)
{
  if (sliderActive && widget && widget->klass == SliderMeta().widget.get_cls())
    return SliderMeta();
  return selectionActive && widget && widget->klass == SelectionMeta().widget.get_cls() ? SelectionMeta()
                                                                                        : ToggleMeta();
}
std::pair<ChoiceSetting*, int> ChoiceFor(Il2CppObject* context)
{
  for (const auto& page : Pages())
    for (auto* choice : page.Controls<ChoiceSetting>())
      for (int i = 0; i < static_cast<int>(choice->labels().size()); ++i)
        if (HasLabel(context, choice->item_id(i).c_str()))
          return {choice, i};
  return {nullptr, 0};
}

SliderSetting* SliderFor(Il2CppObject* context)
{
  for (const auto& page : Pages())
    for (auto* setting : page.Controls<SliderSetting>())
      if (HasLabel(context, setting->state().id().c_str()))
        return setting;
  return nullptr;
}
std::deque<ValueWidget>& ValueViews()
{
  static std::deque<ValueWidget> views(8);
  return views;
}
ValueWidget* renderingView = nullptr;
ValueWidget* bindingView   = nullptr;
void         Restore(ValueWidget& view)
{
  RestoreTint(view.tint);
  if (view.disabled) {
    if (auto* control = Target(view.selectionControl)) {
      void* args[] = {&view.interactableBefore};
      Call(control, "set_interactable", 1, args);
    }
    view.disabled = false;
  }
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
void Clear(ValueWidget& view)
{
  if (view.clearing)
    return;
  struct Scope {
    ValueWidget& view;
    explicit Scope(ValueWidget& view)
        : view(view)
    { view.clearing = true; }
    ~Scope()
    { view.clearing = false; }
  } scope(view);
  try {
    Restore(view);
  } catch (...) {
    Warn();
  }
  // Restoring interactability synchronously calls DoStateTransition. Suppress
  // presentation reentry until every override is restored and the slot detached.
  ClearChoiceStyle(view);
  Free(view.widget);
  Free(view.context);
  Free(view.label);
  Free(view.selectionControl);
  for (auto& handle : view.indicators)
    Free(handle);
  if (view.state)
    view.state->Unbind();
  // Keep this object alive through reentrant release during read/write.
  // Track replaces it only after its rendering/request scope has returned.
  view.overridden = view.hidden = view.disabled = view.preserveNextRefresh = false;
}
ValueWidget* FindValueWidget(Il2CppObject* widget)
{
  for (auto& view : ValueViews())
    if (Target(view.widget) == widget)
      return &view;
  return nullptr;
}
bool OwnsValueContext(Il2CppObject* context)
{
  if (!context)
    return false;
  const bool selection = selectionActive && context->klass == SelectionMeta().row.get_cls();
  const bool slider    = sliderActive && context->klass == SliderMeta().row.get_cls();
  if (!selection && !slider && context->klass != ToggleMeta().row.get_cls())
    return false;
  auto& meta     = slider ? SliderMeta() : selection ? SelectionMeta() : ToggleMeta();
  auto* callback = reinterpret_cast<Il2CppDelegate*>(ReadField(context, meta.queryField));
  return callback && callback->method == query.method() && callback->method_ptr == query.method()->methodPointer
         && (slider      ? SliderFor(context) != nullptr
             : selection ? ChoiceFor(context).first != nullptr
                         : SettingFor(context) != nullptr);
}
bool ChildOf(Il2CppObject* transform, Il2CppObject* parent)
{
  void* args[] = {parent};
  return Boolean(Call(transform, "IsChildOf", 1, args));
}
ValueWidget& Track(Il2CppObject* widget, Il2CppObject* context)
{
  for (auto& view : ValueViews()) {
    if (Target(view.widget) || view.rendering || view.binding || view.requesting || view.clearing)
      continue;
    Clear(view);
    auto&                        metadata = WidgetMeta(widget);
    Root                         label(ReadField(widget, metadata.labelField));
    Root                         widgetTransform(Call(widget, "get_transform"));
    Root                         labelTransform(Call(label.get(), "get_transform"));
    std::array<Il2CppObject*, 2> indicators{ReadField(widget, metadata.toggleField),
                                            ReadField(widget, metadata.stateField)};
    Root                         first(Call(indicators[0], "get_gameObject"));
    Root                         second(Call(indicators[1], "get_gameObject"));
    indicators = {first.get(), second.get()};
    Root selectionControl(metadata.selection || metadata.slider ? ReadField(widget, metadata.toggleField) : nullptr);
    if (metadata.selection) {
      // Selection prefabs can put the toggle/animator on the entire row. Never
      // hide those containers: unknown selection renders -1 and disables input.
      Root transform(Call(selectionControl.get(), "get_transform"));
      if (!ChildOf(transform.get(), widgetTransform.get()) || !ChildOf(labelTransform.get(), widgetTransform.get()))
        throw std::runtime_error("selection control hierarchy");
      (void)Boolean(Call(selectionControl.get(), "get_interactable"));
    } else {
      // Boolean rows suppress unknown ON/OFF by hiding only detached indicators.
      for (auto* indicator : indicators) {
        Root transform(Call(indicator, "get_transform"));
        if (transform.get() == widgetTransform.get() || !ChildOf(transform.get(), widgetTransform.get())
            || ChildOf(labelTransform.get(), transform.get()))
          throw std::runtime_error("settings indicator hierarchy");
      }
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
      if (metadata.slider) {
        auto* setting = SliderFor(context);
        if (!setting)
          throw std::runtime_error("slider owner missing");
        view.state.emplace(*setting);
      } else if (metadata.selection) {
        auto [setting, index] = ChoiceFor(context);
        if (!setting)
          throw std::runtime_error("selection owner missing");
        view.state.emplace(*setting, index);
      } else {
        auto* setting = SettingFor(context);
        if (!setting)
          throw std::runtime_error("settings owner missing");
        view.state.emplace(*setting);
      }
      view.label = weak(label.get());
      if (metadata.selection || metadata.slider)
        view.selectionControl = weak(selectionControl.get());
      if (!metadata.selection)
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

bool GetEnabled(Il2CppObject*, const MethodInfo*)
{
  // Native bool signatures cannot express unknown. Only the owned render scope
  // consumes this placeholder; its indicators are suppressed when value is empty.
  return renderingView && renderingView->state ? renderingView->state->value().value_or(false) : false;
}
int GetSelected(Il2CppObject*, const MethodInfo*)
{ return renderingView && renderingView->state ? renderingView->state->selected() : -1; }
void  SetSelected(Il2CppObject*, int, const MethodInfo*) {}
float GetNumber(Il2CppObject*, const MethodInfo*)
{ return renderingView && renderingView->state ? renderingView->state->number() : 0.0f; }
void SetNumber(Il2CppObject*, float, const MethodInfo*) {}
void SetEnabled(Il2CppObject*, bool, const MethodInfo*)
{
  // Deliberately inert. Only OnToggleValueChanged with a live view snapshot can
  // authorize a write; rendering and reflection cannot mutate game preferences.
}
int QueryState(Il2CppObject*, const MethodInfo*)
{ return 0; }

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
void AddBooleanRow(Il2CppObject* director, Il2CppObject* context, Il2CppObject* category, BooleanSetting& setting)
{
  if (setting.Observe().state.availability == Availability::Unsupported)
    return;
  Root      children(Call(category, "get_Children"));
  const int before = Count(children.get());
  for (int i = 0; i < before; ++i)
    if (HasLabel(Item(children.get(), i), setting.id().c_str()))
      return;
  if (before == PageCatalog::NativeChildLimit)
    throw std::runtime_error("settings category full");
  auto& m = ToggleMeta();
  Root  get(MakeDelegate(il2cpp_class_from_type(m.addToggle->parameters[2]), director, getter.method()));
  Root  set(MakeDelegate(il2cpp_class_from_type(m.addToggle->parameters[3]), director, setter.method()));
  Root  state(MakeDelegate(il2cpp_class_from_type(m.querySetter->parameters[0]), director, query.method()));
  Root  label(reinterpret_cast<Il2CppObject*>(il2cpp_string_new(setting.id().c_str())));
  void* args[] = {category, label.get(), get.get(), set.get()};
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
    Call(category, "RemoveChild", 1, removeArgs);
    throw;
  }
}

void AddRow(Il2CppObject* director, Il2CppObject* context)
{
  Root root(Call(context, "get_RootOption"));
  int  remaining = 128;
  Root category(Category(root.get(), 0, remaining));
  if (!category.get())
    throw std::runtime_error("settings confirmation category");
  AddBooleanRow(director, context, category.get(), FleetCommanderConfirmationSetting());
  AddBooleanRow(director, context, category.get(), ForbiddenTechConfirmationSetting());
}
void Render(ValueWidget& view, auto original, Il2CppObject* widget)
{
  if (view.rendering)
    return;
  Root boundContext(Target(view.context));
  struct Scope {
    ValueWidget&                 view;
    ValueWidget*                 previous;
    NativeViewState::RenderScope suppress;
    Scope(ValueWidget& view)
        : view(view)
        , previous(renderingView)
        , suppress(*view.state)
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
  if (Target(view.widget) != widget || Target(view.context) != boundContext.get())
    return;
  Root        label(Target(view.label));
  std::string text = view.state->label();
  // The native row has limited label width: "Change not applied; try again" was
  // visibly truncated after "; tr" alongside the FC label. Keep these suffixes
  // short; recheck the full label at supported UI scales when changing wording.
  if (!view.state->known())
    text += " — " + std::string(view.state->unavailableReason());
  else if (view.state->failed())
    text += " — Retry";
  else if (!view.state->enabled() && !view.state->disabledReason().empty())
    text += " — " + std::string(view.state->disabledReason());
  // An enabled slider belongs to the selected mode above it. Use a quiet cyan
  // accent, not the native white selection fill (the slider is not a choice).
  if (WidgetMeta(widget).slider && view.state->enabled()) {
    text = "<color=#A8E5EE>" + text + "</color>";
    TintRow(view.tint, widget, {0.70f, 1.0f, 1.0f, 1.0f});
  }
  Root  message(reinterpret_cast<Il2CppObject*>(il2cpp_string_new(text.c_str())));
  void* args[] = {message.get()};
  Call(label.get(), "OverrideLocalizedText", 1, args);
  view.overridden = true;
  TryStyleChoice(view);
  if (!view.state->enabled()) {
    if (auto* control = Target(view.selectionControl)) {
      view.interactableBefore = Boolean(Call(control, "get_interactable"));
      view.disabled           = true;
      bool  interactable      = false;
      void* controlArgs[]     = {&interactable};
      Call(control, "set_interactable", 1, controlArgs);
      if (WidgetMeta(widget).selection || view.state->known())
        return;
    }
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
bool OnUIThread()
{ return active && std::this_thread::get_id() == uiThread; }

void AddChoiceRows(Il2CppObject* director, Il2CppObject* context, Il2CppObject* parent, ChoiceSetting& setting)
{
  if (!selectionActive)
    return;
  auto&       m   = SelectionMeta();
  const auto* add = m.context.GetMethodInfo("AddSelection", 6);
  Root        children(Call(parent, "get_Children"));
  const int   before = Count(children.get());
  const auto  count  = setting.labels().size();
  if (before + count > PageCatalog::NativeChildLimit)
    throw std::runtime_error("selection category capacity");
  Root values(reinterpret_cast<Il2CppObject*>(
      il2cpp_array_new(il2cpp_class_from_name(il2cpp_get_corlib(), "System", "String"), count)));
  for (std::size_t i = 0; i < count; ++i) {
    Root  value(reinterpret_cast<Il2CppObject*>(il2cpp_string_new(setting.item_id(static_cast<int>(i)).c_str())));
    auto* array = reinterpret_cast<Il2CppArraySize*>(values.get());
    il2cpp_gc_wbarrier_set_field(values.get(), reinterpret_cast<void**>(&array->vector[i]), value.get());
  }
  Root  label(reinterpret_cast<Il2CppObject*>(il2cpp_string_new(setting.state().id().c_str())));
  Root  category(reinterpret_cast<Il2CppObject*>(il2cpp_string_new("")));
  Root  get(MakeDelegate(il2cpp_class_from_type(add->parameters[3]), director, selectionGetter.method()));
  Root  set(MakeDelegate(il2cpp_class_from_type(add->parameters[4]), director, selectionSetter.method()));
  void* args[] = {parent, label.get(), values.get(), get.get(), set.get(), category.get()};
  Invoke(add, context, args);
  if (Count(children.get()) != before + static_cast<int>(count))
    throw std::runtime_error("selection row insertion");
  for (int i = 0; i < static_cast<int>(count); ++i) {
    Root row(Item(children.get(), before + i));
    if (row.get()->klass != m.row.get_cls())
      throw std::runtime_error("selection row class");
    Root index(Call(row.get(), "get_Index"));
    if (!index.get() || !Type(il2cpp_class_get_type(index.get()->klass), IL2CPP_TYPE_I4)
        || *static_cast<int*>(il2cpp_object_unbox(index.get())) != i)
      throw std::runtime_error("selection row index");
    Root  text(Call(row.get(), "get_LabelContext"));
    Root  id(reinterpret_cast<Il2CppObject*>(il2cpp_string_new(setting.item_id(i).c_str())));
    void* labelArgs[] = {id.get()};
    Call(text.get(), "set_Identifier", 1, labelArgs);
    Root  state(MakeDelegate(il2cpp_class_from_type(m.querySetter->parameters[0]), director, query.method()));
    void* stateArgs[] = {state.get()};
    Invoke(m.querySetter, row.get(), stateArgs);
  }
}

void AddSliderRow(Il2CppObject* director, Il2CppObject* context, Il2CppObject* parent, SliderSetting& setting)
{
  if (!sliderActive)
    return;
  auto&       m   = SliderMeta();
  const auto* add = m.context.GetMethodInfo("AddSlider", 9);
  Root        children(Call(parent, "get_Children"));
  const int   before = Count(children.get());
  if (before == PageCatalog::NativeChildLimit)
    throw std::runtime_error("slider category capacity");
  Root  label(reinterpret_cast<Il2CppObject*>(il2cpp_string_new(setting.state().id().c_str())));
  Root  get(MakeDelegate(il2cpp_class_from_type(add->parameters[2]), director, sliderGetter.method()));
  Root  set(MakeDelegate(il2cpp_class_from_type(add->parameters[3]), director, sliderSetter.method()));
  Root  state(MakeDelegate(il2cpp_class_from_type(add->parameters[4]), director, query.method()));
  bool  whole   = false;
  float minimum = setting.minimum(), maximum = setting.maximum();
  void* args[] = {parent, label.get(), get.get(), set.get(), state.get(), nullptr, &whole, &minimum, &maximum};
  Invoke(add, context, args);
  if (Count(children.get()) != before + 1)
    throw std::runtime_error("slider row insertion");
  Root row(Item(children.get(), before));
  if (row.get()->klass != m.row.get_cls() || !HasLabel(row.get(), setting.state().id().c_str()))
    throw std::runtime_error("slider row identity");
  // Native SliderOptionLabelType: Value = 0, Percentage = 1. Speed is a raw
  // number; fractional controls retain the existing percentage presentation.
  int   labelType   = setting.label() == SliderLabel::Value ? 0 : 1;
  void* labelArgs[] = {&labelType};
  Call(row.get(), "set_LabelType", 1, labelArgs);
}

void AddGeneralHook(auto original, Il2CppObject* director, Il2CppObject* context)
{
  original(director, context);
  if (!OnUIThread())
    return;
  try {
    AddRow(director, context);
  } catch (...) {
    Warn();
  }
  try {
    AddPages(director, context);
  } catch (...) {
    Warn();
  }
}
void RefreshHook(auto original, Il2CppObject* widget)
{
  if (!OnUIThread()) {
    original(widget);
    return;
  }
  bool owned = false;
  try {
    Root context(Invoke(WidgetMeta(widget).getContext, widget));
    owned      = OwnsValueContext(context.get());
    auto* view = FindValueWidget(widget);
    if (view && (view->rendering || view->binding || view->clearing))
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
      // Observe calls a feature-owned reader. It may synchronously release this
      // widget and bind another, so protect the slot before calling Bind too.
      struct Binding {
        ValueWidget& view;
        ValueWidget* previous;
        explicit Binding(ValueWidget& view)
            : view(view)
            , previous(bindingView)
        {
          view.binding = true;
          bindingView  = &view;
        }
        ~Binding()
        {
          view.binding = false;
          bindingView  = previous;
        }
      } binding(*view);
      if (!view->preserveNextRefresh)
        view->state->Bind();
      if (Target(view->widget) != widget || Target(view->context) != context.get()) {
        view->state->Unbind();
        return;
      }
      view->preserveNextRefresh = false;
      Render(*view, original, widget);
      return;
    }
  } catch (const std::exception& error) {
    Warn(error.what());
    if (owned) {
      HideUnsupported(widget);
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
void SelectionTransitionHook(auto original, Il2CppObject* control, int state, bool instant)
{
  original(control, state, instant);
  if (!OnUIThread() || !selectionActive)
    return;
  static auto* toggleClass = il2cpp_class_from_type(SelectionMeta().toggleField->type);
  if (!control || control->klass != toggleClass)
    return;
  for (auto& view : ValueViews()) {
    if (Target(view.selectionControl) != control || !view.state)
      continue;
    view.pressed = state == 2; // Selectable.SelectionState.Pressed, not Selected/focus.
    if (view.rendering || view.binding || view.requesting || view.clearing)
      return;
    try {
      Root widget(Target(view.widget));
      Root context(widget.get() ? Invoke(WidgetMeta(widget.get()).getContext, widget.get()) : nullptr);
      if (!context.get() || Target(view.context) != context.get() || !OwnsValueContext(context.get()))
        return;
      struct Scope {
        ValueWidget&                 view;
        NativeViewState::RenderScope suppress;
        Scope(ValueWidget& view)
            : view(view)
            , suppress(*view.state)
        { view.rendering = true; }
        ~Scope()
        { view.rendering = false; }
      } scope(view);
      // On/Off animation may have first published its sprite since the last
      // bind. Learn it on input, never through an Update hook or timer.
      if (!HasPressedChoiceSprite())
        for (auto& sibling : ValueViews())
          if (auto* background = Target(sibling.choiceBackground))
            RememberChoiceSprite(background);
      TryStyleChoice(view);
    } catch (...) {
      Warn("settings selection presentation unavailable");
    }
    return;
  }
}
void RefreshViews()
{
  if (!OnUIThread())
    return;
  for (auto& view : ValueViews()) {
    if (view.requesting || view.rendering || view.binding || view.clearing)
      continue;
    Root widget(Target(view.widget));
    if (!widget.get())
      continue;
    try {
      Root context(Invoke(WidgetMeta(widget.get()).getContext, widget.get()));
      if (Target(view.context) != context.get() || !OwnsValueContext(context.get()))
        continue;
      Invoke(WidgetMeta(widget.get()).refresh, widget.get());
    } catch (...) {
      HideUnsupported(widget.get());
    }
  }
  RefreshConditionalSections();
  RefreshPageSummaries();
}
void ChangeValue(auto original, Il2CppObject* widget, auto desired)
{
  if (!OnUIThread()) {
    original(widget, desired);
    return;
  }
  bool owned = false;
  try {
    Root context(Invoke(WidgetMeta(widget).getContext, widget));
    owned = OwnsValueContext(context.get());
    if (owned) {
      auto* view = FindValueWidget(widget);
      if (!view || view->rendering || view->binding || view->requesting || view->clearing
          || Target(view->context) != context.get())
        return;
      {
        struct RequestScope {
          ValueWidget& view;
          explicit RequestScope(ValueWidget* view)
              : view(*view)
          { view->requesting = true; }
          ~RequestScope()
          { view.requesting = false; }
        } requestScope(view);
        auto result = view->state->Request(desired);
        if (Target(view->widget) != widget || Target(view->context) != context.get())
          return;
        if (result == Outcome::Suppressed || result == Outcome::Busy)
          return;
        // Refresh through the hook once, preserving the write result. A fresh Bind
        // here would erase an unverified outcome merely because a later read works.
        view->preserveNextRefresh = true;
        Invoke(WidgetMeta(widget).refresh, widget);
      }
      RefreshConditionalSections(); // The requesting row may now safely be released/rebound.
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
void ChangedHook(auto original, Il2CppObject* widget, bool desired)
{ ChangeValue(original, widget, desired); }
void SliderChangedHook(auto original, Il2CppObject* widget, float desired)
{ ChangeValue(original, widget, desired); }
void SliderValueLabelHook(auto original, Il2CppObject* widget, float value)
{
  if (OnUIThread() && sliderActive) {
    try {
      auto* view = FindValueWidget(widget);
      Root  context(view ? Invoke(SliderMeta().getContext, widget) : nullptr);
      if (view && view->state && Target(view->context) == context.get() && OwnsValueContext(context.get())
          && view->state->known()) {
        // Unity invokes this label listener separately after the change listener.
        // Its raw drag position can otherwise overwrite the refreshed, snapped
        // value. Format the applied snapshot; never send this rounded value to
        // the slider, the setting owner, or the TOML writer.
        value = view->state->displayNumber();
      }
    } catch (...) {
      Warn("settings slider value label unavailable");
    }
  }
  original(widget, value); // Keep native text formatting/localization and pooling.
}
void ReleaseHook(auto original, Il2CppObject* widget)
{
  if (OnUIThread()) {
    try {
      if (auto* view = FindValueWidget(widget))
        Clear(*view);
    } catch (...) {
      Warn();
    }
  }
  original(widget);
}
void Invalidate()
{
  ClearSectionPage();
  InvalidateFleetCommanderConfirmationSession();
  ForbiddenTechConfirmationSetting().InvalidateSession();
  for (const auto& page : Pages())
    for (auto* setting : page.Controls<SliderSetting>())
      setting->state().InvalidateSession();
  for (const auto& page : Pages())
    for (auto* choice : page.Controls<ChoiceSetting>())
      choice->state().InvalidateSession();
  for (const auto& page : Pages())
    for (auto* setting : page.Controls<BooleanSetting>())
      if (setting != &FleetCommanderConfirmationSetting())
        setting->InvalidateSession();
  for (auto& view : ValueViews()) {
    if (!view.state)
      continue;
    view.state->Invalidate();
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
  if (OnUIThread()) {
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
#ifdef _MODDBG
ValueWidget* writeProbeOuter = nullptr;
bool         ReentryProbeEnabled()
{
  const auto* enabled = std::getenv("STFC_MOD_SETTINGS_NAV_REENTRY_TEST");
  return enabled && std::strcmp(enabled, "1") == 0;
}
void ExerciseReadReentry()
{
  static bool exercised = false;
  if (exercised || !bindingView || !ReentryProbeEnabled())
    return;
  exercised      = true;
  auto* previous = bindingView;
  Root  widget(Target(previous->widget));
  // Exercise the actual release bookkeeping and refresh path inside a reader.
  // Keep the native context bound so this is independent of game navigation.
  Clear(*previous);
  Invoke(WidgetMeta(widget.get()).refresh, widget.get());
  auto* rebound = FindValueWidget(widget.get());
  spdlog::info("[ModSettings] Read reentry fixture: {}",
               rebound && rebound != previous && previous->binding ? "PASS" : "FAIL");
}
void ExerciseNestedWrite()
{
  static bool exercised = false;
  if (exercised || !ReentryProbeEnabled())
    return;
  ValueWidget* outer  = nullptr;
  ValueWidget* nested = nullptr;
  for (auto& view : ValueViews()) {
    if (!Target(view.widget) || !view.state)
      continue;
    if (view.requesting && view.state->id() == "community_mod.test.enabled")
      outer = &view;
    if (view.state->id() == "community_mod.test.nested_write")
      nested = &view;
  }
  if (!outer || !nested)
    return;
  exercised = true;
  struct Scope {
    explicit Scope(ValueWidget* outer)
    { writeProbeOuter = outer; }
    ~Scope()
    { writeProbeOuter = nullptr; }
  } scope(outer);
  Root  widget(Target(nested->widget));
  bool  desired = !nested->state->value().value_or(false);
  void* args[]  = {&desired};
  Invoke(ToggleMeta().changed, widget.get(), args);
}
void RebindOuterWrite()
{
  if (!writeProbeOuter)
    return;
  Root widget(Target(writeProbeOuter->widget));
  Clear(*writeProbeOuter);
  Invoke(WidgetMeta(widget.get()).refresh, widget.get());
  auto* rebound = FindValueWidget(widget.get());
  spdlog::info("[ModSettings] Nested write reentry fixture: {}",
               rebound && rebound != writeProbeOuter && writeProbeOuter->requesting ? "PASS" : "FAIL");
}
#endif

void PrepareValueViews(std::size_t count)
{ ValueViews().resize(std::max(ValueViews().size(), count)); }
bool ValueWidgetsBusy()
{
  for (const auto& view : ValueViews())
    if (view.requesting || view.rendering || view.binding || view.clearing)
      return true;
  return false;
}
void InstallChoiceAndSliderWidgets()
{
  auto& m = PageMeta();
  if (std::any_of(Pages().begin(), Pages().end(),
                  [](const auto& page) { return !page.template Controls<ChoiceSetting>().empty(); })) {
    auto&       selection = SelectionMeta();
    const auto* get       = selection.director.GetMethodInfo("GetQualityOptionSelectedIndex", 0);
    const auto* set       = selection.director.GetMethodInfo("OnQualityOptionSelected", 1);
    const auto* add       = selection.context.GetMethodInfo("AddSelection", 6);
    if (!Instance(get, 0, IL2CPP_TYPE_I4) || !Instance(set, 1, IL2CPP_TYPE_VOID)
        || !Type(set->parameters[0], IL2CPP_TYPE_I4) || !Instance(add, 6, IL2CPP_TYPE_VOID)
        || !Reference(add->parameters[0]) || !Type(add->parameters[1], IL2CPP_TYPE_STRING)
        || !Type(add->parameters[2], IL2CPP_TYPE_SZARRAY) || !Reference(add->parameters[3])
        || !Reference(add->parameters[4]) || !Type(add->parameters[5], IL2CPP_TYPE_STRING)
        || !Instance(selection.querySetter, 1, IL2CPP_TYPE_VOID) || !Reference(selection.querySetter->parameters[0])
        || !selection.getContext || !Reference(selection.getContext->return_type)
        || !Instance(selection.getContext, 0, selection.getContext->return_type->type)
        || !selectionGetter.Initialize(get, GetSelected) || !selectionSetter.Initialize(set, SetSelected))
      throw std::runtime_error("selection callback schema");
    static auto selectable = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Selectable");
    const auto* transition = selectable.GetMethodInfo("DoStateTransition", 2);
    if (!Instance(transition, 2, IL2CPP_TYPE_VOID) || !Type(transition->parameters[0], IL2CPP_TYPE_VALUETYPE)
        || !il2cpp_class_is_enum(il2cpp_class_from_type(transition->parameters[0]))
        || !Type(il2cpp_class_enum_basetype(il2cpp_class_from_type(transition->parameters[0])), IL2CPP_TYPE_I4)
        || !Type(transition->parameters[1], IL2CPP_TYPE_BOOLEAN))
      throw std::runtime_error("selection transition signature");
    const std::array targets{selection.refresh, selection.changed, selection.release, transition};
    for (std::size_t i = 0; i < targets.size(); ++i) {
      if (!Instance(targets[i], i == 3 ? 2 : i == 1 ? 1 : 0, IL2CPP_TYPE_VOID) || !Extent(targets[i]))
        throw std::runtime_error("selection hook metadata/extent");
      if (i == 1 && !Type(targets[i]->parameters[0], IL2CPP_TYPE_BOOLEAN))
        throw std::runtime_error("selection changed signature");
      for (std::size_t j = 0; j < i; ++j)
        if (targets[i]->methodPointer == targets[j]->methodPointer)
          throw std::runtime_error("selection shared hook");
      const auto& core = ToggleMeta();
      for (auto* existing : {core.refresh, core.changed, core.release, core.addGeneral, core.reload, core.session,
                             core.load, m.bind, m.release, m.selected, m.destroyed})
        if (targets[i]->methodPointer == existing->methodPointer)
          throw std::runtime_error("selection hook overlap");
    }
    for (const auto& page : Pages())
      for (auto* choice : page.Controls<ChoiceSetting>())
        if (!choice->state().SetChangeObserver(RefreshViews))
          throw std::runtime_error("selection observer ownership");
    SPUD_STATIC_DETOUR(selection.refresh->methodPointer, RefreshHook);
    SPUD_STATIC_DETOUR(selection.changed->methodPointer, ChangedHook);
    SPUD_STATIC_DETOUR(selection.release->methodPointer, ReleaseHook);
    SPUD_STATIC_DETOUR(transition->methodPointer, SelectionTransitionHook);
    selectionActive = true;
  }
  if (std::any_of(Pages().begin(), Pages().end(),
                  [](const auto& page) { return !page.template Controls<SliderSetting>().empty(); })) {
    auto&       slider    = SliderMeta();
    const auto* get       = slider.director.GetMethodInfo("GetCurrentShadowsIndex", 0);
    const auto* set       = slider.director.GetMethodInfo("OnShadowsSettingChanged", 1);
    const auto* add       = slider.context.GetMethodInfo("AddSlider", 9);
    const auto* labelType = slider.row.GetMethodInfo("set_LabelType", 1);
    if (!Instance(get, 0, IL2CPP_TYPE_R4) || !Instance(set, 1, IL2CPP_TYPE_VOID)
        || !Type(set->parameters[0], IL2CPP_TYPE_R4) || !Instance(add, 9, IL2CPP_TYPE_VOID)
        || !Reference(add->parameters[0]) || !Type(add->parameters[1], IL2CPP_TYPE_STRING)
        || !Reference(add->parameters[2]) || !Reference(add->parameters[3]) || !Reference(add->parameters[4])
        || !Type(add->parameters[5], IL2CPP_TYPE_SZARRAY) || !Type(add->parameters[6], IL2CPP_TYPE_BOOLEAN)
        || !Type(add->parameters[7], IL2CPP_TYPE_R4) || !Type(add->parameters[8], IL2CPP_TYPE_R4)
        || !Instance(labelType, 1, IL2CPP_TYPE_VOID) || !Type(labelType->parameters[0], IL2CPP_TYPE_VALUETYPE)
        || !il2cpp_class_is_enum(il2cpp_class_from_type(labelType->parameters[0]))
        || !Type(il2cpp_class_enum_basetype(il2cpp_class_from_type(labelType->parameters[0])), IL2CPP_TYPE_I4)
        || !slider.getContext || !Reference(slider.getContext->return_type)
        || !Instance(slider.getContext, 0, slider.getContext->return_type->type)
        || !sliderGetter.Initialize(get, GetNumber) || !sliderSetter.Initialize(set, SetNumber))
      throw std::runtime_error("slider callback schema");
    const std::array targets{slider.refresh, slider.changed, slider.release, slider.valueLabel};
    for (std::size_t i = 0; i < targets.size(); ++i) {
      const bool takesValue = i == 1 || i == 3;
      if (!Instance(targets[i], takesValue ? 1 : 0, IL2CPP_TYPE_VOID) || !Extent(targets[i]))
        throw std::runtime_error("slider hook metadata/extent");
      if (takesValue && !Type(targets[i]->parameters[0], IL2CPP_TYPE_R4))
        throw std::runtime_error("slider changed signature");
      for (std::size_t j = 0; j < i; ++j)
        if (targets[i]->methodPointer == targets[j]->methodPointer)
          throw std::runtime_error("slider shared hook");
      const auto& core = ToggleMeta();
      for (auto* existing : {core.refresh, core.changed, core.release, core.addGeneral, core.reload, core.session,
                             core.load, m.bind, m.release, m.selected, m.destroyed})
        if (targets[i]->methodPointer == existing->methodPointer)
          throw std::runtime_error("slider hook overlap");
      if (selectionActive)
        for (auto* existing : {SelectionMeta().refresh, SelectionMeta().changed, SelectionMeta().release})
          if (targets[i]->methodPointer == existing->methodPointer)
            throw std::runtime_error("slider selection overlap");
    }
    for (const auto& page : Pages())
      for (auto* setting : page.Controls<SliderSetting>())
        if (!setting->state().SetChangeObserver(RefreshViews))
          throw std::runtime_error("slider observer ownership");
    SPUD_STATIC_DETOUR(slider.refresh->methodPointer, RefreshHook);
    SPUD_STATIC_DETOUR(slider.changed->methodPointer, SliderChangedHook);
    SPUD_STATIC_DETOUR(slider.release->methodPointer, ReleaseHook);
    if (!SPUD_STATIC_DETOUR(slider.valueLabel->methodPointer, SliderValueLabelHook))
      throw std::runtime_error("slider label hook installation");
    sliderActive = true;
  }
}
bool InstallCoreValueWidgets()
{
  if (active || installing)
    return false;
  installing = true;
  try {
    auto&            m = ToggleMeta();
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
    if (!ForbiddenTechConfirmationSetting().SetChangeObserver(RefreshViews))
      throw std::runtime_error("settings observer ownership");
    SPUD_STATIC_DETOUR(m.refresh->methodPointer, RefreshHook);
    SPUD_STATIC_DETOUR(m.changed->methodPointer, ChangedHook);
    SPUD_STATIC_DETOUR(m.release->methodPointer, ReleaseHook);
    SPUD_STATIC_DETOUR(m.reload->methodPointer, ReloadHook);
    SPUD_STATIC_DETOUR(m.session->methodPointer, SessionHook);
    SPUD_STATIC_DETOUR(m.load->methodPointer, LoadHook);
    SPUD_STATIC_DETOUR(m.addGeneral->methodPointer, AddGeneralHook);
    active = true;
    return true;
  } catch (...) {
    Warn();
  }
  return false;
}

} // namespace mod_settings::native
#endif
