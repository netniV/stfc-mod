#include "config.h"
#include "errormsg.h"

#include <patches/mapkey.h>

#include <il2cpp/il2cpp_helper.h>

#include <prime/NavigationPan.h>
#include <prime/NavigationZoom.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

vec3 GetMouseWorldPos(void *cam, vec3 *pos)
{
  static auto class_helper = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Client.Core", "MathUtils");
  static auto fn           = class_helper.GetMethodInfo("GetMouseWorldPos");

  void            *args[2]   = {cam, (void *)pos};
  Il2CppException *exception = NULL;
  auto             result    = il2cpp_runtime_invoke(fn, nullptr, args, &exception);
  return *(vec3 *)(il2cpp_object_unbox(result));
}

auto do_default_zoom = false;

inline void StoreZoom(std::string label, float &zoom, NavigationZoom *_this)
{
  auto old_zoom = zoom;
  zoom          = (_this->Distance - _this->_minimum) / (_this->_maximum - _this->_minimum) * Config::Get().zoom;
  spdlog::info("Changing {} from {} to {}", label, old_zoom, zoom);
}

void NavigationZoom_Update_Hook(auto original, NavigationZoom *_this)
{
  static auto GetMousePosition =
      il2cpp_resolve_icall_typed<void(vec3 *)>("UnityEngine.Input::get_mousePosition_Injected(UnityEngine.Vector3&)");
  static auto GetDeltaTime = il2cpp_resolve_icall_typed<float()>("UnityEngine.Time::get_deltaTime()");

  const auto dt               = GetDeltaTime();
  auto       zoomDelta        = 0.0f;
  bool       do_absolute_zoom = false;
  bool       do_store_zoom    = false;
  auto       config           = &Config::Get();

  if (!Key::IsInputFocused()) {
    if (MapKey::IsDown(GameFunction::SetZoomPreset1)) {
      return StoreZoom("System Preset 1", config->system_zoom_preset_1, _this);
    } else if (MapKey::IsDown(GameFunction::SetZoomPreset2)) {
      return StoreZoom("System Preset 2", config->system_zoom_preset_2, _this);
    } else if (MapKey::IsDown(GameFunction::SetZoomPreset3)) {
      return StoreZoom("System Preset 3", config->system_zoom_preset_3, _this);
    } else if (MapKey::IsDown(GameFunction::SetZoomPreset4)) {
      return StoreZoom("System Preset 4", config->system_zoom_preset_4, _this);
    } else if (MapKey::IsDown(GameFunction::SetZoomPreset5)) {
      return StoreZoom("System Preset 5", config->system_zoom_preset_5, _this);
    } else if (MapKey::IsDown(GameFunction::SetZoomDefault)) {
      return StoreZoom("System Default", config->default_system_zoom, _this);
    }

    do_absolute_zoom = true;
    if (MapKey::IsDown(GameFunction::ZoomPreset1)) {
      zoomDelta     = config->system_zoom_preset_1;
      do_store_zoom = true;
    } else if (MapKey::IsDown(GameFunction::ZoomPreset2)) {
      zoomDelta     = config->system_zoom_preset_2;
      do_store_zoom = true;
    } else if (MapKey::IsDown(GameFunction::ZoomPreset3)) {
      zoomDelta     = config->system_zoom_preset_3;
      do_store_zoom = true;
    } else if (MapKey::IsDown(GameFunction::ZoomPreset4)) {
      zoomDelta     = config->system_zoom_preset_4;
      do_store_zoom = true;
    } else if (MapKey::IsDown(GameFunction::ZoomPreset5)) {
      zoomDelta     = config->system_zoom_preset_5;
      do_store_zoom = true;
    }

    if (config->hotkeys_extended) {
      if (MapKey::IsDown(GameFunction::ZoomReset)) {
        do_absolute_zoom = false;
        do_default_zoom  = true;
      } else if (MapKey::IsDown(GameFunction::ZoomMin)) {
        zoomDelta = config->zoom;
      } else if (MapKey::IsDown(GameFunction::ZoomMax)) {
        zoomDelta = 100;
      }
    }

    if (do_default_zoom) {
      do_absolute_zoom = true;
      zoomDelta        = config->default_system_zoom;
    }

    if (zoomDelta == 0.0f) {
      do_absolute_zoom = false;
      zoomDelta        = config->keyboard_zoom_speed * dt;
    }

    if (MapKey::IsPressed(GameFunction::ZoomIn) || do_absolute_zoom) {
      vec3 mousePos;
      GetMousePosition(&mousePos);
      _this->_zoomLocation = vec2{mousePos.x, mousePos.y};
      if (do_absolute_zoom) {
        auto zoom_distance = _this->_minimum + (_this->_maximum - _this->_minimum) * (zoomDelta / config->zoom);
        _this->Distance    = zoom_distance;
      } else {
        _this->_zoomDelta     = zoomDelta;
        _this->_lastZoomDelta = zoomDelta;
      }
      auto worldPos      = GetMouseWorldPos(_this->_sceneCamera, &mousePos);
      _this->_worldPoint = worldPos;
      _this->ZoomCameraAtWorldPoint();
    } else if (MapKey::IsPressed(GameFunction::ZoomOut) && !Key::IsInputFocused()) {
      vec3 mousePos;
      GetMousePosition(&mousePos);
      _this->_zoomLocation  = vec2{mousePos.x, mousePos.y};
      _this->_zoomDelta     = -1.0f * zoomDelta;
      _this->_lastZoomDelta = -1.0f * zoomDelta;
      auto worldPos         = GetMouseWorldPos(_this->_sceneCamera, &mousePos);
      _this->_worldPoint    = worldPos;
      _this->ZoomCameraAtWorldPoint();
    }
  }

  if (zoomDelta > 0.0f && config->use_presets_as_default && do_store_zoom) {
    StoreZoom("System Preset Default from Preset", config->default_system_zoom, _this);
  }

  do_default_zoom = false;

  original(_this);
}

