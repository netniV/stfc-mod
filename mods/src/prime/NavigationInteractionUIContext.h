#pragma once

#include <cstdint>

#include <il2cpp/il2cpp_helper.h>

struct NavigationInteractionUIContext {
public:
  __declspec(property(get = __get_ContextDataState)) int ContextDataState;
  __declspec(property(get = __get_InputInteractionType)) int InputInteractionType;
  __declspec(property(get = __get_UserId)) Il2CppString* UserId;
  __declspec(property(get = __get_IsMarauder)) bool IsMarauder;
  __declspec(property(get = __get_ThreatLevel)) int ThreatLevel;
  __declspec(property(get = __get_ValidNavigationInput)) bool ValidNavigationInput;
  __declspec(property(get = __get_ShowSetCourseArm)) bool ShowSetCourseArm;
  __declspec(property(get = __get_LocationTranslationId)) int64_t LocationTranslationId;
  __declspec(property(get = __get_Poi)) void* Poi;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationInteractionUIContext");
    return class_helper;
  }

public:
  int __get_ContextDataState()
  {
    static auto prop = get_class_helper().GetProperty("ContextDataState");
    if (auto* value = prop.Get<int>(this); value) {
      return *value;
    }
    return -1;
  }

  int __get_InputInteractionType()
  {
    static auto prop = get_class_helper().GetProperty("InputInteractionType");
    if (auto* value = prop.Get<int>(this); value) {
      return *value;
    }
    return -1;
  }

  Il2CppString* __get_UserId()
  {
    static auto field = get_class_helper().GetField("UserId").offset();
    return *(Il2CppString**)((uintptr_t)this + field);
  }

  bool __get_IsMarauder()
  {
    static auto field = get_class_helper().GetField("IsMarauder").offset();
    return *(bool*)((uintptr_t)this + field);
  }

  int __get_ThreatLevel()
  {
    static auto field = get_class_helper().GetField("ThreatLevel").offset();
    return *(int*)((uintptr_t)this + field);
  }

  bool __get_ValidNavigationInput()
  {
    static auto field = get_class_helper().GetField("ValidNavigationInput").offset();
    return *(bool*)((uintptr_t)this + field);
  }

  bool __get_ShowSetCourseArm()
  {
    static auto field = get_class_helper().GetField("ShowSetCourseArm").offset();
    return *(bool*)((uintptr_t)this + field);
  }

  int64_t __get_LocationTranslationId()
  {
    static auto field = get_class_helper().GetField("LocationTranslationId").offset();
    return *(int64_t*)((uintptr_t)this + field);
  }

  void* __get_Poi()
  {
    static auto field = get_class_helper().GetField("Poi").offset();
    return *(void**)((uintptr_t)this + field);
  }
};
