#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <spdlog/spdlog.h>
#include <str_utils.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <utility>

namespace
{
struct RoadDepthApi {
  Il2CppClass      *renderer = nullptr;
  const MethodInfo *find = nullptr, *children = nullptr, *materials = nullptr;
  const MethodInfo *name = nullptr, *shader = nullptr, *has = nullptr, *get = nullptr, *set = nullptr;

  RoadDepthApi()
  {
    auto cls = [](const char *n) {
      return il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", n).get_cls();
    };
    auto *game_object = cls("GameObject"), *material = cls("Material");
    renderer  = cls("Renderer");
    find      = method_contract::Resolve(game_object, "Find", true, "UnityEngine.GameObject", {"System.String"});
    children  = method_contract::Resolve(game_object, "GetComponentsInChildren", false, "UnityEngine.Component[]",
                                         {"System.Type", "System.Boolean"});
    materials = method_contract::Resolve(renderer, "get_sharedMaterials", false, "UnityEngine.Material[]", {});
    name      = method_contract::Resolve(cls("Object"), "get_name", false, "System.String", {});
    shader    = method_contract::Resolve(material, "get_shader", false, "UnityEngine.Shader", {});
    has       = method_contract::Resolve(material, "HasProperty", false, "System.Boolean", {"System.String"});
    get       = method_contract::Resolve(material, "GetFloat", false, "System.Single", {"System.String"});
    set = method_contract::Resolve(material, "SetFloat", false, "System.Void", {"System.String", "System.Single"});
  }

  bool Valid() const
  { return renderer && find && children && materials && name && shader && has && get && set; }
};

Il2CppObject *Invoke(const MethodInfo *method, Il2CppObject *object, void **args = nullptr)
{
  Il2CppObject *result = nullptr;
  return Il2CppRuntime::TryInvoke(method, object, args, &result) ? result : nullptr;
}

std::string Name(RoadDepthApi &api, Il2CppObject *object)
{
  auto *value = object ? reinterpret_cast<Il2CppString *>(Invoke(api.name, object)) : nullptr;
  return value && value->length <= 256 ? to_string(value) : std::string{};
}

bool IsRoad(const std::string &name)
{
  for (const char *prefix :
       {"mat_PB_3101101_asphalt_", "mat_PB_3101201_asphalt_", "mat_PB_3101301_asphalt_", "mat_PB_3101401_asphalt_",
        "mat_PB_3101501_asphalt_", "mat_PB_3101601_asphalt_", "mat_PB_3101701_asphalt_"}) {
    if (name.starts_with(prefix))
      return true;
  }
  return false;
}

bool ReadDepth(RoadDepthApi &api, Il2CppObject *material, Il2CppString *property, float &value)
{
  void *args[]  = {property};
  bool  present = false;
  if (!Il2CppRuntime::TryBoolean(Invoke(api.has, material, args), present) || !present)
    return false;
  auto *boxed = Invoke(api.get, material, args);
  if (!boxed || !method_contract::Type(il2cpp_class_get_type(boxed->klass), "System.Single"))
    return false;
  value = *static_cast<float *>(il2cpp_object_unbox(boxed));
  return std::isfinite(value);
}
} // namespace

// Called only by the qualified, enabled Haven camera path. The small bias stays
// on at every zoom level, keeping nearly coplanar road surfaces above the ground.
// All scene objects are resolved afresh; no Unity pointers or GC handles are retained.
void ApplyHavenRoadDepthBias()
{
  using Clock      = std::chrono::steady_clock;
  static auto next = Clock::time_point{};
  if (Clock::now() < next)
    return;
  next = Clock::now() + std::chrono::seconds(1);
  static RoadDepthApi api;
  if (!api.Valid()) {
    static bool warned = false;
    if (!warned) {
      if (!api.renderer)
        spdlog::warn("[HavenZoom] road depth unavailable: missing UnityEngine.Renderer class");
      for (const auto& [method, signature] :
           std::array<std::pair<const MethodInfo*, const char*>, 8>{{
               {api.find, "GameObject.Find(String) -> GameObject [static]"},
               {api.children, "GameObject.GetComponentsInChildren(Type, Boolean) -> Component[]"},
               {api.materials, "Renderer.get_sharedMaterials() -> Material[]"},
               {api.name, "Object.get_name() -> String"},
               {api.shader, "Material.get_shader() -> Shader"},
               {api.has, "Material.HasProperty(String) -> Boolean"},
               {api.get, "Material.GetFloat(String) -> Single"},
               {api.set, "Material.SetFloat(String, Single) -> Void"}}}) {
        if (!method)
          spdlog::warn("[HavenZoom] keeping native road depth: missing or incompatible UnityEngine.{}", signature);
      }
      warned = true;
    }
    return;
  }
  void *root_args[] = {il2cpp_string_new("StarbaseRoot/StarbaseManager/StarbasePlanetaryVisualsHolder")};
  auto *root        = Invoke(api.find, nullptr, root_args);
  if (!root)
    return;
  bool  include_inactive = true;
  void *child_args[]     = {il2cpp_type_get_object(il2cpp_class_get_type(api.renderer)), &include_inactive};
  auto *renderers        = reinterpret_cast<Il2CppArraySize *>(Invoke(api.children, root, child_args));
  if (!renderers || renderers->max_length > 4096)
    return;
  for (uintptr_t i = 0; i < renderers->max_length; ++i) {
    auto *renderer = reinterpret_cast<Il2CppObject *>(renderers->vector[i]);
    if (!renderer)
      continue;
    auto *materials = reinterpret_cast<Il2CppArraySize *>(Invoke(api.materials, renderer));
    if (!materials || materials->max_length > 16)
      continue;
    for (uintptr_t j = 0; j < materials->max_length; ++j) {
      auto      *material = reinterpret_cast<Il2CppObject *>(materials->vector[j]);
      const auto name     = Name(api, material);
      if (!material || !IsRoad(name) || Name(api, Invoke(api.shader, material)) != "Custom/LitMSAE")
        continue;
      const std::array<Il2CppString *, 2> properties = {il2cpp_string_new("_OffsetFactor"),
                                                        il2cpp_string_new("_OffsetUnits")};
      std::array<float, 2>                before{};
      if (!ReadDepth(api, material, properties[0], before[0]) || !ReadDepth(api, material, properties[1], before[1]))
        continue;
      bool changed = false, verified = true;
      for (unsigned k = 0; k < 2; ++k) {
        // Preserve an already stronger native bias.
        float target = std::min(before[k], -1.0f);
        if (target == before[k])
          continue;
        void *args[] = {properties[k], &target};
        if (!Il2CppRuntime::TryInvoke(api.set, material, args)) {
          verified = false;
          continue;
        }
        changed     = true;
        float after = 0.0f;
        verified    = ReadDepth(api, material, properties[k], after) && after == target && verified;
      }
      if (changed) {
        spdlog::debug("[HavenZoom] road depth bias {}: verified={}", name, verified);
        static bool reported = false;
        if (verified && !reported) {
          spdlog::info("[HavenZoom] corrected road surface depth for expanded Haven zoom");
          reported = true;
        }
      }
    }
  }
}
