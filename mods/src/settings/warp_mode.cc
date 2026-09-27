#include "warp_mode.h"
#include "config.h"
#include "patches/runtime_config.h"
#include <spdlog/spdlog.h>

namespace mod_settings
{
ChoiceSetting& WarpModeSetting()
{
  static ChoiceSetting setting(
      {"community_mod.warp_mode", "Instant warp mode",
       [] { return ValueReadResult<int>::Known(static_cast<int>(Config::Get().auto_confirm_instant_warp), 1); },
       [](int value, std::uint64_t generation) {
         if (generation != 1)
           return ApplyResult::Rejected;
         return SetWarpMode(static_cast<InstantWarpConfirmation>(value)) ? ApplyResult::Applied : ApplyResult::Rejected;
       }},
      {"Normal (ask)", "Warp", "Jump"});
  return setting;
}
bool SetWarpMode(InstantWarpConfirmation desired)
{
  const char* value = nullptr;
  switch (desired) {
    case InstantWarpConfirmation::None:
      value = "none";
      break;
    case InstantWarpConfirmation::Warp:
      value = "warp";
      break;
    case InstantWarpConfirmation::Jump:
      value = "jump";
      break;
    default:
      return false;
  }
  auto& config = Config::Get();
  if (config.auto_confirm_instant_warp == desired)
    return true;
  config.auto_confirm_instant_warp = desired;
  spdlog::info("Auto-confirm instant warp set to {}", value);
  runtime_config::SaveWarpMode(value);
  return true;
}

void CycleWarpMode()
{
  auto&      setting  = WarpModeSetting().state();
  const auto snapshot = setting.Observe();
  if (snapshot.state.known())
    setting.SetFromUser((*snapshot.state.value + 1) % 3, snapshot);
}
} // namespace mod_settings
