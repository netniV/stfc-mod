/**
 * @file action_queue_repair.cc
 * @brief Narrow Kir'Shara queue completion repair.
 *
 * Repairs the observed off-screen queue stall by replaying the target-id ProcessQueue completion seam only when a
 * recently planned course target is still present in the same fleet's live queue. The repair intentionally installs
 * only the completion hooks in this file.
 */
#include "config.h"
#include "errormsg.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <prime/ActionQueueManager.h>
#include <prime/FleetDeployedData.h>
#include <prime/FleetPlayerData.h>
#include <prime/FleetsManager.h>
#include <prime/IList.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>

namespace
{
struct CourseTargetDeployedFleetId {
  bool         present = false;
  std::int64_t value   = 0;
};

struct CourseTargetCompletionCandidate {
  std::int64_t target_id     = 0;
  std::int64_t updated_at_ms = 0;
  bool         consumed      = false;
};

struct PlayerOnlyBattleStartCandidate {
  std::int64_t deployed_id   = 0;
  std::int64_t updated_at_ms = 0;
};

struct FleetDeployedDataSnapshot {
  bool         present             = false;
  std::int64_t id                  = 0;
  int          state               = -1;
  int          previous_state      = -1;
  int          type                = -1;
  bool         destroyed           = false;
  bool         currently_battling  = false;
  bool         player_combat_start = false;
};

struct CourseTargetQueueGuard {
  bool  relevant     = false;
  int   queue_count  = -1;
  int   target_index = -1;
  void* instance     = nullptr;
};

struct CourseTargetCompletionSynthesis {
  bool         should_synthesize = false;
  std::int64_t fleet_key         = 0;
  std::int64_t deployed_id       = 0;
  std::int64_t target_id         = 0;
  std::int64_t course_age_ms     = 0;
  std::int64_t battle_age_ms     = 0;
  int          queue_count       = -1;
  int          target_index      = -1;
};

using ProcessQueueTargetMethod = void(ActionQueueManager*, std::int64_t, bool);

bool RepairEnabled()
{
  const auto& config = Config::Get();
  return config.queue_enabled && config.kirshara_queue_repair;
}

std::int64_t NowMs()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::int64_t AgeMs(std::int64_t now, std::int64_t previous)
{ return now >= previous ? now - previous : previous - now; }

std::mutex& CourseTargetCompletionMutex()
{
  static std::mutex mutex;
  return mutex;
}

std::unordered_map<std::int64_t, CourseTargetCompletionCandidate>& CourseTargetCompletionTargets()
{
  static std::unordered_map<std::int64_t, CourseTargetCompletionCandidate> targets;
  return targets;
}

std::unordered_map<std::int64_t, PlayerOnlyBattleStartCandidate>& CourseTargetCompletionBattleStarts()
{
  static std::unordered_map<std::int64_t, PlayerOnlyBattleStartCandidate> starts;
  return starts;
}

FleetDeployedDataSnapshot SnapshotFleetDeployedData(FleetDeployedData* deployed_data)
{
  FleetDeployedDataSnapshot snapshot;
  if (!deployed_data) {
    return snapshot;
  }

  snapshot.present             = true;
  snapshot.id                  = deployed_data->ID;
  snapshot.state               = deployed_data->CurrentState;
  snapshot.previous_state      = deployed_data->PreviousState;
  snapshot.type                = static_cast<int>(deployed_data->FleetType);
  snapshot.destroyed           = deployed_data->IsDestroyed;
  snapshot.currently_battling  = deployed_data->CurrentlyBattling;
  snapshot.player_combat_start = snapshot.type == static_cast<int>(DeployedFleetType::Player) && snapshot.state == 6
                                 && (snapshot.previous_state == 0 || snapshot.previous_state == 1)
                                 && snapshot.currently_battling && !snapshot.destroyed;
  return snapshot;
}

CourseTargetDeployedFleetId ReadCourseDataTargetDeployedFleetId(void* course_data)
{
  if (!course_data) {
    return {};
  }

  static auto class_helper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "CourseData");
  if (!class_helper.isValidHelper()) {
    return {};
  }

