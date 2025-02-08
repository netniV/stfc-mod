#pragma once

#include "errormsg.h"
#include <il2cpp/il2cpp_helper.h>

#include "GenericButtonContext.h"
#include "ObjectViewerBaseWidget.h"
#include "Widget.h"

struct ArmadaObjectViewerWidget : public ObjectViewerBaseWidget<ArmadaObjectViewerWidget> {
public:
  void SetCourseToArmada()
  {
    static auto SetCourseToArmadaMethod =
        get_class_helper().GetMethod<void(ArmadaObjectViewerWidget*)>("SetCourseToArmada");
    static auto SetCourseToAramdaWarn = true;

    if (SetCourseToArmadaMethod) {
      SetCourseToArmadaMethod(this);
    } else if (SetCourseToAramdaWarn) {
      SetCourseToAramdaWarn = false;
      ErrorMsg::MissingMethod("ArmadaObjectViewerWidget", "SetCourseToArmada");
    }
  }

  void ValidateThenJoinArmada()
  {
    static auto ValidateThenJoinArmadaMethod =
        get_class_helper().GetMethod<void(ArmadaObjectViewerWidget*)>("ValidateThenJoinArmada");
    static auto ValidateThenJoinArmadaWarn = true;

    if (ValidateThenJoinArmadaMethod) {
      ValidateThenJoinArmadaMethod(this);
    } else if (ValidateThenJoinArmadaWarn) {
      ValidateThenJoinArmadaWarn = false;
      ErrorMsg::MissingMethod("ArmadaObjectViewerWidget", "ValidateThenJoinArmada");
    }
  }

  void JoinArmada()
  {
    static auto JoinArmadaMethod = get_class_helper().GetMethod<void(ArmadaObjectViewerWidget*)>("JoinArmada");
    static auto JoinArmadaWarn   = true;

    if (JoinArmadaMethod) {
      JoinArmadaMethod(this);
    } else if (JoinArmadaWarn) {
      JoinArmadaWarn = false;
      ErrorMsg::MissingMethod("ArmadaObjectViewerWidget", "JoinArmada");
    }
  }

  bool HasJoinButton()
  {
    // TODO
    return false;
  }

private:
  friend class ObjectFinder<ArmadaObjectViewerWidget>;
  friend class ObjectViewerBaseWidget<ArmadaObjectViewerWidget>;

public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.ObjectViewer", "ArmadaObjectViewerWidget");
    return class_helper;
  }

public:
  GenericButtonContext* __get__joinContext()
  {
    static auto field = get_class_helper().GetField("_joinContext").offset();
    return *(GenericButtonContext**)((char*)this + field);
  }
};
