#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "PopulatedSystemData.h"

struct PlanetViewUtils {
public:
  __declspec(property(get = __get__popData)) PopulatedSystemData* _popData;
  __declspec(property(get = __get__activeSystemParent)) void* _activeSystemParent;

  void* GetFlatRenderable()
  {
    static auto fn = get_class_helper().GetMethod<void*(PlanetViewUtils*)>("get_FlatRenderable");
    return fn(this);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "PlanetViewUtils");
    return class_helper;
  }

public:
  PopulatedSystemData* __get__popData()
  {
    static auto field = get_class_helper().GetField("_popData");
    return *(PopulatedSystemData**)((ptrdiff_t)this + field.offset());
  }

  void* __get__activeSystemParent()
  {
    static auto field = get_class_helper().GetField("_activeSystemParent");
    return *(void**)((ptrdiff_t)this + field.offset());
  }
};
