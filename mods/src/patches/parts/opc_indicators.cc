#include "config.h"
#include "errormsg.h"

#include <il2cpp-tabledefs.h>
#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>
#include <prime/FleetPlayerData.h>
#include <prime/GameObject.h>
#include <prime/Transform.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace
{
constexpr const char* kOpcHighlightName     = "CommunityOPCHighlight";
constexpr const char* kOpcEtaLabelName      = "CommunityOpcEtaLabel";
constexpr const char* kOpcEtaBackgroundName = "CommunityOpcEtaBackground";
constexpr const char* kOpcCardLabelName     = "CommunityOpcCardEtaLabel";
constexpr int         kFleetSlotCount       = 10;
constexpr int64_t     kOpcEtaRefreshMs      = 1'000;
constexpr int64_t     kUiRetryInitialMs     = 1'000;
constexpr int64_t     kUiRetryMaximumMs     = 30'000;

struct OpcEtaLogState {
  uint64_t    fleet_id = 0;
  std::string display;
};

struct OpcEtaRenderState {
  uint64_t    fleet_id = 0;
  std::string computed_display;
  std::string computed_card_display;
  std::string rendered_display;
  FleetState  fleet_state           = FleetState::Unknown;
  bool        selected              = false;
  bool        computed_safe         = false;
  bool        rendered_safe_on_node = false;
  bool        fleet_state_known     = false;
  bool        layout_initialized    = false;
  uint8_t     setup_failures        = 0;
  int64_t     setup_retry_at_ms     = 0;
};

struct OpcCardRenderState {
  uint64_t    fleet_id        = 0;
  uintptr_t   timer_anchor_id = 0;
  std::string display;
  bool        layout_initialized = false;
  uint8_t     setup_failures     = 0;
  int64_t     setup_retry_at_ms  = 0;
};

struct UiVector2 {
  float x;
  float y;
};

std::array<OpcEtaLogState, kFleetSlotCount>    s_last_opc_eta_log_states{};
std::array<OpcEtaRenderState, kFleetSlotCount> s_opc_eta_render_states{};
std::array<uint64_t, kFleetSlotCount>          s_last_opc_eta_refresh_fleet_ids{};
std::array<int64_t, kFleetSlotCount>           s_last_opc_eta_refresh_ms{};
std::array<uint8_t, kFleetSlotCount>           s_opc_highlight_setup_failures{};
std::array<int64_t, kFleetSlotCount>           s_opc_highlight_retry_at_ms{};
std::array<uint64_t, kFleetSlotCount>          s_opc_highlight_retry_fleet_ids{};
std::array<uintptr_t, kFleetSlotCount>         s_opc_highlight_retry_anchor_ids{};
bool                                           s_highlight_enabled = false;
bool                                           s_eta_enabled       = false;
OpcCardRenderState                             s_opc_card_render_state{};

struct Color {
  float r;
  float g;
  float b;
  float a;
};

struct FleetOpcStatus {
  bool    mining           = false;
  bool    cargo_known      = false;
  bool    rate_known       = false;
  bool    node_known       = false;
  bool    opc              = false;
  bool    safe_on_node     = false;
  double  current_cargo    = 0.0;
  double  protected_limit  = 0.0;
  double  rate_per_second  = 0.0;
  double  node_remaining   = 0.0;
  int64_t eta_seconds      = -1;
  int64_t node_eta_seconds = -1;
};

constexpr bool cargo_is_opc(double current_value, double protected_limit)
{ return current_value > protected_limit; }

constexpr bool node_stays_protected(double current_value, double protected_limit, double node_remaining)
{ return !cargo_is_opc(current_value, protected_limit) && node_remaining <= protected_limit - current_value; }

static_assert(!cargo_is_opc(99.0, 100.0));
static_assert(!cargo_is_opc(100.0, 100.0));
static_assert(cargo_is_opc(101.0, 100.0));
static_assert(node_stays_protected(99.0, 100.0, 1.0));
static_assert(!node_stays_protected(99.0, 100.0, 2.0));

int64_t steady_now_milliseconds()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void schedule_ui_retry(uint8_t& failure_count, int64_t& retry_at_ms)
{
  if (failure_count < 6) {
    ++failure_count;
  }
  auto delay_ms = kUiRetryInitialMs << (failure_count - 1);
  if (delay_ms > kUiRetryMaximumMs) {
    delay_ms = kUiRetryMaximumMs;
  }
  retry_at_ms = steady_now_milliseconds() + delay_ms;
}

void clear_ui_retry(uint8_t& failure_count, int64_t& retry_at_ms)
{
  failure_count = 0;
  retry_at_ms   = 0;
}

bool read_opc(FleetPlayerData* fleet, bool& known)
{
  known             = false;
  auto* cargo_hold  = fleet ? fleet->CargoHoldData : nullptr;
  auto* unprotected = cargo_hold ? cargo_hold->UnprotectedCargoProgress : nullptr;
  if (!unprotected) {
    return false;
  }

  const auto current_value   = unprotected->CurrentValue;
  const auto protected_limit = unprotected->MinValue;
  if (!std::isfinite(current_value) || !std::isfinite(protected_limit)) {
    return false;
  }

  known = true;
  return cargo_is_opc(current_value, protected_limit);
}

FleetOpcStatus read_opc_status(FleetPlayerData* fleet)
{
  FleetOpcStatus status;
  if (!fleet || fleet->CurrentState != FleetState::Mining) {
    return status;
  }

  status.mining     = true;
  auto* cargo_hold  = fleet->CargoHoldData;
  auto* unprotected = cargo_hold ? cargo_hold->UnprotectedCargoProgress : nullptr;
  if (unprotected) {
    status.current_cargo   = unprotected->CurrentValue;
    status.protected_limit = unprotected->MinValue;
    status.cargo_known     = std::isfinite(status.current_cargo) && std::isfinite(status.protected_limit);
    status.opc             = status.cargo_known && cargo_is_opc(status.current_cargo, status.protected_limit);
  }

  auto* mining_data      = fleet->MiningData;
  status.rate_per_second = mining_data ? mining_data->MiningSpeed : 0.0;
  status.rate_known      = std::isfinite(status.rate_per_second) && status.rate_per_second > 0.0;

  if (mining_data) {
    const auto node_current = mining_data->CurrentValue;
    const auto node_max     = mining_data->MaxValue;
    status.node_known =
        std::isfinite(node_current) && std::isfinite(node_max) && node_current >= 0.0 && node_max >= node_current;
    if (status.node_known) {
      status.node_remaining = node_max - node_current;
    }
  }

  if (status.rate_known && status.node_known) {
    const auto node_seconds = std::ceil(status.node_remaining / status.rate_per_second);
    if (std::isfinite(node_seconds) && node_seconds <= static_cast<double>(std::numeric_limits<int64_t>::max())) {
      status.node_eta_seconds = static_cast<int64_t>(node_seconds);
    }
  }

  if (!status.cargo_known || status.opc || !status.rate_known) {
    status.eta_seconds = status.opc ? 0 : -1;
    return status;
  }

  const auto remaining = std::max(0.0, status.protected_limit - status.current_cargo);
  const auto seconds   = std::ceil(remaining / status.rate_per_second);
  if (std::isfinite(seconds) && seconds <= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    status.eta_seconds = static_cast<int64_t>(seconds);
  }
  if (status.node_eta_seconds >= 0) {
    status.safe_on_node = node_stays_protected(status.current_cargo, status.protected_limit, status.node_remaining);
  }
  return status;
}

std::string format_duration(int64_t seconds)
{
  if (seconds < 0) {
    return {};
  }
  if (seconds < 60) {
    return "<1m";
  }

  const auto minutes = seconds / 60 + (seconds % 60 != 0);
  if (minutes < 60) {
    return std::to_string(minutes) + "m";
  }

  const auto hours             = minutes / 60;
  const auto remaining_minutes = minutes % 60;
  if (hours < 24) {
    auto result = std::to_string(hours) + "h";
    if (remaining_minutes != 0) {
      result += " " + std::to_string(remaining_minutes) + "m";
    }
    return result;
  }

  const auto days            = hours / 24;
  const auto remaining_hours = hours % 24;
  auto       result          = std::to_string(days) + "d";
  if (remaining_hours != 0) {
    result += " " + std::to_string(remaining_hours) + "h";
  }
  return result;
}

std::string format_opc_eta(const FleetOpcStatus& status)
{
  if (!status.mining || !status.cargo_known) {
    return {};
  }
  if (status.opc) {
    return "OPC";
  }
  if (!status.rate_known || status.eta_seconds < 0) {
    return {};
  }
  return status.safe_on_node ? "SAFE" : format_duration(status.eta_seconds);
}

std::string format_opc_card_display(const FleetOpcStatus& status)
{
  if (!status.mining || !status.cargo_known) {
    return {};
  }

  const auto opc = status.opc ? "Now" : status.eta_seconds >= 0 ? format_duration(status.eta_seconds) : "Unknown";
  return "OPC: " + opc;
}

Il2CppObject* invoke(const MethodInfo* method, void* target, void** args, const char* operation)
{
  if (!method) {
    return nullptr;
  }

  Il2CppException* exception = nullptr;
  auto*            result    = il2cpp_runtime_invoke(method, target, args, &exception);
  if (exception) {
    spdlog::warn("[OpcIndicators] {} failed", operation);
    return nullptr;
  }
  return result;
}

bool invoke_void(const MethodInfo* method, void* target, void** args, const char* operation)
{
  if (!method) {
    return false;
  }

  Il2CppException* exception = nullptr;
  il2cpp_runtime_invoke(method, target, args, &exception);
  if (exception) {
    spdlog::warn("[OpcIndicators] {} failed", operation);
    return false;
  }
  return true;
}

const MethodInfo* resolve_instance_void(IL2CppClassHelper& helper, const char* name, int parameter_count)
{
  auto* method = helper.GetMethodInfoSpecial(
      name, [parameter_count](int count, const Il2CppType**) { return count == parameter_count; });
  return method && !(method->flags & METHOD_ATTRIBUTE_STATIC) && method->methodPointer && method->return_type
                 && method->return_type->type == IL2CPP_TYPE_VOID
             ? method
             : nullptr;
}

void destroy_game_object(GameObject* game_object)
{
  if (!game_object) {
    return;
  }

  game_object->SetActive(false);
  static auto object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
  static auto destroy       = object_helper.GetMethodInfo("Destroy", 1);
  void*       args[1]       = {game_object};
  invoke_void(destroy, nullptr, args, "Object.Destroy");
}

Transform* component_transform(void* component)
{
  if (!component) {
    return nullptr;
  }

  static auto component_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
  static auto get_transform    = component_helper.GetMethodInfo("get_transform");
  return reinterpret_cast<Transform*>(invoke(get_transform, component, nullptr, "Component.get_transform"));
}

Transform* direct_child_named(Transform* parent, const char* name)
{
  if (!parent) {
    return nullptr;
  }

  for (int32_t index = 0; index < parent->childCount; ++index) {
    auto* child        = parent->GetChild(index);
    auto* child_object = child ? child->gameObject : nullptr;
    if (child_object && child_object->Name() == name) {
      return child;
    }
  }
  return nullptr;
}

Transform* opc_anchor_from_tile(Transform* tile_transform)
{ return direct_child_named(tile_transform, "BodyContainer"); }

void* component_in_parent(void* component, IL2CppClassHelper& target_helper)
{
  auto* transform   = component_transform(component);
  auto* game_object = transform ? transform->gameObject : nullptr;
  if (!game_object) {
    return nullptr;
  }

  static auto game_object_helper      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto get_component_in_parent = game_object_helper.GetMethodInfo("GetComponentInParent", 2);
  if (!target_helper.isValidHelper() || !get_component_in_parent) {
    return nullptr;
  }

  void* target_type      = target_helper.GetType();
  bool  include_inactive = true;
  void* args[2]          = {target_type, &include_inactive};
  return invoke(get_component_in_parent, game_object, args, "GameObject.GetComponentInParent");
}

Transform* opc_anchor_from_component(void* component)
{
  static auto fleet_tile_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Ships", "FleetLocalViewController");
  auto* fleet_tile = component_in_parent(component, fleet_tile_helper);
  return opc_anchor_from_tile(component_transform(fleet_tile));
}

bool is_instance_class_field(const FieldInfo* field, std::string_view expected_namespace,
                             std::string_view expected_name)
{
  if (!field || !field->type || field->type->byref || field->type->type != IL2CPP_TYPE_CLASS
      || (il2cpp_field_get_flags(const_cast<FieldInfo*>(field)) & FIELD_ATTRIBUTE_STATIC) != 0
      || field->offset < static_cast<int32_t>(sizeof(Il2CppObject))) {
    return false;
  }

  auto* field_class = il2cpp_class_from_type(field->type);
  return field_class && field_class->namespaze && field_class->name && field_class->namespaze == expected_namespace
         && field_class->name == expected_name;
}

void* fleet_panel_controller(void* component)
{
  if (!component) {
    return nullptr;
  }

  static auto fleet_bar_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetBarViewController");
  static auto* panel_field = fleet_bar_helper.get_cls()
                                 ? il2cpp_class_get_field_from_name(fleet_bar_helper.get_cls(), "_fleetPanelController")
                                 : nullptr;
  auto*        fleet_bar   = component_in_parent(component, fleet_bar_helper);
  if (!fleet_bar || !is_instance_class_field(panel_field, "Digit.Prime.Ships", "FleetLocalViewController")) {
    return nullptr;
  }
  return *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(fleet_bar) + panel_field->offset);
}

