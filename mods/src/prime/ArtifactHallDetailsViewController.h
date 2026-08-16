#pragma once

#include "SemaphoreButtonListener.h"

#include <il2cpp/il2cpp_helper.h>

struct ArtifactHallDetailsViewController {
public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.ArtifactHall", "ArtifactHallDetailsViewController");
    return class_helper;
  }

  bool isActiveAndEnabled()
  {
    static auto get_isActiveAndEnabled = il2cpp_resolve_icall_typed<bool(ArtifactHallDetailsViewController*)>(
        "UnityEngine.Behaviour::get_isActiveAndEnabled()");
    if (get_isActiveAndEnabled) {
      return get_isActiveAndEnabled(this);
    }
    return true;
  }

  void PressLeftArrow()
  {
    auto listener = LeftArrowButton();
    if (!listener) {
      spdlog::trace("No left button listener");
      return;
    }

    if (!listener->TheButton) {
      spdlog::trace("No left button");
      return;
    }

    listener->TheButton->Press();
  }

  void PressRightArrow()
  {
    auto listener = RightArrowButton();
    if (!listener) {
      spdlog::info("No right button listener");
      return;
    }

    if (!listener->TheButton) {
      spdlog::info("No right button");
      return;
    }

    listener->TheButton->Press();
  }

private:
  SemaphoreButtonListener* LeftArrowButton()
  {
    static auto field = get_class_helper().GetField("_leftArrowButton").offset();
    return *reinterpret_cast<SemaphoreButtonListener**>(reinterpret_cast<uint8_t*>(this) + field);
  }

  SemaphoreButtonListener* RightArrowButton()
  {
    static auto field = get_class_helper().GetField("_rightArrowButton").offset();
    return *reinterpret_cast<SemaphoreButtonListener**>(reinterpret_cast<uint8_t*>(this) + field);
  }
};
