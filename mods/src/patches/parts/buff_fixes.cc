#include "config.h"
#include "errormsg.h"
#include "prime_types.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/IBuffComparer.h>
#include <prime/IBuffData.h>

#include <spud/detour.h>

static bool BuffService_IsBuffConditionMet(auto original, int64_t _unused, BuffCondition condition,
                                           IBuffComparer *comparer, IBuffData *buffToCompare, bool excludeFactionBuffs)
{
  switch (condition) {
    case BuffCondition::CondSelfAtStation: {
      if (Config::Get().use_out_of_dock_power) {
        return false;
      }
    }

    default:
      break;
  }

  return original(_unused, condition, comparer, buffToCompare, excludeFactionBuffs);
}

void InstallBuffFixHooks()
{
  auto buffHelper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService");
  if (!buffHelper.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffService");
  } else {
    if (const auto ptr = buffHelper.GetMethod("IsBuffConditionMet"); ptr == nullptr) {
      ErrorMsg::MissingMethod("BuffServices", "IsBuffConditionMet");
    } else {
      SPUD_STATIC_DETOUR(ptr, BuffService_IsBuffConditionMet);
    }
  }
}