Transform* fleet_panel_timer_anchor(void* component)
{
  auto* fleet_panel = fleet_panel_controller(component);
  if (!fleet_panel) {
    return nullptr;
  }

  static auto fleet_local_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Ships", "FleetLocalViewController");
  static auto* timer_widget_field = fleet_local_helper.get_cls()
                                        ? il2cpp_class_get_field_from_name(fleet_local_helper.get_cls(), "_timerWidget")
                                        : nullptr;
  if (!is_instance_class_field(timer_widget_field, "Digit.Client.UI", "TimerWidget")) {
    return nullptr;
  }
  auto* timer_widget =
      *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(fleet_panel) + timer_widget_field->offset);
  return component_transform(timer_widget);
}

Transform* opc_anchor_from_fleetbar_flag(void* fleetbar_flag_widget)
{ return opc_anchor_from_component(fleetbar_flag_widget); }

GameObject* find_opc_highlight(Transform* body_transform)
{
  auto* transform = direct_child_named(body_transform, kOpcHighlightName);
  return transform ? transform->gameObject : nullptr;
}

bool configure_opc_frame(Transform* transform)
{
  if (!transform) {
    return false;
  }

  static auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto image_helper       = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  static auto get_component      = game_object_helper.GetMethodInfo("GetComponent", 1);
  static auto set_color          = image_helper.GetMethodInfo("set_color");
  static auto set_raycast_target = image_helper.GetMethodInfo("set_raycastTarget");
  if (!image_helper.isValidHelper() || !get_component || !set_color || !set_raycast_target) {
    return false;
  }

  bool  tinted_background = false;
  auto* game_object       = transform->gameObject;
  void* image_type        = image_helper.GetType();
  void* component_args[1] = {image_type};
  if (auto* image =
          game_object ? invoke(get_component, game_object, component_args, "GameObject.GetComponent<Image>") : nullptr;
      image) {
    Color color{1.0f, 0.66f, 0.12f, 1.0f};
    void* color_args[1]   = {&color};
    bool  raycast         = false;
    void* raycast_args[1] = {&raycast};
    tinted_background     = invoke_void(set_color, image, color_args, "Image.set_color")
                            && invoke_void(set_raycast_target, image, raycast_args, "Graphic.set_raycastTarget");
  }

  for (int32_t index = 0; index < transform->childCount; ++index) {
    if (auto* child = transform->GetChild(index); child && child->gameObject) {
      child->gameObject->SetActive(false);
    }
  }
  return tinted_background;
}

