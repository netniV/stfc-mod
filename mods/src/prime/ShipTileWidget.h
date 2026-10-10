#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "FleetPlayerData.h"
#include "Widget.h"

struct ShipTileWidget : public Widget<FleetPlayerData, ShipTileWidget> {
public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Ships", "ShipTileWidget");
    return class_helper;
  }

private:
  friend struct Widget<FleetPlayerData, ShipTileWidget>;
};
