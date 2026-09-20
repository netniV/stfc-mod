#include "config.h"
#include "patches/screen_update_hook.h"

#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>

namespace
{
// All Unity work runs on ScreenManager's main-thread dispatcher. No additional detours.
struct Vector2 {
  float x, y;
};
struct Color {
  float r, g, b, a;
};

Il2CppClass* Class(const char* assembly, const char* ns, const char* name)
{
  auto* a     = il2cpp_domain_assembly_open(il2cpp_domain_get(), assembly);
  auto* image = a ? il2cpp_assembly_get_image(a) : nullptr;
  return image ? il2cpp_class_from_name(image, ns, name) : nullptr;
}

const MethodInfo* Method(Il2CppClass* cls, const char* name, int count)
{ return cls ? il2cpp_class_get_method_from_name(cls, name, count) : nullptr; }

// Unity has Type/string and generic overloads with the same argument count.
const MethodInfo* TypeMethod(Il2CppClass* cls, const char* name)
{
  void* iter = nullptr;
  while (cls) {
    while (auto* method = il2cpp_class_get_methods(cls, &iter)) {
      if (method->is_generic || std::strcmp(method->name, name) || method->parameters_count != 1)
        continue;
      auto* param = il2cpp_class_from_type(il2cpp_method_get_param(method, 0));
      if (param && !std::strcmp(il2cpp_class_get_namespace(param), "System")
          && !std::strcmp(il2cpp_class_get_name(param), "Type"))
        return method;
    }
    cls  = il2cpp_class_get_parent(cls);
    iter = nullptr;
  }
  return nullptr;
}

bool Invoke(const MethodInfo* method, void* target, void** args, Il2CppObject** result = nullptr)
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

Il2CppObject* Get(Il2CppObject* object, const char* name)
{
  Il2CppObject* result = nullptr;
  if (object)
    Invoke(Method(object->klass, name, 0), object, nullptr, &result);
  return result;
}

template <typename T> bool Value(Il2CppObject* object, const char* name, T& value)
{
  auto*    boxed     = Get(object, name);
  uint32_t alignment = 0;
  if (!boxed || !il2cpp_class_is_valuetype(boxed->klass)
      || il2cpp_class_value_size(boxed->klass, &alignment) != sizeof(T))
    return false;
  std::memcpy(&value, il2cpp_object_unbox(boxed), sizeof(T));
  return true;
}

bool Set(Il2CppObject* object, const char* name, void* value)
{
  void* args[] = {value};
  return object && Invoke(Method(object->klass, name, 1), object, args);
}

Il2CppObject* Field(Il2CppObject* object, const char* name)
{
  auto* field = object ? il2cpp_class_get_field_from_name(object->klass, name) : nullptr;
  if (!field || il2cpp_type_is_byref(il2cpp_field_get_type(field)))
    return nullptr;
  auto* cls = il2cpp_class_from_type(il2cpp_field_get_type(field));
  if (!cls || il2cpp_class_is_valuetype(cls))
    return nullptr;
  Il2CppObject* result = nullptr;
  il2cpp_field_get_value(object, field, &result);
  return result;
}

Il2CppObject* WithType(const MethodInfo* method, Il2CppObject* target, Il2CppClass* type)
{
  if (!type)
    return nullptr;
  void*         args[] = {il2cpp_type_get_object(il2cpp_class_get_type(type))};
  Il2CppObject* result = nullptr;
  Invoke(method, target, args, &result);
  return result;
}

Il2CppClass* UnityObject()
{
  static auto* cls = Class("UnityEngine.CoreModule", "UnityEngine", "Object");
  return cls;
}

bool Alive(Il2CppObject* object)
{
  if (!object)
    return false;
  void*         args[] = {object};
  Il2CppObject* result = nullptr;
  static auto*  method = Method(UnityObject(), "op_Implicit", 1);
  return Invoke(method, nullptr, args, &result) && result && *static_cast<bool*>(il2cpp_object_unbox(result));
}

struct Root {
  Il2CppGCHandle handle{};
  Il2CppObject*  get() const
  { return handle ? il2cpp_gchandle_get_target(handle) : nullptr; }
  void reset(Il2CppObject* value = nullptr)
  {
    if (handle)
      il2cpp_gchandle_free(handle);
    handle = value ? il2cpp_gchandle_new(value, false) : 0;
  }
};

Root navigation, panel, label, anomalyManager;

void Clear()
{
  if (Alive(panel.get())) {
    bool active = false;
    Set(panel.get(), "SetActive", &active);
    void* args[] = {panel.get()};
    Invoke(Method(UnityObject(), "Destroy", 1), nullptr, args);
  }
  label.reset();
  panel.reset();
  navigation.reset();
  anomalyManager.reset();
}

Il2CppObject* Find(Il2CppClass* type)
{
  static auto* method = TypeMethod(UnityObject(), "FindObjectOfType");
  return WithType(method, nullptr, type);
}

Il2CppObject* Component(Il2CppObject* object, Il2CppClass* type)
{ return object ? WithType(TypeMethod(object->klass, "GetComponent"), object, type) : nullptr; }

bool Rect(Il2CppObject* transform, Vector2 anchor, Vector2 pivot, Vector2 size, Vector2 position)
{
  return Set(transform, "set_anchorMin", &anchor) && Set(transform, "set_anchorMax", &anchor)
         && Set(transform, "set_pivot", &pivot) && Set(transform, "set_sizeDelta", &size)
         && Set(transform, "set_anchoredPosition", &position);
}

Il2CppObject* NewObject(const char* name, Il2CppObject* parent, Root& root)
{
  static auto* go = Class("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto* rt = Class("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  if (!go || !rt)
    return nullptr;
  auto* object = il2cpp_object_new(go);
  root.reset(object);
  void* args[] = {il2cpp_string_new(name)};
  if (!Invoke(Method(go, ".ctor", 1), object, args))
    return nullptr;
  bool active = false;
  if (!Set(object, "SetActive", &active))
    return nullptr;
  auto* transform          = WithType(TypeMethod(go, "AddComponent"), object, rt);
  bool  worldPositionStays = false;
  void* parentArgs[]       = {parent, &worldPositionStays};
  if (!transform || !Invoke(Method(transform->klass, "SetParent", 2), transform, parentArgs))
    return nullptr;
  return transform;
}

Il2CppObject* GraphicSprite(Il2CppObject* image)
{
  // ImageSelector applies localized artwork through overrideSprite.
  auto* sprite = Get(image, "get_overrideSprite");
  return sprite ? sprite : Get(image, "get_sprite");
}

Il2CppObject* NativeBackgroundSprite(Il2CppObject* nav)
{
  auto*         button     = Field(nav, "_galacticAnomaliesButtonWidget");
  auto*         parent     = Get(Get(button, "get_transform"), "get_parent");
  void*         args[]     = {il2cpp_string_new("ShortcutKeybindHint/Background")};
  Il2CppObject* background = nullptr;
  if (!parent || !Invoke(Method(parent->klass, "Find", 1), parent, args, &background))
    return nullptr;
  static auto* image = Class("UnityEngine.UI", "UnityEngine.UI", "Image");
  return GraphicSprite(Component(background, image));
}

void Slice(Il2CppObject* image, Il2CppObject* sprite)
{
  if (!image || !sprite)
    return;
  int   sliced       = 1;
  float spritePixels = 100;
  if (!Value(sprite, "get_pixelsPerUnit", spritePixels) || spritePixels <= 0)
    return;
  float multiplier = 100 / spritePixels;
  Set(image, "set_type", &sliced);
  Set(image, "set_pixelsPerUnitMultiplier", &multiplier);
}

Il2CppObject* Decoration(const char* name, Il2CppObject* parent, Vector2 size, Vector2 position, Color color,
                         Il2CppObject* sprite = nullptr)
{
  Root         object;
  auto*        transform  = NewObject(name, parent, object);
  static auto* imageClass = Class("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto*        image =
      object.get() ? WithType(TypeMethod(object.get()->klass, "AddComponent"), object.get(), imageClass) : nullptr;
  bool no = false, yes = true;
  bool ok = transform && image && Rect(transform, {0.5f, 0.5f}, {0.5f, 0.5f}, size, position)
            && Set(image, "set_color", &color) && Set(image, "set_raycastTarget", &no);
  if (sprite)
    ok = ok && Set(image, "set_sprite", sprite);
  ok = ok && Set(object.get(), "SetActive", &yes);
  if (!ok && Alive(object.get())) {
    void* args[] = {object.get()};
    Invoke(Method(UnityObject(), "Destroy", 1), nullptr, args);
  }
  object.reset();
  return ok ? image : nullptr;
}

bool Create(Il2CppObject* nav)
{
  // Keep the native system-name font, but place the timer under the upper-right
  // Help All / Engage / Claim drawer. Parenting to the drawer follows its scale
  // and visibility without changing any native button positions.
  auto* names = Field(Field(nav, "_factionInformationHelper"), "_systemName");
  int   count = 0;
  if (!Value(names, "get_Count", count) || count < 1)
    return false;
  int           index  = 0;
  void*         args[] = {&index};
  Il2CppObject* name   = nullptr;
  if (!Invoke(Method(names->klass, "get_Item", 1), names, args, &name) || !Alive(name))
    return false;
  static auto* drawerClass = Class("Assembly-CSharp", "Digit.Prime.HUD", "HudAllianceAndNewsViewController");
  auto*        drawer      = Find(drawerClass);
  auto*        chest       = Get(Field(drawer, "_chestPromotion"), "get_transform");
  auto*        parent      = Get(chest, "get_parent");
  static auto* tmp         = Class("Unity.TextMeshPro", "TMPro", "TextMeshProUGUI");
  auto*        source      = Component(name, tmp);
  auto*        font        = Get(source, "get_font");
  if (!parent || !font)
    return false;

  auto* transform = NewObject("CommunityMod_AnomalyTimer", parent, panel);
  if (!transform || !Rect(transform, {0.5f, 0}, {0.5f, 1}, {198, 38}, {0, -40}))
    return false;
  static auto* image      = Class("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto*        background = WithType(TypeMethod(panel.get()->klass, "AddComponent"), panel.get(), image);
  Color        border{0.64f, 0.34f, 0.46f, 0.85f};
  bool         no = false;
  if (!background || !Set(background, "set_color", &border) || !Set(background, "set_raycastTarget", &no))
    return false;
  // Use the existing native key-hint silhouette for both layers, with a narrow
  // inset creating the border. A missing sprite still yields a visible frame.
  auto* rounded = NativeBackgroundSprite(nav);
  if (rounded) {
    Set(background, "set_sprite", rounded);
    Slice(background, rounded);
  }
  auto* fill = Decoration("AnomalyFill", transform, {194, 34}, {0, 0}, {0.06f, 0.10f, 0.14f, 0.96f}, rounded);
  if (!fill)
    return false;
  Slice(fill, rounded);

  Root  textObject;
  auto* textTransform = NewObject("Countdown", transform, textObject);
  auto* text =
      WithType(TypeMethod(textObject.get() ? textObject.get()->klass : nullptr, "AddComponent"), textObject.get(), tmp);
  label.reset(text);
  bool  ok        = textTransform && text && Rect(textTransform, {0.5f, 0.5f}, {0.5f, 0.5f}, {190, 28}, {0, 0});
  float fontSize  = 22;
  int   alignment = 0x202; // TMP: Center (514).
  Color color{0.95f, 0.83f, 0.89f, 1};
  ok       = ok && Set(text, "set_font", font) && Set(text, "set_fontSize", &fontSize)
             && Set(text, "set_alignment", &alignment) && Set(text, "set_color", &color)
             && Set(text, "set_raycastTarget", &no) && Set(text, "set_enableWordWrapping", &no);
  bool yes = true;
  ok       = ok && Set(textObject.get(), "SetActive", &yes);
  if (!ok && Alive(textObject.get())) {
    void* destroyArgs[] = {textObject.get()};
    Invoke(Method(UnityObject(), "Destroy", 1), nullptr, destroyArgs);
  }
  textObject.reset(); // The parent hierarchy now owns this child.
  return ok;
}

void Update()
{
  static auto next = std::chrono::steady_clock::time_point{};
  auto        now  = std::chrono::steady_clock::now();
  if (now < next)
    return;
  next = now + std::chrono::milliseconds(250);

  if (!Config::Get().galactic_anomaly_timer) {
    Clear();
    return;
  }
  if (!Alive(navigation.get())) {
    Clear();
    static auto* cls = Class("Assembly-CSharp", "Digit.Prime.Navigation.UI", "HudNavigationViewController");
    navigation.reset(Find(cls));
  }
  auto*        nav   = navigation.get();
  bool         show  = false;
  std::int64_t ticks = 0;
  if (Alive(nav) && Value(nav, "get_isActiveAndEnabled", show) && show && Value(nav, "CanShowGalacticAnomaly", show)
      && show) {
    auto*        address      = Get(Get(nav, "get_CanvasContext"), "get_ViewingAddress");
    std::int64_t system       = -1;
    static auto* managerClass = Class("Assembly-CSharp", "Digit.Prime.GalacticAnomalies", "GalacticAnomaliesManager");
    // Cache the Unity object, not its timer data. Unity's liveness check detects
    // destruction even while the managed wrapper is retained by our GC handle.
    if (!Alive(anomalyManager.get()))
      anomalyManager.reset(Find(managerClass));
    auto*         manager = anomalyManager.get();
    Il2CppObject* anomaly = nullptr;
    void*         args[]  = {&system};
    show = manager && Value(address, "get_System", system) && system > 0
           && Invoke(Method(managerClass, "GetSystemGalacticAnomalies", 1), manager, args, &anomaly)
           && Value(anomaly, "get_IsActive", show) && show
           && Value(Get(anomaly, "get_EndTimeTimerDataContext"), "get_RemainingTime", ticks) && ticks > 0;
  } else {
    show = false;
  }
  if (show && (!Alive(panel.get()) || !Alive(label.get()))) {
    // Release old UI without releasing the current navigation reference.
    auto* current = nav;
    Clear();
    navigation.reset(current);
    if (!Create(current)) {
      static bool warned = false;
      if (!warned) {
        spdlog::warn("Galactic anomaly countdown: HUD creation failed; leaving the native HUD unchanged");
        warned = true;
      }
      Clear();
      next = now + std::chrono::seconds(5);
      return;
    }
  }
  if (!Alive(panel.get()))
    return;
  if (show) {
    auto seconds = ticks / 10000000 + (ticks % 10000000 != 0);
    auto text    = seconds >= 3600 ? fmt::format("{}h {:02}m {:02}s", seconds / 3600, seconds / 60 % 60, seconds % 60)
                                   : fmt::format("{:02}m {:02}s", seconds / 60, seconds % 60);
    show         = Set(label.get(), "set_text", il2cpp_string_new(text.c_str()));
  }
  Set(panel.get(), "SetActive", &show);
}
} // namespace

void InstallGalacticAnomalyTimer()
{
  if (install_screen_manager_update_hook()) {
    register_screen_manager_update_callback(Update);
    spdlog::info("Galactic anomaly countdown enabled (below Help All / Engage / Claim)");
  }
}