bool configure_opc_inner(Transform* highlight_transform, Transform* body_transform)
{
  auto* inner_transform  = direct_child_named(highlight_transform, "WaveDefenseBackground");
  auto* source_transform = direct_child_named(body_transform, "MiningContainer");
  auto* inner_object     = inner_transform ? inner_transform->gameObject : nullptr;
  auto* source_object    = source_transform ? source_transform->gameObject : nullptr;
  if (!inner_object || !source_object) {
    return false;
  }

  static auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto image_helper       = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  static auto get_component      = game_object_helper.GetMethodInfo("GetComponent", 1);
  static auto get_sprite         = image_helper.GetMethodInfo("get_sprite");
  static auto set_sprite         = image_helper.GetMethodInfo("set_sprite");
  static auto set_type           = image_helper.GetMethodInfo("set_type");
  static auto set_fill_amount    = image_helper.GetMethodInfo("set_fillAmount");
  static auto set_color          = image_helper.GetMethodInfo("set_color");
  static auto set_raycast_target = image_helper.GetMethodInfo("set_raycastTarget");
  if (!image_helper.isValidHelper() || !get_component || !get_sprite || !set_sprite || !set_type || !set_fill_amount
      || !set_color || !set_raycast_target) {
    return false;
  }

  void* image_type        = image_helper.GetType();
  void* component_args[1] = {image_type};
  auto* inner_image       = invoke(get_component, inner_object, component_args, "GameObject.GetComponent<Image>");
  auto* source_image      = invoke(get_component, source_object, component_args, "GameObject.GetComponent<Image>");
  auto* source_sprite     = source_image ? invoke(get_sprite, source_image, nullptr, "Image.get_sprite") : nullptr;
  if (!inner_image || !source_sprite) {
    return false;
  }

  void*      sprite_args[1] = {source_sprite};
  int32_t    filled         = 3;
  void*      type_args[1]   = {&filled};
  float      fill_amount    = 1.0f;
  void*      fill_args[1]   = {&fill_amount};
  Color      color{0.78f, 0.40f, 0.06f, 0.62f};
  void*      color_args[1]   = {&color};
  bool       raycast         = false;
  void*      raycast_args[1] = {&raycast};
  const bool configured = invoke_void(set_sprite, inner_image, sprite_args, "Image.set_sprite")
                          && invoke_void(set_type, inner_image, type_args, "Image.set_type")
                          && invoke_void(set_fill_amount, inner_image, fill_args, "Image.set_fillAmount")
                          && invoke_void(set_color, inner_image, color_args, "Image.set_color")
                          && invoke_void(set_raycast_target, inner_image, raycast_args, "Graphic.set_raycastTarget");
  inner_object->SetActive(configured);
  return configured;
}

GameObject* create_opc_highlight(Transform* body_transform)
{
  auto* source_transform = direct_child_named(body_transform, "Background");
  auto* source_object    = source_transform ? source_transform->gameObject : nullptr;
  if (!source_object) {
    return nullptr;
  }

  static auto object_helper      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
  static auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto transform_helper   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  static auto instantiate = object_helper.GetMethodInfoSpecial("Instantiate", [](int count, const Il2CppType** params) {
    return count == 3 && params[0]->type == IL2CPP_TYPE_CLASS && params[1]->type == IL2CPP_TYPE_CLASS
           && params[2]->type == IL2CPP_TYPE_BOOLEAN;
  });
  static auto get_transform     = game_object_helper.GetMethodInfo("get_transform");
  static auto set_name          = object_helper.GetMethodInfo("set_name");
  static auto set_sibling_index = transform_helper.GetMethodInfo("SetSiblingIndex");

  if (!object_helper.isValidHelper() || !game_object_helper.isValidHelper() || !transform_helper.isValidHelper()
      || !instantiate || !get_transform || !set_name || !set_sibling_index) {
    spdlog::warn("[OpcIndicators] required Unity methods are unavailable; OPC highlight was not created");
    return nullptr;
  }

  bool  world_position_stays = false;
  void* instantiate_args[3]  = {source_object, body_transform, &world_position_stays};
  auto* highlight = reinterpret_cast<GameObject*>(invoke(instantiate, nullptr, instantiate_args, "Object.Instantiate"));
  if (!highlight) {
    return nullptr;
  }

  const auto highlight_handle = il2cpp_gchandle_new(reinterpret_cast<Il2CppObject*>(highlight), false);
  if (!highlight_handle) {
    destroy_game_object(highlight);
    return nullptr;
  }

  highlight->SetActive(false);
  auto* highlight_transform =
      reinterpret_cast<Transform*>(invoke(get_transform, highlight, nullptr, "GameObject.get_transform"));
  if (!configure_opc_frame(highlight_transform) || !configure_opc_inner(highlight_transform, body_transform)) {
    destroy_game_object(highlight);
    il2cpp_gchandle_free(highlight_handle);
    return nullptr;
  }

  int32_t sibling_index = 0;
  for (; sibling_index < body_transform->childCount; ++sibling_index) {
    if (body_transform->GetChild(sibling_index) == source_transform) {
      break;
    }
  }
  void* sibling_args[1] = {&sibling_index};
  void* name_args[1]    = {il2cpp_string_new(kOpcHighlightName)};
  // Publish the discoverable name only after setup succeeds; Unity destruction is deferred until frame end.
  if (sibling_index >= body_transform->childCount
      || !invoke_void(set_sibling_index, highlight_transform, sibling_args, "Transform.SetSiblingIndex")
      || !invoke_void(set_name, highlight, name_args, "Object.set_name")) {
    destroy_game_object(highlight);
    il2cpp_gchandle_free(highlight_handle);
    return nullptr;
  }

  auto* result = reinterpret_cast<GameObject*>(il2cpp_gchandle_get_target(highlight_handle));
  il2cpp_gchandle_free(highlight_handle);
  spdlog::debug("[OpcIndicators] created fleet-bar OPC highlight");
  return result;
}

constexpr int32_t kCurrentDeployedStateMask = 8133;

constexpr bool is_deployed(FleetState state)
{ return (static_cast<int32_t>(state) & kCurrentDeployedStateMask) != 0; }

static_assert(is_deployed(FleetState::Mining));
static_assert(is_deployed(FleetState::Warping));
static_assert(!is_deployed(FleetState::Docked));
static_assert(!is_deployed(FleetState::Destroyed));

