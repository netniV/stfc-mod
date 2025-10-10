#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

#include <prime/NavigationPan.h>
#include <prime/TKTouch.h>

#include <spud/detour.h>

TKTouch *TKTouch_populateWithPosition_Hook(auto original, TKTouch *_this, uintptr_t pos, TouchPhase phase)
{
  auto r = original(_this, pos, phase);
  if (r->phase == TouchPhase::Stationary) {
    r->phase = TouchPhase::Moved;
  }
  return r;
}

bool NavigationPan_LateUpdate_Hook(auto original, NavigationPan *_this)
{
  auto d = _this->_lastDelta;

  if (!Config::Get().disable_move_keys) {
    original(_this);
  }

  static auto GetMouseButton = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButton(System.Int32)");
  static auto GetTouchCount  = il2cpp_resolve_icall_typed<int()>("UnityEngine.Input::get_touchCount()");

  if (_this->BlockPan() || _this->_trackingPOI) {
    d->x = 0.0f;
    d->y = 0.0f;
  } else if (GetMouseButton(0) || GetTouchCount() > 0) {
    //
  } else {
    d->x = d->x * Config::Get().system_pan_momentum_falloff;
    d->y = d->y * Config::Get().system_pan_momentum_falloff;
    _this->MoveCamera(vec2{d->x, d->y}, true);
  }
  _this->_farMagRadiusRatioSystemExtended = _this->_farMagRadiusRatioSystemNormal;
  return true;
}

void InstallPanHooks()
{
  if (auto touchHelper = il2cpp_get_class_helper("TouchKit", "", "TKTouch"); !touchHelper.isValidHelper()) {
    ErrorMsg::MissingHelper("<global>", "TKTouch");
  } else {
    if (const auto ptr = touchHelper.GetMethod("populateWithPosition"); ptr == nullptr) {
      ErrorMsg::MissingMethod("TKTouch", "populateWithPosition");
    } else {
      SPUD_STATIC_DETOUR(ptr, TKTouch_populateWithPosition_Hook);
    }
  }

  if (auto navHelper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationPan");
      !navHelper.isValidHelper()) {
    ErrorMsg::MissingHelper("Navigation", "NavigationPan");
  } else {
    if (const auto ptr = navHelper.GetMethod("LateUpdate"); ptr == nullptr) {
      ErrorMsg::MissingMethod("NavigationPan", "LateUpdate");
    } else {
      SPUD_STATIC_DETOUR(ptr, NavigationPan_LateUpdate_Hook);
    }
  }
}
