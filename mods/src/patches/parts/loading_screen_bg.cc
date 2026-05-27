#include "config.h"
#include "errormsg.h"
#include "prime/TransitionManager.h"
#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#if _WIN32
#include <Windows.h>
#endif
#include "embedded_loading_image.h"
#include "embedded_logo_image.h"
#include "embedded_cc_logo_image.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

struct FakeRect {
  float x, y, width, height;
};
struct FakeVector2 {
  float x, y;
};
struct FakeVector3 {
  float x, y, z;
};
struct FakeColor {
  float r, g, b, a;
};

// Per-instance state (reset in TVC.Awake)
static void* g_customLoadingTexture = nullptr;
static bool  g_spriteApplied        = false;
static void* g_bgImageComp          = nullptr;
static void* g_bgRectTransform      = nullptr;
static void* g_logoTexture          = nullptr;
static void* g_logoGO               = nullptr;
static void* g_ccLogoTexture        = nullptr;
static void* g_ccLogoGO             = nullptr;
static void* g_bgOverlayGO          = nullptr;
static void* g_canvasAnimator       = nullptr; // TVC._animator; disabled after show, re-enabled before hide

static Il2CppObject* InvokeRuntime(const MethodInfo* method, void* target, void** args, const char* name)
{
  if (!method)
    return nullptr;
  Il2CppException* exception = nullptr;
  Il2CppObject*    result    = il2cpp_runtime_invoke(method, target, args, &exception);
  if (exception) {
    spdlog::warn("[LS] {} invocation failed", name);
    return nullptr;
  }
  return result;
}

static bool InvokeVoid(const MethodInfo* method, void* target, void** args, const char* name)
{
  if (!method)
    return false;
  Il2CppException* exception = nullptr;
  il2cpp_runtime_invoke(method, target, args, &exception);
  if (exception) {
    spdlog::warn("[LS] {} invocation failed", name);
    return false;
  }
  return true;
}

static bool InvokeBool(const MethodInfo* method, void* target, void** args, const char* name)
{
  Il2CppObject* result = InvokeRuntime(method, target, args, name);
  if (!result)
    return false;
  return *reinterpret_cast<bool*>(il2cpp_object_unbox(result));
}

static int32_t InvokeInt32(const MethodInfo* method, void* target, int32_t fallback, const char* name)
{
  Il2CppObject* result = InvokeRuntime(method, target, nullptr, name);
  if (!result)
    return fallback;
  return *reinterpret_cast<int32_t*>(il2cpp_object_unbox(result));
}

static void* InvokeObject(const MethodInfo* method, void* target, void** args, const char* name)
{ return InvokeRuntime(method, target, args, name); }

// Sets all 5 RectTransform layout properties in one call. Resolves setters once via static cache.
static void SetFullRect(void* rt, FakeVector2 aMin, FakeVector2 aMax, FakeVector2 pivot, FakeVector2 sd, FakeVector2 ap)
{
  if (!rt)
    return;
  static auto rt_h  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  static auto fn_am = rt_h.GetMethodInfo("set_anchorMin");
  static auto fn_ax = rt_h.GetMethodInfo("set_anchorMax");
  static auto fn_pv = rt_h.GetMethodInfo("set_pivot");
  static auto fn_sd = rt_h.GetMethodInfo("set_sizeDelta");
  static auto fn_ap = rt_h.GetMethodInfo("set_anchoredPosition");
  if (!fn_am || !fn_ax || !fn_pv || !fn_sd || !fn_ap)
    return;
  void* amA[1] = {&aMin};
  void* axA[1] = {&aMax};
  void* pvA[1] = {&pivot};
  void* sdA[1] = {&sd};
  void* apA[1] = {&ap};
  InvokeVoid(fn_am, rt, amA, "RT.set_anchorMin");
  InvokeVoid(fn_ax, rt, axA, "RT.set_anchorMax");
  InvokeVoid(fn_pv, rt, pvA, "RT.set_pivot");
  InvokeVoid(fn_sd, rt, sdA, "RT.set_sizeDelta");
  InvokeVoid(fn_ap, rt, apA, "RT.set_anchoredPosition");
}

// Reads Texture2D pixel dimensions, returning fallbacks if the accessors fail.
static void GetTextureSize(void* tex, int32_t& w, int32_t& h, int32_t fallbackW, int32_t fallbackH)
{
  static auto tex_h = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Texture2D");
  static auto fn_w  = tex_h.GetMethodInfo("get_width");
  static auto fn_h  = tex_h.GetMethodInfo("get_height");
  w = InvokeInt32(fn_w, tex, fallbackW, "Texture2D.get_width");
  h = InvokeInt32(fn_h, tex, fallbackH, "Texture2D.get_height");
}