  static auto field_offset = []() -> ptrdiff_t {
    auto field = class_helper.GetField("<TargetDeployedFleetId>k__BackingField");
    return field.isValidHelper() ? field.offset() : -1;
  }();
  if (field_offset < 0) {
    return {};
  }

  auto*      nullable = reinterpret_cast<const char*>(course_data) + field_offset;
  const auto present  = *reinterpret_cast<const bool*>(nullable);
  const auto value    = present ? *reinterpret_cast<const std::int64_t*>(nullable + 8) : 0;
  return {present, value};
}

FleetPlayerData* FindPlayerFleetDataById(std::int64_t fleet_id)
{
  auto* fleets_manager = FleetsManager::Instance();
  if (!fleets_manager || fleet_id == 0) {
    return nullptr;
  }

  for (int index = 0; index < 8; ++index) {
    auto* fleet = fleets_manager->GetFleetPlayerData(index);
    if (fleet && static_cast<std::int64_t>(fleet->Id) == fleet_id) {
      return fleet;
    }
  }

  return nullptr;
}

FleetPlayerData* FindPlayerFleetDataByCourseFleetId(std::int64_t fleet_id)
{
  auto* fleets_manager = FleetsManager::Instance();
  if (!fleets_manager || fleet_id == 0) {
    return nullptr;
  }

  const auto low_fleet_id = static_cast<std::uint32_t>(static_cast<std::uint64_t>(fleet_id));
  auto*      low_match    = static_cast<FleetPlayerData*>(nullptr);
  for (int index = 0; index < 8; ++index) {
    auto* fleet = fleets_manager->GetFleetPlayerData(index);
    if (!fleet) {
      continue;
    }

    if (static_cast<std::int64_t>(fleet->Id) == fleet_id) {
      return fleet;
    }

    if (static_cast<std::uint32_t>(fleet->Id) == low_fleet_id) {
      low_match = fleet;
    }
  }

  return low_match;
}

std::uint64_t ActionQueueInstanceFleetId(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return 0;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return 0;
  }

  static auto field_offset = []() -> ptrdiff_t {
    auto field = class_helper.GetField("<PlayerFleetId>k__BackingField");
    return field.isValidHelper() ? field.offset() : -1;
  }();
  if (field_offset < 0) {
    return 0;
  }

  return *reinterpret_cast<std::uint64_t*>(reinterpret_cast<char*>(action_queue_instance) + field_offset);
}

