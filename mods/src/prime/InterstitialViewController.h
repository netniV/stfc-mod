#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "TMP_InputField.h"

struct InterstitialViewController {
public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Interstitial", "InterstitialViewController");
    return class_helper;
  }

  void CloseWhenReady()
  {
    auto CloseWhenReady = get_class_helper().GetMethod<void(InterstitialViewController*)>("CloseWhenReady");
    return CloseWhenReady(this);
  }
};
