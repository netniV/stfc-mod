#include "errormsg.h"

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>

#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace
{
constexpr float kDedupeDistance = 25.0f;

struct SlotObject
{
  void* klass;
  void* monitor;
  void* unknownFields;
  float x;
  float y;
};

struct TreeNodeView
{
  void*       klass;
  void*       monitor;
  void*       address;
  void*       attributes;
  int64_t     id;
  int64_t     parentNullable[2];
  int32_t     type;
  SlotObject* coords;
};

struct PlanetIconDataView
{
  void*   klass;
  void*   monitor;
  int64_t planet_id;
};

struct CreatedObject
{
  void*   resource;
  float   x;
  float   y;
  void*   game_object;
  int64_t node_id;
};

std::mutex                 s_mutex;
std::vector<CreatedObject> s_created;
std::unordered_set<int64_t> s_deduped_node_ids;

void (*s_set_active)(void* game_object, bool value) = nullptr;
void* (*s_get_game_object)(void* component)         = nullptr;
const MethodInfo* s_get_context                     = nullptr;

void* s_last_created_go = nullptr;

void Deactivate(void* game_object)
{
  if (game_object != nullptr && s_set_active != nullptr) {
    s_set_active(game_object, false);
  }
}

void HandleCreated(const TreeNodeView* node, const void* resource)
{
  if (node == nullptr || node->coords == nullptr || resource == nullptr) {
    s_last_created_go = nullptr;
    return;
  }

  const std::lock_guard<std::mutex> lock(s_mutex);

  for (auto it = s_created.begin(); it != s_created.end(); ++it) {
    if (it->resource != resource) {
      continue;
    }
    const auto dx = node->coords->x - it->x;
    const auto dy = node->coords->y - it->y;
    if (dx * dx + dy * dy < kDedupeDistance * kDedupeDistance) {
      Deactivate(it->game_object);
      s_deduped_node_ids.insert(it->node_id);
      *it = CreatedObject{const_cast<void*>(resource), node->coords->x, node->coords->y, s_last_created_go, node->id};
      s_last_created_go = nullptr;
      return;
    }
  }

  s_created.push_back(
      CreatedObject{const_cast<void*>(resource), node->coords->x, node->coords->y, s_last_created_go, node->id});
  s_last_created_go = nullptr;
}

void PlanetViewUtils_Populate_Hook(auto original, void* _this, void* system, int64_t nodeId, int32_t depth)
{
  {
    const std::lock_guard<std::mutex> lock(s_mutex);
    s_created.clear();
    s_deduped_node_ids.clear();
  }
  original(_this, system, nodeId, depth);
}

void PlanetViewUtils_CreateGameObjectAndBlock_Hook(auto original, void* _this, void** go, void* resource, void** block)
{
  s_last_created_go = nullptr;
  original(_this, go, resource, block);
  s_last_created_go = go != nullptr ? *go : nullptr;
}

void PlanetViewUtils_CreateCelestialBody_Hook(auto original, void* _this, TreeNodeView* node, void* resource,
                                              bool usingDefault)
{
  s_last_created_go = nullptr;
  original(_this, node, resource, usingDefault);
  HandleCreated(node, resource);
}

void PlanetViewUtils_CreateEntity_Hook(auto original, void* _this, TreeNodeView* node, void* resource,
                                       bool usingDefault, Il2CppString* entityName)
{
  s_last_created_go = nullptr;
  original(_this, node, resource, usingDefault, entityName);
  HandleCreated(node, resource);
}

void NavigationPlanetWidget_OnDidBindContext_Hook(auto original, void* _this)
{
  original(_this);

  if (s_get_context == nullptr) {
    return;
  }

  Il2CppException* exc  = nullptr;
  auto*            data = reinterpret_cast<PlanetIconDataView*>(il2cpp_runtime_invoke(s_get_context, _this, nullptr, &exc));
  if (exc != nullptr || data == nullptr) {
    return;
  }

  bool deduped = false;
  {
    const std::lock_guard<std::mutex> lock(s_mutex);
    deduped = s_deduped_node_ids.find(data->planet_id) != s_deduped_node_ids.end();
  }

  if (deduped) {
    Deactivate(s_get_game_object != nullptr ? s_get_game_object(_this) : nullptr);
  }
}
} // namespace

void InstallSystemViewHooks()
{
  static auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  if (game_object_helper.isValidHelper()) {
    s_set_active = game_object_helper.GetMethod<void(void*, bool)>("SetActive", 1);
  }

  static auto component_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
  if (component_helper.isValidHelper()) {
    s_get_game_object = component_helper.GetMethod<void*(void*)>("get_gameObject", 0);
  }

  static auto planet_view_utils =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "PlanetViewUtils");
  if (!planet_view_utils.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Navigation", "PlanetViewUtils");
    return;
  }

  static auto planet_widget_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationPlanetWidget");
  if (!planet_widget_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Navigation", "NavigationPlanetWidget");
    return;
  }

  s_get_context = planet_widget_helper.GetMethodInfo("get_Context", 0);

  if (const auto ptr = planet_view_utils.GetMethod("Populate", 3); ptr != nullptr) {
    SPUD_STATIC_DETOUR(ptr, PlanetViewUtils_Populate_Hook);
  } else {
    ErrorMsg::MissingMethod("PlanetViewUtils", "Populate");
  }

  if (const auto ptr = planet_view_utils.GetMethod("CreateGameObjectAndBlock", 3); ptr != nullptr) {
    SPUD_STATIC_DETOUR(ptr, PlanetViewUtils_CreateGameObjectAndBlock_Hook);
  } else {
    ErrorMsg::MissingMethod("PlanetViewUtils", "CreateGameObjectAndBlock");
  }

  if (const auto ptr = planet_view_utils.GetMethod("CreateCelestialBody", 3); ptr != nullptr) {
    SPUD_STATIC_DETOUR(ptr, PlanetViewUtils_CreateCelestialBody_Hook);
  } else {
    ErrorMsg::MissingMethod("PlanetViewUtils", "CreateCelestialBody");
  }

  if (const auto ptr = planet_view_utils.GetMethod("CreateEntity", 4); ptr != nullptr) {
    SPUD_STATIC_DETOUR(ptr, PlanetViewUtils_CreateEntity_Hook);
  } else {
    ErrorMsg::MissingMethod("PlanetViewUtils", "CreateEntity");
  }

  if (const auto ptr = planet_widget_helper.GetMethod("OnDidBindContext", 0); ptr != nullptr) {
    SPUD_STATIC_DETOUR(ptr, NavigationPlanetWidget_OnDidBindContext_Hook);
  } else {
    ErrorMsg::MissingMethod("NavigationPlanetWidget", "OnDidBindContext");
  }
}
