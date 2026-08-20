#include <il2cpp/il2cpp_helper.h>

#include "prime/AllianceStarbaseObjectViewerWidget.h"
#include "prime/AnimatedRewardsScreenViewController.h"
#include "prime/ArmadaObjectViewerWidget.h"
#include "prime/ArtifactHallDetailsViewController.h"
#include "prime/AssignShipsWidget.h"
#include "prime/CelestialObjectViewerWidget.h"
#include "prime/ElementSelectorViewController.h"
#include "prime/EmbassyObjectViewer.h"
#include "prime/FleetBarViewController.h"
#include "prime/FleetMeshSelector.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/HousingObjectViewerWidget.h"
#include "prime/InventoryListViewController.h"
#include "prime/MiningObjectViewerWidget.h"
#include "prime/MissionsObjectViewerWidget.h"
#include "prime/NavigationInteractionUIViewController.h"
#include "prime/OfficerAssignmentViewController.h"
#include "prime/PreScanTargetWidget.h"
#include "prime/StarNodeObjectViewerWidget.h"

#include <EASTL/unordered_map.h>
#include <EASTL/unordered_set.h>
#include <EASTL/vector.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#include <spud/signature.h>

#include <mutex>
#include <unordered_map>

std::mutex                                                   tracked_objects_mutex;
eastl::unordered_map<Il2CppClass*, eastl::vector<uintptr_t>> tracked_objects;

using FinalizerCallback = void (*)(void* object, void* client_data);

struct PreviousFinalizer {
  FinalizerCallback callback = nullptr;
  void*             data     = nullptr;
};

static constexpr size_t                             kObjectTrackerMaxClassWalkDepth = 64;
static std::unordered_map<void*, PreviousFinalizer> previous_finalizers;

void (*GC_register_finalizer_inner)(unsigned __int64 obj, void (*fn)(void*, void*), void* cd,
                                    void (**ofn)(void*, void*), void** ocd) = nullptr;

static Il2CppClass* NormalizeClassPointer(Il2CppClass* klass)
{ return reinterpret_cast<Il2CppClass*>(reinterpret_cast<size_t>(klass) & ~size_t{1}); }

static const char* SafeClassName(Il2CppClass* klass)
{
  klass = NormalizeClassPointer(klass);
  if (!klass || !klass->name) {
    return "<unknown>";
  }

  return klass->name;
}

static PreviousFinalizer take_previous_finalizer_locked(void* object)
{
  if (const auto found = previous_finalizers.find(object); found != previous_finalizers.end()) {
    auto previous = found->second;
    previous_finalizers.erase(found);
    return previous;
  }

  return {};
}

static void restore_previous_finalizer(void* object, const PreviousFinalizer& previous)
{
  if (!object || !previous.callback || !GC_register_finalizer_inner) {
    return;
  }

  FinalizerCallback ignoredCallback = nullptr;
  void*             ignoredData     = nullptr;
  GC_register_finalizer_inner(reinterpret_cast<uintptr_t>(object), previous.callback, previous.data, &ignoredCallback,
                              &ignoredData);
}

void add_to_tracking_recursive(Il2CppClass* klass, void* _this)
{
  eastl::unordered_set<Il2CppClass*> visited;
  size_t                             depth = 0;

  while (auto* normalized = NormalizeClassPointer(klass)) {
    if (depth++ >= kObjectTrackerMaxClassWalkDepth) {
      spdlog::warn("Object tracker stopped tracking parent classes for {} after {} levels", _this,
                   kObjectTrackerMaxClassWalkDepth);
      return;
    }

    if (visited.find(normalized) != visited.end()) {
      spdlog::warn("Object tracker detected a class hierarchy cycle at {} while tracking {}", SafeClassName(normalized),
                   _this);
      return;
    }

    visited.emplace(normalized);
    auto& tracked_object_vector = tracked_objects[normalized];
    tracked_object_vector.emplace_back(uintptr_t(_this));

    klass = normalized->parent;
  }
}

void remove_from_tracking_all(void* _this)
{
  for (auto& [klass, tracked_object_vector] : tracked_objects) {
    (void)klass;
    for (auto iter = tracked_object_vector.begin(); iter != tracked_object_vector.end();) {
      if (*iter == uintptr_t(_this)) {
        iter = tracked_object_vector.erase(iter);
      } else {
        ++iter;
      }
    }
  }
}

