#pragma once

#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

#include "ChatManager.h"

class SwipeScroller
{
public:
  __declspec(property(get = __get__currentContentIndex)) int32_t _currentContentIndex;

  void FocusOnInstantly(int32_t index)
  {
    static auto FocusOnInstantlyMethod = get_class_helper().GetMethod<void(SwipeScroller*, int32_t)>("FocusOnInstantly");
    static auto FocusOnInstantlyWarn = true;
    if (FocusOnInstantlyMethod) {
      FocusOnInstantlyMethod(this, index);
    } else if (FocusOnInstantlyWarn) {
      FocusOnInstantlyWarn = false;
      ErrorMsg::MissingMethod("SwipeScroller", "FocusOnInstantly");
    }
  }

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "SwipeScroller");
    return class_helper;
  }

  int32_t __get__currentContentIndex()
  {
    static auto field = get_class_helper().GetField("_currentContentIndex");
    return *(int32_t*)((ptrdiff_t)this + field.offset());
  }
};

class ChatPreviewController
{
public:
  __declspec(property(get = __get__panelIndicators)) Il2CppArray* _panelIndicators;
  __declspec(property(get = __get__swipeScroller)) SwipeScroller* _swipeScroller;
  __declspec(property(get = __get__focusedPanel, put = __set__focusedPanel)) ChatChannelCategory _focusedPanel;

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Chat", "ChatPreviewController");
    return class_helper;
  }

  Il2CppArray* __get__panelIndicators()
  {
    static auto field = get_class_helper().GetField("_panelIndicators");
    return *(Il2CppArray**)((ptrdiff_t)this + field.offset());
  }

  SwipeScroller* __get__swipeScroller()
  {
    static auto field = get_class_helper().GetField("_swipeScroller");
    return *(SwipeScroller**)((ptrdiff_t)this + field.offset());
  }

  ChatChannelCategory __get__focusedPanel()
  {
    static auto field = get_class_helper().GetField("_focusedPanel");
    return *(ChatChannelCategory*)((ptrdiff_t)this + field.offset());
  }

  void __set__focusedPanel(ChatChannelCategory v)
  {
    static auto field                                         = get_class_helper().GetField("_focusedPanel");
    *(ChatChannelCategory*)((ptrdiff_t)this + field.offset()) = v;
  }
};
