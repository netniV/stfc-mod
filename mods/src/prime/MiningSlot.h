#pragma once

#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <cstdint>
#include <limits>

// FleetPlayerData.MiningData is IMiningSlot; resolve its actual implementation.
// Both MiningSlot and FleetMiningSlot use the same validated numeric contracts.
struct MiningSlot {
  __declspec(property(get = __get_ResourceId)) int64_t ResourceId;
  __declspec(property(get = __get_MiningSpeed)) float MiningSpeed;
  __declspec(property(get = __get_AmountMined)) double AmountMined;
  __declspec(property(get = __get_Amount)) int64_t Amount;

private:
  template <typename T> T Read(const char* name, const char* explicit_name, const char* type, T unavailable)
  {
    auto* object = reinterpret_cast<Il2CppObject*>(this);
    auto* cls = il2cpp_object_get_class(object);
    static auto slot_interface = il2cpp_get_class_helper(
        "Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "IMiningSlot");
    if (!cls || !slot_interface.get_cls() || !il2cpp_class_is_assignable_from(slot_interface.get_cls(), cls))
      return unavailable;
    const MethodInfo* method = nullptr;
    for (auto* owner = cls; owner && !method; owner = il2cpp_class_get_parent(owner)) {
      const auto* public_getter = method_contract::Resolve(owner, name, false, type, {});
      const auto* explicit_getter = method_contract::Resolve(owner, explicit_name, false, type, {});
      if (public_getter && explicit_getter) return unavailable;
      method = public_getter ? public_getter : explicit_getter;
    }
    Il2CppObject* value = nullptr;
    if (!method || method->has_full_generic_sharing_signature
        || !Il2CppRuntime::TryInvoke(method, object, nullptr, &value) || !value
        || il2cpp_object_get_class(value) != il2cpp_class_from_type(method->return_type))
      return unavailable;
    return *static_cast<T*>(il2cpp_object_unbox(value));
  }

public:
  int64_t __get_ResourceId()
  { return Read<int64_t>("get_ResourceId", "Digit.PrimeServer.Models.IMiningSlot.get_ResourceId", "System.Int64", 0); }
  float __get_MiningSpeed()
  { return Read<float>("get_MiningSpeed", "Digit.PrimeServer.Models.IMiningSlot.get_MiningSpeed", "System.Single", 0.0f); }
  double __get_AmountMined()
  { return Read<double>("get_AmountMined", "Digit.PrimeServer.Models.IMiningSlot.get_AmountMined", "System.Double",
                        std::numeric_limits<double>::quiet_NaN()); }
  int64_t __get_Amount()
  { return Read<int64_t>("get_Amount", "Digit.PrimeServer.Models.IMiningSlot.get_Amount", "System.Int64", -1); }
};
