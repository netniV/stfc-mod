#pragma once

#include "Hub.h"
#include "MonoSingleton.h"
#include <spdlog/spdlog.h>

struct PlanetaryBaseManager : MonoSingleton<PlanetaryBaseManager> {
  friend struct MonoSingleton<PlanetaryBaseManager>;

public:
  static void ViewOwnHaven()
  {
    // M95 v2: Haven is Starbase_Planetary, with a PlanetarySectionContext argument.
    static auto& manager_class = get_class_helper();
    static auto  context_class =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Starbase", "PlanetarySectionContext");
    static auto  is_ready = manager_class.GetMethod<bool(PlanetaryBaseManager*, bool)>("IsPlanetaryBaseCenterReady", 1);
    static auto  get_user_id = manager_class.GetMethod<Il2CppString*(PlanetaryBaseManager*)>("get_LocalUserId", 0);
    static auto  ctor        = context_class.GetMethod<void(void*)>(".ctor", 0);
    static auto* owner_field =
        context_class.get_cls() ? il2cpp_class_get_field_from_name(context_class.get_cls(), "OwnerUserId") : nullptr;
    if (!manager_class.get_cls() || !is_ready || !get_user_id || !ctor || !owner_field) {
      static bool warned = false;
      if (!warned) {
        warned = true;
        spdlog::warn("[ShowHaven] Planetary navigation is unavailable in this client");
      }
      return;
    }

    auto* manager  = Instance();
    auto* sections = Hub::get_SectionManager();
    if (!manager || !sections) {
      return;
    }

    // Preserve the game's Iconian Gateway prerequisite and its locked-building message.
    if (!is_ready(manager, true)) {
      return;
    }
    auto* owner = get_user_id(manager);
    if (!owner || il2cpp_string_length(owner) == 0) {
      return;
    }

    auto* context = context_class.New<Il2CppObject>();
    if (!context) {
      return;
    }
    ctor(context);
    // A fresh context targets our base even after visiting another player's Haven.
    // Use the runtime setter so the managed reference receives the GC write barrier.
    il2cpp_field_set_value_object(context, owner_field, reinterpret_cast<Il2CppObject*>(owner));
    sections->TriggerSectionChange(SectionID::Starbase_Planetary, context, false, false, true);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.PlanetaryBase", "PlanetaryBaseManager");
    return helper;
  }
};
