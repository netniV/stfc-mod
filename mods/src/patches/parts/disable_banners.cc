#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/Toast.h>

#include <spud/detour.h>

struct ToastObserver {
};

void ToastObserver_EnqueueToast_Hook(auto original, ToastObserver *_this, Toast *toast)
{
  if (std::ranges::find(Config::Get().disabled_banner_types, toast->get_State())
      != Config::Get().disabled_banner_types.end()) {
    return;
  }

  original(_this, toast);
}

void ToastObserver_EnqueueOrCombineToast_Hook(auto original, ToastObserver *_this, Toast *toast, uintptr_t cmpAction)
{
  if (std::ranges::find(Config::Get().disabled_banner_types, toast->get_State())
      != Config::Get().disabled_banner_types.end()) {
    return;
  }

  original(_this, toast, cmpAction);
}

void InstallToastBannerHooks()
{
  if (auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "ToastObserver");
      !helper.isValidHelper()) {
    ErrorMsg::MissingHelper("HUD", "ToastObserver");
  } else {
    if (const auto ptr = helper.GetMethod("EnqueueToast"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ToastObserver", "EnqueueToast");
    } else {
      SPUD_STATIC_DETOUR(ptr, ToastObserver_EnqueueToast_Hook);
    }

    if (const auto ptr = helper.GetMethod("EnqueueOrCombineToast"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ToastObserver", "EnqueueOrCombineToast");
    } else {
      SPUD_STATIC_DETOUR(ptr, ToastObserver_EnqueueOrCombineToast_Hook);
    }
  }
}