// Creates a Sprite covering the entire texture, with a centered pivot. Returns nullptr on failure.
static void* CreateSpriteFromTexture(void* tex, int32_t fallbackW, int32_t fallbackH)
{
  if (!tex)
    return nullptr;
  static auto spr_h  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Sprite");
  static auto fn_cre = spr_h.GetMethodInfo("Create", 3);
  if (!fn_cre)
    return nullptr;
  int32_t w, h;
  GetTextureSize(tex, w, h, fallbackW, fallbackH);
  FakeRect    rect{0.0f, 0.0f, (float)w, (float)h};
  FakeVector2 pivot{0.5f, 0.5f};
  void*       args[3] = {tex, &rect, &pivot};
  return InvokeObject(fn_cre, nullptr, args, "Sprite.Create");
}

// Configures a UnityEngine.UI.Image: assigns the sprite, sets white tint, Simple draw type, and preserveAspect.
static void ConfigureImageSprite(void* imgComp, void* spr, bool preserveAspect)
{
  if (!imgComp)
    return;
  static auto img_h  = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  static auto fn_spr = img_h.GetMethodInfo("set_sprite");
  static auto fn_col = img_h.GetMethodInfo("set_color");
  static auto fn_typ = img_h.GetMethodInfo("set_type");
  static auto fn_asp = img_h.GetMethodInfo("set_preserveAspect");
  void*       sprArgs[1] = {spr};
  InvokeVoid(fn_spr, imgComp, sprArgs, "Image.set_sprite");
  FakeColor white{1.0f, 1.0f, 1.0f, 1.0f};
  void*     colArgs[1] = {&white};
  InvokeVoid(fn_col, imgComp, colArgs, "Image.set_color");
  int32_t simple     = 0; // Image.Type.Simple
  void*   typArgs[1] = {&simple};
  InvokeVoid(fn_typ, imgComp, typArgs, "Image.set_type");
  void* aspArgs[1] = {&preserveAspect};
  InvokeVoid(fn_asp, imgComp, aspArgs, "Image.set_preserveAspect");
}

// Sets a UI Image color to fully transparent (hides without disabling the component).
static void HideImage(void* imgComp)
{
  if (!imgComp)
    return;
  static auto img_h  = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  static auto fn_col = img_h.GetMethodInfo("set_color");
  FakeColor   clear{0.0f, 0.0f, 0.0f, 0.0f};
  void*       args[1] = {&clear};
  InvokeVoid(fn_col, imgComp, args, "Image.set_color (transparent)");
}

static void ReadIl2CppString(void* strObj, char* buf, int bufSize)
{
  buf[0] = '\0';
  if (!strObj || bufSize <= 1)
    return;
  const auto* chars = reinterpret_cast<const char16_t*>((ptrdiff_t)strObj + 0x14);
  int         i     = 0;
  for (; i < bufSize - 1 && chars[i]; ++i)
    buf[i] = static_cast<char>(chars[i]);
  buf[i] = '\0';
}

static void* LoadTextureFromBytes(const uint8_t* data, size_t size)
{
  if (!data || size == 0)
    return nullptr;
  static auto tex_h   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Texture2D");
  static auto conv_h  = il2cpp_get_class_helper("UnityEngine.ImageConversionModule", "UnityEngine", "ImageConversion");
  static auto fn_load = conv_h.GetMethodInfoSpecial("LoadImage", [](int param_count, const Il2CppType** param) {
    return param_count == 3 && param[1]->type == IL2CPP_TYPE_SZARRAY && param[2]->type == IL2CPP_TYPE_BOOLEAN;
  });
  if (!tex_h.isValidHelper() || !fn_load)
    return nullptr;
  static auto  byte_h = il2cpp_get_class_helper("mscorlib", "System", "Byte");
  Il2CppArray* arr    = il2cpp_array_new(byte_h.get_cls(), size);
  if (!arr)
    return nullptr;
  std::memcpy(((Il2CppArraySize*)arr)->vector, data, size);
  void* tex = il2cpp_object_new(tex_h.get_cls());
  if (!tex)
    return nullptr;
  static auto ctor = tex_h.GetMethodInfoSpecial(".ctor", [](int param_count, const Il2CppType** param) {
    return param_count == 4 && param[3]->type == IL2CPP_TYPE_BOOLEAN;
  });
  if (ctor) {
    int32_t width = 2, height = 2, textureFormat = 4;
    bool    mipChain    = false;
    void*   ctorArgs[4] = {&width, &height, &textureFormat, &mipChain};
    if (!InvokeVoid(ctor, tex, ctorArgs, "Texture2D.ctor"))
      return nullptr;
  }
  bool  markNonReadable = false;
  void* loadArgs[3]     = {tex, arr, &markNonReadable};
  return InvokeBool(fn_load, nullptr, loadArgs, "ImageConversion.LoadImage") ? tex : nullptr;
}

