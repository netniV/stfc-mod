#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <str_utils.h>

// Field offsets on TextLocalizer (from IL2CPP dump):
//   m_identifier          (string, GraphicLocalizer base) — resolved via field API
//   m_significantDecimals (int)    offset 0x70
//   m_abbreviateNumbers   (bool)   offset 0x6C
//   m_truncateDecimals    (bool)   offset 0x6D
//
// The cargo display uses a ColourTextLocalizer with identifier "shared_x_of_y_x_coloured".
// We hook SetLocalTextParameters to temporarily override m_significantDecimals before
// the original method formats the number, then restore it afterwards.

static const char* kCargoIdentifier = "shared_x_of_y_x_coloured";

void ColourTextLocalizer_SetLocalTextParameters_Hook(auto original, void* _this, bool parseID, void* args)
{
  if (!_this) {
    original(_this, parseID, args);
    return;
  }

  const auto desired_decimals = Config::Get().cargo_significant_decimals;
  if (desired_decimals < 0) {
    original(_this, parseID, args);
    return;
  }

  // Resolve the m_identifier field from the GraphicLocalizer base class
  static auto text_localizer_h =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "TextLocalizer");
  static auto identifier_field = text_localizer_h.GetField("m_identifier");
  static auto sig_decimals_field = text_localizer_h.GetField("m_significantDecimals");

  if (!identifier_field.isValidHelper() || !sig_decimals_field.isValidHelper()) {
    original(_this, parseID, args);
    return;
  }

  // Read the identifier string
  auto* identifier_ptr = *reinterpret_cast<Il2CppString**>((char*)_this + identifier_field.offset());
  if (!identifier_ptr) {
    original(_this, parseID, args);
    return;
  }

  auto identifier = to_string(identifier_ptr);
  if (identifier != kCargoIdentifier) {
    original(_this, parseID, args);
    return;
  }

  // Save and override m_significantDecimals
  auto* sig_decimals_ptr = reinterpret_cast<int*>((char*)_this + sig_decimals_field.offset());
  int  original_decimals = *sig_decimals_ptr;

  if (original_decimals != desired_decimals) {
    *sig_decimals_ptr = desired_decimals;
    original(_this, parseID, args);
    *sig_decimals_ptr = original_decimals;
  } else {
    original(_this, parseID, args);
  }
}

void InstallCargoFormatHooks()
{
  auto colour_text_localizer_h =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "ColourTextLocalizer");
  if (!colour_text_localizer_h.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Client.UI", "ColourTextLocalizer");
    return;
  }

  auto ptr = colour_text_localizer_h.GetMethod("SetLocalTextParameters");
  if (!ptr) {
    ErrorMsg::MissingMethod("ColourTextLocalizer", "SetLocalTextParameters");
    return;
  }

  SPUD_STATIC_DETOUR(ptr, ColourTextLocalizer_SetLocalTextParameters_Hook);
  spdlog::info("Cargo format: significant decimals = {}", Config::Get().cargo_significant_decimals);
}
