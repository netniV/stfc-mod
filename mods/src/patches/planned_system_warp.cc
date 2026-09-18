#include "planned_system_warp.h"
#include "errormsg.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <il2cpp-tabledefs.h>
#include <optional>
#include <prime/DeploymentManager.h>
#include <prime/Hub.h>
#include <prime/NavigationSectionManager.h>
#include <spdlog/spdlog.h>
#include <type_traits>

namespace
{
constexpr int PlannedCourse = 0, NoTargetAction = -1, OrbitAction = 1, ValidPath = 1;
constexpr int GalaxyDepth = 1, SystemDepth = 2, SystemTreeNode = 0;

bool IsInstanceMethod(const MethodInfo* method, int arguments)
{
  return method && method->methodPointer && !(method->flags & METHOD_ATTRIBUTE_STATIC)
         && method->parameters_count == arguments && method->return_type && !method->return_type->byref;
}

// Check the managed representation before reading a C++ value (including enum underlying types).
template <typename T> bool MatchesValueType(const Il2CppType* type)
{
  if (!type || type->byref) {
    return false;
  }
  auto* cls = il2cpp_class_from_type(type);
  if (!cls || !il2cpp_class_is_valuetype(cls)) {
    return false;
  }
  uint32_t alignment = 0;
  if (il2cpp_class_value_size(cls, &alignment) != sizeof(T)) {
    return false;
  }
  const auto* scalar = il2cpp_class_is_enum(cls) ? il2cpp_class_enum_basetype(cls) : type;
  if constexpr (std::is_same_v<T, bool>) {
    return scalar && scalar->type == IL2CPP_TYPE_BOOLEAN;
  } else if constexpr (std::is_same_v<T, int>) {
    return scalar && scalar->type == IL2CPP_TYPE_I4;
  } else if constexpr (std::is_same_v<T, std::int64_t>) {
    return scalar && scalar->type == IL2CPP_TYPE_I8;
  } else if constexpr (std::is_same_v<T, Vector3>) {
    return std::strcmp(cls->namespaze, "UnityEngine") == 0 && std::strcmp(cls->name, "Vector3") == 0;
  }
  return false;
}

template <typename T> std::optional<T> Unbox(Il2CppObject* object)
{
  if (!object || !MatchesValueType<T>(il2cpp_class_get_type(il2cpp_object_get_class(object)))) {
    return std::nullopt;
  }
  const auto* value = static_cast<T*>(il2cpp_object_unbox(object));
  return value ? std::optional<T>(*value) : std::nullopt;
}

// Reads occur only on action edges. Box field values so a changed field size cannot overwrite T.
Il2CppObject* ReadMember(void* object, const char* name, bool field = false)
{
  if (!object) {
    return nullptr;
  }
  auto* instance = static_cast<Il2CppObject*>(object);
  auto* cls      = il2cpp_object_get_class(instance);
  if (field) {
    auto* member = il2cpp_class_get_field_from_name(cls, name);
    return member ? il2cpp_field_get_value_object(member, instance) : nullptr;
  }
  auto* property = il2cpp_class_get_property_from_name(cls, name);
  auto* getter   = property ? il2cpp_property_get_get_method(const_cast<PropertyInfo*>(property)) : nullptr;
  if (!IsInstanceMethod(getter, 0)) {
    return nullptr;
  }
  getter = il2cpp_object_get_virtual_method(instance, getter);
  if (!IsInstanceMethod(getter, 0)) {
    return nullptr;
  }
  Il2CppException* exception = nullptr;
  auto*            result    = il2cpp_runtime_invoke(getter, instance, nullptr, &exception);
  return exception ? nullptr : result;
}

template <typename T> std::optional<T> ReadValue(void* object, const char* name, bool field = false)
{ return Unbox<T>(ReadMember(object, name, field)); }

Il2CppObject* ReadObject(void* object, const char* name, bool field = false)
{
  auto* result = ReadMember(object, name, field);
  return result && !il2cpp_class_is_valuetype(il2cpp_object_get_class(result)) ? result : nullptr;
}

bool NativeCanNavigate(void* navigation, std::int64_t fleet_id)
{
  auto* controller = ReadObject(navigation, "NavigationInteractionUIViewController");
  auto* helper     = ReadObject(controller, "_fleetNavHelper", true);
  auto* method =
      helper ? IL2CppClassHelper(il2cpp_object_get_class(helper)).GetMethodInfo("NavigationValidateInput", 2) : nullptr;
  if (!IsInstanceMethod(method, 2) || !MatchesValueType<bool>(method->return_type)
      || !MatchesValueType<std::int64_t>(method->parameters[0]) || !method->parameters[1]->byref) {
    return false;
  }
  auto* message_class = il2cpp_class_from_type(method->parameters[1]);
  if (!message_class || il2cpp_class_is_valuetype(message_class)
      || std::strcmp(message_class->namespaze, "Digit.Client.UI") != 0
      || std::strcmp(message_class->name, "LocaleTextContext") != 0) {
    return false;
  }
  Il2CppObject*    message   = nullptr;
  void*            args[]    = {&fleet_id, &message};
  Il2CppException* exception = nullptr;
  auto*            result    = il2cpp_runtime_invoke(method, helper, args, &exception);
  return !exception && Unbox<bool>(result) == true;
}

bool MatchesRetainedPosition(void* navigation, void* context, void* target)
{
  auto  world_position  = ReadValue<Vector3>(context, "Position", true);
  auto  galaxy_position = ReadValue<Vector3>(target, "GalaxyPosition");
  auto* animator        = ReadObject(navigation, "_transitionAnimator", true);
  if (!world_position || !galaxy_position || !animator) {
    return false;
  }
  auto  helper  = IL2CppClassHelper(il2cpp_object_get_class(animator));
  auto* convert = helper.GetMethodInfo("WorldToDepth", 2);
  if (!IsInstanceMethod(convert, 2) || !MatchesValueType<Vector3>(convert->return_type)
      || !MatchesValueType<Vector3>(convert->parameters[0]) || !MatchesValueType<int>(convert->parameters[1])) {
    return false;
  }
  int              galaxy_depth = GalaxyDepth;
  void*            args[]       = {&*world_position, &galaxy_depth};
  Il2CppException* exception    = nullptr;
  auto*            result       = il2cpp_runtime_invoke(convert, animator, args, &exception);
  if (exception) {
    return false;
  }
  const auto converted = Unbox<Vector3>(result);
  if (!converted) {
    return false;
  }
  const auto position = *converted;
  // Compare in galaxy coordinates, allowing only floating-point projection roundoff.
  const auto matches = [](float a, float b) { return std::isfinite(a) && std::isfinite(b) && std::abs(a - b) < 0.01f; };
  return matches(position.x, galaxy_position->x) && matches(position.y, galaxy_position->y)
         && matches(position.z, galaxy_position->z);
}

struct OptionalPosition {
  bool      has_value = true;
  std::byte padding[3]{};
  Vector3   value{};
};
static_assert(sizeof(OptionalPosition) == 16 && offsetof(OptionalPosition, value) == 4);

bool MatchesMoveSignature(const MethodInfo* method)
{
  if (!IsInstanceMethod(method, 4) || method->return_type->type != IL2CPP_TYPE_VOID || method->parameters[0]->byref
      || !MatchesValueType<std::int64_t>(method->parameters[1])
      || !MatchesValueType<std::int64_t>(method->parameters[2]) || !MatchesValueType<bool>(method->parameters[3])) {
    return false;
  }
  // runtime_invoke handles the platform ABI; verify the actual Nullable<Vector3> payload layout.
  auto* position_class = il2cpp_class_from_type(method->parameters[0]);
  if (!position_class || !il2cpp_class_is_valuetype(position_class)
      || std::strcmp(position_class->namespaze, "System") != 0
      || std::strcmp(position_class->name, "Nullable`1") != 0) {
    return false;
  }
  uint32_t alignment = 0;
  auto*    has_value = il2cpp_class_get_field_from_name(position_class, "hasValue");
  auto*    value     = il2cpp_class_get_field_from_name(position_class, "value");
  return il2cpp_class_value_size(position_class, &alignment) == sizeof(OptionalPosition) && has_value && value
         && MatchesValueType<bool>(has_value->type) && MatchesValueType<Vector3>(value->type)
         && has_value->offset == sizeof(Il2CppObject)
         && value->offset == sizeof(Il2CppObject) + offsetof(OptionalPosition, value);
}
} // namespace

