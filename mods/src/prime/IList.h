#pragma once

#include <il2cpp/il2cpp_helper.h>

class IList : Il2CppObject
{
public:
  __declspec(property(get = __get_Count)) int Count;

  Il2CppObject* Get(int32_t index)
  {
    static auto GetImpl =
        get_class_helper().GetMethodSpecial2<Il2CppObject*(IList*, int32_t)>((Il2CppObject*)(this), "get_Item");
    return GetImpl(this, index);
  }

  void RemoveAt(int32_t index)
  {
    static auto RemoveAtImpl =
        get_class_helper().GetMethodSpecial2<void(IList*, int32_t)>((Il2CppObject*)(this), "RemoveAt");
    if (RemoveAtImpl != nullptr) {
      RemoveAtImpl(this, index);
    }
  }

private:
  IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = IL2CppClassHelper{this->klass};
    return class_helper;
  }

public:
  int __get_Count()
  {
    static auto field = get_class_helper().GetField("_size").offset();
    return *(int*)((char*)this + field);
  }
};