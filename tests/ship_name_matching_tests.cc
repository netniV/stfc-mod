#include "ship_name_match.h"
#include <cstring>
#include <memory>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(std::declval<FleetPlayerData>().GetLocaId()), int64_t>);

void CheckName(std::u16string_view native_name, std::string_view configured_name)
{
  auto storage = std::make_unique<std::byte[]>(sizeof(Il2CppString) + native_name.size() * sizeof(char16_t));
  auto* native = reinterpret_cast<Il2CppString*>(storage.get());
  native->length = static_cast<int32_t>(native_name.size());
  std::memcpy(native->chars, native_name.data(), native_name.size() * sizeof(char16_t));
  const auto display = ShipNameMatch::SplitWords(ShipNameMatch::NormalizeKey(to_string(native)));
  const auto configured = ShipNameMatch::SplitWords(ShipNameMatch::NormalizeKey(configured_name));
  if (!ShipNameMatch::MatchesDisplay(display, configured))
    throw std::runtime_error("Localized ship name did not match its UTF-8 config value");
  if (ShipNameMatch::MatchesDisplay(display, {"UNRELATED"}))
    throw std::runtime_error("Unrelated ship matched");
}

int main()
{
  CheckName(u"U.S.S. Enterprise", "Enterprise");
  CheckName(u"La Sir\u00e8ne", "La Sir\xc3\xa8ne");
  CheckName(u"\u041a\u043e\u0440\u0430\u0431\u043b\u044c", "\xd0\x9a\xd0\xbe\xd1\x80\xd0\xb0\xd0\xb1\xd0\xbb\xd1\x8c");
  if (ShipNameMatch::MatchesDisplay({"TITAN", "A"}, {"TITAN"}))
    throw std::runtime_error("Titan incorrectly matched Titan-A");
}