bool TryRequestPlannedSystemWarp(FleetPlayerData* fleet, NavigationInteractionUIContext* context)
{
  auto* sections = Hub::get_SectionManager();
  if (!fleet || !context || !sections || sections->CurrentSection != SectionID::Navigation_Galaxy) {
    return false;
  }
  static auto deployment_class = il2cpp_get_class_helper("Assembly-CSharp", "", "DeploymentManager");
  static auto navigation_class =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationManager");
  if (!deployment_class.isValidHelper() || !navigation_class.isValidHelper()) {
    return false;
  }
  static auto*      get_course           = deployment_class.GetMethodInfo("GetPlannedCourse", 1);
  static auto*      move                 = navigation_class.GetMethodInfo("OnMoveFleetAction", 4);
  static const bool move_signature_valid = MatchesMoveSignature(move);
  if (!IsInstanceMethod(get_course, 1) || !MatchesValueType<std::int64_t>(get_course->parameters[0])
      || !move_signature_valid) {
    spdlog::warn("[Hotkeys] planned system warp unavailable: native method or argument layout mismatch");
    return false;
  }
  auto* deployment         = DeploymentManger::Instance();
  auto* navigation_section = NavigationSectionManager::Instance();
  auto* navigation         = navigation_section ? navigation_section->SNavigationManager : nullptr;
  if (!deployment || !navigation) {
    return false;
  }

  auto fleet_id = static_cast<std::int64_t>(fleet->Id);
  if (!NativeCanNavigate(navigation, fleet_id)) {
    return false;
  }
  void*            course_args[] = {&fleet_id};
  Il2CppException* exception     = nullptr;
  auto*            course        = il2cpp_runtime_invoke(get_course, deployment, course_args, &exception);
  if (exception || !course) {
    return false;
  }
  // Deployed courses can have DeployedFleet without PlayerFleet. The native ID getter covers both.
  const auto course_fleet_id = ReadValue<std::int64_t>(course, "FleetID");
  if (course_fleet_id != fleet_id) {
    return false;
  }
  // A preview without a backing Course reports TargetAction::Null (-1). The native movement handler
  // assigns the movement action; accept that preview as well as Orbit (1), but reject a missing read.
  // Special travel keeps its own native popup flow.
  const auto type        = ReadValue<int>(course, "Type");
  const auto action      = ReadValue<int>(course, "TargetAction");
  const auto in_use      = ReadValue<bool>(course, "IsInUse");
  const auto warp        = ReadValue<bool>(course, "IsWarpCourse");
  const auto path_result = ReadValue<int>(ReadObject(course, "PathFinderResult"), "Result");
  if (type != PlannedCourse || (action != NoTargetAction && action != OrbitAction) || in_use != false || warp != true
      || ReadValue<bool>(course, "IsRecallCourse") != false || ReadValue<bool>(course, "IsWormholeCourse") != false
      || ReadValue<bool>(course, "HasToll") != false || path_result != ValidPath) {
    return false;
  }
  auto*      target         = ReadObject(course, "TargetNode");
  auto*      target_address = ReadObject(target, "Address");
  auto*      start_address  = ReadObject(ReadObject(course, "StartNode"), "Address");
  auto*      fleet_address  = ReadObject(fleet, "Address");
  const auto start_system   = ReadValue<std::int64_t>(start_address, "System");
  const auto start_instance = ReadValue<int>(start_address, "Instance");
  if (ReadValue<int>(target_address, "NodeDepth") != SystemDepth || !start_system || *start_system <= 0
      || !start_instance || start_system != ReadValue<std::int64_t>(fleet_address, "System")
      || start_instance != ReadValue<int>(fleet_address, "Instance")) {
    return false;
  }
  const auto position = ReadValue<Vector3>(target, "SystemPosition");
  auto       node_id  = ReadValue<std::int64_t>(target, "ID");
  // Dismissal can also discard TreeNode. UpdatePosition and UpdateWithCourse retain the last
  // selected world position and path result, so bind the course to both before using its destination.
  // Position changes before asynchronous planning finishes; matching the path alone is insufficient.
  auto* context_path = ReadObject(context, "PathFindResult", true);
  if (!context_path || context_path != ReadObject(course, "PathFinderResult")
      || !MatchesRetainedPosition(navigation, context, target)) {
    return false;
  }
  auto*      selected_node = ReadObject(context, "TreeNode");
  const auto selected_id   = ReadValue<std::int64_t>(selected_node, "ID");
  if (selected_node
      && (!selected_id || selected_id != node_id || ReadValue<int>(selected_node, "Type") != SystemTreeNode)) {
    return false;
  }
  if (!position || !node_id || *node_id <= 0 || !std::isfinite(position->x) || !std::isfinite(position->y)
      || !std::isfinite(position->z)) {
    return false;
  }

  // Match StarNodeObjectViewerWidget.GetPositionAndTargetID: final course SystemPosition + node ID.
  // A cleared POI would otherwise pass -1 and NavigationManager substitutes the viewed galaxy node.
  OptionalPosition destination{.value = *position};
  bool             force_move = false;
  void*            args[]     = {&destination, &*node_id, &fleet_id, &force_move};
  exception                   = nullptr;
  il2cpp_runtime_invoke(move, navigation, args, &exception);
  if (exception) {
    spdlog::warn("[Hotkeys] planned system warp native movement handler raised an exception");
    return false;
  }
  spdlog::debug("[Hotkeys] requested planned system warp; node={} fleet_state={}", *node_id,
                static_cast<int>(fleet->CurrentState));
  return true;
}
