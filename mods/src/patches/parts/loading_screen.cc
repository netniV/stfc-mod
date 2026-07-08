// =============================================================================
// LOADING SCREEN PATCH — Custom background + logos on the login/loading screen
// =============================================================================
//
// Hooks LoginSequence.Awake to replace the game's default login background image
// with a custom texture and add two logo overlays (mod logo + CC logo).
//
// Lifecycle:
//   - Fires at launch step 7 (LoginSequence.Awake, sectionID=73596745)
//   - Fires at reload step 15 (new LoginSequence after DoReload completes)
//
// The login screen is static — no animator, no blur, no state machine.
// Logos are placed as siblings on the login canvas.
//
// =============================================================================

#include "loading_screen_common.h"

#include <spud/detour.h>

namespace ls = ls_common;

// Per-instance state (reset on each LoginSequence.Awake)
static void* g_loginLogoGO    = nullptr;
static void* g_loginCCLogoGO  = nullptr;

// Called from PrepareAllForReload (via transition_screen.cc) to destroy overlays
// and null stale pointers before reload destroys the Unity objects.
void ResetLoadingScreenState()
{
  // Don't SafeDestroy — objects may already be destroyed by pre-reload.
  // The reload process will clean up remaining Unity objects. Just null pointers.
  g_loginLogoGO   = nullptr;
  g_loginCCLogoGO = nullptr;
}

// Recursively searches the transform hierarchy for an Image component whose
// GameObject name matches common background names. Falls back to first Image found.
static void* FindLoginBGImage(void* transform, int depth, int maxDepth,
                              void* fn_cc, void* fn_ch, void* fn_gg,
                              void* fn_nm, void* fn_gc, void* imageType, void** outFirst)
{
  if (!transform || depth > maxDepth) return nullptr;
  int32_t n = reinterpret_cast<int32_t (*)(void*)>(fn_cc)(transform);
  for (int i = 0; i < n; ++i) {
    void* child = reinterpret_cast<void* (*)(void*, int32_t)>(fn_ch)(transform, i);
    if (!child) continue;
    void* cgo = reinterpret_cast<void* (*)(void*)>(fn_gg)(child);
    if (!cgo) continue;
    void* img = reinterpret_cast<void* (*)(void*, void*)>(fn_gc)(cgo, imageType);
    if (img) {
      if (!*outFirst) *outFirst = img;
      char buf[64] = {};
      ls::ReadIl2CppString(reinterpret_cast<void* (*)(void*)>(fn_nm)(cgo), buf, sizeof(buf));
      if (strcmp(buf, "Background") == 0 || strcmp(buf, "BG") == 0 || strcmp(buf, "bg_image") == 0
          || strcmp(buf, "LoadingBackground") == 0 || strcmp(buf, "LoginBG") == 0 || strcmp(buf, "SplashBG") == 0
          || strcmp(buf, "Image") == 0)
        return img;
    }
    void* found = FindLoginBGImage(child, depth + 1, maxDepth, fn_cc, fn_ch, fn_gg, fn_nm, fn_gc, imageType, outFirst);
    if (found) return found;
  }
  return nullptr;
}