void update_opc_highlight(Transform* body_transform, FleetPlayerData* fleet)
{
  if (!body_transform) {
    return;
  }

  auto* highlight = find_opc_highlight(body_transform);
  if (!fleet || fleet->Index < 0 || fleet->Index >= kFleetSlotCount) {
    if (highlight) {
      highlight->SetActive(false);
    }
    return;
  }

  const auto slot = fleet->Index;
  // The address is an identity token only; retaining it never implies that the Unity object is still live.
  const auto anchor_id = reinterpret_cast<uintptr_t>(body_transform);
  if (s_opc_highlight_retry_fleet_ids[slot] != fleet->Id || s_opc_highlight_retry_anchor_ids[slot] != anchor_id) {
    s_opc_highlight_retry_fleet_ids[slot]  = fleet->Id;
    s_opc_highlight_retry_anchor_ids[slot] = anchor_id;
    clear_ui_retry(s_opc_highlight_setup_failures[slot], s_opc_highlight_retry_at_ms[slot]);
  }
  bool       known = false;
  const bool show  = fleet->HasShip && is_deployed(fleet->CurrentState) && read_opc(fleet, known);
  if (!show) {
    clear_ui_retry(s_opc_highlight_setup_failures[slot], s_opc_highlight_retry_at_ms[slot]);
    if (highlight) {
      highlight->SetActive(false);
    }
    return;
  }

  if (s_opc_highlight_retry_at_ms[slot] > steady_now_milliseconds()) {
    if (highlight) {
      highlight->SetActive(false);
    }
    return;
  }

  if (!highlight) {
    highlight = create_opc_highlight(body_transform);
    if (!highlight) {
      schedule_ui_retry(s_opc_highlight_setup_failures[slot], s_opc_highlight_retry_at_ms[slot]);
      return;
    }
    clear_ui_retry(s_opc_highlight_setup_failures[slot], s_opc_highlight_retry_at_ms[slot]);
  }
  highlight->SetActive(true);
}

Transform* fleet_state_widget_label_anchor(void* fleet_state_widget)
{ return opc_anchor_from_component(fleet_state_widget); }

bool fleet_tile_is_selected(void* fleet_state_widget, FleetPlayerData* fleet)
{
  if (!fleet_state_widget || !fleet) {
    return false;
  }

  static auto fleet_bar_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetBarViewController");
  static auto is_index_selected = fleet_bar_helper.GetMethod<bool(void*, int32_t)>("IsIndexSelected", 1);
  auto*       fleet_bar         = component_in_parent(fleet_state_widget, fleet_bar_helper);
  return fleet_bar && is_index_selected && is_index_selected(fleet_bar, fleet->Index);
}

bool configure_opc_eta_label(void* label, Transform* transform, bool selected, bool safe_on_node)
{
  if (!label || !transform) {
    return false;
  }

  static auto rect_helper      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  static auto anchor_min       = rect_helper.GetMethodInfo("set_anchorMin");
  static auto anchor_max       = rect_helper.GetMethodInfo("set_anchorMax");
  static auto pivot_method     = rect_helper.GetMethodInfo("set_pivot");
  static auto size_delta       = rect_helper.GetMethodInfo("set_sizeDelta");
  static auto position         = rect_helper.GetMethodInfo("set_anchoredPosition");
  static auto transform_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  static auto last_sibling     = transform_helper.GetMethodInfo("SetAsLastSibling");
  static auto text_helper      = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TMP_Text");
  static auto set_font_size    = text_helper.GetMethodInfo("set_fontSize", 1);
  static auto set_alignment    = text_helper.GetMethodInfo("set_alignment", 1);
  static auto graphic_helper   = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Graphic");
  static auto set_raycast      = graphic_helper.GetMethodInfo("set_raycastTarget", 1);
  static auto set_color        = graphic_helper.GetMethodInfo("set_color", 1);
  if (!anchor_min || !anchor_max || !pivot_method || !size_delta || !position || !last_sibling || !set_font_size
      || !set_alignment || !set_raycast || !set_color) {
    return false;
  }

  UiVector2 top_center{0.5f, 1.0f};
  UiVector2 bottom_center{0.5f, 0.0f};
  UiVector2 size{148.0f, 36.0f};
  UiVector2 offset{0.0f, selected ? 24.0f : 14.0f};
  float     font_size         = 18.0f;
  int32_t   alignment         = 514; // TMPro.TextAlignmentOptions.Center
  bool      raycast_target    = false;
  Color     text_color        = safe_on_node ? Color{0.45f, 0.86f, 1.0f, 1.0f} : Color{1.0f, 0.69f, 0.13f, 1.0f};
  void*     top_args[1]       = {&top_center};
  void*     bottom_args[1]    = {&bottom_center};
  void*     size_args[1]      = {&size};
  void*     offset_args[1]    = {&offset};
  void*     font_args[1]      = {&font_size};
  void*     alignment_args[1] = {&alignment};
  void*     raycast_args[1]   = {&raycast_target};
  void*     color_args[1]     = {&text_color};
  return invoke_void(anchor_min, transform, top_args, "RectTransform.set_anchorMin")
         && invoke_void(anchor_max, transform, top_args, "RectTransform.set_anchorMax")
         && invoke_void(pivot_method, transform, bottom_args, "RectTransform.set_pivot")
         && invoke_void(size_delta, transform, size_args, "RectTransform.set_sizeDelta")
         && invoke_void(position, transform, offset_args, "RectTransform.set_anchoredPosition")
         && invoke_void(last_sibling, transform, nullptr, "Transform.SetAsLastSibling")
         && invoke_void(set_font_size, label, font_args, "TMP_Text.set_fontSize")
         && invoke_void(set_alignment, label, alignment_args, "TMP_Text.set_alignment")
         && invoke_void(set_raycast, label, raycast_args, "Graphic.set_raycastTarget")
         && invoke_void(set_color, label, color_args, "Graphic.set_color");
}

bool configure_opc_card_label(void* label, Transform* transform)
{
  if (!label || !transform) {
    return false;
  }

  static auto rect_helper      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  static auto anchor_min       = rect_helper.GetMethodInfo("set_anchorMin");
  static auto anchor_max       = rect_helper.GetMethodInfo("set_anchorMax");
  static auto pivot_method     = rect_helper.GetMethodInfo("set_pivot");
  static auto size_delta       = rect_helper.GetMethodInfo("set_sizeDelta");
  static auto position         = rect_helper.GetMethodInfo("set_anchoredPosition");
  static auto transform_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  static auto last_sibling     = transform_helper.GetMethodInfo("SetAsLastSibling");
  static auto text_helper      = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TMP_Text");
  static auto set_font_size    = text_helper.GetMethodInfo("set_fontSize", 1);
  static auto set_alignment    = text_helper.GetMethodInfo("set_alignment", 1);
  static auto graphic_helper   = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Graphic");
  static auto set_raycast      = graphic_helper.GetMethodInfo("set_raycastTarget", 1);
  static auto set_color        = graphic_helper.GetMethodInfo("set_color", 1);
  if (!anchor_min || !anchor_max || !pivot_method || !size_delta || !position || !last_sibling || !set_font_size
      || !set_alignment || !set_raycast || !set_color) {
    return false;
  }

  // Runtime probe: the expanded panel's TimerContainer is 280x49.8 and its native Timer is centered at x=7.6.
  // Place this single line just beneath the native 43.4-high timer without changing the panel's own layout.
  UiVector2 center{0.5f, 0.5f};
  UiVector2 size{190.0f, 26.0f};
  UiVector2 offset{7.6f, -39.0f};
  float     font_size      = 18.0f;
  int32_t   alignment      = 514; // TMPro.TextAlignmentOptions.Center
  bool      raycast_target = false;
  Color     text_color{0.82f, 0.91f, 0.96f, 1.0f};
  void*     anchor_args[1]    = {&center};
  void*     pivot_args[1]     = {&center};
  void*     size_args[1]      = {&size};
  void*     offset_args[1]    = {&offset};
  void*     font_args[1]      = {&font_size};
  void*     alignment_args[1] = {&alignment};
  void*     raycast_args[1]   = {&raycast_target};
  void*     color_args[1]     = {&text_color};
  return invoke_void(anchor_min, transform, anchor_args, "RectTransform.set_anchorMin")
         && invoke_void(anchor_max, transform, anchor_args, "RectTransform.set_anchorMax")
         && invoke_void(pivot_method, transform, pivot_args, "RectTransform.set_pivot")
         && invoke_void(size_delta, transform, size_args, "RectTransform.set_sizeDelta")
         && invoke_void(position, transform, offset_args, "RectTransform.set_anchoredPosition")
         && invoke_void(last_sibling, transform, nullptr, "Transform.SetAsLastSibling")
         && invoke_void(set_font_size, label, font_args, "TMP_Text.set_fontSize")
         && invoke_void(set_alignment, label, alignment_args, "TMP_Text.set_alignment")
         && invoke_void(set_raycast, label, raycast_args, "Graphic.set_raycastTarget")
         && invoke_void(set_color, label, color_args, "Graphic.set_color");
}