static void EnsureTextureLoaded()
{
  if (g_customLoadingTexture)
    return;
  const std::string& path = Config::Get().loader_image;
  if (!path.empty() && std::filesystem::exists(path)) {
    std::ifstream f(path, std::ios::binary);
    if (f.is_open()) {
      std::vector<uint8_t> data(std::istreambuf_iterator<char>(f), {});
      if (!data.empty())
        g_customLoadingTexture = LoadTextureFromBytes(data.data(), data.size());
    }
  }
  if (!g_customLoadingTexture)
    g_customLoadingTexture = LoadTextureFromBytes(g_embeddedLoadingImage, g_embeddedLoadingImage_SIZE);
  if (!g_customLoadingTexture)
    spdlog::warn("[LS] failed to load loading screen image texture");
}

static void EnsureLogoLoaded()
{
  if (!g_logoTexture) {
    g_logoTexture = LoadTextureFromBytes(g_embeddedLogoImage, g_embeddedLogoImage_SIZE);
    if (!g_logoTexture)
      spdlog::warn("[LS] failed to load logo texture");
  }
  if (!g_ccLogoTexture) {
    g_ccLogoTexture = LoadTextureFromBytes(g_embeddedCCLogoImage, g_embeddedCCLogoImage_SIZE);
    if (!g_ccLogoTexture)
      spdlog::warn("[LS] failed to load CC logo texture");
  }
}

// Walks up the Transform hierarchy and returns the root Canvas transform,
// or parentTransform itself if no Canvas is found.
static void* FindRootCanvas(void* parentTransform)
{
  static auto go_h     = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto tr_h     = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  static auto canvas_h = il2cpp_get_class_helper("UnityEngine.UIModule", "UnityEngine", "Canvas");

  static auto fn_get_parent = tr_h.GetMethod("get_parent");
  static auto fn_get_go     = tr_h.GetMethod("get_gameObject");
  static auto fn_gc_go      = go_h.GetMethod("GetComponent", 1);
  void*       canvasType    = canvas_h.GetType();

  void* cur = parentTransform;
  while (cur) {
    void* curGO = fn_get_go ? reinterpret_cast<void* (*)(void*)>(fn_get_go)(cur) : nullptr;
    if (curGO && fn_gc_go && canvasType) {
      if (reinterpret_cast<void* (*)(void*, void*)>(fn_gc_go)(curGO, canvasType))
        return cur;
    }
    cur = fn_get_parent ? reinterpret_cast<void* (*)(void*)>(fn_get_parent)(cur) : nullptr;
  }
  return parentTransform;
}

// Creates a new GameObject parented to the root Canvas of parentTransform, adds a UI Image
// with a sprite built from `texture`, and configures its RectTransform via SetFullRect.
// If `placeAsFirstSibling` is true, the new object is pushed behind all existing canvas children.
// Returns the new GameObject, or nullptr on failure.
static void* CreateImageOverlay(const char* name, void* texture, void* parentTransform, FakeVector2 aMin,
                                FakeVector2 aMax, FakeVector2 pivot, FakeVector2 sd, FakeVector2 ap,
                                bool preserveAspect, bool placeAsFirstSibling, int32_t fallbackW, int32_t fallbackH)
{
  if (!texture || !parentTransform)
    return nullptr;

  static auto go_h  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto rt_h  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
  static auto tr_h  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
  static auto img_h = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");

  static auto fn_go_ctor = go_h.GetMethodInfoSpecial(".ctor", [](int n, const Il2CppType** p) {
    return n == 1 && p[0]->type == IL2CPP_TYPE_STRING;
  });
  static auto fn_get_tr  = go_h.GetMethodInfo("get_transform");
  static auto fn_setpar  = tr_h.GetMethodInfoSpecial("SetParent", [](int n, const Il2CppType**) { return n == 2; });
  static auto fn_setfst  = tr_h.GetMethodInfo("SetAsFirstSibling");
  static auto fn_addcomp = go_h.GetMethodInfo("AddComponent", 1);
  static auto fn_gc      = go_h.GetMethodInfo("GetComponent", 1);
  if (!fn_go_ctor || !fn_get_tr || !fn_setpar || !fn_addcomp || !fn_gc)
    return nullptr;

  void* spr = CreateSpriteFromTexture(texture, fallbackW, fallbackH);
  if (!spr)
    return nullptr;

  void* go = il2cpp_object_new(go_h.get_cls());
  if (!go)
    return nullptr;
  void* nameStr   = il2cpp_string_new(name);
  void* goArgs[1] = {nameStr};
  if (!InvokeVoid(fn_go_ctor, go, goArgs, "GameObject.ctor"))
    return nullptr;

  void* tr = InvokeObject(fn_get_tr, go, nullptr, "get_transform");
  if (!tr)
    return nullptr;
  bool  worldStays = false;
  void* parArgs[2] = {FindRootCanvas(parentTransform), &worldStays};
  InvokeVoid(fn_setpar, tr, parArgs, "Transform.SetParent");
  if (placeAsFirstSibling && fn_setfst)
    InvokeVoid(fn_setfst, tr, nullptr, "Transform.SetAsFirstSibling");

  void* imgType   = img_h.GetType();
  void* acArgs[1] = {imgType};
  void* imgComp   = InvokeObject(fn_addcomp, go, acArgs, "AddComponent<Image>");

  void* rtType    = rt_h.GetType();
  void* rtArgs[1] = {rtType};
  void* rt        = InvokeObject(fn_gc, go, rtArgs, "GetComponent<RectTransform>");
  SetFullRect(rt, aMin, aMax, pivot, sd, ap);

  ConfigureImageSprite(imgComp, spr, preserveAspect);
  return go;
}

