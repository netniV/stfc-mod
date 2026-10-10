#pragma once

#include "InventoryForPopup.h"
#include "Widget.h"

struct InventoryUseRowWidget : public Widget<InventoryForPopup, InventoryUseRowWidget> {
public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Inventories", "InventoryUseRowWidget");
    return class_helper;
  }

private:
  friend struct Widget<InventoryForPopup, InventoryUseRowWidget>;
};