IList* ActionQueueInstanceList(void* action_queue_instance)
{
  if (!action_queue_instance) {
    return nullptr;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  if (!class_helper.isValidHelper()) {
    return nullptr;
  }

  static auto field_offset = []() -> ptrdiff_t {
    auto field = class_helper.GetField("_actionQueue");
    return field.isValidHelper() ? field.offset() : -1;
  }();
  if (field_offset < 0) {
    return nullptr;
  }

  return *reinterpret_cast<IList**>(reinterpret_cast<char*>(action_queue_instance) + field_offset);
}

std::int64_t QueueableActionFleetId(Il2CppObject* queueable_action)
{
  if (!queueable_action) {
    return 0;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "QueueableAction");
  if (!class_helper.isValidHelper()) {
    return 0;
  }

  static auto field_offset = []() -> ptrdiff_t {
    auto field = class_helper.GetField("<FleetId>k__BackingField");
    return field.isValidHelper() ? field.offset() : -1;
  }();
  if (field_offset < 0) {
    return 0;
  }

  return *reinterpret_cast<std::int64_t*>(reinterpret_cast<char*>(queueable_action) + field_offset);
}

int FindActionQueueItemIndex(IList* list, std::int64_t target_id)
{
  if (!list) {
    return -1;
  }

  const auto item_count = std::min(list->Count, 32);
  for (int index = 0; index < item_count; ++index) {
    if (QueueableActionFleetId(list->Get(index)) == target_id) {
      return index;
    }
  }

  return -1;
}

bool TryGetNativeActionQueueInstance(ActionQueueManager* manager, FleetPlayerData* fleet, void** action_queue_instance)
{
  if (action_queue_instance) {
    *action_queue_instance = nullptr;
  }
  if (!manager || !fleet) {
    return false;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!class_helper.isValidHelper()) {
    return false;
  }

  static const MethodInfo* try_get_action_queue_instance =
      class_helper.GetMethodInfoSpecial("TryGetActionQueueInstance", [](int param_count, const Il2CppType** params) {
        return param_count == 2 && params && params[0] && params[1] && params[1]->byref;
      });
  if (!try_get_action_queue_instance) {
    return false;
  }

  void*            instance  = nullptr;
  void*            args[2]   = {fleet, &instance};
  Il2CppException* exception = nullptr;
  auto*            result    = il2cpp_runtime_invoke(try_get_action_queue_instance, manager, args, &exception);
  if (exception || !result) {
    return false;
  }

  const auto success = *reinterpret_cast<bool*>(il2cpp_object_unbox(result));
  if (success && action_queue_instance) {
    *action_queue_instance = instance;
  }
  return success;
}

void* FindActionQueueInstanceForFleet(ActionQueueManager* manager, FleetPlayerData* fleet)
{
  if (!manager || !fleet) {
    return nullptr;
  }

  void* native_action_queue_instance = nullptr;
  if (TryGetNativeActionQueueInstance(manager, fleet, &native_action_queue_instance) && native_action_queue_instance) {
    return native_action_queue_instance;
  }

  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!class_helper.isValidHelper()) {
    return nullptr;
  }

  static auto battle_queue_field_offset = []() -> ptrdiff_t {
    auto field = class_helper.GetField("_battleQueue");
    return field.isValidHelper() ? field.offset() : -1;
  }();
  if (battle_queue_field_offset < 0) {
    return nullptr;
  }

  auto* battle_queue = *reinterpret_cast<Il2CppArray**>(reinterpret_cast<char*>(manager) + battle_queue_field_offset);
  if (!battle_queue) {
    return nullptr;
  }

  auto* sized_array = reinterpret_cast<Il2CppArraySize*>(battle_queue);
  for (size_t index = 0; index < static_cast<size_t>(sized_array->max_length); ++index) {
    auto* action_queue_instance = il2cpp_get_array_element<Il2CppObject>(battle_queue, index);
    if (ActionQueueInstanceFleetId(action_queue_instance) == fleet->Id) {
      return action_queue_instance;
    }
  }

  return nullptr;
}

CourseTargetQueueGuard CheckCourseTargetStillQueued(ActionQueueManager* manager, std::int64_t deployed_id,
                                                    std::int64_t target_id)
{
  CourseTargetQueueGuard guard;
  if (!manager || target_id == 0) {
    return guard;
  }

  auto* fleet = FindPlayerFleetDataById(deployed_id);
  if (!fleet || !manager->IsFleetInQueue(fleet)) {
    return guard;
  }

  guard.queue_count = manager->GetActionQueueCount(fleet);
  if (guard.queue_count <= 0) {
    return guard;
  }

  guard.instance     = FindActionQueueInstanceForFleet(manager, fleet);
  auto* list         = ActionQueueInstanceList(guard.instance);
  guard.target_index = FindActionQueueItemIndex(list, target_id);
  guard.relevant     = guard.target_index >= 0;
  return guard;
}

void LatchCourseTargetCompletionTarget(std::int64_t fleet_id, std::int64_t target_id)
{
  if (!RepairEnabled() || target_id == 0) {
    return;
  }

  auto* fleet = FindPlayerFleetDataByCourseFleetId(fleet_id);
  if (!fleet) {
    return;
  }

  const auto key = static_cast<std::int64_t>(fleet->Id);
  {
    std::lock_guard lk(CourseTargetCompletionMutex());
    auto&           state = CourseTargetCompletionTargets()[key];
    state.target_id       = target_id;
    state.updated_at_ms   = NowMs();
    state.consumed        = false;
  }
}