// Creates a stretch-fill BG image displaying the custom loading texture, pushed behind all UI siblings.
static void CreateTransitionBGOverlay(void* parentTransform)
{
  if (g_bgOverlayGO)
    return;
  try {
    g_bgOverlayGO = CreateImageOverlay("STFCModTransitionBG", g_customLoadingTexture, parentTransform,
                                       /*aMin*/ {0.0f, 0.0f}, /*aMax*/ {1.0f, 1.0f}, /*pivot*/ {0.5f, 0.5f},
                                       /*sd*/ {0.0f, 0.0f}, /*ap*/ {0.0f, 0.0f}, /*preserveAspect*/ false,
                                       /*placeAsFirstSibling*/ true, /*fallbackW*/ 792, /*fallbackH*/ 450);
    if (!g_bgOverlayGO && g_customLoadingTexture && parentTransform)
      spdlog::warn("[LS] Failed to create transition BG overlay");
  } catch (...) {
    spdlog::warn("[LS] Failed to create transition BG overlay");
  }
}

// Creates a logo overlay. xSide=-1 places it on the left (padding from left edge), +1 on the right.
// Width, padding and vertical position match between both logos.
static void CreateLogoOverlayEx(const char* name, void* texture, void* parentTransform, void*& outGO, float xSide)
{
  if (outGO)
    return;
  try {
    if (!texture || !parentTransform)
      return;

    constexpr float kLogoPixels = 200.0f;
    constexpr float kPadXPixels = 40.0f;
    constexpr float kPadYPixels = 120.0f;

    static auto screen_h = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Screen");
    static auto fn_sw    = screen_h.GetMethodInfo("get_width");
    static auto fn_sh    = screen_h.GetMethodInfo("get_height");
    float       sw       = (float)(fn_sw ? InvokeInt32(fn_sw, nullptr, 0, "Screen.get_width") : 0);
    float       sh       = (float)(fn_sh ? InvokeInt32(fn_sh, nullptr, 0, "Screen.get_height") : 0);
    if (sw <= 0.0f)
      sw = 1334.0f;
    if (sh <= 0.0f)
      sh = 750.0f;

    int32_t lw, lh;
    GetTextureSize(texture, lw, lh, 256, 256);
    float logoPixH = kLogoPixels * ((lw > 0) ? (float)lh / (float)lw : 1.0f);

    FakeVector2 aMin, aMax;
    if (xSide < 0.0f) {
      // left side: anchor from left edge
      aMin = {kPadXPixels / sw, kPadYPixels / sh};
      aMax = {(kPadXPixels + kLogoPixels) / sw, (kPadYPixels + logoPixH) / sh};
    } else {
      // right side: anchor from right edge
      aMin = {(sw - kPadXPixels - kLogoPixels) / sw, kPadYPixels / sh};
      aMax = {(sw - kPadXPixels) / sw, (kPadYPixels + logoPixH) / sh};
    }

    outGO = CreateImageOverlay(name, texture, parentTransform, aMin, aMax,
                               /*pivot*/ {0.5f, 0.5f}, /*sd*/ {0.0f, 0.0f}, /*ap*/ {0.0f, 0.0f},
                               /*preserveAspect*/ true, /*placeAsFirstSibling*/ false,
                               /*fallbackW*/ 256, /*fallbackH*/ 256);
    if (!outGO)
      spdlog::warn("[LS] Failed to create logo overlay: {}", name);
  } catch (...) {
    spdlog::warn("[LS] Failed to create logo overlay: {}", name);
  }
}

// Creates the mod logo image in the bottom-right corner. Uses anchor-fraction sizing so the
// logo lands at the desired physical pixel size regardless of CanvasScaler reference resolution.
static void CreateLogoOverlay(void* parentTransform)
{
  CreateLogoOverlayEx("STFCModLogo", g_logoTexture, parentTransform, g_logoGO, /*right*/ 1.0f);
}

static void CreateCCLogoOverlay(void* parentTransform)
{
  CreateLogoOverlayEx("STFCCCLogo", g_ccLogoTexture, parentTransform, g_ccLogoGO, /*left*/ -1.0f);
}

