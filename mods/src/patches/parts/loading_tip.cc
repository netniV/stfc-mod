#include "config.h"
#include "errormsg.h"
#include "version.h"

#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/il2cpp-functions.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <cstring>
#include <random>

// =============================================================================
// Loading Tip Patch
//
// Hooks LoadingTipViewController.SetRandomTipLocalisedText to override the
// first tip shown during each loading/transition with a custom message.
//
// Behavior:
// - Loading screen (first OnEnable after init/reload): always show the
//   welcome tip with the mod version.
// - Transitions (subsequent OnEnable calls): 25% chance of overriding with
//   a custom tip, 75% chance of letting the game's server tip pass through.
//   Subsequent tips always pass through to the game.
// =============================================================================

namespace
{
// Tip counter — reset on each OnEnable (new transition/loading cycle).
static int  g_tipCount       = 0;
// True on the first OnEnable after init/reload (loading screen context).
static bool g_isLoadingScreen = true;

// RNG for weighted tip selection
static std::mt19937 g_rng{std::random_device{}()};

// LoadingTipViewController field offsets (from il2cpp.cs dump)
// _textMeshPro: TextMeshProUGUI at 0x20
constexpr ptrdiff_t kTextMeshProOffset = 0x20;

// Custom transition tips — 25% chance of overriding the game's server tip.
// Tips are weighted within the custom pool.
struct TipEntry
{
  const char* text;
  int         weight;
};
const TipEntry kCustomTips[] = {
  {"Did you know? You can customize zoom presets, hotkeys, and UI scale in the community mod settings file to suit your playstyle.", 15},
  {"The community mod supports custom loading screens, transition backgrounds, and borderless fullscreen mode for a better experience.", 10},
  {"Press F11 at any time to toggle between borderless fullscreen and windowed mode without restarting the game or losing your session.", 15},
  {"Visit https://stfc.pro to track player rankings and stats across over 236,000 players and 114 servers in Star Trek Fleet Command.", 50},
  {"Join the community Discord server to report bugs, request new features, and stay up to date with the latest mod releases and updates.", 10},
};

constexpr size_t kNumCustomTips = std::size(kCustomTips);

// 50% chance of overriding with a custom tip, 50% chance of pass-through.
constexpr int kCustomTipChance    = 50;
constexpr int kPassThroughChance  = 50;

// Build the welcome tip string with the mod version embedded.
std::string GetLoadingScreenTip()
{
  return "Welcome to Star Trek Fleet Command, Supported by the Community Mod v"
         VER_FILE_VERSION_STR "! Please check our discord for the latest information!";
}

// Set the text on the TextMeshProUGUI component
void SetTMPText(void* tmpObj, const std::string& text)
{
  if (!tmpObj) {
    spdlog::warn("[LoadingTip] _textMeshPro is null");
    return;
  }

  void* tipStr = il2cpp_string_new(text.c_str());
  if (!tipStr) {
    spdlog::warn("[LoadingTip] Failed to create IL2CPP string");
    return;
  }

  static auto tmp_h   = il2cpp_get_class_helper("Unity.TextMeshPro", "TMPro", "TMP_Text");
  static auto fn_text = tmp_h.GetMethodInfo("set_text");
  if (!fn_text) {
    spdlog::warn("[LoadingTip] TMP_Text.set_text method not found");
    return;
  }

  void* args[1] = {tipStr};
  Il2CppException* exception = nullptr;
  il2cpp_runtime_invoke(fn_text, tmpObj, args, &exception);
  if (exception) {
    spdlog::warn("[LoadingTip] TMP_Text.set_text invocation failed");
    return;
  }
}

// Track last custom tip index to avoid consecutive repeats
static size_t g_lastCustomTipIdx = SIZE_MAX;

// Pick a transition tip: 50% chance to override with a custom tip,
// 50% chance to return empty (pass-through to the game's server tip).
// Avoids showing the same custom tip twice in a row.
std::string PickTransitionTip()
{
  // Roll 1-100: 1-50 = custom override, 51-100 = pass-through
  std::uniform_int_distribution<int> rollDist(1, kCustomTipChance + kPassThroughChance);
  int roll = rollDist(g_rng);

  if (roll > kCustomTipChance) {
    spdlog::debug("[LoadingTip] Pass-through to game tip (roll={})", roll);
    return "";
  }

  spdlog::debug("[LoadingTip] Rolled custom tip (roll={})", roll);

  if (kNumCustomTips == 0) return "";

  // Build weighted pool excluding the last shown tip (if possible)
  int  totalWeight = 0;
  int  indices[kNumCustomTips];
  int  numCandidates = 0;

  for (size_t i = 0; i < kNumCustomTips; ++i) {
    if (i == g_lastCustomTipIdx && kNumCustomTips > 1) continue;
    totalWeight += kCustomTips[i].weight;
    indices[numCandidates++] = (int)i;
  }

  if (totalWeight <= 0 || numCandidates == 0) {
    // Fallback: use first tip
    g_lastCustomTipIdx = 0;
    return kCustomTips[0].text;
  }

  std::uniform_int_distribution<int> dist(1, totalWeight);
  int tipRoll = dist(g_rng);

  for (int j = 0; j < numCandidates; ++j) {
    int i = indices[j];
    tipRoll -= kCustomTips[i].weight;
    if (tipRoll <= 0) {
      g_lastCustomTipIdx = (size_t)i;
      return kCustomTips[i].text;
    }
  }

  g_lastCustomTipIdx = (size_t)indices[numCandidates - 1];
  return kCustomTips[indices[numCandidates - 1]].text;
}

void LTVC_SetRandomTipLocalisedText_Hook(auto original, void* _this)
{
  spdlog::debug("[LoadingTip] SetRandomTipLocalisedText fired (this={:x})", (uintptr_t)_this);

  // Call original first — it picks a random tip and sets it on _textMeshPro
  original(_this);

  try {
    const auto& cfg = Config::Get();
    if (!cfg.loader_tip_enabled) return;

    g_tipCount++;
    spdlog::info("[LoadingTip] Tip #{} (loadingScreen={})", g_tipCount, g_isLoadingScreen);

    std::string chosenTip;

    if (g_isLoadingScreen) {
      // Loading screen: always show the welcome tip with mod version
      chosenTip = GetLoadingScreenTip();
      // After showing the loading screen tip, subsequent cycles are transitions
      g_isLoadingScreen = false;
    } else {
      // Transition: 50% chance to override with custom tip, 50% pass-through
      chosenTip = PickTransitionTip();
    }

    if (chosenTip.empty()) {
      // Pass-through — let the game's server tip stay
      return;
    }

    // Get _textMeshPro (TextMeshProUGUI at offset 0x20)
    void* tmp = *reinterpret_cast<void**>((char*)_this + kTextMeshProOffset);
    SetTMPText(tmp, chosenTip);

    spdlog::info("[LoadingTip] Override tip #{} with: \"{}\"", g_tipCount, chosenTip);
  } catch (...) {
    spdlog::warn("[LoadingTip] Exception in SetRandomTipLocalisedText hook");
  }
}

// Hook OnEnable to reset the tip counter when the tip view controller is re-enabled
// (happens at the start of each new transition)
void LTVC_OnEnable_Hook(auto original, void* _this)
{
  spdlog::debug("[LoadingTip] OnEnable fired (loadingScreen={})", g_isLoadingScreen);
  g_tipCount = 0;
  original(_this);
  // g_isLoadingScreen is managed in SetRandomTipLocalisedText —
  // it stays true through the loading screen, then flips to false
  // after the first tip is shown, so subsequent cycles use transition tips.
}

} // namespace