void* find_opc_eta_label(Transform* label_anchor)
{
  auto* label_transform = direct_child_named(label_anchor, kOpcEtaLabelName);
  auto* label_object    = label_transform ? label_transform->gameObject : nullptr;
  if (!label_object) {
    return nullptr;
  }

  static auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto text_helper        = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TextMeshProUGUI");
  static auto get_component      = game_object_helper.GetMethodInfo("GetComponent", 1);
  void*       text_type          = text_helper.GetType();
  void*       args[1]            = {text_type};
  return invoke(get_component, label_object, args, "GameObject.GetComponent<TextMeshProUGUI>");
}

void* create_ui_component(const char* name, Transform* parent, IL2CppClassHelper& component_helper,
                          const char* operation)
{
  if (!parent || !component_helper.isValidHelper()) {
    return nullptr;
  }

  static auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto transform_helper   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  static auto constructor = game_object_helper.GetMethodInfoSpecial(".ctor", [](int count, const Il2CppType** params) {
    return count == 1 && params[0]->type == IL2CPP_TYPE_STRING;
  });
  static auto get_transform = game_object_helper.GetMethodInfo("get_transform");
  static auto set_parent =
      transform_helper.GetMethodInfoSpecial("SetParent", [](int count, const Il2CppType**) { return count == 2; });
  static auto add_component = game_object_helper.GetMethodInfo("AddComponent", 1);
  if (!constructor || !get_transform || !set_parent || !add_component) {
    spdlog::warn("[OpcIndicators] {} Unity methods are unavailable", operation);
    return nullptr;
  }

  auto* game_object = reinterpret_cast<GameObject*>(il2cpp_object_new(game_object_helper.get_cls()));
  if (!game_object) {
    spdlog::warn("[OpcIndicators] {} could not allocate a GameObject", operation);
    return nullptr;
  }
  const auto handle = il2cpp_gchandle_new(reinterpret_cast<Il2CppObject*>(game_object), false);
  if (!handle) {
    return nullptr;
  }

  void* name_args[1] = {il2cpp_string_new(name)};
  if (!invoke_void(constructor, game_object, name_args, "GameObject.ctor")) {
    il2cpp_gchandle_free(handle);
    return nullptr;
  }

  auto* transform = reinterpret_cast<Transform*>(invoke(get_transform, game_object, nullptr, "get_transform"));
  bool  world_position_stays = false;
  void* parent_args[2]       = {parent, &world_position_stays};
  if (!transform || !invoke_void(set_parent, transform, parent_args, "Transform.SetParent")) {
    destroy_game_object(game_object);
    il2cpp_gchandle_free(handle);
    return nullptr;
  }

  void* component_type = component_helper.GetType();
  if (!component_type) {
    destroy_game_object(game_object);
    il2cpp_gchandle_free(handle);
    return nullptr;
  }
  void* component_args[1] = {component_type};
  auto* component         = invoke(add_component, game_object, component_args, operation);
  if (!component) {
    destroy_game_object(game_object);
    il2cpp_gchandle_free(handle);
    return nullptr;
  }

  game_object->SetActive(true);
  il2cpp_gchandle_free(handle);
  return component;
}

GameObject* find_opc_eta_background(Transform* label_anchor)
{
  auto* background_transform = direct_child_named(label_anchor, kOpcEtaBackgroundName);
  return background_transform ? background_transform->gameObject : nullptr;
}

void* opc_eta_background_image(GameObject* background)
{
  if (!background) {
    return nullptr;
  }

  static auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto image_helper       = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  static auto get_component      = game_object_helper.GetMethodInfo("GetComponent", 1);
  void*       image_type         = image_helper.GetType();
  void*       args[1]            = {image_type};
  return invoke(get_component, background, args, "GameObject.GetComponent<Image>");
}

bool configure_opc_eta_background(void* image, Transform* transform, bool selected)
{
  if (!image || !transform) {
    return false;
  }

  static auto rect_helper      = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  static auto anchor_min       = rect_helper.GetMethodInfo("set_anchorMin");
  static auto anchor_max       = rect_helper.GetMethodInfo("set_anchorMax");
  static auto pivot_method     = rect_helper.GetMethodInfo("set_pivot");
  static auto size_delta       = rect_helper.GetMethodInfo("set_sizeDelta");
  static auto position         = rect_helper.GetMethodInfo("set_anchoredPosition");
  static auto transform_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  static auto last_sibling     = transform_helper.GetMethodInfo("SetAsLastSibling");
  static auto image_helper     = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  static auto set_color        = image_helper.GetMethodInfo("set_color", 1);
  static auto set_raycast      = image_helper.GetMethodInfo("set_raycastTarget", 1);
  if (!anchor_min || !anchor_max || !pivot_method || !size_delta || !position || !last_sibling || !set_color
      || !set_raycast) {
    return false;
  }

  UiVector2 top_center{0.5f, 1.0f};
  UiVector2 bottom_center{0.5f, 0.0f};
  UiVector2 size{108.0f, 30.0f};
  UiVector2 offset{0.0f, selected ? 25.0f : 15.0f};
  Color     background{0.035f, 0.075f, 0.10f, 0.86f};
  bool      raycast_target  = false;
  void*     top_args[1]     = {&top_center};
  void*     bottom_args[1]  = {&bottom_center};
  void*     size_args[1]    = {&size};
  void*     offset_args[1]  = {&offset};
  void*     color_args[1]   = {&background};
  void*     raycast_args[1] = {&raycast_target};
  return invoke_void(anchor_min, transform, top_args, "RectTransform.set_anchorMin")
         && invoke_void(anchor_max, transform, top_args, "RectTransform.set_anchorMax")
         && invoke_void(pivot_method, transform, bottom_args, "RectTransform.set_pivot")
         && invoke_void(size_delta, transform, size_args, "RectTransform.set_sizeDelta")
         && invoke_void(position, transform, offset_args, "RectTransform.set_anchoredPosition")
         && invoke_void(last_sibling, transform, nullptr, "Transform.SetAsLastSibling")
         && invoke_void(set_color, image, color_args, "Image.set_color")
         && invoke_void(set_raycast, image, raycast_args, "Image.set_raycastTarget");
}