void remove_from_tracking_recursive(Il2CppClass* klass, void* _this)
{
  eastl::unordered_set<Il2CppClass*> visited;
  size_t                             depth = 0;

  while (auto* normalized = NormalizeClassPointer(klass)) {
    if (depth++ >= kObjectTrackerMaxClassWalkDepth) {
      spdlog::warn("Object tracker stopped removing parent classes for {} after {} levels", _this,
                   kObjectTrackerMaxClassWalkDepth);
      return;
    }

    if (visited.find(normalized) != visited.end()) {
      spdlog::warn("Object tracker detected a class hierarchy cycle at {} while removing {}", SafeClassName(normalized),
                   _this);
      return;
    }

    visited.emplace(normalized);
    if (auto found = tracked_objects.find(normalized); found != tracked_objects.end()) {
      auto& objects = found->second;
      for (auto iter = objects.begin(); iter != objects.end();) {
        if (*iter == uintptr_t(_this)) {
          iter = objects.erase(iter);
        } else {
          ++iter;
        }
      }
    }

    klass = normalized->parent;
  }
}

void track_finalizer(void* _this, void*)
{
  PreviousFinalizer previous{};

  {
    std::scoped_lock lk{tracked_objects_mutex};
    const auto*      object = reinterpret_cast<Il2CppObject*>(_this);
    spdlog::trace("Clearing {}({})", _this, object ? SafeClassName(object->klass) : "<null>");
    remove_from_tracking_all(_this);
    previous = take_previous_finalizer_locked(_this);
  }

  if (previous.callback) {
    previous.callback(_this, previous.data);
  }
}

void* track_ctor(auto original, void* _this)
{
  auto obj = original(_this);
  if (_this == nullptr) {
    return _this;
  }

  auto cls = (Il2CppObject*)_this;
  if (cls->klass == nullptr) {
    return obj;
  }

  std::scoped_lock lk{tracked_objects_mutex};
  spdlog::trace("Tracking {}({})", _this, SafeClassName(cls->klass));
  if (GC_register_finalizer_inner != nullptr) {
    FinalizerCallback oldCallback = nullptr;
    void*             oldData     = nullptr;
    GC_register_finalizer_inner(reinterpret_cast<uintptr_t>(_this), track_finalizer, nullptr, &oldCallback, &oldData);
    if (oldCallback && oldCallback != track_finalizer) {
      spdlog::warn("Object tracker is chaining existing GC finalizer for {}({})", _this, SafeClassName(cls->klass));
      previous_finalizers[_this] = PreviousFinalizer{oldCallback, oldData};
    } else if (!oldCallback) {
      previous_finalizers.erase(_this);
    }
  }
  add_to_tracking_recursive(cls->klass, _this);
  return obj;
}

void track_destroy(auto original, Il2CppObject* _this, uint64_t a2, uint64_t a3)
{
  PreviousFinalizer previous{};
  if (_this != nullptr) {
    std::scoped_lock lk{tracked_objects_mutex};
    spdlog::trace("Clearing {}({})", (void*)_this, SafeClassName(_this->klass));
    remove_from_tracking_all(_this);
    previous = take_previous_finalizer_locked(_this);
  }
  restore_previous_finalizer(_this, previous);
  return original(_this, a2, a3);
}

void track_free(auto original, void* _this)
{
  PreviousFinalizer previous{};
  if (_this != nullptr) {
    std::scoped_lock lk{tracked_objects_mutex};
    remove_from_tracking_all(_this);
    previous = take_previous_finalizer_locked(_this);
  }

  restore_previous_finalizer(_this, previous);
  return original(_this);
}