// Reset state for reload — called from transition_screen.cc PrepareAllForReload hook
void ResetLoadingTipState()
{
  g_tipCount           = 0;
  g_isLoadingScreen    = true;
  g_lastCustomTipIdx   = SIZE_MAX;
  spdlog::info("[LoadingTip] State reset for reload");
}

void InstallLoadingTipHooks()
{
  const auto& cfg = Config::Get();
  if (!cfg.loader_tip_enabled) {
    spdlog::info("[LoadingTip] Disabled by config (loader_tip_enabled=false)");
    return;
  }

  auto ltv_h = il2cpp_get_class_helper("Assembly-CSharp", "Prime.LoadingScreen", "LoadingTipViewController");
  if (!ltv_h.isValidHelper()) {
    ErrorMsg::MissingHelper("Prime.LoadingScreen", "LoadingTipViewController");
    spdlog::error("[LoadingTip] LoadingTipViewController not found — tip hooks skipped");
    return;
  }

  bool ok = false;

  if (auto m = ltv_h.GetMethod("SetRandomTipLocalisedText")) {
    SPUD_STATIC_DETOUR(m, LTVC_SetRandomTipLocalisedText_Hook);
    spdlog::info("[LoadingTip] SetRandomTipLocalisedText hook installed");
    ok = true;
  } else {
    spdlog::error("[LoadingTip] SetRandomTipLocalisedText not found");
  }

  if (auto m = ltv_h.GetMethod("OnEnable")) {
    SPUD_STATIC_DETOUR(m, LTVC_OnEnable_Hook);
    spdlog::info("[LoadingTip] OnEnable hook installed (tip counter reset)");
  } else {
    spdlog::warn("[LoadingTip] OnEnable not found — tip counter will not reset per transition");
  }

  if (ok) {
    spdlog::info("[LoadingTip] Hooks installed: welcome tip + {} custom transition tips (25% override / 75% pass-through)",
                 kNumCustomTips);
  } else {
    spdlog::error("[LoadingTip] Some hooks failed to install");
  }
}
