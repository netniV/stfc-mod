#include "config.h"

#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <prime/Camera.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <cmath>
#include <cstring>

void ApplyHavenRoadDepthBias();

namespace
{
Il2CppClass      *planetary_provider_class = nullptr;
FieldInfo        *blend_source = nullptr, *blend_target = nullptr, *blend_curve = nullptr;
FieldInfo        *blend_minimum = nullptr, *blend_maximum = nullptr, *blend_ratio = nullptr;
FieldInfo        *blend_source_frame = nullptr, *blend_target_frame = nullptr, *blend_result_frame = nullptr;
FieldInfo        *provider_constraint = nullptr, *provider_radius = nullptr, *constraint_radius = nullptr;
FieldInfo        *provider_pivot = nullptr, *provider_look_target = nullptr;
FieldInfo        *radius_enabled = nullptr, *radius_minimum = nullptr, *radius_maximum = nullptr;
FieldInfo        *frame_position = nullptr, *frame_rotation = nullptr, *frame_fov = nullptr;
FieldInfo        *frame_far_clip = nullptr, *frame_orthographic = nullptr;
const MethodInfo *curve_evaluate = nullptr;

struct HavenVector {
  float x, y, z;
};
struct HavenRotation {
  float x, y, z, w;
};

FieldInfo *HavenField(Il2CppClass *cls, const char *name, const char *type)
{
  auto *field = cls != nullptr ? il2cpp_class_get_field_from_name(cls, name) : nullptr;
  return field != nullptr && !(il2cpp_field_get_flags(field) & FIELD_ATTRIBUTE_STATIC)
                 && method_contract::Type(field->type, type)
             ? field
             : nullptr;
}

FieldInfo *HavenReferenceField(Il2CppClass *cls, const char *name, Il2CppClass *expected)
{
  auto *field = cls != nullptr ? il2cpp_class_get_field_from_name(cls, name) : nullptr;
  return field != nullptr && field->type != nullptr && !field->type->byref && expected != nullptr
                 && !(il2cpp_field_get_flags(field) & FIELD_ATTRIBUTE_STATIC) && !il2cpp_class_is_valuetype(expected)
                 && il2cpp_class_from_type(field->type) == expected
             ? field
             : nullptr;
}

template <typename T> T ReadHavenField(Il2CppObject *object, FieldInfo *field)
{
  T value{};
  il2cpp_field_get_value(object, field, &value);
  return value;
}

bool ReadHavenRadius(Il2CppObject *provider, float &radius)
{
  if (provider == nullptr || !il2cpp_class_is_assignable_from(planetary_provider_class, provider->klass))
    return false;
  auto *constraint = ReadHavenField<Il2CppObject *>(provider, provider_constraint);
  auto *data       = constraint != nullptr ? ReadHavenField<Il2CppObject *>(constraint, constraint_radius) : nullptr;
  if (data == nullptr || !ReadHavenField<bool>(data, radius_enabled))
    return false;
  const auto minimum = ReadHavenField<float>(data, radius_minimum);
  const auto maximum = ReadHavenField<float>(data, radius_maximum);
  radius             = ReadHavenField<float>(provider, provider_radius);
  return std::isfinite(radius) && std::isfinite(maximum) && maximum > 0.0f && minimum == maximum
         && std::abs(radius - maximum) < 0.01f;
}

void HavenCamera_UpdateCameraFrame_Hook(auto original, Il2CppObject *provider, Camera *camera)
{
  // Native input, constraints and endpoint updates run first. Only this blend's
  // newly generated output is extended; endpoint assets and normalized LOD stay native.
  original(provider, camera);
  const auto max_zoom = Config::Get().haven_zoom;
  if (provider == nullptr || !std::isfinite(max_zoom) || max_zoom <= 0.0f)
    return;

  auto *source        = ReadHavenField<Il2CppObject *>(provider, blend_source);
  auto *target        = ReadHavenField<Il2CppObject *>(provider, blend_target);
  float source_radius = 0.0f, target_radius = 0.0f;
  if (!ReadHavenRadius(source, source_radius) || !ReadHavenRadius(target, target_radius)
      || source_radius == target_radius || max_zoom <= std::max(source_radius, target_radius))
    return;

  auto *pivot = ReadHavenField<Il2CppObject *>(source, provider_pivot);
  if (pivot == nullptr || ReadHavenField<Il2CppObject *>(target, provider_pivot) != pivot
      || ReadHavenField<Il2CppObject *>(source, provider_look_target) != pivot
      || ReadHavenField<Il2CppObject *>(target, provider_look_target) != pivot)
    return;
  auto      *source_frame = ReadHavenField<Il2CppObject *>(provider, blend_source_frame);
  auto      *target_frame = ReadHavenField<Il2CppObject *>(provider, blend_target_frame);
  auto      *result       = ReadHavenField<Il2CppObject *>(provider, blend_result_frame);
  auto      *curve        = ReadHavenField<Il2CppObject *>(provider, blend_curve);
  const auto minimum      = ReadHavenField<float>(provider, blend_minimum);
  const auto maximum      = ReadHavenField<float>(provider, blend_maximum);
  const auto ratio        = ReadHavenField<float>(provider, blend_ratio);
  if (source_frame == nullptr || target_frame == nullptr || result == nullptr || curve == nullptr
      || source_frame == target_frame || result == source_frame || result == target_frame || !std::isfinite(minimum)
      || !std::isfinite(maximum) || !std::isfinite(ratio) || minimum < 0.0f || maximum > 1.0f || maximum <= minimum)
    return;

  auto       position        = ReadHavenField<HavenVector>(result, frame_position);
  const auto ar              = ReadHavenField<HavenRotation>(source_frame, frame_rotation);
  const auto br              = ReadHavenField<HavenRotation>(target_frame, frame_rotation);
  const auto af              = ReadHavenField<float>(source_frame, frame_fov);
  const auto bf              = ReadHavenField<float>(target_frame, frame_fov);
  const auto rotation_dot    = ar.x * br.x + ar.y * br.y + ar.z * br.z + ar.w * br.w;
  const auto gap             = std::abs(source_radius - target_radius);
  const auto rotation_length = ar.x * ar.x + ar.y * ar.y + ar.z * ar.z + ar.w * ar.w;
  // Qualify the common-pivot, same-facing perspective orbit measured in Haven.
  // Preserve native output when a client changes that geometry or camera mode.
  if (!std::isfinite(rotation_length) || std::abs(rotation_length - 1.0f) > 0.0001f || !std::isfinite(rotation_dot)
      || std::abs(std::abs(rotation_dot) - 1.0f) > 0.0001f || !std::isfinite(af) || !std::isfinite(bf) || af <= 0.0f
      || std::abs(af - bf) > 0.001f || ReadHavenField<bool>(source_frame, frame_orthographic)
      || ReadHavenField<bool>(target_frame, frame_orthographic) || ReadHavenField<bool>(result, frame_orthographic))
    return;

  const auto evaluate =
      reinterpret_cast<float (*)(Il2CppObject *, float, const MethodInfo *)>(curve_evaluate->methodPointer);
  const auto at_ratio   = evaluate(curve, ratio, curve_evaluate);
  const auto at_minimum = evaluate(curve, minimum, curve_evaluate);
  const auto at_maximum = evaluate(curve, maximum, curve_evaluate);
  if (!std::isfinite(at_ratio) || !std::isfinite(at_minimum) || !std::isfinite(at_maximum))
    return;
  const bool source_is_far = source_radius > target_radius;
  const auto outer_weight =
      source_is_far ? 1.0f - std::clamp(at_minimum, 0.0f, 1.0f) : std::clamp(at_maximum, 0.0f, 1.0f);
  const auto inner_weight =
      source_is_far ? 1.0f - std::clamp(at_maximum, 0.0f, 1.0f) : std::clamp(at_minimum, 0.0f, 1.0f);
  if (outer_weight <= inner_weight)
    return;
  const auto weight       = source_is_far ? 1.0 - std::clamp(at_ratio, 0.0f, 1.0f) : std::clamp(at_ratio, 0.0f, 1.0f);
  const auto inner_radius = std::min(source_radius, target_radius);
  const auto native_outer = inner_radius + gap * outer_weight;
  const auto native_distance = inner_radius + gap * weight;
  const auto progress        = std::clamp((weight - inner_weight) / (outer_weight - inner_weight), 0.0, 1.0);
  const auto extra           = std::min(max_zoom - native_distance, (max_zoom - native_outer) * progress);
  if (!std::isfinite(extra))
    return;
  ApplyHavenRoadDepthBias();
  if (extra <= 0.0)
    return;

  // Sequential endpoint updates can cache different positions of their shared
  // pivot during pan. Use the camera back axis so expansion remains steady.
  position.x += static_cast<float>(-2.0 * (ar.x * ar.z + ar.w * ar.y) * extra);
  position.y += static_cast<float>(-2.0 * (ar.y * ar.z - ar.w * ar.x) * extra);
  position.z += static_cast<float>((2.0 * (ar.x * ar.x + ar.y * ar.y) - 1.0) * extra);
  auto clip = ReadHavenField<float>(result, frame_far_clip);
  if (!std::isfinite(clip) || clip <= 0.0f)
    return;
  clip += static_cast<float>(extra);
  if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || !std::isfinite(clip)
      || clip <= 0.0f)
    return;
  il2cpp_field_set_value(result, frame_position, &position);
  il2cpp_field_set_value(result, frame_far_clip, &clip);
  static bool reported = false;
  if (!reported) {
    spdlog::info("[HavenZoom] expanded native outer distance {} to maximum {}", native_outer, max_zoom);
    reported = true;
  }
}
} // namespace

