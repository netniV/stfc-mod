#include "galaxy_policy.h"
#include <array>

namespace
{
using namespace galaxy_controls;

static_assert(ResolveDetail({ZoomMode::Native, .5f}, true, 0.f) == Detail::Native);
static_assert(ResolveDetail({ZoomMode::Native, .5f}, true, 1.f) == Detail::Native);
static_assert(ResolveDetail({ZoomMode::Always, .5f}, false, 0.f) == Detail::Expanded);
static_assert(ResolveDetail({ZoomMode::Always, .5f}, true, 1.f) == Detail::Expanded);
static_assert(ResolveDetail({ZoomMode::Threshold, 0.f}, true, 0.f) == Detail::Compact);
static_assert(ResolveDetail({ZoomMode::Threshold, 1.f}, true, 1.f) == Detail::Expanded);
static_assert(ResolveDetail({ZoomMode::Threshold, .5f}, true, .499f) == Detail::Expanded);
static_assert(ResolveDetail({ZoomMode::Threshold, .5f}, true, .5f) == Detail::Expanded);
static_assert(ResolveDetail({ZoomMode::Threshold, .5f}, true, .501f) == Detail::Compact);
static_assert(ResolveDetail({ZoomMode::Threshold, .5f}, false, 0.f) == Detail::Native);
static_assert(ResolveDetail({ZoomMode::Threshold, 1.f}, false, 1.f) == Detail::Native);

// All 15 non-empty combinations, expressed in native button order rather than
// the runtime's bit encoding. Each click must affect only that layer, except
// that removing the last layer restores Default.
constexpr bool CheckSelections()
{
  constexpr std::array<std::array<bool, 4>, 15> cases{{
      {true, false, false, false}, {false, true, false, false},
      {false, false, true, false}, {false, false, false, true},
      {true, true, false, false}, {true, false, true, false}, {true, false, false, true},
      {false, true, true, false}, {false, true, false, true}, {false, false, true, true},
      {true, true, true, false}, {true, true, false, true}, {true, false, true, true},
      {false, true, true, true}, {true, true, true, true}}};
  for (const auto& layers : cases) {
    auto selection = OverlaySelection::FromNativeMode(-1);
    int count = 0;
    for (int mode = 0; mode < 4; ++mode)
      if (layers[mode]) { selection = selection.Toggle(mode); ++count; }
    if (selection.Multiple() != (count > 1)) return false;
    for (int clicked = 0; clicked < 4; ++clicked) {
      const auto next = selection.Toggle(clicked);
      const bool empty = count == 1 && layers[clicked];
      for (int mode = 0; mode < 4; ++mode) {
        const bool expected = empty ? mode == 0 : mode == clicked ? !layers[mode] : layers[mode];
        if (selection.Contains(mode) != layers[mode] || next.Contains(mode) != expected) return false;
      }
      const int representative = next.Representative(clicked);
      if (!next.Contains(representative)) return false;
      if (next.Contains(0) && representative != 0) return false;
      if (!next.Contains(0) && next.Contains(clicked) && representative != clicked) return false;
      if (!next.Contains(0) && !next.Contains(clicked))
        for (int mode = 1; mode < representative; ++mode)
          if (next.Contains(mode)) return false;
    }
    if (selection.Toggle(-1).Bits() != selection.Bits()
        || selection.Toggle(4).Bits() != selection.Bits()) return false;
  }
  return true;
}
static_assert(CheckSelections());
static_assert(OverlaySelection::FromFlags(true, false, false, false).Bits() == 8);
static_assert(OverlaySelection::FromFlags(false, true, true, true).Bits() == 7);
static_assert(OverlaySelection::FromNativeMode(0).Bits() == 8);
static_assert(OverlaySelection::FromNativeMode(1).Bits() == 1);
static_assert(OverlaySelection::FromNativeMode(2).Bits() == 2);
static_assert(OverlaySelection::FromNativeMode(3).Bits() == 4);
static_assert(!OverlaySelection::FromNativeMode(-1).Multiple());
static_assert(!OverlaySelection::FromNativeMode(4).Contains(4));
} // namespace