// Applies g_customLoadingTexture as a sprite to an existing Image on the login screen.
// Sets both m_Sprite and m_OverrideSprite, white tint, Simple type, no aspect lock.
static void ApplySpriteToImage(void* imageComp)
{
  if (!imageComp)
    return;
  void* spr = CreateSpriteFromTexture(g_customLoadingTexture, 792, 450);
  if (!spr)
    return;
  static auto img_h  = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  static auto fn_ovr = img_h.GetMethodInfo("set_overrideSprite");
  static auto fn_drt = img_h.GetMethodInfo("SetVerticesDirty");
  void*       sprArgs[1] = {spr};
  InvokeVoid(fn_ovr, imageComp, sprArgs, "Image.set_overrideSprite");
  ConfigureImageSprite(imageComp, spr, /*preserveAspect*/ false);
  InvokeVoid(fn_drt, imageComp, nullptr, "Graphic.SetVerticesDirty");
}

// Finds the BG Image component on a TransitionViewController instance, applies the
// custom sprite (or makes it transparent), and resets the BG RectTransform to stretch-fill its parent.
// The game's BG image is hidden (alpha=0); custom texture and logo overlays are placed behind the UI.
static void ApplyCustomSpriteToBGImage(void* _this)
{
  try {
    static auto tv_h =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.LoadingScreen", "TransitionViewController");
    static auto mb_h  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "MonoBehaviour");
    static auto go_h  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
    static auto tr_h  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
    static auto img_h = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");

    void* imageComp = g_bgImageComp;
    if (!imageComp && tv_h.isValidHelper()) {
      static auto f_ov = tv_h.GetField("_staticOverride");
      if (f_ov.isValidHelper())
        imageComp = *reinterpret_cast<void**>((char*)_this + f_ov.offset());
    }
    if (!imageComp) {
      static auto fn_go = mb_h.GetMethod("get_gameObject");
      static auto fn_tr = go_h.GetMethod("get_transform");
      static auto fn_cc = tr_h.GetMethod("get_childCount");
      static auto fn_ch = tr_h.GetMethod("GetChild");
      static auto fn_gg = tr_h.GetMethod("get_gameObject");
      static auto fn_nm = go_h.GetMethod("get_name");
      static auto fn_gc = go_h.GetMethod("GetComponent", 1);
      if (!fn_go || !fn_tr || !fn_cc || !fn_ch || !fn_gg || !fn_nm || !fn_gc)
        return;

      void* go = reinterpret_cast<void* (*)(void*)>(fn_go)(_this);
      if (!go)
        return;
      void* root = reinterpret_cast<void* (*)(void*)>(fn_tr)(go);
      if (!root)
        return;

      void*   bgc = nullptr;
      int32_t n   = reinterpret_cast<int32_t (*)(void*)>(fn_cc)(root);
      for (int i = 0; i < n; ++i) {
        void* ct = reinterpret_cast<void* (*)(void*, int32_t)>(fn_ch)(root, i);
        if (!ct)
          continue;
        void* cg = reinterpret_cast<void* (*)(void*)>(fn_gg)(ct);
        if (!cg)
          continue;
        char buf[64] = {};
        ReadIl2CppString(reinterpret_cast<void* (*)(void*)>(fn_nm)(cg), buf, sizeof(buf));
        if (strcmp(buf, "BGContainer") == 0) {
          bgc = ct;
          break;
        }
      }
      if (!bgc)
        return;
      if (reinterpret_cast<int32_t (*)(void*)>(fn_cc)(bgc) == 0)
        return;

      void* bgt = reinterpret_cast<void* (*)(void*, int32_t)>(fn_ch)(bgc, 0);
      if (!bgt)
        return;
      g_bgRectTransform = bgt;
      void* bgg         = reinterpret_cast<void* (*)(void*)>(fn_gg)(bgt);
      if (!bgg)
        return;
      void* it = img_h.GetType();
      if (!it)
        return;
      imageComp = reinterpret_cast<void* (*)(void*, void*)>(fn_gc)(bgg, it);
      if (!imageComp)
        return;
    }
    g_bgImageComp = imageComp;

    // Hide the game's BG so our custom texture overlay sits behind the UI text cleanly.
    HideImage(imageComp);

    // Reset BG RectTransform to stretch-fill (game oversizes it for parallax bleed).
    if (g_bgRectTransform) {
      SetFullRect(g_bgRectTransform, {0.0f, 0.0f}, {1.0f, 1.0f}, {0.5f, 0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f});
      static auto fn_eu = tr_h.GetMethodInfo("set_localEulerAngles");
      if (fn_eu) {
        FakeVector3 z{0, 0, 0};
        void*       args[1] = {&z};
        InvokeVoid(fn_eu, g_bgRectTransform, args, "Transform.set_localEulerAngles");
      }
      static auto fn_sc = tr_h.GetMethodInfo("set_localScale");
      if (fn_sc) {
        FakeVector3 o{1, 1, 1};
        void*       args[1] = {&o};
        InvokeVoid(fn_sc, g_bgRectTransform, args, "Transform.set_localScale");
      }
    }

    void* logoParent = g_bgRectTransform;
    if (!logoParent && g_bgImageComp) {
      static auto comp_h    = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
      static auto fn_get_tr = comp_h.GetMethod("get_transform");
      if (fn_get_tr)
        logoParent = reinterpret_cast<void* (*)(void*)>(fn_get_tr)(g_bgImageComp);
    }
    if (logoParent) {
      CreateTransitionBGOverlay(logoParent);
      CreateLogoOverlay(logoParent);
      CreateCCLogoOverlay(logoParent);
    }

    // Reposition native TVC children: LogoContainer → top-right, LoadingTipsContainer → lower-center.
    {
      static auto mb_hR  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "MonoBehaviour");
      static auto go_hR  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
      static auto tr_hR  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
      static auto rt_hR  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
      static auto fn_goR = mb_hR.GetMethod("get_gameObject");
      static auto fn_trR = go_hR.GetMethod("get_transform");
      static auto fn_ccR = tr_hR.GetMethod("get_childCount");
      static auto fn_chR = tr_hR.GetMethod("GetChild");
      static auto fn_ggR = tr_hR.GetMethod("get_gameObject");
      static auto fn_nmR = go_hR.GetMethod("get_name");
      static auto fn_gcR = go_hR.GetMethod("GetComponent", 1);

      if (fn_goR && fn_trR && fn_ccR && fn_chR && fn_ggR && fn_nmR && fn_gcR && rt_hR.isValidHelper()) {
        void*   tvcGO  = reinterpret_cast<void* (*)(void*)>(fn_goR)(_this);
        void*   tvcTr  = tvcGO ? reinterpret_cast<void* (*)(void*)>(fn_trR)(tvcGO) : nullptr;
        int32_t n      = tvcTr ? reinterpret_cast<int32_t (*)(void*)>(fn_ccR)(tvcTr) : 0;
        void*   rtType = rt_hR.GetType();

        for (int32_t i = 0; i < n; ++i) {
          void* ct = reinterpret_cast<void* (*)(void*, int32_t)>(fn_chR)(tvcTr, i);
          if (!ct)
            continue;
          void* cg = reinterpret_cast<void* (*)(void*)>(fn_ggR)(ct);
          if (!cg || !rtType)
            continue;
          char buf[64] = {};
          ReadIl2CppString(reinterpret_cast<void* (*)(void*)>(fn_nmR)(cg), buf, sizeof(buf));

          void* rt = nullptr;
          if (strcmp(buf, "LogoContainer") == 0) {
            rt = reinterpret_cast<void* (*)(void*, void*)>(fn_gcR)(cg, rtType);
            SetFullRect(rt, {1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f}, {586.0f, 248.0f}, {-20.0f, -20.0f});
          } else if (strcmp(buf, "LoadingTipsContainer") == 0) {
            rt = reinterpret_cast<void* (*)(void*, void*)>(fn_gcR)(cg, rtType);
            SetFullRect(rt, {0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {1024.0f, 100.0f}, {0.0f, -320.0f});
          }
        }
      }
    }

    // Disable the root canvas animator (TVC._animator) so it stops overriding child RT values
    // at their "ShowComplete" keyframes. Re-enabled in AboutToHide so the hide animation plays.
    if (!g_canvasAnimator) {
      static auto fn_animFieldCA = tv_h.GetField("_animator");
      static auto fn_behavCA     = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Behaviour");
      static auto fn_setEnCA     = fn_behavCA.GetMethodInfo("set_enabled");
      if (fn_animFieldCA.isValidHelper() && fn_setEnCA) {
        void* anim = *reinterpret_cast<void**>((char*)_this + fn_animFieldCA.offset());
        if (anim) {
          bool  off     = false;
          void* args[1] = {&off};
          InvokeVoid(fn_setEnCA, anim, args, "canvasAnimator.set_enabled(false)");
          g_canvasAnimator = anim;
        }
      }
    }

    g_spriteApplied = true;
  } catch (...) {
  }
}

