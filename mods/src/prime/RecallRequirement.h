#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "ActionRequirement.h"

struct RecallRequirement : public ActionRequirement<RecallRequirement> {

public:
  friend struct ActionRequirement<RecallRequirement>;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "", "FleetPlayerData.RecallRequirement");
    return class_helper;
  }
};