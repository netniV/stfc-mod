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

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <list>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
struct TrackedObjectRecord {
  Il2CppGCHandle            weak_handle = nullptr;
  std::vector<Il2CppClass*> classes;
};

static constexpr size_t kObjectTrackerMaxClassWalkDepth = 64;

std::mutex                                                          tracked_objects_mutex;
std::list<TrackedObjectRecord>                                      tracked_objects;
std::unordered_map<Il2CppClass*, std::vector<TrackedObjectRecord*>> tracked_objects_by_class;

Il2CppClass* NormalizeClassPointer(Il2CppClass* klass)
{ return reinterpret_cast<Il2CppClass*>(reinterpret_cast<size_t>(klass) & ~size_t{1}); }

const char* SafeClassName(Il2CppClass* klass)
{
  klass = NormalizeClassPointer(klass);
  if (!klass || !klass->name) {
    return "<unknown>";
  }

  return klass->name;
}

void AddClassReferencesLocked(TrackedObjectRecord& record, Il2CppClass* klass)
{
  std::unordered_set<Il2CppClass*> visited;
  size_t                           depth = 0;

  while (auto* normalized = NormalizeClassPointer(klass)) {
    if (depth++ >= kObjectTrackerMaxClassWalkDepth) {
      spdlog::warn("Object tracker stopped tracking parent classes after {} levels", kObjectTrackerMaxClassWalkDepth);
      return;
    }

    if (!visited.emplace(normalized).second) {
      spdlog::warn("Object tracker detected a class hierarchy cycle at {}", SafeClassName(normalized));
      return;
    }

    if (std::find(record.classes.begin(), record.classes.end(), normalized) == record.classes.end()) {
      record.classes.emplace_back(normalized);
      tracked_objects_by_class[normalized].emplace_back(&record);
    }

    klass = normalized->parent;
  }
}

void EraseClassReferencesLocked(TrackedObjectRecord& record)
{
  for (auto* klass : record.classes) {
    auto found = tracked_objects_by_class.find(klass);
    if (found == tracked_objects_by_class.end()) {
      continue;
    }

    auto& records = found->second;
    records.erase(std::remove(records.begin(), records.end(), &record), records.end());
    if (records.empty()) {
      tracked_objects_by_class.erase(found);
    }
  }
}

std::list<TrackedObjectRecord>::iterator EraseTrackedObjectLocked(std::list<TrackedObjectRecord>::iterator record)
{
  EraseClassReferencesLocked(*record);
  if (record->weak_handle) {
    il2cpp_gchandle_free(record->weak_handle);
  }
  return tracked_objects.erase(record);
}

void PruneDeadObjectsLocked()
{
  for (auto record = tracked_objects.begin(); record != tracked_objects.end();) {
    if (!record->weak_handle || !il2cpp_gchandle_get_target(record->weak_handle)) {
      record = EraseTrackedObjectLocked(record);
    } else {
      ++record;
    }
  }
}

TrackedObjectRecord* FindTrackedObjectLocked(Il2CppObject* object)
{
  for (auto& record : tracked_objects) {
    if (record.weak_handle && il2cpp_gchandle_get_target(record.weak_handle) == object) {
      return &record;
    }
  }
  return nullptr;
}

void RemoveTrackedObjectLocked(Il2CppObject* object)
{
  for (auto record = tracked_objects.begin(); record != tracked_objects.end(); ++record) {
    if (record->weak_handle && il2cpp_gchandle_get_target(record->weak_handle) == object) {
      EraseTrackedObjectLocked(record);
      return;
    }
  }
}
} // namespace

namespace object_tracker
{
ObjectLeaseHandle AcquireLatest(Il2CppClass* klass)
{
  klass = NormalizeClassPointer(klass);
  if (!klass) {
    return {};
  }

  std::scoped_lock lock{tracked_objects_mutex};
  PruneDeadObjectsLocked();

  const auto found = tracked_objects_by_class.find(klass);
  if (found == tracked_objects_by_class.end()) {
    return {};
  }

  for (auto record = found->second.rbegin(); record != found->second.rend(); ++record) {
    auto* target = il2cpp_gchandle_get_target((*record)->weak_handle);
    if (target) {
      auto handle = il2cpp_gchandle_new(target, false);
      if (handle) {
        return ObjectLeaseHandle{handle};
      }
    }
  }

  return {};
}

std::vector<ObjectLeaseHandle> AcquireAll(Il2CppClass* klass)
{
  std::vector<ObjectLeaseHandle> leases;

  klass = NormalizeClassPointer(klass);
  if (!klass) {
    return leases;
  }

  std::scoped_lock lock{tracked_objects_mutex};
  PruneDeadObjectsLocked();

  const auto found = tracked_objects_by_class.find(klass);
  if (found == tracked_objects_by_class.end()) {
    return leases;
  }

  leases.reserve(found->second.size());
  for (auto* record : found->second) {
    auto* target = il2cpp_gchandle_get_target(record->weak_handle);
    if (target) {
      auto handle = il2cpp_gchandle_new(target, false);
      if (handle) {
        leases.emplace_back(handle);
      }
    }
  }

  return leases;
}
} // namespace object_tracker

void* track_ctor(auto original, void* _this)
{
  auto result = original(_this);
  if (!_this) {
    return result;
  }

  auto* object = reinterpret_cast<Il2CppObject*>(_this);
  if (!object->klass) {
    return result;
  }

  std::scoped_lock lock{tracked_objects_mutex};
  PruneDeadObjectsLocked();

  if (auto* existing = FindTrackedObjectLocked(object)) {
    AddClassReferencesLocked(*existing, object->klass);
    return result;
  }

  auto weak_handle = il2cpp_gchandle_new_weakref(object, false);
  if (!weak_handle) {
    spdlog::warn("Object tracker could not create a weak GC handle for {}({})", _this, SafeClassName(object->klass));
    return result;
  }

  spdlog::trace("Tracking {}({})", _this, SafeClassName(object->klass));
  tracked_objects.emplace_back(TrackedObjectRecord{weak_handle});
  AddClassReferencesLocked(tracked_objects.back(), object->klass);
  return result;
}

void track_destroy(auto original, Il2CppObject* _this, uint64_t a2, uint64_t a3)
{
  if (_this) {
    std::scoped_lock lock{tracked_objects_mutex};
    spdlog::trace("Clearing {}({})", static_cast<void*>(_this), SafeClassName(_this->klass));
    RemoveTrackedObjectLocked(_this);
  }

  return original(_this, a2, a3);
}

static std::unordered_set<void*> seen_ctor;
static std::unordered_set<void*> seen_destroy;

template <typename T> void TrackObject()
{
  auto& object_class = T::get_class_helper();
  auto  ctor         = object_class.GetMethod(".ctor");
  auto  on_destroy   = object_class.GetMethod("OnDestroy");

  if (ctor && seen_ctor.emplace(ctor).second) {
    SPUD_STATIC_DETOUR(ctor, track_ctor);
  }

  if (on_destroy && seen_destroy.emplace(on_destroy).second) {
    SPUD_STATIC_DETOUR(on_destroy, track_destroy);
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

  spdlog::info("Object tracker: using weak IL2CPP GC handles with leased snapshots");
}