void InstallHavenZoomHooks()
{
  auto blend = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "BlendFrameProvider");
  auto frame_provider = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "FrameProvider");
  auto orbit =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "AbstractOrbitFrameProvider");
  auto target = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "AbstractTargetController");
  auto planetary =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "PlanetaryOrbitFrameProvider");
  auto orbit_constraint =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "OrbitConstraint");
  auto constraint = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "Constraint");
  auto frame      = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "CameraFrame");
  auto editable   = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.CameraController", "EditableCameraFrame");
  auto curve      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "AnimationCurve");
  Il2CppClass *radius_class = nullptr;
  if (constraint.get_cls() != nullptr) {
    void *iterator = nullptr;
    while (auto *nested = il2cpp_class_get_nested_types(constraint.get_cls(), &iterator)) {
      if (std::strcmp(il2cpp_class_get_name(nested), "FloatData") == 0) {
        radius_class = nested;
        break;
      }
    }
  }
  const auto *update =
      method_contract::Resolve(blend.get_cls(), "UpdateCameraFrame", false, "System.Void", {"UnityEngine.Camera"});
  curve_evaluate = method_contract::Resolve(curve.get_cls(), "Evaluate", false, "System.Single", {"System.Single"});
  planetary_provider_class = planetary.get_cls();
  blend_source             = HavenReferenceField(blend.get_cls(), "_sourceFrameProvider", frame_provider.get_cls());
  blend_target             = HavenReferenceField(blend.get_cls(), "_targetFrameProvider", frame_provider.get_cls());
  blend_curve              = HavenReferenceField(blend.get_cls(), "_blendCurve", curve.get_cls());
  blend_minimum            = HavenField(blend.get_cls(), "_softLimitMin", "System.Single");
  blend_maximum            = HavenField(blend.get_cls(), "_softLimitMax", "System.Single");
  blend_ratio              = HavenField(blend.get_cls(), "_blendRatio", "System.Single");
  blend_source_frame       = HavenReferenceField(blend.get_cls(), "_sourceFrame", frame.get_cls());
  blend_target_frame       = HavenReferenceField(blend.get_cls(), "_targetFrame", frame.get_cls());
  blend_result_frame       = HavenReferenceField(blend.get_cls(), "_blendedCameraFrame", editable.get_cls());
  provider_constraint      = HavenReferenceField(orbit.get_cls(), "_constraint", orbit_constraint.get_cls());
  provider_radius          = HavenField(orbit.get_cls(), "_radius", "System.Single");
  provider_pivot           = HavenReferenceField(orbit.get_cls(), "_orbitPivot", target.get_cls());
  provider_look_target     = HavenReferenceField(orbit.get_cls(), "_lookTarget", target.get_cls());
  constraint_radius        = HavenReferenceField(orbit_constraint.get_cls(), "_radius", radius_class);
  radius_enabled           = HavenField(radius_class, "_enabled", "System.Boolean");
  radius_minimum           = HavenField(radius_class, "_min", "System.Single");
  radius_maximum           = HavenField(radius_class, "_max", "System.Single");
  frame_position           = HavenField(frame.get_cls(), "_position", "UnityEngine.Vector3");
  frame_rotation           = HavenField(frame.get_cls(), "_rotation", "UnityEngine.Quaternion");
  frame_fov                = HavenField(frame.get_cls(), "_FOV", "System.Single");
  frame_far_clip           = HavenField(frame.get_cls(), "_farPlane", "System.Single");
  frame_orthographic       = HavenField(frame.get_cls(), "_orthographic", "System.Boolean");
  if (update == nullptr || update->has_full_generic_sharing_signature || curve_evaluate == nullptr
      || curve_evaluate->has_full_generic_sharing_signature || planetary_provider_class == nullptr
      || orbit.get_cls() == nullptr || !il2cpp_class_is_assignable_from(orbit.get_cls(), planetary_provider_class)) {
    spdlog::warn("[HavenZoom] camera API unavailable; keeping native zoom range");
    return;
  }
  for (auto *field :
       {blend_source,       blend_target,        blend_curve,          blend_minimum,      blend_maximum,
        blend_ratio,        provider_pivot,      provider_look_target, blend_source_frame, blend_target_frame,
        blend_result_frame, provider_constraint, provider_radius,      constraint_radius,  radius_enabled,
        radius_minimum,     radius_maximum,      frame_position,       frame_rotation,     frame_fov,
        frame_far_clip,     frame_orthographic}) {
    if (field == nullptr) {
      spdlog::warn("[HavenZoom] camera fields unavailable; keeping native zoom range");
      return;
    }
  }
  // [patches].havenzoomhooks controls installation independently of haven_zoom.
  if (SPUD_STATIC_DETOUR(update->methodPointer, HavenCamera_UpdateCameraFrame_Hook)) {
    spdlog::info("[HavenZoom] installed planetary blend camera hook (maximum distance {})", Config::Get().haven_zoom);
  } else {
    spdlog::warn("[HavenZoom] camera hook was not installed; keeping native zoom range");
  }
}
