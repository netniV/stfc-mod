#include "forbidden_tech.h"
#include "config.h"
#include "patches/runtime_config.h"

namespace mod_settings
{
BooleanSetting& ForbiddenTechConfirmationSetting()
{
  static BooleanSetting setting({"community_mod.ft_confirmation", "[MOD] Confirm Forbidden Tech upgrades",
                                 [] {
                                   return ForbiddenTechControlsAvailable()
                                              ? ReadResult::Known(!Config::Get().auto_confirm_ft_upgrade, 1)
                                              : ReadResult{};
                                 },
                                 [](bool confirm, std::uint64_t generation) {
                                   if (generation != 1 || !ForbiddenTechControlsAvailable())
                                     return ApplyResult::Rejected;
                                   Config::Get().auto_confirm_ft_upgrade = !confirm;
                                   runtime_config::SaveSetting("ui", "auto_confirm_ft_upgrade", !confirm);
                                   return ApplyResult::Applied;
                                 }});
  return setting;
}
} // namespace mod_settings
