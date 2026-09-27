#pragma once

namespace galaxy_controls
{
enum class ZoomMode { Native, Always, Threshold };
enum class Detail { Native, Expanded, Compact };

struct ZoomProfile {
  ZoomMode mode = ZoomMode::Native;
  float threshold = 1.f;
  bool operator==(const ZoomProfile&) const = default;
};

// Normalized zoom runs from near=0 to far=1. Compact requests native Far
// presentation; native rules may still show names for some systems.
constexpr Detail ResolveDetail(ZoomProfile profile, bool zoom_valid, float zoom)
{
  if (profile.mode == ZoomMode::Always) return Detail::Expanded;
  if (profile.mode != ZoomMode::Threshold || !zoom_valid) return Detail::Native;
  return profile.threshold >= 1.f || (profile.threshold > 0.f && zoom <= profile.threshold)
             ? Detail::Expanded : Detail::Compact;
}

// Native boundary: Default=0, Mining=1, Hostiles=2, Hazards=3.
// Compact selection storage; names use bit 3 and overlays use bits 0 through 2.
class OverlaySelection {
public:
  static constexpr OverlaySelection FromFlags(bool names, bool mining, bool hostiles, bool hazards)
  { return OverlaySelection((names ? 8 : 0) | (mining ? 1 : 0) | (hostiles ? 2 : 0) | (hazards ? 4 : 0)); }
  static constexpr OverlaySelection FromNativeMode(int mode) { return OverlaySelection(Bit(mode)); }
  constexpr bool Contains(int native_mode) const { return (bits_ & Bit(native_mode)) != 0; }
  constexpr bool Multiple() const { return (bits_ & (bits_ - 1)) != 0; }
  constexpr int Bits() const { return bits_; }

  constexpr OverlaySelection Toggle(int native_mode) const
  {
    if (!Bit(native_mode)) return *this;
    const int next = bits_ ^ Bit(native_mode);
    return OverlaySelection(next ? next : Bit(0));
  }

  constexpr int Representative(int preferred_mode) const
  {
    if (Contains(0)) return 0;
    if (Contains(preferred_mode)) return preferred_mode;
    for (int mode = 1; mode <= 3; ++mode)
      if (Contains(mode)) return mode;
    return 0;
  }

private:
  explicit constexpr OverlaySelection(int bits) : bits_(bits) {}
  static constexpr int Bit(int mode)
  { return mode == 0 ? 8 : mode >= 1 && mode <= 3 ? 1 << (mode - 1) : 0; }
  int bits_;
};
} // namespace galaxy_controls