void calc_liveness_hook(auto original, void* state)
{
  original(state);

  std::scoped_lock                                    lk{tracked_objects_mutex};
  eastl::vector<eastl::pair<Il2CppClass*, uintptr_t>> objects_to_free;
  eastl::unordered_set<uintptr_t>                     objects_seen;
#define IS_MARKED(obj) (((size_t)(obj)->klass) & (size_t)1)
  for (auto& [klass, objects] : tracked_objects) {
    for (auto object : objects) {
      if (IS_MARKED((Il2CppObject*)object) && objects_seen.find(object) == objects_seen.end()) {
        objects_to_free.emplace_back(klass, object);
        objects_seen.emplace(object);
      }
    }
  }

#undef IS_MARKED

  for (auto& [klass, object] : objects_to_free) {
    spdlog::trace("Clearing {}({})", (void*)object, SafeClassName(klass));
    remove_from_tracking_all((void*)object);
  }
}

static eastl::unordered_set<void*> seen_ctor;
static eastl::unordered_set<void*> seen_destroy;

template <typename T> void TrackObject()
{
  auto& object_class = T::get_class_helper();
  auto  ctor         = object_class.GetMethod(".ctor");
  auto  on_destroy   = object_class.GetMethod("OnDestroy");
  if (seen_ctor.find(ctor) == seen_ctor.end()) {
    SPUD_STATIC_DETOUR(ctor, track_ctor);
    seen_ctor.emplace(ctor);
  }

  if (seen_destroy.find(on_destroy) == seen_destroy.end()) {
    SPUD_STATIC_DETOUR(on_destroy, track_destroy);
    seen_destroy.emplace(on_destroy);
  }
}

void InstallObjectTrackers()
{
  TrackObject<PreScanTargetWidget>();
  TrackObject<FleetBarViewController>();
  TrackObject<FleetMeshSelector>();
  TrackObject<AllianceStarbaseObjectViewerWidget>();
  TrackObject<AnimatedRewardsScreenViewController>();
  TrackObject<ArmadaObjectViewerWidget>();
  TrackObject<ArtifactHallDetailsViewController>();
  TrackObject<AssignShipsWidget>();
  TrackObject<CelestialObjectViewerWidget>();
  TrackObject<EmbassyObjectViewer>();
  TrackObject<FullScreenChatViewController>();
  TrackObject<HousingObjectViewerWidget>();
  TrackObject<InventoryListViewController>();
  TrackObject<MiningObjectViewerWidget>();
  TrackObject<MissionsObjectViewerWidget>();
  TrackObject<NavigationInteractionUIViewController>();
  TrackObject<OfficerAssignmentViewController>();
  TrackObject<ElementSelectorViewController>();
  TrackObject<StarNodeObjectViewerWidget>();

  SPUD_STATIC_DETOUR(il2cpp_unity_liveness_finalize, calc_liveness_hook);

#if defined(__APPLE__) && defined(SPUD_ARCH_ARM64)
  // This private Boehm entry point replaces an object's existing finalizer. Avoid
  // touching that state on Apple Silicon, where forced collection can expose a
  // null/stale finalizer callback. The hooks above retain two independent cleanup paths.
  spdlog::warn("Object tracker: native per-object GC finalizers disabled on macOS ARM64; using OnDestroy and Unity "
               "liveness cleanup");
  return;
#endif

#if _WIN32
  auto GC_register_finalizer_inner_matches =
      spud::find_in_module("40 56 57 41 57 48 83 EC ? 83 3D", "GameAssembly.dll");
#else
#if SPUD_ARCH_ARM64
  auto GC_register_finalizer_inner_matches = spud::find_in_module(
      "FF ? 02 D1 FC 6F ? A9 FA 67 ? A9 F8 5F ? A9 F6 57 ? A9 F4 4F ? A9 FD 7B ? A9 FD ? 02 91 E4 0F ? A9",
      "GameAssembly.dylib");
#else
  auto GC_register_finalizer_inner_matches = spud::find_in_module(
      "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC ? 4C 89 45 ? 48 89 4D ? 83 3D", "GameAssembly.dylib");
#endif
#endif

  if (GC_register_finalizer_inner_matches.size() == 0) {
    spdlog::warn("Unable to resolve GC_register_finalizer_inner; object finalizers disabled");
    return;
  }

  const auto GC_register_finalizer_inner_match = GC_register_finalizer_inner_matches.get(0);
  GC_register_finalizer_inner = (decltype(GC_register_finalizer_inner))GC_register_finalizer_inner_match.address();
}
