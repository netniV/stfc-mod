#pragma once

#include <il2cpp/il2cpp_helper.h>

struct RequestDispatcherBase {
public:
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
  void* __get__armadaButton()
  {
    static auto field = get_class_helper().GetField("_armadaButton");
    return *(void**)((ptrdiff_t)this + field.offset());
  }
};
