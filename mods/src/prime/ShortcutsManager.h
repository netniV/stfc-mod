#pragma once

#include "MonoSingleton.h"

struct ShortcutsManager : MonoSingleton<ShortcutsManager> {
  friend struct MonoSingleton<ShortcutsManager>;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.GameInput", "ShortcutsManager");
    return class_helper;
  }
};