static void LS_LoginSequence_Awake_Hook(auto original, void* _this)
{
  spdlog::info("[LoadingScreen] LoginSequence.Awake fired (this={:x})", (uintptr_t)_this);
  original(_this);

  try {
    if (!Config::Get().loader_enabled) {
      spdlog::info("[LoadingScreen] loader_enabled=false, skipping");
      return;
    }

    // Reset stale state from previous login instance.
    // After a reload, Unity has already destroyed old GOs — just null the pointers.
    g_loginLogoGO   = nullptr;
    g_loginCCLogoGO = nullptr;

    void* texture = ls::GetLoadingTexture();
    if (!texture) {
      spdlog::warn("[LoadingScreen] GetLoadingTexture returned null");
      return;
    }

    // Find _mainCanvas on LoginSequence
    static auto ls_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Login", "LoginSequence");
    if (!ls_h.isValidHelper()) {
      spdlog::warn("[LoadingScreen] LoginSequence class helper invalid");
      return;
    }
    static auto f_mc = ls_h.GetField("_mainCanvas");
    if (!f_mc.isValidHelper()) {
      spdlog::warn("[LoadingScreen] _mainCanvas field not found");
      return;
    }
    void* canvas = *reinterpret_cast<void**>((char*)_this + f_mc.offset());
    if (!canvas) {
      spdlog::warn("[LoadingScreen] _mainCanvas is null (offset=0x{:x})", f_mc.offset());
      return;
    }

    // Get canvas transform
    static auto co_h    = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
    static auto fn_ct   = co_h.GetMethod("get_transform");
    if (!fn_ct) {
      spdlog::warn("[LoadingScreen] Component.get_transform not found");
      return;
    }
    void* canvasTrans = reinterpret_cast<void* (*)(void*)>(fn_ct)(canvas);
    if (!canvasTrans) {
      spdlog::warn("[LoadingScreen] canvas transform is null");
      return;
    }

    // Find BG image on the login canvas
    static auto tr_h = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
    static auto go_h = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
    static auto im_h = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
    static auto fn_cc = tr_h.GetMethod("get_childCount");
    static auto fn_ch = tr_h.GetMethod("GetChild");
    static auto fn_gg = tr_h.GetMethod("get_gameObject");
    static auto fn_nm = go_h.GetMethod("get_name");
    static auto fn_gc = go_h.GetMethod("GetComponent", 1);
    if (!fn_cc || !fn_ch || !fn_gg || !fn_nm || !fn_gc) {
      spdlog::warn("[LoadingScreen] missing Unity helper functions (cc={} ch={} gg={} nm={} gc={})",
                   (bool)fn_cc, (bool)fn_ch, (bool)fn_gg, (bool)fn_nm, (bool)fn_gc);
      return;
    }

    void* imgType = im_h.GetType();
    if (!imgType) {
      spdlog::warn("[LoadingScreen] Image type not found");
      return;
    }

    spdlog::info("[LoadingScreen] Searching for BG image in canvas hierarchy...");
    void* first  = nullptr;
    void* bgImg  = FindLoginBGImage(canvasTrans, 0, 4, fn_cc, fn_ch, fn_gg, fn_nm, fn_gc, imgType, &first);
    if (!bgImg) bgImg = first;
    if (!bgImg) {
      spdlog::warn("[LoadingScreen] No Image component found in login canvas hierarchy");
      return;
    }
    spdlog::info("[LoadingScreen] Found BG image at {:x}", (uintptr_t)bgImg);

    // Replace the game's background with our custom texture
    ls::ApplySpriteToImage(bgImg, texture);

    // Add logo overlays on the login canvas
    void* bgImgTr = reinterpret_cast<void* (*)(void*)>(fn_ct)(bgImg);
    if (bgImgTr) {
      ls::CreateLogoOverlay(bgImgTr, g_loginLogoGO);
      ls::CreateCCLogoOverlay(bgImgTr, g_loginCCLogoGO);
    }

    spdlog::info("[LoadingScreen] Applied custom background + logos to login screen");
  } catch (...) {
    spdlog::warn("[LoadingScreen] Failed to apply custom background (exception)");
  }
}

void InstallLoadingScreenHooks()
{
  const auto& cfg = Config::Get();
  if (!cfg.loader_enabled) {
    spdlog::info("[LoadingScreen] Disabled by config (loader_enabled=false)");
    return;
  }

  auto ls_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Login", "LoginSequence");
  if (!ls_h.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Login", "LoginSequence");
    spdlog::error("[LoadingScreen] LoginSequence not found — loading screen hooks skipped");
    return;
  }

  if (auto m = ls_h.GetMethod("Awake")) {
    spdlog::info("[LoadingScreen] LoginSequence.Awake method at {:x}", (uintptr_t)m);
    SPUD_STATIC_DETOUR(m, LS_LoginSequence_Awake_Hook);
    spdlog::info("[LoadingScreen] LoginSequence.Awake hook installed");
  } else {
    ErrorMsg::MissingMethod("LoginSequence", "Awake");
    spdlog::error("[LoadingScreen] LoginSequence.Awake not found — hooks skipped");
  }
}