void NavigationZoom_SetViewParameters_Hook(auto original, NavigationZoom *_this, float radius, NodeDepth depth)
{
  if (depth == NodeDepth::SolarSystem) {
    auto ratio                     = (Config::Get().zoom / radius);
    _this->_farRatioSystemNormal   = 0.55f * ratio;
    _this->_farRatioSystemExtended = 1 * ratio;
    original(_this, radius, depth);
    _this->_sceneCamera->farClipPlane = Config::Get().zoom * 2.75f;
    do_default_zoom                   = true;
  } else {
    original(_this, radius, depth);
  }
}

void NavigationZoom_ApplyRangeChanges_Hook(auto original, NavigationZoom *_this)
{
  if (_this->_depth == NodeDepth::SolarSystem) {
    auto ratio                     = (Config::Get().zoom / _this->_viewRadius);
    _this->_farRatioSystemNormal   = 0.55f * ratio;
    _this->_farRatioSystemExtended = 1 * ratio;
    original(_this);
    _this->_sceneCamera->farClipPlane = Config::Get().zoom * 2.75f;
    do_default_zoom                   = true;
  } else {
    original(_this);
  }
}

void NavigationZoom_SetDepth_Hook(auto original, NavigationZoom *_this, NodeDepth depth)
{
  if (depth == NodeDepth::SolarSystem) {
    auto ratio                        = (Config::Get().zoom / _this->_viewRadius);
    _this->_farRatioSystemNormal      = 0.55f * ratio;
    _this->_farRatioSystemExtended    = 1 * ratio;
    _this->_sceneCamera->farClipPlane = Config::Get().zoom * 3.75f;
    original(_this, depth);
    _this->_sceneCamera->farClipPlane = Config::Get().zoom * 3.75f;
    do_default_zoom                   = true;
  } else {
    original(_this, depth);
  }
}

void NavigationCamera_SetSystemViewSizeData_Hook(auto original, uint8_t *_this_cam, float radius, Vector3 *systemPos,
                                                 NodeDepth depth)
{
  if (depth == NodeDepth::SolarSystem) {
    auto _this                     = *(NavigationZoom **)(_this_cam + 0x20);
    auto ratio                     = (Config::Get().zoom / radius);
    _this->_farRatioSystemNormal   = 0.55f * ratio;
    _this->_farRatioSystemExtended = 1 * ratio;
    original(_this_cam, radius, systemPos, depth);
    _this->_sceneCamera->farClipPlane = Config::Get().zoom * 2.75f;
    do_default_zoom                   = true;
  } else {
    original(_this_cam, radius, systemPos, depth);
  }
}

void InstallZoomHooks()
{
  auto screen_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationZoom");
  if (!screen_manager_helper.HasClass()) {
    ErrorMsg::MissingHelper("Navigation", "NavigationZoom");
  } else {
    auto ptr_update = screen_manager_helper.GetMethod("Update");
    if (ptr_update == nullptr) {
      ErrorMsg::MissingMethod("NavigationZoom", "Update");
    } else {
      SPUD_STATIC_DETOUR(ptr_update, NavigationZoom_Update_Hook);
    }

    auto ptr_set_depth = screen_manager_helper.GetMethod("SetDepth");
    if (ptr_set_depth == nullptr) {
      ErrorMsg::MissingMethod("NavigationZoom", "SetDepth");
    } else {
      SPUD_STATIC_DETOUR(ptr_set_depth, NavigationZoom_SetDepth_Hook);
    }

#if _WIN32
    auto ptr_set_view_parameters = screen_manager_helper.GetMethod("SetViewParameters");
    if (ptr_set_view_parameters == nullptr) {
      ErrorMsg::MissingMethod("NavigationZoom", "SetViewParameters");
    } else {
      SPUD_STATIC_DETOUR(ptr_set_view_parameters, NavigationZoom_SetViewParameters_Hook);
    }

    // auto ptr_apply_range_changes = screen_manager_helper.GetMethod("ApplyRangeChanges");
    // if (ptr_apply_range_changes == nullptr) {
    //   ErrorMsg::MissingMethod("NavigationZoom", "ApplyRangeChanges");
    // } else {
    //   SPUD_STATIC_DETOUR(ptr_apply_range_changes, NavigationZoom_ApplyRangeChanges_Hook);
    // }
#endif

    // auto navigation_camera = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation",
    // "NavigationCamera"); auto ptr_set_system_view_size_data = navigation_camera.GetMethod("SetSystemViewSizeData");
    // SPUD_STATIC_DETOUR(ptr_set_system_view_size_data, NavigationCamera_SetSystemViewSizeData_Hook);
  }
}
