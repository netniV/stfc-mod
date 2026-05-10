#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "Widget.h"

#include "NavigationInteractionUIContext.h"
#include "ObjectViewerBaseWidget.h"
#include "ScanEngageButtonsWidget.h"
#include "TimerDataContext.h"

enum OccupiedState // TypeDefIndex: 12900
{
  NotOccupied         = 0,
  LocalPlayerOccupied = 1,
  OtherPlayerOccupied = 2
};

struct MiningObjectViewerWidget : public ObjectViewerBaseWidget<MiningObjectViewerWidget> {
public:
  __declspec(property(get = __get__occupiedState)) OccupiedState _occupiedState;
  __declspec(property(get = __get__scanEngageButtonsWidget)) ScanEngageButtonsWidget* _scanEngageButtonsWidget;
  __declspec(property(get = __get__miningTimerWidgetContext)) TimerDataContext* _miningTimerWidgetContext;

  void MineClicked()
  {
    static auto MineClicked = get_class_helper().GetMethod<void(MiningObjectViewerWidget*)>("MineClicked");
    MineClicked(this);
  }

public:
  friend class ObjectFinder<MiningObjectViewerWidget>;
  friend class ObjectViewerBaseWidget<MiningObjectViewerWidget>;
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.ObjectViewer", "MiningObjectViewerWidget");
    return class_helper;
  }

public:
  OccupiedState __get__occupiedState()
  {
    static auto field = get_class_helper().GetField("_occupiedState");
    return *(OccupiedState*)((ptrdiff_t)this + field.offset());
  }

  ScanEngageButtonsWidget* __get__scanEngageButtonsWidget()
  {
    static auto field = get_class_helper().GetField("_scanEngageButtonsWidget").offset();
    return *(ScanEngageButtonsWidget**)((char*)this + field);
  }

  TimerDataContext* __get__miningTimerWidgetContext()
  {
    static auto field = get_class_helper().GetField("_miningTimerWidgetContext").offset();
    return *(TimerDataContext**)((char*)this + field);
  }
};
