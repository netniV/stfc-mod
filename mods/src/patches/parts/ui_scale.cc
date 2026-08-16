#include "errormsg.h"
#include <config.h>

#include <il2cpp/il2cpp_helper.h>

#include <prime/CanvasScaler.h>
#include <prime/FleetMeshSelector.h>
#include <prime/ScreenManager.h>
#include <prime/Transform.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <prime/Vector3.h>
#include <str_utils.h>

namespace
{
float ShipScaleMultiplier()
{
  const auto multiplier = Config::Get().ui_scale_ship;
  return multiplier > 0.0f ? multiplier : 1.0f;
}

Transform* GetGameObjectTransform(void* game_object)
{
  if (game_object == nullptr) {
    return nullptr;
  }

  static auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  if (!game_object_helper.isValidHelper()) {
    return nullptr;
  }

  static auto transform_property = game_object_helper.GetProperty("transform");
  if (!transform_property.isValidHelper()) {
    return nullptr;
  }

  return reinterpret_cast<Transform*>(transform_property.GetRaw<Il2CppObject>(game_object));
}
} // namespace

void ApplyUiShipScaleToLoadedShips(float old_multiplier, float new_multiplier)
{
  if (old_multiplier <= 0.0f || new_multiplier <= 0.0f) {
    return;
  }

  const auto ratio = new_multiplier / old_multiplier;

  for (auto* selector : ObjectFinder<FleetMeshSelector>::GetAll()) {
    if (selector == nullptr) {
      continue;
    }

    auto* transform = GetGameObjectTransform(selector->LoadedObject());
    if (transform == nullptr) {
      continue;
    }

    auto* local_scale = transform->localScale;
    if (local_scale == nullptr) {
      continue;
    }

    Vector3 adjusted_scale{local_scale->x * ratio, local_scale->y * ratio, local_scale->z * ratio};
    transform->localScale = &adjusted_scale;
  }
}

bool SystemViewShip_TryGetShipAssetBundleResource_Hook(auto original, void* _this, float* scale,
                                                       void** asset_bundle_resource)
{
  const auto result = original(_this, scale, asset_bundle_resource);
  if (result && scale != nullptr) {
    *scale *= ShipScaleMultiplier();
  }
  return result;
}

void ScreenManager_UpdateCanvasRootScaleFactor_Hook(auto original, ScreenManager* _this)
{
  original(_this);

  #if _WIN32
  static auto cursor = LoadCursor(NULL, IDC_ARROW);
  if (!Config::Get().allow_cursor) {
    SetCursor(cursor);
  }
  #endif

  if (Config::Get().ui_scale != 0.0f) {
    static auto get_height_method = il2cpp_resolve_icall_typed<int()>("UnityEngine.Screen::get_height()");
    static auto get_width_method  = il2cpp_resolve_icall_typed<int()>("UnityEngine.Screen::get_width()");

    static auto ref_height = 1080;
    static auto ref_width  = 1920;

    auto scr_height = (float)get_height_method();
    auto scr_width  = (float)get_width_method();
    auto dpi        = Config::GetDPI();

    auto adjustedFactor = scr_height / (float)ref_height;

    if (!Config::Get().adjust_scale_res) {
      adjustedFactor = 1.0f;
    }

    auto n = (Config::Get().ui_scale * adjustedFactor * dpi);
    if (isnan(n)) {
      n = 1.0f;
    }
    n = std::clamp(n, 0.1f, 5.0f);

    _this->m_canvasRootScaler->scaleFactor = n;
  }
}

void CanvasController_Show(auto original, CanvasController* _this, int desiredEntryPoint, bool instant)
{
  const auto ui_scale_viewer = Config::Get().ui_scale_viewer;
  if (_this && ui_scale_viewer != 0.0f && to_wstring(_this->name) == L"ObjectViewerTemplate_Canvas") {
    auto transform = _this->transform;
    if (transform == nullptr) {
      return original(_this, desiredEntryPoint, instant);
    }
    auto localScale = transform->localScale;
    if (localScale == nullptr) {
      return original(_this, desiredEntryPoint, instant);
    }
    localScale->x         = ui_scale_viewer;
    localScale->y         = ui_scale_viewer;
    localScale->z         = ui_scale_viewer;
    transform->localScale = localScale;
  }
  return original(_this, desiredEntryPoint, instant);
}

void InstallUiScaleHooks()
{
  auto system_view_ship =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "SystemViewShip");
  if (!system_view_ship.isValidHelper()) {
    ErrorMsg::MissingHelper("Navigation", "SystemViewShip");
  } else if (auto try_get_ship_asset = system_view_ship.GetMethod("TryGetShipAssetBundleResource", 2);
             try_get_ship_asset == nullptr) {
    ErrorMsg::MissingMethod("SystemViewShip", "TryGetShipAssetBundleResource");
  } else {
    SPUD_STATIC_DETOUR(try_get_ship_asset, SystemViewShip_TryGetShipAssetBundleResource_Hook);
  }

  auto screen_manager_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ScreenManager");
  if (!screen_manager_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("UI", "ScreenManager");
  } else {
    auto ptr_update_scale = screen_manager_helper.GetMethod("UpdateCanvasRootScaleFactor");
    if (ptr_update_scale == nullptr) {
      ErrorMsg::MissingMethod("ScreenManager", "UpdateCanvasRootScaleFactor");
    } else {
      SPUD_STATIC_DETOUR(ptr_update_scale, ScreenManager_UpdateCanvasRootScaleFactor_Hook);
    }
  }

  auto canvas_controller_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "CanvasController");
  if (!canvas_controller_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("UI", "CanvasController");
  } else {
    auto ptr_canvas_show = canvas_controller_helper.GetMethodSpecial("Show", [](auto count, const Il2CppType** params) {
      if (count != 2) {
        return false;
      }

      auto p1 = params[0]->type;
      auto p2 = params[1]->type;

      if (p1 == IL2CPP_TYPE_I4 && p2 == IL2CPP_TYPE_BOOLEAN) {
        return true;
      }
      return false;
    });

    if (ptr_canvas_show == nullptr) {
      ErrorMsg::MissingMethod("CanvasController", "Show");
    } else {
      SPUD_STATIC_DETOUR(ptr_canvas_show, CanvasController_Show);
    }
  }

  Config::RefreshDPI();
}