GameObject* create_opc_eta_background(Transform* label_anchor, bool selected)
{
  if (auto* existing = find_opc_eta_background(label_anchor); existing) {
    if (opc_eta_background_image(existing)) {
      return existing;
    }
    destroy_game_object(existing);
    return nullptr;
  }

  static auto image_helper = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto*       image =
      create_ui_component(kOpcEtaBackgroundName, label_anchor, image_helper, "GameObject.AddComponent<OPC ETA Image>");
  auto* transform   = component_transform(image);
  auto* game_object = transform ? transform->gameObject : nullptr;
  if (!game_object || !configure_opc_eta_background(image, transform, selected)) {
    destroy_game_object(game_object);
    return nullptr;
  }
  return game_object;
}

void* create_opc_eta_label(Transform* label_anchor, bool selected, bool safe_on_node)
{
  if (!label_anchor) {
    return nullptr;
  }
  if (auto* child = direct_child_named(label_anchor, kOpcEtaLabelName); child) {
    if (auto* existing = find_opc_eta_label(label_anchor); existing && component_transform(existing)) {
      return existing;
    }
    destroy_game_object(child->gameObject);
    return nullptr;
  }

  static auto text_helper = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TextMeshProUGUI");
  auto*       label =
      create_ui_component(kOpcEtaLabelName, label_anchor, text_helper, "GameObject.AddComponent<TextMeshProUGUI>");
  auto* transform   = component_transform(label);
  auto* game_object = transform ? transform->gameObject : nullptr;
  if (!game_object || !configure_opc_eta_label(label, transform, selected, safe_on_node)) {
    destroy_game_object(game_object);
    return nullptr;
  }

  spdlog::info("[OpcIndicators] created fleet timer OPC ETA label");
  return label;
}

void* find_opc_card_label(Transform* timer_anchor)
{
  auto* label_transform = direct_child_named(timer_anchor, kOpcCardLabelName);
  auto* label_object    = label_transform ? label_transform->gameObject : nullptr;
  if (!label_object) {
    return nullptr;
  }

  static auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto text_helper        = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TextMeshProUGUI");
  static auto get_component      = game_object_helper.GetMethodInfo("GetComponent", 1);
  void*       text_type          = text_helper.GetType();
  void*       args[1]            = {text_type};
  return invoke(get_component, label_object, args, "GameObject.GetComponent<OPC Card TextMeshProUGUI>");
}

void* create_opc_card_label(Transform* timer_anchor)
{
  if (!timer_anchor) {
    return nullptr;
  }
  if (auto* child = direct_child_named(timer_anchor, kOpcCardLabelName); child) {
    if (auto* existing = find_opc_card_label(timer_anchor); existing && component_transform(existing)) {
      return existing;
    }
    destroy_game_object(child->gameObject);
    return nullptr;
  }

  static auto text_helper = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TextMeshProUGUI");
  auto*       label       = create_ui_component(kOpcCardLabelName, timer_anchor, text_helper,
                                                "GameObject.AddComponent<OPC Card TextMeshProUGUI>");
  auto*       transform   = component_transform(label);
  auto*       game_object = transform ? transform->gameObject : nullptr;
  if (!game_object || !configure_opc_card_label(label, transform)) {
    destroy_game_object(game_object);
    return nullptr;
  }

  spdlog::info("[OpcIndicators] created selected-fleet OPC card label");
  return label;
}

void hide_opc_card_label(Transform* timer_anchor)
{
  if (auto* transform = direct_child_named(timer_anchor, kOpcCardLabelName); transform && transform->gameObject) {
    transform->gameObject->SetActive(false);
  }
}

void destroy_opc_card_label(Transform* timer_anchor)
{
  auto* label = direct_child_named(timer_anchor, kOpcCardLabelName);
  destroy_game_object(label ? label->gameObject : nullptr);
}

void update_opc_card_label(void* ui_component, FleetPlayerData* fleet, const std::string& display)
{
  auto*      timer_anchor    = fleet_panel_timer_anchor(ui_component);
  const auto timer_anchor_id = reinterpret_cast<uintptr_t>(timer_anchor);
  auto&      state           = s_opc_card_render_state;
  if (!fleet || display.empty()) {
    hide_opc_card_label(timer_anchor);
    state = {};
    return;
  }

  if (state.fleet_id != fleet->Id || state.timer_anchor_id != timer_anchor_id) {
    state                 = {};
    state.fleet_id        = fleet->Id;
    state.timer_anchor_id = timer_anchor_id;
  }
  if (state.setup_retry_at_ms > steady_now_milliseconds()) {
    return;
  }

  auto* label   = find_opc_card_label(timer_anchor);
  bool  created = false;
  if (!label) {
    label   = create_opc_card_label(timer_anchor);
    created = label != nullptr;
  }
  auto* transform   = component_transform(label);
  auto* game_object = transform ? transform->gameObject : nullptr;
  if (!label || !transform || !game_object
      || ((!state.layout_initialized || created) && !configure_opc_card_label(label, transform))) {
    destroy_opc_card_label(timer_anchor);
    state.layout_initialized = false;
    schedule_ui_retry(state.setup_failures, state.setup_retry_at_ms);
    return;
  }

  if (created || state.display != display) {
    static auto tmp_helper = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TMP_Text");
    static auto set_text   = tmp_helper.GetMethodInfo("set_text", 1);
    void*       args[1]    = {il2cpp_string_new(display.c_str())};
    if (!set_text || !invoke_void(set_text, label, args, "TMP_Text.set_text")) {
      destroy_opc_card_label(timer_anchor);
      state.layout_initialized = false;
      schedule_ui_retry(state.setup_failures, state.setup_retry_at_ms);
      return;
    }
  }

  state.display            = display;
  state.layout_initialized = true;
  clear_ui_retry(state.setup_failures, state.setup_retry_at_ms);
  game_object->SetActive(true);
}

void log_opc_eta(FleetPlayerData* fleet, const FleetOpcStatus& status, const std::string& display)
{
  const auto slot = fleet ? fleet->Index : -1;
  if (slot < 0 || slot >= kFleetSlotCount) {
    return;
  }

  const auto fleet_id = fleet ? fleet->Id : 0;
  auto&      previous = s_last_opc_eta_log_states[slot];
  if (previous.fleet_id == fleet_id && previous.display == display) {
    return;
  }

  const bool previously_visible = !previous.display.empty();
  previous                      = {fleet_id, display};
  if (display.empty()) {
    if (previously_visible) {
      spdlog::debug("[OpcIndicators] slot={} fleet={} opcEta=hidden", slot, fleet_id);
    }
    return;
  }

  spdlog::debug("[OpcIndicators] slot={} fleet={} cargo={:.0f} protected={:.0f} ratePerSecond={:.3f} opc={} "
                "etaSeconds={} nodeEtaSeconds={} safeOnNode={} label='{}'",
                slot, fleet_id, status.current_cargo, status.protected_limit, status.rate_per_second, status.opc,
                status.eta_seconds, status.node_eta_seconds, status.safe_on_node, display);
}

void hide_opc_eta(Transform* label_anchor)
{
  if (!label_anchor) {
    return;
  }
  if (auto* transform = direct_child_named(label_anchor, kOpcEtaLabelName); transform && transform->gameObject) {
    transform->gameObject->SetActive(false);
  }
  if (auto* transform = direct_child_named(label_anchor, kOpcEtaBackgroundName); transform && transform->gameObject) {
    transform->gameObject->SetActive(false);
  }
}