void TransitionManager_SetLoadingScreen_Hook(auto original, void* _this, void* status, int32_t type,
                                             int32_t messagingType)
{
  try {
    const auto& cfg = Config::Get();
    if (cfg.loader_enabled)
      EnsureTextureLoaded();
    original(_this, status, type, messagingType);
  } catch (...) {
    original(_this, status, type, messagingType);
  }
}

void TransitionViewController_Awake_Hook(auto original, void* _this)
{
  original(_this);
  try {
    if (!Config::Get().loader_transition)
      return;
    g_spriteApplied        = false;
    g_bgImageComp          = nullptr;
    g_bgRectTransform      = nullptr;
    g_customLoadingTexture = nullptr; // reset stale Unity object on re-login
    g_logoTexture          = nullptr;
    g_logoGO               = nullptr;
    g_ccLogoTexture        = nullptr;
    g_ccLogoGO             = nullptr;
    g_bgOverlayGO          = nullptr;
    g_canvasAnimator       = nullptr;
    EnsureTextureLoaded();
    EnsureLogoLoaded();
  } catch (...) {
  }
}

void TransitionViewController_AboutToHide_Hook(auto original, void* _this)
{
  // Re-enable canvas animator BEFORE the original runs so it can play the hide animation.
  try {
    if (g_canvasAnimator) {
      static auto fn_behav = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Behaviour");
      static auto fn_setEn = fn_behav.GetMethodInfo("set_enabled");
      if (fn_setEn) {
        bool  on      = true;
        void* args[1] = {&on};
        InvokeVoid(fn_setEn, g_canvasAnimator, args, "canvasAnimator.set_enabled(true)");
      }
      g_canvasAnimator = nullptr;
    }
  } catch (...) {
  }
  original(_this);
}

