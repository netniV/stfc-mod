#pragma once

#include <il2cpp/il2cpp_helper.h>

template <typename T> struct MonoSingleton {
public:
  static T* Instance()
  {
    IL2CppClassHelper& helper = T::get_class_helper();
    static auto        parent = helper.GetParent("MonoSingleton`1");
    auto               p      = parent.GetProperty("Instance");
    auto               p2     = p.GetRaw<T>(nullptr);

    // Il2CppClassPointerStore<MonoSingleton<T>>.NativeClassPtr =
    // (__Null)IL2CPP.il2cpp_class_from_type(Il2CppSystem.Type.internal_from_handle(IL2CPP.il2cpp_class_get_type(IL2CPP.GetIl2CppClass("Assembly-CSharp.dll",
    // "", "MonoSingleton`1"))).MakeGenericType(new Il2CppReferenceArray<Il2CppSystem.Type>(new Il2CppSystem.Type[1]
    //	{
    //	  Il2CppSystem.Type.internal_from_handle(IL2CPP.il2cpp_class_get_type((System.IntPtr)
    // Il2CppClassPointerStore<T>.NativeClassPtr))
    //	})).TypeHandle.value);

    return p2;
  }
};
