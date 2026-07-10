#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "Widget.h"

#include "NavigationInteractionUIContext.h"
#include "ObjectViewerBaseWidget.h"
#include "ScanEngageButtonsWidget.h"

struct AllianceStarbaseObjectViewerWidget : public ObjectViewerBaseWidget<AllianceStarbaseObjectViewerWidget> {
public:
  friend class ObjectFinder<AllianceStarbaseObjectViewerWidget>;
  friend class ObjectViewerBaseWidget<AllianceStarbaseObjectViewerWidget>;
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.ObjectViewer", "AllianceStarbaseObjectViewerWidget");
    return class_helper;
  }

};
