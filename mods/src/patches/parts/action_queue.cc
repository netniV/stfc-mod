// Temporary opt-in recovery study; STFC_QUEUE_RECOVER=1 enables the candidate. Enable with STFC_QUEUE_TRACE=1 in
// releasedbg.
#if defined(_WIN32) && defined(_M_X64) && defined(_MODDBG)
#include <Windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <il2cpp/il2cpp_helper.h>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

namespace
{
using Clock = std::chrono::steady_clock;
Clock::time_point deadline;
std::atomic_uint  budget{1024};
std::atomic_bool  ready{false};
Il2CppClass*      actionClass{};
Il2CppClass*      queueClass{};
bool              recover{};
struct Request {
  Il2CppGCHandle    queueHandle{};
  std::int64_t      fleet{}, target{};
  float             attempt{};
  Clock::time_point started{};
};
std::array<Request, 8> requests{};
std::mutex             requestsMutex;
struct CourseContext {
  std::int64_t  fleet{}, target{};
  Il2CppObject* queue{}; // Borrowed only within the synchronous native Course call.
  bool          engagingAtEntry{}, failed{};
};
thread_local CourseContext* currentCourse{};

bool Active()
{ return ready.load() && budget.load() != 0 && Clock::now() < deadline; }
bool Reserve()
{
  auto remaining = budget.load();
  while (remaining && !budget.compare_exchange_weak(remaining, remaining - 1)) {}
  return remaining != 0;
}
template <typename T> T Read(const void* object, std::size_t offset)
{
  T value{};
  if (object)
    std::memcpy(&value, static_cast<const char*>(object) + offset, sizeof(value));
  return value;
}
struct Snapshot {
  std::int64_t fleet{}, last{}, pending{};
  std::int64_t front{}, next{};
  int          retries{-1};
  bool         frontKnown{}, nextKnown{};
  float        attempt{};
  int          count{-1}, state{-1};
  bool         engaging{};
  bool         operator==(const Snapshot&) const = default;
};
Snapshot Capture(Il2CppObject* queue, Il2CppObject* deployed = nullptr)
{
  Snapshot s;
  if (!queue)
    return s;
  s.fleet    = Read<std::int64_t>(queue, 0x30);
  s.last     = Read<std::int64_t>(queue, 0x18);
  s.pending  = Read<std::int64_t>(queue, 0x20);
  s.attempt  = Read<float>(queue, 0x14);
  s.engaging = Read<bool>(queue, 0x10);
  if (deployed)
    s.state = Read<int>(deployed, 0x80);
  // Inspect List<T>'s count only if the actual object exposes the expected field.
  if (auto* list = Read<Il2CppObject*>(queue, 0x28)) {
    auto* field = il2cpp_class_get_field_from_name(il2cpp_object_get_class(list), "_size");
    if (field && field->type && field->type->type == IL2CPP_TYPE_I4)
      il2cpp_field_get_value(list, field, &s.count);
    auto* itemsField = il2cpp_class_get_field_from_name(il2cpp_object_get_class(list), "_items");
    if (s.count > 0 && itemsField && itemsField->type && itemsField->type->type == IL2CPP_TYPE_SZARRAY) {
      Il2CppArray* items{};
      il2cpp_field_get_value(list, itemsField, &items);
      if (items && il2cpp_array_length(items) >= static_cast<unsigned>(s.count)
          && il2cpp_class_get_element_class(il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(items)))
                 == actionClass) {
        auto* array = reinterpret_cast<Il2CppArraySize*>(items);
        auto* front = static_cast<Il2CppObject*>(array->vector[0]);
        if (front && il2cpp_object_get_class(front) == actionClass) {
          s.frontKnown = true;
          s.front      = Read<std::int64_t>(front, 0x10);
          s.retries    = Read<int>(front, 0x18);
        }
        if (s.count > 1) {
          auto* next = static_cast<Il2CppObject*>(array->vector[1]);
          if (next && il2cpp_object_get_class(next) == actionClass) {
            s.nextKnown = true;
            s.next      = Read<std::int64_t>(next, 0x10);
          }
        }
      }
    }
  }
  return s;
}
// Record the most recent native engagement attempt; weak handles prevent recycled object identities matching.
void Remember(Il2CppObject* queue, const Snapshot& s)
{
  std::lock_guard lock(requestsMutex);
  auto*           slot = &requests.front();
  for (auto& r : requests) {
    if (r.fleet == s.fleet) {
      slot = &r;
      break;
    }
    if (r.started < slot->started)
      slot = &r;
  }
  if (slot->queueHandle)
    il2cpp_gchandle_free(slot->queueHandle);
  *slot = {il2cpp_gchandle_new_weakref(queue, false), s.fleet, s.frontKnown ? s.front : 0, s.attempt, Clock::now()};
}
bool ConsumeRequest(Il2CppObject* queue, const Snapshot& s, std::int64_t failedTarget)
{
  std::lock_guard lock(requestsMutex);
  for (auto& r : requests) {
    if (r.fleet != s.fleet)
      continue;
    const bool match = r.queueHandle && il2cpp_gchandle_get_target(r.queueHandle) == queue && r.target == failedTarget
                       && r.attempt == s.attempt && Clock::now() - r.started < std::chrono::seconds(30);
    if (match) {
      il2cpp_gchandle_free(r.queueHandle);
      r = {};
    }
    return match;
  }
  return false;
}
bool TargetAbsent(Il2CppObject* queue, std::int64_t target)
{
  auto* list = Read<Il2CppObject*>(queue, 0x28);
  if (!list)
    return false;
  auto* cls     = il2cpp_object_get_class(list);
  auto* size    = il2cpp_class_get_field_from_name(cls, "_size");
  auto* storage = il2cpp_class_get_field_from_name(cls, "_items");
  if (!size || !storage || !size->type || !storage->type || size->type->type != IL2CPP_TYPE_I4
      || storage->type->type != IL2CPP_TYPE_SZARRAY)
    return false;
  int          count{};
  Il2CppArray* items{};
  il2cpp_field_get_value(list, size, &count);
  il2cpp_field_get_value(list, storage, &items);
  if (count <= 0 || count > 128 || !items || il2cpp_array_length(items) < static_cast<unsigned>(count)
      || il2cpp_class_get_element_class(il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(items))) != actionClass)
    return false;
  auto* array = reinterpret_cast<Il2CppArraySize*>(items);
  for (int i = 0; i < count; ++i) {
    auto* action = static_cast<Il2CppObject*>(array->vector[i]);
    if (!action || il2cpp_object_get_class(action) != actionClass || Read<std::int64_t>(action, 0x10) == target)
      return false;
  }
  return true;
}
Il2CppObject* FindQueue(Il2CppObject* manager, std::int64_t fleet)
{
  auto* array = Read<Il2CppArray*>(manager, 0x48);
  if (!array || il2cpp_array_length(array) > 64
      || il2cpp_class_get_element_class(il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(array))) != queueClass)
    return nullptr;
  auto* values = reinterpret_cast<Il2CppArraySize*>(array);
  for (unsigned i = 0; i < il2cpp_array_length(array); ++i) {
    auto* queue = static_cast<Il2CppObject*>(values->vector[i]);
    if (queue && il2cpp_object_get_class(queue) == queueClass && Read<std::int64_t>(queue, 0x30) == fleet)
      return queue;
  }
  return nullptr;
}
// Preserve native cross-fleet cleanup: do not take the planner branch if any queue still holds this target.
bool AbsentFromAllQueues(Il2CppObject* manager, std::int64_t target)
{
  auto* array = Read<Il2CppArray*>(manager, 0x48);
  if (!array || il2cpp_array_length(array) > 64
      || il2cpp_class_get_element_class(il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(array))) != queueClass)
    return false;
  auto* values = reinterpret_cast<Il2CppArraySize*>(array);
  for (unsigned i = 0; i < il2cpp_array_length(array); ++i) {
    auto* queue = static_cast<Il2CppObject*>(values->vector[i]);
    if (!queue)
      continue;
    if (il2cpp_object_get_class(queue) != queueClass)
      return false;
    const auto snapshot = Capture(queue);
    if (snapshot.count < 0 || (snapshot.count > 0 && !TargetAbsent(queue, target)))
      return false;
  }
  return true;
}
void Log(const char* event, const Snapshot& s, int result = -1, std::int64_t sameSnapshotMs = -1,
         std::int64_t target = 0)
{
  if (Reserve())
    spdlog::info(
        "[QueueTrace] {} fleet={} count={} engaging={} last={} pending={} attempt={} state={} result={} "
        "same_snapshot_observed_ms={} front_known={} front={} retries={} next_known={} next={} decision_target={}",
        event, s.fleet, s.count, s.engaging, s.last, s.pending, s.attempt, s.state, result, sameSnapshotMs,
        s.frontKnown, s.front, s.retries, s.nextKnown, s.next, target);
}
bool Changed(const Snapshot& s, std::int64_t& sameSnapshotMs)
{
  struct Record {
    Snapshot          snapshot;
    Clock::time_point logged{}, firstObserved{};
  };
  static std::array<Record, 8> records{};
  static std::mutex            mutex;
  std::lock_guard              lock(mutex);
  auto*                        record = &records.front();
  for (auto& entry : records) {
    if (entry.snapshot.fleet == s.fleet) {
      record = &entry;
      break;
    }
    if (entry.logged < record->logged)
      record = &entry;
  }
  const auto now  = Clock::now();
  const bool same = record->logged != Clock::time_point{} && record->snapshot == s;
  if (!same)
    record->firstObserved = now;
  // This is equality at native watchdog samples, not proof that nothing changed between them.
  sameSnapshotMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - record->firstObserved).count();
  // Idle nonempty queues deserve closer observation even when IsEngaging blocks the native watchdog.
  // Reuse its callbacks: no timer, polling loop, or changes to retry eligibility.
  const auto interval = std::chrono::seconds(s.state == 0 && s.count != 0 ? 3 : 30);
  if (same && now - record->logged < interval)
    return false;
  record->snapshot = s;
  record->logged   = now;
  return true;
}
void Stall(auto original, Il2CppObject* manager, Il2CppObject* queue, Il2CppObject* player, Il2CppObject* deployed)
{
  try {
    if (Active()) {
      const auto   snapshot = Capture(queue, deployed);
      std::int64_t sameSnapshotMs{};
      if (Changed(snapshot, sameSnapshotMs))
        Log("watchdog-before", snapshot, -1, sameSnapshotMs);
    }
  } catch (...) {
  }
  original(manager, queue, player, deployed);
}
int Engage(auto original, Il2CppObject* manager, Il2CppObject* player, Il2CppObject* queue)
{
  bool     trace = false;
  Snapshot before;
  try {
    trace = Active();
    if (trace || (recover && ready.load())) {
      before = Capture(queue);
      if (recover && ready.load())
        Remember(queue, before);
    }
  } catch (...) {
    trace = false;
  }
  const auto result = original(manager, player, queue);
  // Retain primitives only across the native call; do not reread possibly-released objects.
  try {
    if (trace)
      Log("engage-result", before, result);
  } catch (...) {
  }
  return result;
}
// Capture only primitives before native code; a decision is not proof that removal occurred.
bool Retry(auto original, Il2CppObject* manager, std::int64_t target, Il2CppObject* queue)
{
  bool     trace = false;
  Snapshot before;
  try {
    trace = Active();
    if (trace || (recover && ready.load()))
      before = Capture(queue);
  } catch (...) {
    trace = false;
  }
  const bool result = original(manager, target, queue);
  try {
    if (trace)
      Log("retry-decision", before, result ? 1 : 0, -1, target);
  } catch (...) {
  }
  // Course has already cleared IsEngaging. Only promote a non-retry after the exact outstanding
  // target disappeared. Returning true uses Course's existing native planner/eligibility path.
  try {
    const auto* c = currentCourse;
    if (!result && recover && ready.load() && c && c->failed && c->engagingAtEntry && c->queue == queue
        && c->fleet == before.fleet && c->target == target && target != 0 && before.pending == target
        && !before.engaging && before.frontKnown && before.front != target && before.count > 0
        && TargetAbsent(queue, target) && AbsentFromAllQueues(manager, target)
        && ConsumeRequest(queue, before, target)) {
      if (Active())
        Log("advance-removed-front", before, 1, -1, target);
      return true;
    }
  } catch (...) {
  }
  return result;
}
void ProcessTarget(auto original, Il2CppObject* manager, std::int64_t target, bool canSelect)
{
  try {
    if (Active() && Reserve())
      spdlog::info("[QueueTrace] process-target-before target={} can_select_new={}", target, canSelect);
  } catch (...) {
  }
  original(manager, target, canSelect);
}
struct CourseResponse {
  std::int64_t  fleet;
  bool          success, recall;
  unsigned char padding[6];
  Il2CppObject* target;
};
static_assert(sizeof(CourseResponse) == 24 && offsetof(CourseResponse, target) == 16);
void Course(auto original, Il2CppObject* manager, CourseResponse args)
{
  CourseContext context{args.fleet};
  try {
    std::int64_t target{};
    if (args.target) {
      const auto* name = il2cpp_class_get_name(il2cpp_object_get_class(args.target));
      if (name && std::strcmp(name, "Int64") == 0)
        std::memcpy(&target, il2cpp_object_unbox(args.target), sizeof(target));
    }
    context.target = target;
    context.failed = !args.success && !args.recall;
    if (recover && ready.load() && context.failed && target != 0) {
      context.queue           = FindQueue(manager, args.fleet);
      context.engagingAtEntry = context.queue && Read<bool>(context.queue, 0x10);
    }
    if (Active() && Reserve())
      spdlog::info("[QueueTrace] course-response fleet={} target={} success={} recall={}", args.fleet, target,
                   args.success, args.recall);
  } catch (...) {
  }
  struct Scope {
    CourseContext* previous{currentCourse};
    explicit Scope(CourseContext* value)
    { currentCourse = value; }
    ~Scope()
    { currentCourse = previous; }
  } scope(&context);
  original(manager, args);
}
bool Field(IL2CppClassHelper& cls, const char* name, std::ptrdiff_t offset)
{
  if (!cls.isValidHelper())
    return false;
  auto field = cls.GetField(name);
  return field.isValidHelper() && field.offset() == offset;
}
bool Method(void* method, std::uintptr_t rva, unsigned extent, const std::array<unsigned char, 24>& bytes)
{
  const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"GameAssembly.dll"));
  if (!base || reinterpret_cast<std::uintptr_t>(method) != base + rva)
    return false;
  DWORD64 image{};
  auto*   entry = RtlLookupFunctionEntry(base + rva, &image, nullptr);
  return entry && image == base && entry->BeginAddress == rva && entry->EndAddress - entry->BeginAddress == extent
         && std::memcmp(method, bytes.data(), bytes.size()) == 0;
}
} // namespace