void TransitionViewController_AboutToShow_Hook(auto original, void* _this)
{
  original(_this);
  try {
    if (!Config::Get().loader_transition || g_spriteApplied)
      return;
    ApplyCustomSpriteToBGImage(_this);
  } catch (...) {
  }
}

void TransitionViewController_OnAssetBundleDidBeginDownload_Hook(auto original, void* _this, uint16_t ID,
                                                                 bool downloading)
{
  original(_this, ID, downloading);
  try {
    if (!Config::Get().loader_transition || g_spriteApplied)
      return;
    ApplyCustomSpriteToBGImage(_this);
  } catch (...) {
  }
}

void TransitionViewController_DidAssetBundleDownloadComplete_Hook(auto original, void* _this, uint16_t ID, void* error)
{
  original(_this, ID, error);
  try {
    if (!Config::Get().loader_transition)
      return;
    g_spriteApplied = false;
    ApplyCustomSpriteToBGImage(_this);
  } catch (...) {
  }
}

void SlideShowViewController_ShowCurrentSlide_Hook(auto original, void* _this)
{
  original(_this);
  try {
    if (!Config::Get().loader_transition)
      return;
    static auto h  = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.SlideShow", "SlideShowViewController");
    static auto fi = h.GetField("_image");
    if (!h.isValidHelper() || !fi.isValidHelper())
      return;
    void* img = *reinterpret_cast<void**>((char*)_this + fi.offset());
    HideImage(img);
  } catch (...) {
  }
}

static void* FindLoginBGImage(void* transform, int depth, int maxDepth, void* fn_cc, void* fn_ch, void* fn_gg,
                              void* fn_nm, void* fn_gc, void* imageType, void** outFirst)
{
  if (!transform || depth > maxDepth)
    return nullptr;
  int32_t n = reinterpret_cast<int32_t (*)(void*)>(fn_cc)(transform);
  for (int i = 0; i < n; ++i) {
    void* child = reinterpret_cast<void* (*)(void*, int32_t)>(fn_ch)(transform, i);
    if (!child)
      continue;
    void* cgo = reinterpret_cast<void* (*)(void*)>(fn_gg)(child);
    if (!cgo)
      continue;
    void* img = reinterpret_cast<void* (*)(void*, void*)>(fn_gc)(cgo, imageType);
    if (img) {
      if (!*outFirst)
        *outFirst = img;
      char buf[64] = {};
      ReadIl2CppString(reinterpret_cast<void* (*)(void*)>(fn_nm)(cgo), buf, sizeof(buf));
      if (strcmp(buf, "Background") == 0 || strcmp(buf, "BG") == 0 || strcmp(buf, "bg_image") == 0
          || strcmp(buf, "LoadingBackground") == 0 || strcmp(buf, "LoginBG") == 0 || strcmp(buf, "SplashBG") == 0
          || strcmp(buf, "Image") == 0)
        return img;
    }
    void* found = FindLoginBGImage(child, depth + 1, maxDepth, fn_cc, fn_ch, fn_gg, fn_nm, fn_gc, imageType, outFirst);
    if (found)
      return found;
  }
  return nullptr;
}

