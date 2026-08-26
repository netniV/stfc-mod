#pragma once

#include "str_utils.h"

#include <prime/FleetPlayerData.h>
#include <prime/HullSpec.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

// Shared ship-name matching for pinned_ships and the instant_warp_* filters.
// Compares a config entry against a ship's HullSpec.Name and HullSpec.IdStr,
// normalized and matched on trailing words only (so "Athena" matches "USS
// Academy Athena" without a generic "Titan" matching "Titan-A").
namespace ShipNameMatch {

// Upper-cases, drops "_LIVE"/abbreviation dots/apostrophes, turns
// underscores/hyphens into spaces, and makes a leading "USS " optional.
inline std::string NormalizeKey(std::string_view raw)
{
  auto upper = AsciiStrToUpper(raw);
  upper      = std::string(StripSuffix(upper, "_LIVE"));

  std::string cleaned;
  cleaned.reserve(upper.size());
  for (const char c : upper) {
    if (c == '_' || c == '-') {
      cleaned.push_back(' ');
    } else if (c == '.' || c == '\'') {
      continue; // drop, don't split: "U.S.S." -> "USS", not "U S S"
    } else {
      cleaned.push_back(c);
    }
  }

  std::string collapsed;
  collapsed.reserve(cleaned.size());
  bool prev_space = false;
  for (const char c : cleaned) {
    const bool is_space = (c == ' ');
    if (is_space && prev_space) continue;
    collapsed.push_back(c);
    prev_space = is_space;
  }

  auto trimmed = StripAsciiWhitespace(collapsed);
  return std::string(StripPrefix(trimmed, "USS "));
}

inline std::vector<std::string> SplitWords(std::string_view key)
{
  std::vector<std::string> words;
  std::string              current;
  for (const char c : key) {
    if (c == ' ') {
      if (!current.empty()) {
        words.push_back(std::move(current));
        current.clear();
      }
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty()) words.push_back(std::move(current));
  return words;
}

// True if `pinned` is a trailing (suffix) subsequence of `candidate`'s words,
// e.g. pinned=["ATHENA"] matches candidate=["ACADEMY","ATHENA"].
inline bool EndsWithWords(const std::vector<std::string>& candidate, const std::vector<std::string>& pinned)
{
  if (pinned.empty() || candidate.size() < pinned.size()) return false;
  const size_t offset = candidate.size() - pinned.size();
  for (size_t i = 0; i < pinned.size(); ++i) {
    if (candidate[offset + i] != pinned[i]) return false;
  }
  return true;
}

// Ships whose HullSpec.Name/IdStr share no words with what players actually
// call them (e.g. GS-31's HullSpec.Name is "Junker"). `canonical` must exactly
// match one of a ship's normal candidates for its aliases to apply.
struct ShipAliasGroup {
  std::string_view              canonical;
  std::vector<std::string_view> aliases;
};

inline const std::vector<ShipAliasGroup>& KnownShipAliases()
{
  static const std::vector<ShipAliasGroup> groups = {
      {"Junker", {"GS-31"}},
      {"Franklin 2.0", {"Franklin-A"}},
      {"Vidar 2", {"Vidar Talios", "Talios", "Vi'dar Talios"}},
      {"USS Luna", {"USS Beatty", "Beatty"}},
      {"D'Vor NanDi", {"D'Vor Feesha", "Feesha", "Dvor Feesha"}},
      {"ROM_KID_Aug", {"Hijacked Legionary"}},
      {"FED_KID_Aug", {"Hijacked USS Mayflower", "Hijacked Mayflower"}},
      {"KLG_KID_Aug", {"Hijacked D3", "Hijacked D3 Class"}},
  };
  return groups;
}

// Word-sequences from a ship's Name/IdStr plus any known aliases (see
// KnownShipAliases). Optionally captures the raw strings for diagnostics.
inline std::vector<std::vector<std::string>> CandidateWords(FleetPlayerData* ship, std::string* debug_name = nullptr,
                                                             std::string* debug_idstr = nullptr)
{
  std::vector<std::vector<std::string>> candidates;
  if (!ship) return candidates;

  const auto hull = ship->Hull;
  if (!hull) return candidates;

  if (const auto name = hull->Name; name != nullptr) {
    const auto raw = to_string(name);
    if (debug_name) *debug_name = raw;
    if (auto words = SplitWords(NormalizeKey(raw)); !words.empty()) {
      candidates.push_back(std::move(words));
    }
  }

  if (const auto idstr = hull->IdStr; idstr != nullptr) {
    const auto raw = to_string(idstr);
    if (debug_idstr) *debug_idstr = raw;
    if (auto words = SplitWords(NormalizeKey(raw));
        !words.empty() && std::ranges::find(candidates, words) == candidates.end()) {
      candidates.push_back(std::move(words));
    }
  }

  for (const auto& group : KnownShipAliases()) {
    const auto canonical_words = SplitWords(NormalizeKey(group.canonical));
    if (canonical_words.empty()) continue;
    const bool is_this_ship =
        std::ranges::any_of(candidates, [&](const auto& words) { return words == canonical_words; });
    if (!is_this_ship) continue;

    for (const auto alias : group.aliases) {
      if (auto words = SplitWords(NormalizeKey(alias));
          !words.empty() && std::ranges::find(candidates, words) == candidates.end()) {
        candidates.push_back(std::move(words));
      }
    }
  }

  return candidates;
}

// True if any of `candidates` ends with `pinned_words`.
inline bool MatchesAny(const std::vector<std::vector<std::string>>& candidates,
                       const std::vector<std::string>& pinned_words)
{
  return std::ranges::any_of(candidates, [&](const auto& words) { return EndsWithWords(words, pinned_words); });
}

} // namespace ShipNameMatch