void destroy_opc_eta(Transform* label_anchor)
{
  if (!label_anchor) {
    return;
  }
  auto* label      = direct_child_named(label_anchor, kOpcEtaLabelName);
  auto* background = direct_child_named(label_anchor, kOpcEtaBackgroundName);
  destroy_game_object(label ? label->gameObject : nullptr);
  destroy_game_object(background ? background->gameObject : nullptr);
}

void reset_opc_eta_slot(int slot)
{
  if (slot < 0 || slot >= kFleetSlotCount) {
    return;
  }
  s_opc_eta_render_states[slot]          = {};
  s_last_opc_eta_log_states[slot]        = {};
  s_last_opc_eta_refresh_fleet_ids[slot] = 0;
  s_last_opc_eta_refresh_ms[slot]        = 0;
}

bool opc_eta_refresh_due(FleetPlayerData* fleet, bool force)
{
  const auto slot = fleet ? fleet->Index : -1;
  if (slot < 0 || slot >= kFleetSlotCount) {
    return true;
  }

  const auto fleet_id = fleet->Id;
  const auto now_ms   = steady_now_milliseconds();
  if (!force && s_last_opc_eta_refresh_fleet_ids[slot] == fleet_id && s_last_opc_eta_refresh_ms[slot] != 0
      && now_ms - s_last_opc_eta_refresh_ms[slot] < kOpcEtaRefreshMs) {
    return false;
  }

  s_last_opc_eta_refresh_fleet_ids[slot] = fleet_id;
  s_last_opc_eta_refresh_ms[slot]        = now_ms;
  return true;
}

void update_opc_eta_label(void* ui_component, FleetPlayerData* fleet, Transform* known_label_anchor = nullptr)
{
  auto*      label_anchor    = known_label_anchor ? known_label_anchor : fleet_state_widget_label_anchor(ui_component);
  const bool panel_component = ui_component && fleet_panel_controller(ui_component) == ui_component;
  if (!ui_component || !fleet) {
    hide_opc_eta(label_anchor);
    if (panel_component) {
      update_opc_card_label(ui_component, nullptr, {});
    }
    return;
  }
  const auto slot = fleet->Index;
  if (slot < 0 || slot >= kFleetSlotCount) {
    hide_opc_eta(label_anchor);
    if (panel_component) {
      update_opc_card_label(ui_component, nullptr, {});
    }
    return;
  }
  const bool selected      = panel_component || fleet_tile_is_selected(ui_component, fleet);
  auto&      render        = s_opc_eta_render_states[slot];
  const bool fleet_changed = render.fleet_id != fleet->Id;
  if (fleet_changed) {
    render          = {};
    render.fleet_id = fleet->Id;
  }

  const auto fleet_state   = fleet->CurrentState;
  const bool state_changed = !render.fleet_state_known || render.fleet_state != fleet_state;
  render.fleet_state       = fleet_state;
  render.fleet_state_known = true;

  const bool refresh_due  = opc_eta_refresh_due(fleet, fleet_changed || state_changed);
  auto       display      = render.computed_display;
  auto       card_display = render.computed_card_display;
  auto       safe_on_node = render.computed_safe;
  if (refresh_due) {
    const auto status = read_opc_status(fleet);
    display           = Config::Get().fleet_hud_opc_eta ? format_opc_eta(status) : std::string{};
    card_display      = display.empty() ? std::string{} : format_opc_card_display(status);
    safe_on_node      = status.safe_on_node;
    log_opc_eta(fleet, status, display);
  }

  if (display.empty()) {
    hide_opc_eta(label_anchor);
    if (selected) {
      update_opc_card_label(ui_component, fleet, {});
    }
    render.computed_display.clear();
    render.computed_card_display.clear();
    render.rendered_display.clear();
    render.computed_safe         = false;
    render.rendered_safe_on_node = false;
    render.layout_initialized    = false;
    clear_ui_retry(render.setup_failures, render.setup_retry_at_ms);
    return;
  }

  if (selected) {
    update_opc_card_label(ui_component, fleet, card_display);
  }
  if (!label_anchor) {
    render.computed_display      = display;
    render.computed_card_display = card_display;
    render.computed_safe         = safe_on_node;
    return;
  }

  if (render.setup_retry_at_ms > steady_now_milliseconds()) {
    render.computed_display      = display;
    render.computed_card_display = card_display;
    render.computed_safe         = safe_on_node;
    return;
  }

  auto* background = find_opc_eta_background(label_anchor);
  auto* label      = find_opc_eta_label(label_anchor);

  bool created = false;
  if (!background) {
    background = create_opc_eta_background(label_anchor, selected);
    created    = background != nullptr;
  }
  if (!label) {
    label   = create_opc_eta_label(label_anchor, selected, safe_on_node);
    created = created || label != nullptr;
  }
  if (created) {
    render.layout_initialized = false;
  }
  auto* label_transform      = component_transform(label);
  auto* label_object         = label_transform ? label_transform->gameObject : nullptr;
  auto* background_image     = opc_eta_background_image(background);
  auto* background_transform = component_transform(background_image);
  if (!background || !background_image || !background_transform || !label || !label_transform || !label_object) {
    destroy_opc_eta(label_anchor);
    render.computed_display      = display;
    render.computed_card_display = card_display;
    render.computed_safe         = safe_on_node;
    render.layout_initialized    = false;
    schedule_ui_retry(render.setup_failures, render.setup_retry_at_ms);
    return;
  }

  if (!render.layout_initialized || render.selected != selected || render.rendered_safe_on_node != safe_on_node) {
    const bool background_configured = configure_opc_eta_background(background_image, background_transform, selected);
    const bool label_configured      = configure_opc_eta_label(label, label_transform, selected, safe_on_node);
    render.layout_initialized        = label_configured && background_configured;
    render.selected                  = selected;
    if (!render.layout_initialized) {
      destroy_opc_eta(label_anchor);
      render.computed_display      = display;
      render.computed_card_display = card_display;
      render.computed_safe         = safe_on_node;
      schedule_ui_retry(render.setup_failures, render.setup_retry_at_ms);
      return;
    }
  }

  if (created || render.rendered_display != display) {
    static auto tmp_helper = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TMP_Text");
    static auto set_text   = tmp_helper.GetMethodInfo("set_text", 1);
    const auto  desired    = "<b>" + display + "</b>";
    void*       args[1]    = {il2cpp_string_new(desired.c_str())};
    if (!set_text || !invoke_void(set_text, label, args, "TMP_Text.set_text")) {
      destroy_opc_eta(label_anchor);
      render.computed_display      = display;
      render.computed_card_display = card_display;
      render.computed_safe         = safe_on_node;
      render.layout_initialized    = false;
      schedule_ui_retry(render.setup_failures, render.setup_retry_at_ms);
      return;
    }
  }

  render.computed_display      = display;
  render.computed_card_display = card_display;
  render.computed_safe         = safe_on_node;
  render.rendered_display      = display;
  render.rendered_safe_on_node = safe_on_node;
  clear_ui_retry(render.setup_failures, render.setup_retry_at_ms);
  background->SetActive(true);
  label_object->SetActive(true);
}

FleetPlayerData* fleet_state_widget_context(void* self)
{
  if (!self) {
    return nullptr;
  }

  static auto helper      = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetStateWidget");
  static auto get_context = helper.GetMethod<FleetPlayerData*(void*)>("get_Context", 0);
  return get_context ? get_context(self) : nullptr;
}

FleetPlayerData* fleetbar_flag_widget_context(void* self)
{
  if (!self) {
    return nullptr;
  }

  static auto helper      = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetbarFlagWidget");
  static auto get_context = helper.GetMethod<FleetPlayerData*(void*)>("get_Context", 0);
  return get_context ? get_context(self) : nullptr;
}

