#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "Widget.h"

class Bundle
{
public:
  //
private:
  friend struct Widget<void, Bundle>;
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Content", "Bundle");
    return class_helper;
  }
};

class BundleDataWidget : public Widget<void, Bundle>
{
public:
  enum ItemState {
    None               = 0,
    Package            = 1,
    Highlighted        = 2,
    LegendOn           = 4,
    ExpiryTimerOn      = 8,
    DiscountOn         = 16,    // 0x00000010
    RemoteImage        = 32,    // 0x00000020
    Showcase           = 256,   // 0x00000100
    HasBanner          = 512,   // 0x00000200
    ShowViewButton     = 1024,  // 0x00000400
    ShowMaxPurchases   = 2048,  // 0x00000800
    ShowItemRarity     = 4096,  // 0x00001000
    ShowItemType       = 8192,  // 0x00002000
    CooldownTimerOn    = 16384, // 0x00004000
    ShowSynthetic      = 32768, // 0x00008000
    ShowPurchaseLimits = 65536, // 0x00010000
  };

  __declspec(property(get = __get_CurrentState)) ItemState CurrentState;

  void AuxViewButtonPressedHandler()
  {
    static auto AuxViewButtonPressedHandler =
        get_class_helper().GetMethod<void(BundleDataWidget*)>("AuxViewButtonPressedHandler");
    static auto AuxViewButtonPressedWarn = true;

    if (AuxViewButtonPressedHandler) {
      AuxViewButtonPressedHandler(this);
    } else if (AuxViewButtonPressedWarn) {
      AuxViewButtonPressedWarn = false;
      ErrorMsg::MissingMethod("BundleDataWidget", "AuxViewButtonPressedHandelr");
    }
  }

private:
  friend struct Widget<void, BundleDataWidget>;
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "BundleDataWidget");
    return class_helper;
  }

public:
  ItemState __get_CurrentState()
  {
    static auto field = get_class_helper().GetField("_currentState");
    return *(ItemState*)((ptrdiff_t)this + field.offset());
  }
};
