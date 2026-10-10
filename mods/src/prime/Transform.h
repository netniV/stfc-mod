#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "GameObject.h"
#include "Vector3.h"

struct Transform {
  __declspec(property(get = __get_LocalScale, put = __set_LocalScale)) Vector3* localScale;
  __declspec(property(get = __get_ChildCount)) int32_t childCount;
  __declspec(property(get = __get_GameObject)) GameObject* gameObject;
  __declspec(property(get = __get_Parent)) Transform* parent;

  Vector3* __get_LocalScale()
  {
    static auto field = get_class_helper().GetProperty("localScale");
    return field.Get<Vector3>(this);
  }

  void __set_LocalScale(Vector3* v)
  {
    static auto prop = get_class_helper().GetProperty("localScale");
    return prop.SetRaw((void*)this, *v);
  }

  int32_t __get_ChildCount()
  {
    static auto field = get_class_helper().GetProperty("childCount");
    return *field.Get<int32_t>(this);
  }

  GameObject* __get_GameObject()
  {
    static auto field = get_class_helper().GetParent("Component").GetProperty("gameObject");
    return field.GetRaw<GameObject>(this);
  }

  Transform* GetChild(int32_t index)
  {
    static auto method = get_class_helper().GetMethod<Transform*(Transform*, int32_t)>("GetChild");
    return method != nullptr ? method(this, index) : nullptr;
  }

  Transform* __get_Parent()
  {
    static auto field = get_class_helper().GetProperty("parent");
    return field.GetRaw<Transform>(this);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
    return class_helper;
  }
};