void InstallActionQueueTrace()
{
  char enabled[8]{};
  if (GetEnvironmentVariableA("STFC_QUEUE_TRACE", enabled, sizeof(enabled)) != 1 || enabled[0] != '1')
    return;
  char recovery[8]{};
  recover      = GetEnvironmentVariableA("STFC_QUEUE_RECOVER", recovery, sizeof(recovery)) == 1 && recovery[0] == '1';
  auto manager = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  auto action  = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "QueueableAction");
  actionClass  = action.get_cls();
  auto queue   = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueInstance");
  queueClass   = queue.get_cls();
  auto deployed =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetDeployedData");
  if (!manager.isValidHelper()) {
    spdlog::warn("[QueueTrace] disabled: queue manager unavailable");
    return;
  }
  auto* stall = manager.GetMethodSpecial("HandleStall", [](int n, const Il2CppType**) { return n == 3; });
  auto* engage =
      manager.GetMethodSpecial("TryPlanPathAndEngageTarget", [](int n, const Il2CppType**) { return n == 2; });
  auto* retry      = manager.GetMethodSpecial("ShouldRetryFailedSetCourse", [](int n, const Il2CppType** p) {
    return n == 2 && p && p[0] && !p[0]->byref && p[0]->type == IL2CPP_TYPE_I8;
  });
  auto* process    = manager.GetMethodSpecial("ProcessQueue", [](int n, const Il2CppType** p) {
    return n == 2 && p && p[0] && p[1] && !p[0]->byref && !p[1]->byref && p[0]->type == IL2CPP_TYPE_I8
           && p[1]->type == IL2CPP_TYPE_BOOLEAN;
  });
  auto* courseInfo = manager.GetMethodInfoSpecial("OnSetCourseResponseEventHandler", [](int n, const Il2CppType** p) {
    return n == 1 && p && p[0] && !p[0]->byref && p[0]->type == IL2CPP_TYPE_VALUETYPE;
  });
  auto* course     = courseInfo ? reinterpret_cast<void*>(courseInfo->methodPointer) : nullptr;
  auto* eventClass = courseInfo ? il2cpp_class_from_type(courseInfo->parameters[0]) : nullptr;
  IL2CppClassHelper event(eventClass);
  std::uint32_t     alignment{};
  const bool        valid =
      Field(manager, "_battleQueue", 0x48) && Field(action, "<FleetId>k__BackingField", 0x10)
      && Field(action, "SetCourseFailRetryCount", 0x18)
      && Method(retry, 0x110d280, 662, {0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x6c, 0x24, 0x18, 0x57, 0x48,
                                        0x83, 0xec, 0x20, 0x80, 0x3d, 0x7a, 0x04, 0xb1, 0x04, 0x00, 0x49, 0x8b})
      && Method(process, 0x110cb70, 367, {0x40, 0x53, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xec,
                                          0x30, 0x80, 0x3d, 0x89, 0x0b, 0xb1, 0x04, 0x00, 0x45, 0x0f, 0xb6, 0xf0})
      && eventClass && il2cpp_class_value_size(eventClass, &alignment) == sizeof(CourseResponse)
      && Field(event, "<FleetId>k__BackingField", 0x10) && Field(event, "<Success>k__BackingField", 0x18)
      && Field(event, "<IsRecall>k__BackingField", 0x19) && Field(event, "<TargetId>k__BackingField", 0x20)
      && Method(course, 0x110d070, 514, {0x48, 0x89, 0x5c, 0x24, 0x18, 0x57, 0x48, 0x83, 0xec, 0x20, 0x80, 0x3d,
                                         0x8e, 0x06, 0xb1, 0x04, 0x00, 0x48, 0x8b, 0xfa, 0x48, 0x8b, 0xd9, 0x75})
      && Field(queue, "IsEngaging", 0x10) && Field(queue, "LastEngageAttemptTime", 0x14)
      && Field(queue, "LastEngagedTargetId", 0x18) && Field(queue, "PendingEngageTargetId", 0x20)
      && Field(queue, "_actionQueue", 0x28) && Field(queue, "<PlayerFleetId>k__BackingField", 0x30)
      && Field(deployed, "_stateContainer", 0x80)
      && Method(stall, 0x110df70, 341, {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10, 0x48, 0x89,
                                        0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xec, 0x40, 0x0f, 0x29, 0x74, 0x24})
      && Method(engage, 0x1109f60, 2340, {0x4c, 0x89, 0x44, 0x24, 0x18, 0x48, 0x89, 0x54, 0x24, 0x10, 0x48, 0x89,
                                          0x4c, 0x24, 0x08, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56});
  if (!valid) {
    spdlog::warn("[QueueTrace] disabled: native build/layout does not match validated client261");
    return;
  }
  const bool first  = SPUD_STATIC_DETOUR(stall, Stall) != nullptr;
  const bool second = SPUD_STATIC_DETOUR(engage, Engage) != nullptr;
  const bool third  = SPUD_STATIC_DETOUR(course, Course) != nullptr;
  const bool fourth = SPUD_STATIC_DETOUR(retry, Retry) != nullptr;
  const bool fifth  = SPUD_STATIC_DETOUR(process, ProcessTarget) != nullptr;
  deadline          = Clock::now() + std::chrono::minutes(30);
  ready.store(first && second && third && fourth && fifth);
  spdlog::info("[QueueTrace] ready={} recovery={} budget=1024 window=30min; engage result 0=success 1=skip "
               "2=stop; retry result 0=no 1=yes",
               ready.load(), recover);
}
#else
void InstallActionQueueTrace() {}
#endif