void LatchPlayerOnlyBattleStart(void* fleets)
{
  if (!RepairEnabled() || !fleets) {
    return;
  }

  auto* list = static_cast<IList*>(fleets);
  if (list->Count != 1) {
    return;
  }

  auto deployed = SnapshotFleetDeployedData(reinterpret_cast<FleetDeployedData*>(list->Get(0)));
  if (!deployed.player_combat_start) {
    return;
  }

  const auto key = deployed.id;
  {
    std::lock_guard lk(CourseTargetCompletionMutex());
    CourseTargetCompletionBattleStarts()[key] = {
        .deployed_id   = deployed.id,
        .updated_at_ms = NowMs(),
    };
  }
}

bool MarkCourseTargetCompletionConsumed(std::int64_t fleet_key, const CourseTargetCompletionCandidate& expected)
{
  std::lock_guard lk(CourseTargetCompletionMutex());
  auto&           targets = CourseTargetCompletionTargets();
  const auto      target  = targets.find(fleet_key);
  if (target == targets.end() || target->second.target_id != expected.target_id
      || target->second.updated_at_ms != expected.updated_at_ms) {
    return false;
  }

  target->second.consumed = true;
  return true;
}

CourseTargetCompletionSynthesis TakeCourseTargetCompletionSynthesis(ActionQueueManager*              manager,
                                                                    const FleetDeployedDataSnapshot& deployed)
{
  if (!RepairEnabled() || !deployed.player_combat_start) {
    return {};
  }

  constexpr std::int64_t kBattleStartWindowMs  = 5000;
  constexpr std::int64_t kCourseTargetWindowMs = 300000;

  const auto now = NowMs();
  const auto key = deployed.id;

  CourseTargetCompletionCandidate target_state;
  PlayerOnlyBattleStartCandidate  start_state;
  {
    std::lock_guard lk(CourseTargetCompletionMutex());
    auto&           targets = CourseTargetCompletionTargets();
    auto&           starts  = CourseTargetCompletionBattleStarts();
    const auto      target  = targets.find(key);
    const auto      start   = starts.find(key);
    if (target == targets.end() || start == starts.end() || target->second.consumed || target->second.target_id == 0) {
      return {};
    }
    target_state = target->second;
    start_state  = start->second;
  }

  const auto battle_age = AgeMs(now, start_state.updated_at_ms);
  const auto course_age = AgeMs(now, target_state.updated_at_ms);
  if (battle_age > kBattleStartWindowMs || course_age > kCourseTargetWindowMs) {
    return {};
  }

  const auto guard = CheckCourseTargetStillQueued(manager, key, target_state.target_id);
  if (!guard.relevant) {
    MarkCourseTargetCompletionConsumed(key, target_state);
    return {};
  }

  MarkCourseTargetCompletionConsumed(key, target_state);
  return {
      .should_synthesize = true,
      .fleet_key         = key,
      .deployed_id       = deployed.id,
      .target_id         = target_state.target_id,
      .course_age_ms     = course_age,
      .battle_age_ms     = battle_age,
      .queue_count       = guard.queue_count,
      .target_index      = guard.target_index,
  };
}

bool IsProcessQueueTargetSignature(int param_count, const Il2CppType** params)
{
  return param_count == 2 && params && params[0] && params[1] && params[0]->type == IL2CPP_TYPE_I8
         && params[1]->type == IL2CPP_TYPE_BOOLEAN;
}

std::string TypeName(const Il2CppType* type)
{
  if (!type) {
    return {};
  }

  auto* raw_name = il2cpp_type_get_name(type);
  if (!raw_name) {
    return {};
  }

  std::string name = raw_name;
  il2cpp_free(raw_name);
  return name;
}

bool IsProcessQueueDeployedSignature(int param_count, const Il2CppType** params)
{
  return param_count == 2 && params && params[0] && params[1] && params[1]->type == IL2CPP_TYPE_BOOLEAN
         && TypeName(params[0]).find("FleetDeployedData") != std::string::npos;
}

bool IsOnSetCourseResponseSignature(int param_count, const Il2CppType** params)
{
  return param_count == 4 && params && params[0] && params[1] && params[2] && params[0]->type == IL2CPP_TYPE_I8
         && params[1]->type == IL2CPP_TYPE_BOOLEAN && params[2]->type == IL2CPP_TYPE_BOOLEAN;
}