FleetPlayerData* fleet_local_view_fleet(void* self)
{
  if (!self) {
    return nullptr;
  }

  static auto helper    = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Ships", "FleetLocalViewController");
  static auto get_fleet = helper.GetMethod<FleetPlayerData*(void*)>("get_fleet", 0);
  return get_fleet ? get_fleet(self) : nullptr;
}

void FleetStateWidget_SetWidgetData_Hook(auto original, void* self)
{
  original(self);
  update_opc_eta_label(self, fleet_state_widget_context(self));
}

void FleetStateWidget_ClearWidgetData_Hook(auto original, void* self)
{
  auto*      fleet        = fleet_state_widget_context(self);
  auto*      label_anchor = fleet_state_widget_label_anchor(self);
  const auto slot         = fleet ? fleet->Index : -1;
  original(self);
  if (label_anchor) {
    hide_opc_eta(label_anchor);
    reset_opc_eta_slot(slot);
  }
  if (fleet && s_opc_card_render_state.fleet_id == fleet->Id) {
    auto* panel_fleet = fleet_local_view_fleet(fleet_panel_controller(self));
    if (!panel_fleet || panel_fleet->Id != fleet->Id) {
      update_opc_card_label(self, nullptr, {});
    }
  }
}

void FleetbarFlagWidget_SetWidgetData_Hook(auto original, void* self)
{
  original(self);
  update_opc_highlight(opc_anchor_from_fleetbar_flag(self), fleetbar_flag_widget_context(self));
}

void FleetbarFlagWidget_ClearWidgetData_Hook(auto original, void* self)
{
  auto*      fleet  = fleetbar_flag_widget_context(self);
  auto*      anchor = opc_anchor_from_fleetbar_flag(self);
  const auto slot   = fleet ? fleet->Index : -1;
  original(self);
  if (anchor) {
    if (auto* highlight = find_opc_highlight(anchor); highlight) {
      highlight->SetActive(false);
    }
    if (slot >= 0 && slot < kFleetSlotCount) {
      s_opc_highlight_retry_fleet_ids[slot]  = 0;
      s_opc_highlight_retry_anchor_ids[slot] = 0;
      clear_ui_retry(s_opc_highlight_setup_failures[slot], s_opc_highlight_retry_at_ms[slot]);
    }
  }
}

void FleetLocalViewController_BindDataContext_Hook(auto original, void* self, void* provider, void* data_context)
{
  original(self, provider, data_context);

  auto* tile_transform = component_transform(self);
  auto* label_anchor   = opc_anchor_from_tile(tile_transform);
  auto* fleet          = fleet_local_view_fleet(self);
  if (s_highlight_enabled) {
    update_opc_highlight(label_anchor, fleet);
  }
  const bool panel_component = fleet_panel_controller(self) == self;
  if (s_eta_enabled && (label_anchor || panel_component)) {
    reset_opc_eta_slot(fleet ? fleet->Index : -1);
    update_opc_eta_label(self, fleet, label_anchor);
  }
}

void FleetLocalViewController_OnCurrentCargoReactiveEvent_Hook(auto original, void* self, int32_t dirty_flags)
{
  original(self, dirty_flags);
  auto* tile_transform = component_transform(self);
  auto* fleet          = fleet_local_view_fleet(self);
  if (s_highlight_enabled) {
    update_opc_highlight(opc_anchor_from_tile(tile_transform), fleet);
  }
  if (s_eta_enabled) {
    update_opc_eta_label(self, fleet, opc_anchor_from_tile(tile_transform));
  }
}

} // namespace

void InstallOpcIndicatorHooks()
{
  const bool use_opc_highlight = Config::Get().highlight_opc_fleets;
  const bool use_opc_eta       = Config::Get().fleet_hud_opc_eta;
#if !defined(_WIN32)
  if (use_opc_highlight || use_opc_eta) {
    spdlog::warn("[OpcIndicators] disabled: native hook validation is currently Windows-only");
  }
  return;
#endif

  if (!use_opc_highlight && !use_opc_eta) {
    return;
  }

  auto fleet_local_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Ships", "FleetLocalViewController");
  const auto* bind_data_context =
      fleet_local_helper.isValidHelper() ? resolve_instance_void(fleet_local_helper, "BindDataContext", 2) : nullptr;
  const auto* cargo_updated = fleet_local_helper.isValidHelper()
                                  ? resolve_instance_void(fleet_local_helper, "OnCurrentCargoReactiveEvent", 1)
                                  : nullptr;
  if (!fleet_local_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Ships", "FleetLocalViewController");
  } else {
    if (!bind_data_context) {
      ErrorMsg::MissingMethod("FleetLocalViewController", "BindDataContext");
    }
    if (!cargo_updated) {
      ErrorMsg::MissingMethod("FleetLocalViewController", "OnCurrentCargoReactiveEvent");
    }
  }
  const bool local_ready = bind_data_context && cargo_updated;

  const MethodInfo* state_set   = nullptr;
  const MethodInfo* state_clear = nullptr;
  if (use_opc_eta) {
    auto state_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetStateWidget");
    if (!state_helper.isValidHelper()) {
      ErrorMsg::MissingHelper("HUD", "FleetStateWidget");
    } else {
      state_set   = resolve_instance_void(state_helper, "SetWidgetData", 0);
      state_clear = resolve_instance_void(state_helper, "ClearWidgetData", 0);
      if (!state_set) {
        ErrorMsg::MissingMethod("FleetStateWidget", "SetWidgetData");
      }
      if (!state_clear) {
        ErrorMsg::MissingMethod("FleetStateWidget", "ClearWidgetData");
      }
    }
  }

  const MethodInfo* flag_set   = nullptr;
  const MethodInfo* flag_clear = nullptr;
  if (use_opc_highlight) {
    auto flag_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetbarFlagWidget");
    if (!flag_helper.isValidHelper()) {
      ErrorMsg::MissingHelper("HUD", "FleetbarFlagWidget");
    } else {
      flag_set   = resolve_instance_void(flag_helper, "SetWidgetData", 0);
      flag_clear = resolve_instance_void(flag_helper, "ClearWidgetData", 0);
      if (!flag_set) {
        ErrorMsg::MissingMethod("FleetbarFlagWidget", "SetWidgetData");
      }
      if (!flag_clear) {
        ErrorMsg::MissingMethod("FleetbarFlagWidget", "ClearWidgetData");
      }
    }
  }

  s_eta_enabled       = use_opc_eta && local_ready && state_set && state_clear;
  s_highlight_enabled = use_opc_highlight && local_ready && flag_set && flag_clear;
  if (s_eta_enabled) {
    SPUD_STATIC_DETOUR(state_clear->methodPointer, FleetStateWidget_ClearWidgetData_Hook);
    SPUD_STATIC_DETOUR(state_set->methodPointer, FleetStateWidget_SetWidgetData_Hook);
  }
  if (s_highlight_enabled) {
    SPUD_STATIC_DETOUR(flag_clear->methodPointer, FleetbarFlagWidget_ClearWidgetData_Hook);
    SPUD_STATIC_DETOUR(flag_set->methodPointer, FleetbarFlagWidget_SetWidgetData_Hook);
  }
  if (s_eta_enabled || s_highlight_enabled) {
    SPUD_STATIC_DETOUR(bind_data_context->methodPointer, FleetLocalViewController_BindDataContext_Hook);
    SPUD_STATIC_DETOUR(cargo_updated->methodPointer, FleetLocalViewController_OnCurrentCargoReactiveEvent_Hook);
  }
}
