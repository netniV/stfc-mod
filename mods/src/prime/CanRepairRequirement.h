#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "ActionRequirement.h"

struct CanRepairRequirement : public ActionRequirement<CanRepairRequirement> {

public:
  friend struct ActionRequirement<CanRepairRequirement>;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "", "FleetPlayerData.CanRepairRequirement");
    return class_helper;
  }
};