#pragma once

#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

struct AnimatedRewardsScreenViewController {
public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Missions.UI", "AnimatedRewardsScreenViewController");
    return class_helper;
  }

  void GoBackToLastSection()
  {
    static auto GoBackToLastSectionMethod =
        get_class_helper().GetMethod<void(AnimatedRewardsScreenViewController*)>("GoBackToLastSection");
    static auto GoBackToLastSectionWarn = true;
    if (GoBackToLastSectionMethod) {
      GoBackToLastSectionMethod(this);
    } else if (GoBackToLastSectionWarn) {
      GoBackToLastSectionWarn = false;
      ErrorMsg::MissingMethod("AnimatedRewardsScreenViewController", "GoBackToLastSection");
    }
  }

  void OnCollectClicked()
  {
    static auto OnCollectClickedMethod =
        get_class_helper().GetMethod<void(AnimatedRewardsScreenViewController*)>("OnCollectClicked");
    static auto OnCollectClickedWarn = true;

    if (OnCollectClickedMethod) {
      OnCollectClickedMethod(this);
    } else if (OnCollectClickedWarn) {
      OnCollectClickedWarn = false;
      ErrorMsg::MissingMethod("AnimatedRewardsScreenViewController", "OnCollectClicked");
    }
  }

  bool IsActive()
  {
    auto IsActiveMethod = get_class_helper().GetMethod<bool(AnimatedRewardsScreenViewController*)>("IsActive");
    auto IsActiveWarn   = true;

    if (IsActiveMethod) {
      return IsActiveMethod(this);
    } else if (IsActiveWarn) {
      IsActiveWarn = false;
      ErrorMsg::MissingMethod("AnimatedRewardsScreenViewController", "IsActive");
    }

    return false;
  }
};
