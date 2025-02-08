#pragma once

#include "errormsg.h"
#include <il2cpp/il2cpp_helper.h>

struct GenericRewardsScreenViewController {
public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.SharedFeatures", "GenericRewardsScreenViewController");
    return class_helper;
  }

  void OnCollectClicked()
  {
    static auto OnCollectClickedWarn = true;
    static auto OnCollectClickedMethod =
        get_class_helper().GetMethod<void(GenericRewardsScreenViewController*)>("OnCollectClicked");

    if (OnCollectClickedMethod) {
      OnCollectClickedMethod(this);
    } else if (OnCollectClickedWarn) {
      OnCollectClickedWarn = false;
      ErrorMsg::MissingMethod("GenericRewardsScreenViewController", "OnCollectClicked");
    }
  }

  bool IsActive()
  {
    auto IsActiveMethod = get_class_helper().GetMethod<bool(GenericRewardsScreenViewController*)>("IsActive");
    auto IsActiveWarn   = true;

    if (IsActiveMethod) {
      return IsActiveMethod(this);
    } else if (IsActiveWarn) {
      IsActiveWarn = false;
      ErrorMsg::MissingMethod("GenericRewardsSCreenViewController", "IsActive");
    }
  }
};
