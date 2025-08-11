#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "BattleTargetData.h"
#include "GenericButtonWidget.h"
#include "Widget.h"

struct ScanEngageButtonsWidget : public Widget<BattleTargetData, ScanEngageButtonsWidget> {
public:
  __declspec(property(get = __get__armadaButton)) GenericButtonWidget* _armadaButton;

  void OnArmadaButtonClicked()
  {
    static auto OnArmadaButtonClicked =
        get_class_helper().GetMethod<void(ScanEngageButtonsWidget*)>("OnArmadaButtonClicked");
    OnArmadaButtonClicked(this);
  }
  void OnEngageButtonClicked()
  {
    static auto OnEngageButtonClicked =
        get_class_helper().GetMethod<void(ScanEngageButtonsWidget*)>("OnEngageButtonClicked");
    OnEngageButtonClicked(this);
  }
  void OnScanButtonClicked()
  {
    static auto OnScanButtonClicked =
        get_class_helper().GetMethod<void(ScanEngageButtonsWidget*)>("OnScanButtonClicked");
    OnScanButtonClicked(this);
  }

private:
  friend class ObjectFinder<ScanEngageButtonsWidget>;
  friend struct Widget<BattleTargetData, ScanEngageButtonsWidget>;
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Combat", "ScanEngageButtonsWidget");
    return class_helper;
  }

public:
  GenericButtonWidget* __get__armadaButton()
  {
    static auto field = get_class_helper().GetField("_armadaButton").offset();
    return *(GenericButtonWidget**)((char*)this + field);
  }
};
