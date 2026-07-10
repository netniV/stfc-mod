#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "Widget.h"

#include "NavigationInteractionUIContext.h"
#include "ObjectViewerBaseWidget.h"
#include "ScanEngageButtonsWidget.h"

struct EmbassyObjectViewer : public ObjectViewerBaseWidget<EmbassyObjectViewer> {
public:
  friend class ObjectFinder<EmbassyObjectViewer>;
  friend class ObjectViewerBaseWidget<EmbassyObjectViewer>;
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.ObjectViewer", "EmbassyObjectViewer");
    return class_helper;
  }

};
