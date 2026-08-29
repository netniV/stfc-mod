#include "patches/notification_audio.h"

#include "patches/notification_audio_platform.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#if _WIN32
#include <Windows.h>
#include <mmsystem.h>
#endif

namespace
{
struct ToneSegment {
  double frequency_hz;
  int    duration_ms;
};

constexpr int    kSampleRate = 44100;
constexpr double kTwoPi      = 6.28318530717958647692;
constexpr double kAmplitude  = 0.26;

constexpr std::array<ToneSegment, 4> kDefaultPattern{{{740.0, 70}, {0.0, 22}, {880.0, 85}, {0.0, 18}}};
constexpr std::array<ToneSegment, 3> kInfoPattern{{{659.0, 80}, {0.0, 24}, {880.0, 110}}};
constexpr std::array<ToneSegment, 5> kSuccessPattern{{{587.0, 70}, {0.0, 18}, {740.0, 70}, {0.0, 18}, {988.0, 120}}};
constexpr std::array<ToneSegment, 5> kWarningPattern{{{622.0, 90}, {0.0, 36}, {466.0, 110}, {0.0, 28}, {466.0, 90}}};
constexpr std::array<ToneSegment, 5> kAlarmPattern{{{880.0, 90}, {0.0, 42}, {880.0, 90}, {0.0, 42}, {698.0, 160}}};
constexpr std::array<ToneSegment, 5> kArrivalPattern{{{523.0, 65}, {0.0, 18}, {659.0, 70}, {0.0, 18}, {1046.0, 125}}};
constexpr std::array<ToneSegment, 3> kSoftPattern{{{523.0, 90}, {0.0, 26}, {659.0, 110}}};
constexpr std::array<ToneSegment, 1> kPingPattern{{{1046.0, 95}}};
constexpr std::array<ToneSegment, 5> kRepairPattern{{{440.0, 70}, {0.0, 18}, {554.0, 70}, {0.0, 18}, {740.0, 140}}};

std::once_flag                                                                  s_sound_buffers_once;
std::array<std::vector<uint8_t>, static_cast<size_t>(NotificationSound::Count)> s_sound_buffers;

std::string normalize_sound_name(std::string_view value)
{
  std::string normalized;
  normalized.reserve(value.size());
  for (const auto ch : value) {
    if (ch >= 'A' && ch <= 'Z') {
      normalized.push_back(static_cast<char>(ch - 'A' + 'a'));
    } else if (ch == '-' || ch == ' ') {
      normalized.push_back('_');
    } else {
      normalized.push_back(ch);
    }
  }
  return normalized;
}

std::span<const ToneSegment> sound_pattern(NotificationSound sound)
{
  switch (sound) {
    case NotificationSound::Default:
      return kDefaultPattern;
    case NotificationSound::Info:
      return kInfoPattern;
    case NotificationSound::Success:
      return kSuccessPattern;
    case NotificationSound::Warning:
      return kWarningPattern;
    case NotificationSound::Alarm:
      return kAlarmPattern;
    case NotificationSound::Arrival:
      return kArrivalPattern;
    case NotificationSound::Soft:
      return kSoftPattern;
    case NotificationSound::Ping:
      return kPingPattern;
    case NotificationSound::Repair:
      return kRepairPattern;
    default:
      return {};
  }
}

void append_u16(std::vector<uint8_t>& buffer, uint16_t value)
{
  buffer.push_back(static_cast<uint8_t>(value & 0xff));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

void append_u32(std::vector<uint8_t>& buffer, uint32_t value)
{
  buffer.push_back(static_cast<uint8_t>(value & 0xff));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

void append_ascii(std::vector<uint8_t>& buffer, std::string_view value)
{ buffer.insert(buffer.end(), value.begin(), value.end()); }

std::vector<uint8_t> build_wav(std::span<const ToneSegment> pattern)
{
  uint32_t sample_count = 0;
  for (const auto& segment : pattern) {
    sample_count += static_cast<uint32_t>((static_cast<int64_t>(segment.duration_ms) * kSampleRate) / 1000);
  }

  constexpr uint16_t channels        = 1;
  constexpr uint16_t bits_per_sample = 16;
  const uint32_t     data_bytes      = sample_count * channels * (bits_per_sample / 8);

  std::vector<uint8_t> buffer;
  buffer.reserve(44 + data_bytes);
  append_ascii(buffer, "RIFF");
  append_u32(buffer, 36 + data_bytes);
  append_ascii(buffer, "WAVE");
  append_ascii(buffer, "fmt ");
  append_u32(buffer, 16);
  append_u16(buffer, 1);
  append_u16(buffer, channels);
  append_u32(buffer, kSampleRate);
  append_u32(buffer, kSampleRate * channels * (bits_per_sample / 8));
  append_u16(buffer, channels * (bits_per_sample / 8));
  append_u16(buffer, bits_per_sample);
  append_ascii(buffer, "data");
  append_u32(buffer, data_bytes);

  double phase = 0.0;
  for (const auto& segment : pattern) {
    const auto segment_samples = static_cast<int>((static_cast<int64_t>(segment.duration_ms) * kSampleRate) / 1000);
    const auto step            = segment.frequency_hz > 0.0 ? kTwoPi * segment.frequency_hz / kSampleRate : 0.0;
    for (int index = 0; index < segment_samples; ++index) {
      const auto fade_in  = std::min(1.0, static_cast<double>(index) / 80.0);
      const auto fade_out = std::min(1.0, static_cast<double>(segment_samples - index) / 120.0);
      const auto sample   = segment.frequency_hz > 0.0 ? std::sin(phase) * kAmplitude * fade_in * fade_out : 0.0;
      append_u16(buffer, static_cast<uint16_t>(static_cast<int16_t>(sample * 32767.0)));
      phase += step;
      if (phase > kTwoPi) {
        phase -= kTwoPi;
      }
    }
  }
  return buffer;
}

void initialize_sound_buffers()
{
  for (size_t index = 0; index < s_sound_buffers.size(); ++index) {
    const auto pattern = sound_pattern(static_cast<NotificationSound>(index));
    if (!pattern.empty()) {
      s_sound_buffers[index] = build_wav(pattern);
    }
  }
}
} // namespace

const char* notification_sound_name(NotificationSound sound)
{
  switch (sound) {
    case NotificationSound::None:
      return "none";
    case NotificationSound::Default:
      return "default";
    case NotificationSound::Info:
      return "info";
    case NotificationSound::Success:
      return "success";
    case NotificationSound::Warning:
      return "warning";
    case NotificationSound::Alarm:
      return "alarm";
    case NotificationSound::Arrival:
      return "arrival";
    case NotificationSound::Soft:
      return "soft";
    case NotificationSound::Ping:
      return "ping";
    case NotificationSound::Repair:
      return "repair";
    default:
      return "none";
  }
}

std::optional<NotificationSound> notification_sound_from_name(std::string_view name)
{
  const auto normalized = normalize_sound_name(name);
  if (normalized == "none" || normalized == "off" || normalized == "silent")
    return NotificationSound::None;
  if (normalized == "default")
    return NotificationSound::Default;
  if (normalized == "info")
    return NotificationSound::Info;
  if (normalized == "success" || normalized == "victory")
    return NotificationSound::Success;
  if (normalized == "warning" || normalized == "warn")
    return NotificationSound::Warning;
  if (normalized == "alarm" || normalized == "attack")
    return NotificationSound::Alarm;
  if (normalized == "arrival" || normalized == "arrive")
    return NotificationSound::Arrival;
  if (normalized == "soft" || normalized == "quiet")
    return NotificationSound::Soft;
  if (normalized == "ping")
    return NotificationSound::Ping;
  if (normalized == "repair" || normalized == "repaired")
    return NotificationSound::Repair;
  return std::nullopt;
}

void notification_audio_play(NotificationSound sound)
{
  if (sound == NotificationSound::None)
    return;

  std::call_once(s_sound_buffers_once, initialize_sound_buffers);
  const auto index = static_cast<size_t>(sound);
  if (index >= s_sound_buffers.size() || s_sound_buffers[index].empty())
    return;

  const auto& buffer = s_sound_buffers[index];
  if (!notification_audio_platform_play(buffer.data(), buffer.size())) {
    spdlog::warn("[NotifyAudio] Failed to play '{}' cue", notification_sound_name(sound));
  }
}

#if _WIN32
bool notification_audio_platform_play(const uint8_t* data, size_t)
{ return PlaySoundW(reinterpret_cast<LPCWSTR>(data), nullptr, SND_ASYNC | SND_MEMORY | SND_NODEFAULT) != FALSE; }
#elif !defined(__APPLE__)
bool notification_audio_platform_play(const uint8_t*, size_t)
{ return false; }
#endif