void LoginSequence_Awake_Hook(auto original, void* _this)
{
  original(_this);
  try {
    if (!Config::Get().loader_enabled)
      return;
    g_customLoadingTexture = nullptr; // reset stale Unity object on re-login
    g_logoTexture          = nullptr;
    g_logoGO               = nullptr;
    g_ccLogoTexture        = nullptr;
    g_ccLogoGO             = nullptr;
    EnsureTextureLoaded();
    EnsureLogoLoaded();
    if (!g_customLoadingTexture)
      return;
    static auto ls_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Login", "LoginSequence");
    static auto tr_h = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
    static auto go_h = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
    static auto co_h = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
    static auto im_h = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
    if (!ls_h.isValidHelper())
      return;
    static auto f_mc = ls_h.GetField("_mainCanvas");
    if (!f_mc.isValidHelper())
      return;
    void* canvas = *reinterpret_cast<void**>((char*)_this + f_mc.offset());
    if (!canvas)
      return;
    static auto fn_ct = co_h.GetMethod("get_transform");
    if (!fn_ct)
      return;
    void* canvasTrans = reinterpret_cast<void* (*)(void*)>(fn_ct)(canvas);
    if (!canvasTrans)
      return;
    static auto fn_cc = tr_h.GetMethod("get_childCount");
    static auto fn_ch = tr_h.GetMethod("GetChild");
    static auto fn_gg = tr_h.GetMethod("get_gameObject");
    static auto fn_nm = go_h.GetMethod("get_name");
    static auto fn_gc = go_h.GetMethod("GetComponent", 1);
    if (!fn_cc || !fn_ch || !fn_gg || !fn_nm || !fn_gc)
      return;
    void* imgType = im_h.GetType();
    if (!imgType)
      return;
    void* first = nullptr;
    void* bgImg = FindLoginBGImage(canvasTrans, 0, 4, fn_cc, fn_ch, fn_gg, fn_nm, fn_gc, imgType, &first);
    if (!bgImg)
      bgImg = first;
    if (bgImg) {
      ApplySpriteToImage(bgImg);
      void* bgImgTr = reinterpret_cast<void* (*)(void*)>(fn_ct)(bgImg);
      if (bgImgTr) {
        CreateLogoOverlay(bgImgTr);
        CreateCCLogoOverlay(bgImgTr);
      }
    }
  } catch (...) {
  }
}

// Installs a single spud detour. Logs success or a MissingMethod error. Used only in
// InstallLoadingScreenBgHooks below; #undef'd at end of function to keep the macro local.
#define LS_INSTALL_HOOK(HELPER, KLASS, METHOD, HOOK, LABEL)                                                            \
  do {                                                                                                                 \
    if (auto m = (HELPER).GetMethod(METHOD)) {                                                                         \
      SPUD_STATIC_DETOUR(m, HOOK);                                                                                     \
      spdlog::info("Loading screen hook installed (" LABEL ")");                                                       \
    } else {                                                                                                           \
      ErrorMsg::MissingMethod(KLASS, METHOD);                                                                          \
    }                                                                                                                  \
  } while (0)

void InstallLoadingScreenBgHooks()
{
  const auto& cfg = Config::Get();
  auto tv_h       = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.LoadingScreen", "TransitionViewController");
  auto tm_h       = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.LoadingScreen", "TransitionManager");

  if (!tm_h.isValidHelper()) {
    ErrorMsg::MissingHelper("LoadingScreen", "TransitionManager");
  } else {
    LS_INSTALL_HOOK(tm_h, "TransitionManager", "SetLoadingScreen", TransitionManager_SetLoadingScreen_Hook,
                    "TransitionManager.SetLoadingScreen");
  }

  if (cfg.loader_transition) {
    if (!tv_h.isValidHelper()) {
      ErrorMsg::MissingHelper("LoadingScreen", "TransitionViewController");
      spdlog::error("[LS] TransitionViewController not found — transition background hooks skipped");
    } else {
      LS_INSTALL_HOOK(tv_h, "TransitionViewController", "Awake", TransitionViewController_Awake_Hook, "TVC.Awake");
      LS_INSTALL_HOOK(tv_h, "TransitionViewController", "AboutToShow", TransitionViewController_AboutToShow_Hook,
                      "TVC.AboutToShow");
      LS_INSTALL_HOOK(tv_h, "TransitionViewController", "AboutToHide", TransitionViewController_AboutToHide_Hook,
                      "TVC.AboutToHide");
      LS_INSTALL_HOOK(tv_h, "TransitionViewController", "OnAssetBundleDidBeginDownloadEventCallback",
                      TransitionViewController_OnAssetBundleDidBeginDownload_Hook, "TVC.OnAssetBundleDidBeginDownload");
      LS_INSTALL_HOOK(tv_h, "TransitionViewController", "DidAssetBundleDownloadCompleteEvent",
                      TransitionViewController_DidAssetBundleDownloadComplete_Hook,
                      "TVC.DidAssetBundleDownloadComplete");

      auto ss_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.SlideShow", "SlideShowViewController");
      if (!ss_h.isValidHelper()) {
        ErrorMsg::MissingHelper("SlideShow", "SlideShowViewer");
      } else {
        LS_INSTALL_HOOK(ss_h, "SlideShowViewer", "ShowCurrentSlide", SlideShowViewController_ShowCurrentSlide_Hook,
                        "SlideShow.ShowCurrentSlide");
      }
    }
  } else {
    spdlog::info("[LS] Transition screen background disabled");
  }

  if (cfg.loader_enabled) {
    auto ls_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Login", "LoginSequence");
    if (!ls_h.isValidHelper()) {
      ErrorMsg::MissingHelper("Login", "LoginSequence");
      spdlog::warn("[LS] LoginSequence not found — login background disabled");
    } else {
      LS_INSTALL_HOOK(ls_h, "LoginSequence", "Awake", LoginSequence_Awake_Hook, "LoginSequence.Awake");
    }
  } else {
    spdlog::info("[LS] Login screen background disabled");
  }
}

#undef LS_INSTALL_HOOK
