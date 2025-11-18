#include "errormsg.h"
#include <config.h>

#include <il2cpp/il2cpp_helper.h>

#include <prime/CanvasScaler.h>
#include <prime/ScreenManager.h>
#include <prime/Transform.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <prime/Vector3.h>
#include <str_utils.h>

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
  if (ui_scale_viewer != 0.0f && to_wstring(_this->name) == L"ObjectViewerTemplate_Canvas") {
    auto transform        = _this->transform;
    auto localScale       = transform->localScale;
    localScale->x         = ui_scale_viewer;
    localScale->y         = ui_scale_viewer;
    localScale->z         = ui_scale_viewer;
    transform->localScale = localScale;
  }
  return original(_this, desiredEntryPoint, instant);
}

void InstallUiScaleHooks()
{
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
