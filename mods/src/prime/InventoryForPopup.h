#pragma once

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

struct InventoryForPopup {
public:
  __declspec(property(get = __get_MaxItemsToUse, put = __set_MaxItemsToUse)) int64_t MaxItemsToUse;
  __declspec(property(get = __get_IsDonationUse, put = __set_IsDonationUse)) bool    IsDonationUse;
  __declspec(property(get = __get_IsChestPurchase)) bool                             IsChestPurchase;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Inventories", "InventoryForPopup");
    return class_helper;
  }

public:
  int64_t __get_MaxItemsToUse()
  {
    static auto property = get_class_helper().GetProperty("MaxItemsToUse");
    const auto* value    = property.Get<int64_t>(this);
    return value ? *value : 0;
  }

  void __set_MaxItemsToUse(int64_t value)
  {
    static auto property = get_class_helper().GetProperty("MaxItemsToUse");
    property.SetRaw(this, value);
  }

  bool __get_IsDonationUse()
  {
    static auto field = get_class_helper().GetProperty("IsDonationUse");
    return field.GetRaw(this);
  }

  void __set_IsDonationUse(float v)
  {
    static auto field = get_class_helper().GetProperty("IsDonationUse");
    return field.SetRaw(this, v);
  }

  bool __get_IsChestPurchase()
  {
    static auto property = get_class_helper().GetProperty("IsChestPurchase");
    const auto* value    = property.Get<bool>(this);
    return value ? *value : false;
  }
};
