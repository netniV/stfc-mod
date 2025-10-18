#pragma once

#include <il2cpp/il2cpp_helper.h>

enum DataType : int32_t
{
  Unspecified = -1,
  JSON = 0,
  MsgPack = 1,
  ProtoBuff = 2,
  UrlEncoded = 3,
};

struct RealtimeDataPayload
{
public:
  __declspec(property(get = __get_dataType)) Il2CppString* DataType;
  __declspec(property(get = __get_channelId)) Il2CppString* ChannelId;
  __declspec(property(get = __get_instanceIdJson)) int32_t InstanceIdJson;
  __declspec(property(get = __get_target)) Il2CppString* Target;
  __declspec(property(get = __get_source)) Il2CppString* Source;
  __declspec(property(get = __get_data)) Il2CppString* Data;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Networking.RTC", "RealtimeDataPayload");
    return class_helper;
  }

public:
  Il2CppString* __get_dataType()
  {
    static auto prop = get_class_helper().GetProperty("DataType");
    return prop.GetRaw<Il2CppString>(this);
  }

  Il2CppString* __get_channelId()
  {
    static auto prop = get_class_helper().GetProperty("ChannelId");
    return prop.GetRaw<Il2CppString>(this);
  }

  int32_t __get_instanceIdJson()
  {
    static auto prop = get_class_helper().GetProperty("InstanceIdJson");
    return *prop.Get<int32_t>(this);
  }

  Il2CppString* __get_target()
  {
    static auto prop = get_class_helper().GetProperty("Target");
    return prop.GetRaw<Il2CppString>(this);
  }

  Il2CppString* __get_source()
  {
    static auto prop = get_class_helper().GetProperty("Source");
    return prop.GetRaw<Il2CppString>(this);
  }

  Il2CppString* __get_data()
  {
    static auto prop = get_class_helper().GetProperty("Data");
    return prop.GetRaw<Il2CppString>(this);
  }
};
