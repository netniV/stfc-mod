#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

// Cargo display uses a ColourTextLocalizer (Digit.Client.UI.ColourTextLocalizer)
// with identifier "shared_x_of_y_x_coloured". Number formatting is controlled by
// the m_significantDecimals field on the TextLocalizer base class
// (Digit.Client.UI.TextLocalizer). Fields are resolved at runtime via the field
// API — no hardcoded offsets are used.
//
// The number formatting that reads m_significantDecimals happens in Localize(),
// which runs AFTER SetLocalTextParameters. So we permanently set the field on
// matching cargo localizer instances (rather than temporarily overriding and
// restoring), ensuring Localize() reads the overridden value.

static constexpr char  kCargoIdentifier[]    = "shared_x_of_y_x_coloured";
static constexpr size_t kCargoIdentifierLen  = sizeof(kCargoIdentifier) - 1; // exclude trailing '\0'

void ColourTextLocalizer_SetLocalTextParameters_Hook(auto original, void* _this, bool parseID, void* args)
{
  if (!_this) {
    original(_this, parseID, args);
    return;
  }

  // Clamp user config to a safe range. Negative values are treated as 0
  // (no decimals); values above 6 are capped to avoid absurd formatting.
  int desired_decimals = Config::Get().cargo_significant_decimals;
  if (desired_decimals < 0) desired_decimals = 0;
  if (desired_decimals > 6) desired_decimals = 6;

  // Resolve fields from the TextLocalizer base class (resolved once, cached).
  static auto text_localizer_h =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "TextLocalizer");
  static auto identifier_field = text_localizer_h.GetField("m_identifier");
  static auto sig_decimals_field = text_localizer_h.GetField("m_significantDecimals");

  if (!identifier_field.isValidHelper() || !sig_decimals_field.isValidHelper()) {
    original(_this, parseID, args);
    return;
  }

  // Read the identifier string pointer.
  auto* identifier_ptr = *reinterpret_cast<Il2CppString**>((char*)_this + identifier_field.offset());
  if (!identifier_ptr) {
    original(_this, parseID, args);
    return;
  }

  // Fast path: compare length first, then raw UTF-16 chars (the identifier is
  // ASCII, so each Il2CppChar fits in the low byte). This avoids allocating a
  // std::string via to_string on every call — SetLocalTextParameters fires for
  // every ColourTextLocalizer in the UI, not just cargo.
  if ((size_t)identifier_ptr->length != kCargoIdentifierLen) {
    original(_this, parseID, args);
    return;
  }
  const Il2CppChar* chars = identifier_ptr->chars;
  for (size_t i = 0; i < kCargoIdentifierLen; ++i) {
    if ((char)chars[i] != kCargoIdentifier[i]) {
      original(_this, parseID, args);
      return;
    }
  }

  // Identifier matches — permanently set m_significantDecimals on this instance.
  // The actual number formatting happens later in Localize(), so we must not
  // restore the value here. The field is set every time SetLocalTextParameters
  // fires for this localizer, keeping it consistent.
  auto* sig_decimals_ptr = reinterpret_cast<int*>((char*)_this + sig_decimals_field.offset());
  if (*sig_decimals_ptr != desired_decimals) {
    *sig_decimals_ptr = desired_decimals;
  }

  original(_this, parseID, args);
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
