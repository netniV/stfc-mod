#pragma once

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>

#include "errormsg.h"

struct SelectableList {
public:
  __declspec(property(get = __get_SelectedIndex)) int32_t SelectedIndex;
  __declspec(property(get = __get_Count)) int32_t Count;

  bool RequestSelect(int32_t index, bool simulated = false)
  {
    static auto RequestSelectWarn   = true;
    static auto RequestSelectMethod =
        get_selectable_list_base_helper().GetMethodSpecial<void(SelectableList*, int32_t, bool)>(
            "RequestSelect", [](auto count, auto params) {
              if (count != 2) {
                return false;
              }
              return params[0]->type == IL2CPP_TYPE_I4;
            });

    if (RequestSelectMethod) {
      RequestSelectMethod(this, index, simulated);
      return true;
    } else if (RequestSelectWarn) {
      RequestSelectWarn = false;
      ErrorMsg::MissingMethod("SelectableListBase", "RequestSelect");
    }

    return false;
  }

  void* DataItem(int32_t index)
  {
    static auto dataProp = get_selectable_list_base_helper().GetProperty("Data");
    auto*       list     = dataProp.GetRaw<Il2CppObject>(this);
    if (!list) return nullptr;

    auto* listClass = il2cpp_object_get_class(list);
    if (!listClass) return nullptr;

    auto* getItem = il2cpp_class_get_method_from_name(listClass, "get_Item", 1);
    if (!getItem) return nullptr;

    Il2CppException* exception = nullptr;
    void*             args[1]  = {&index};
    auto*             item     = il2cpp_runtime_invoke(getItem, list, args, &exception);
    if (exception) return nullptr;

    return item;
  }

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "SelectableList");
    return class_helper;
  }

private:
  static IL2CppClassHelper& get_selectable_list_base_helper()
  {
    static IL2CppClassHelper class_helper = get_class_helper().GetParent("SelectableListBase");
    return class_helper;
  }

  static IL2CppClassHelper& get_base_list_container_helper()
  {
    static IL2CppClassHelper class_helper = get_class_helper().GetParent("BaseListContainer");
    return class_helper;
  }

public:
  int32_t __get_SelectedIndex()
  {
    static auto field = get_class_helper().GetProperty("SelectedIndex");
    return *field.Get<int32_t>(this);
  }

  int32_t __get_Count()
  {
    static auto field = get_base_list_container_helper().GetProperty("Count");
    return *field.Get<int32_t>(this);
  }
};
