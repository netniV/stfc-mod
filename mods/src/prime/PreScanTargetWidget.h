#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "BattleTargetData.h"
#include "GenericButtonWidget.h"
#include "NavigationInteractionUIContext.h"
#include "RewardsButtonWidget.h"
#include "Widget.h"

struct PreScanTargetWidget : public ObjectViewerBaseWidget<PreScanTargetWidget> {
public:
  __declspec(property(get = __get__battleTargetData)) BattleTargetData* _battleTargetData;
  __declspec(property(get = __get__scanEngageButtonsWidget)) ScanEngageButtonsWidget* _scanEngageButtonsWidget;
  __declspec(property(get = __get__rewardsButtonWidget)) RewardsButtonWidget* _rewardsButtonWidget;
  __declspec(property(get = __get__addToQueueButtonWidget)) GenericButtonWidget* _addToQueueButtonWidget;

  void OnAddToQueueClickedEventHandler() {
    static auto OnAddToQueueClickedEventHandlerMethod = get_class_helper().GetMethod<void(Widget*)>("OnAddToQueueClickedEventHandler");
    OnAddToQueueClickedEventHandlerMethod(this);
  }

  void OnInfoClick()
  {
    static auto OnInfoClickMethod = get_class_helper().GetMethod<void(Widget*)>("OnInfoClick");
    return OnInfoClickMethod(this);
  }

private:
  friend class ObjectFinder<PreScanTargetWidget>;
  friend class ObjectViewerBaseWidget<PreScanTargetWidget>;

 public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Combat", "PreScanTargetWidget");
    return class_helper;
  }

public:
  BattleTargetData* __get__battleTargetData()
  {
    static auto field = get_class_helper().GetField("_battleTargetData").offset();
    return *(BattleTargetData**)((char*)this + field);
  }

  GenericButtonWidget* __get__addToQueueButtonWidget()
  {
    static auto field = get_class_helper().GetField("_addToQueueButtonWidget").offset();
    return *(GenericButtonWidget**)((char*)this + field);
  }

  ScanEngageButtonsWidget* __get__scanEngageButtonsWidget()
  {
    static auto field = get_class_helper().GetField("_scanEngageButtonsWidget").offset();
    return *(ScanEngageButtonsWidget**)((char*)this + field);
  }

  RewardsButtonWidget* __get__rewardsButtonWidget()
  {
    static auto field = get_class_helper().GetField("_rewardsButtonWidget").offset();
    return *(RewardsButtonWidget**)((char*)this + field);
  }
};