bool IsSingleParameterSignature(int param_count, const Il2CppType** params)
{ return param_count == 1 && params && params[0]; }

ProcessQueueTargetMethod* ResolveProcessQueueTargetForCompletion()
{
  static auto actionqueue_manager =
      il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!actionqueue_manager.isValidHelper()) {
    return nullptr;
  }

  static auto method =
      actionqueue_manager.GetMethodSpecial<ProcessQueueTargetMethod>("ProcessQueue", IsProcessQueueTargetSignature);
  return method;
}

void ActionQueueManager_ProcessQueueDeployed(auto original, ActionQueueManager* _this, FleetDeployedData* deployed_data,
                                             bool can_select_new_target)
{
  const auto deployed = SnapshotFleetDeployedData(deployed_data);

  original(_this, deployed_data, can_select_new_target);

  const auto synthesis = TakeCourseTargetCompletionSynthesis(_this, deployed);
  if (!synthesis.should_synthesize) {
    return;
  }

  auto* process_target = ResolveProcessQueueTargetForCompletion();
  if (!process_target) {
    ErrorMsg::MissingMethod("ActionQueueManager", "ProcessQueue(Int64, bool)");
    return;
  }

  spdlog::info("[KirsharaQueueRepair] completing queued target fleet={} target={} count={} index={} course_age_ms={} "
               "battle_age_ms={}",
               synthesis.deployed_id, synthesis.target_id, synthesis.queue_count, synthesis.target_index,
               synthesis.course_age_ms, synthesis.battle_age_ms);
  process_target(_this, synthesis.target_id, can_select_new_target);
}

void ActionQueueManager_OnSetCourseResponseEventHandler(auto original, ActionQueueManager* _this, std::int64_t fleet_id,
                                                        bool request_successful, bool is_recall,
                                                        void* planned_course_data)
{
  const auto course_target = ReadCourseDataTargetDeployedFleetId(planned_course_data);
  if (request_successful && !is_recall && course_target.present) {
    LatchCourseTargetCompletionTarget(fleet_id, course_target.value);
  }

  original(_this, fleet_id, request_successful, is_recall, planned_course_data);
}

void ActionQueueManager_OnFleetStateChangeEventHandler(auto original, ActionQueueManager* _this, void* fleets)
{
  LatchPlayerOnlyBattleStart(fleets);
  original(_this, fleets);
}
} // namespace

void InstallActionQueueRepairHooks()
{
  if (!RepairEnabled()) {
    return;
  }

  static auto actionqueue_manager =
      il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!actionqueue_manager.isValidHelper()) {
    ErrorMsg::MissingHelper("ActionQueue", "ActionQueueManager");
    return;
  }

  auto* ptr_process_deployed = actionqueue_manager.GetMethodSpecial("ProcessQueue", IsProcessQueueDeployedSignature);
  if (ptr_process_deployed) {
    SPUD_STATIC_DETOUR(ptr_process_deployed, ActionQueueManager_ProcessQueueDeployed);
  } else {
    ErrorMsg::MissingMethod("ActionQueueManager", "ProcessQueue(FleetDeployedData, bool)");
  }

  if (auto* ptr =
          actionqueue_manager.GetMethodSpecial("OnSetCourseResponseEventHandler", IsOnSetCourseResponseSignature);
      ptr) {
    SPUD_STATIC_DETOUR(ptr, ActionQueueManager_OnSetCourseResponseEventHandler);
  } else {
    ErrorMsg::MissingMethod("ActionQueueManager", "OnSetCourseResponseEventHandler");
  }

  if (auto* ptr = actionqueue_manager.GetMethodSpecial("OnFleetStateChangeEventHandler", IsSingleParameterSignature);
      ptr) {
    SPUD_STATIC_DETOUR(ptr, ActionQueueManager_OnFleetStateChangeEventHandler);
  } else {
    ErrorMsg::MissingMethod("ActionQueueManager", "OnFleetStateChangeEventHandler");
  }

  spdlog::info("[KirsharaQueueRepair] installed off-screen queue completion repair");
}
