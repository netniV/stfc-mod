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
static void* g_ourSprite            = nullptr;
static void* g_bgRectTransform      = nullptr;

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

static void* LoadTextureFromBytes(const std::vector<uint8_t>& data)
{
  if (data.empty())
    return nullptr;
  static auto tex_h   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Texture2D");
  static auto conv_h  = il2cpp_get_class_helper("UnityEngine.ImageConversionModule", "UnityEngine", "ImageConversion");
  static auto fn_load = conv_h.GetMethodInfoSpecial("LoadImage", [](int param_count, const Il2CppType** param) {
    return param_count == 3 && param[1]->type == IL2CPP_TYPE_SZARRAY && param[2]->type == IL2CPP_TYPE_BOOLEAN;
  });
  if (!tex_h.isValidHelper() || !fn_load)
    return nullptr;
  static auto  byte_h = il2cpp_get_class_helper("mscorlib", "System", "Byte");
  Il2CppArray* arr    = il2cpp_array_new(byte_h.get_cls(), data.size());
  if (!arr)
    return nullptr;
  std::memcpy(((Il2CppArraySize*)arr)->vector, data.data(), data.size());
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
  const std::string&   path = Config::Get().loader_image;
  std::vector<uint8_t> data;
  if (!path.empty() && std::filesystem::exists(path)) {
    std::ifstream f(path, std::ios::binary);
    if (f.is_open())
      data.assign(std::istreambuf_iterator<char>(f), {});
  }
  if (data.empty())
    data.assign(g_embeddedLoadingImage, g_embeddedLoadingImage + g_embeddedLoadingImage_SIZE);
  g_customLoadingTexture = LoadTextureFromBytes(data);
  if (!g_customLoadingTexture)
    spdlog::warn("[LS] failed to load loading screen image texture");
}

// Applies g_customLoadingTexture as a sprite to imageComp.
// Sets both m_Sprite and m_OverrideSprite, forces white tint, Simple type, no aspect lock.
// If outSprite is non-null, the created sprite pointer is written to it.
static void ApplySpriteToImage(void* imageComp, void** outSprite = nullptr)
{
  static auto tex_h  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Texture2D");
  static auto spr_h  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Sprite");
  static auto img_h  = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
  static auto fn_w   = tex_h.GetMethodInfo("get_width");
  static auto fn_ht  = tex_h.GetMethodInfo("get_height");
  static auto fn_cre = spr_h.GetMethodInfo("Create", 3);
  static auto fn_spr = img_h.GetMethodInfo("set_sprite");
  static auto fn_ovr = img_h.GetMethodInfo("set_overrideSprite");
  static auto fn_col = img_h.GetMethodInfo("set_color");
  static auto fn_typ = img_h.GetMethodInfo("set_type");
  static auto fn_asp = img_h.GetMethodInfo("set_preserveAspect");
  static auto fn_drt = img_h.GetMethodInfo("SetVerticesDirty");

  if (!fn_cre)
    return;
  int32_t     tw = InvokeInt32(fn_w, g_customLoadingTexture, 792, "Texture2D.get_width");
  int32_t     th = InvokeInt32(fn_ht, g_customLoadingTexture, 450, "Texture2D.get_height");
  FakeRect    rect{0.0f, 0.0f, (float)tw, (float)th};
  FakeVector2 pivot{0.5f, 0.5f};
  void*       createArgs[3] = {g_customLoadingTexture, &rect, &pivot};
  void*       spr           = InvokeObject(fn_cre, nullptr, createArgs, "Sprite.Create");
  if (!spr)
    return;
  if (outSprite)
    *outSprite = spr;
  void* spriteArgs[1] = {spr};
  InvokeVoid(fn_ovr, imageComp, spriteArgs, "Image.set_overrideSprite");
  InvokeVoid(fn_spr, imageComp, spriteArgs, "Image.set_sprite");
  FakeColor wh{1, 1, 1, 1};
  void*     colorArgs[1] = {&wh};
  InvokeVoid(fn_col, imageComp, colorArgs, "Image.set_color");
  int32_t imageType   = 0;
  void*   typeArgs[1] = {&imageType};
  InvokeVoid(fn_typ, imageComp, typeArgs, "Image.set_type");
  bool  preserveAspect = false;
  void* aspectArgs[1]  = {&preserveAspect};
  InvokeVoid(fn_asp, imageComp, aspectArgs, "Image.set_preserveAspect");
  InvokeVoid(fn_drt, imageComp, nullptr, "Graphic.SetVerticesDirty");
}

// Finds the BG Image component on a TransitionViewController instance, applies the
// custom sprite, and resets the BG RectTransform to stretch-fill its parent.
static void ApplyCustomSpriteToBGImage(void* _this)
{
  if (!g_customLoadingTexture)
    return;
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

    ApplySpriteToImage(imageComp, &g_ourSprite);

    // Reset BG RectTransform to stretch-fill (game oversizes it for parallax bleed).
    // RectTransform setters take Vector2 by value (8 bytes in a single register).
    if (g_bgRectTransform) {
      static auto rt_h  = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "RectTransform");
      static auto fn_am = rt_h.GetMethodInfo("set_anchorMin");
      static auto fn_ax = rt_h.GetMethodInfo("set_anchorMax");
      static auto fn_sd = rt_h.GetMethodInfo("set_sizeDelta");
      static auto fn_ap = rt_h.GetMethodInfo("set_anchoredPosition");
      if (fn_am && fn_ax && fn_sd && fn_ap) {
        FakeVector2 z{0, 0}, o{1, 1};
        void*       zeroArgs[1] = {&z};
        void*       oneArgs[1]  = {&o};
        InvokeVoid(fn_am, g_bgRectTransform, zeroArgs, "RectTransform.set_anchorMin");
        InvokeVoid(fn_ax, g_bgRectTransform, oneArgs, "RectTransform.set_anchorMax");
        InvokeVoid(fn_sd, g_bgRectTransform, zeroArgs, "RectTransform.set_sizeDelta");
        InvokeVoid(fn_ap, g_bgRectTransform, zeroArgs, "RectTransform.set_anchoredPosition");
      }
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

    g_spriteApplied = true;
  } catch (...) {
  }
}

void TransitionManager_SetLoadingScreen_Hook(auto original, void* _this, void* status, int32_t type,
                                             int32_t messagingType)
{
  try {
    const auto& cfg = Config::Get();
    if (cfg.loader_transition || cfg.loader_enabled)
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
    g_spriteApplied   = false;
    g_bgImageComp     = nullptr;
    g_ourSprite       = nullptr;
    g_bgRectTransform = nullptr;
    EnsureTextureLoaded();
  } catch (...) {
  }
}

void TransitionViewController_AboutToShow_Hook(auto original, void* _this)
{
  original(_this);
  try {
    if (!Config::Get().loader_transition)
      return;
    EnsureTextureLoaded();
    if (g_customLoadingTexture)
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
    EnsureTextureLoaded();
    if (g_customLoadingTexture)
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
    EnsureTextureLoaded();
    if (!g_customLoadingTexture)
      return;
    if (g_bgImageComp && g_ourSprite) {
      static auto img_h = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "Image");
      static auto f_ov  = img_h.GetField("m_OverrideSprite");
      static auto f_sp  = img_h.GetField("m_Sprite");
      void*       co = f_ov.isValidHelper() ? *reinterpret_cast<void**>((char*)g_bgImageComp + f_ov.offset()) : nullptr;
      void*       cs = f_sp.isValidHelper() ? *reinterpret_cast<void**>((char*)g_bgImageComp + f_sp.offset()) : nullptr;
      if (co == g_ourSprite || cs == g_ourSprite)
        return;
    }
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
    EnsureTextureLoaded();
    if (!g_customLoadingTexture)
      return;
    static auto h  = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.SlideShow", "SlideShowViewController");
    static auto fi = h.GetField("_image");
    if (!h.isValidHelper() || !fi.isValidHelper())
      return;
    void* img = *reinterpret_cast<void**>((char*)_this + fi.offset());
    if (img)
      ApplySpriteToImage(img);
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
    EnsureTextureLoaded();
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
    if (bgImg)
      ApplySpriteToImage(bgImg);
  } catch (...) {
  }
}

void InstallLoadingScreenBgHooks()
{
  const auto& cfg = Config::Get();
  auto tv_h       = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.LoadingScreen", "TransitionViewController");
  auto tm_h       = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.LoadingScreen", "TransitionManager");

  if (!tm_h.isValidHelper()) {
    ErrorMsg::MissingHelper("LoadingScreen", "TransitionManager");
  } else {
    if (auto m = tm_h.GetMethod("SetLoadingScreen")) {
      SPUD_STATIC_DETOUR(m, TransitionManager_SetLoadingScreen_Hook);
      spdlog::info("Loading screen hook installed (TransitionManager.SetLoadingScreen)");
    } else {
      ErrorMsg::MissingMethod("TransitionManager", "SetLoadingScreen");
    }
  }

  if (cfg.loader_transition) {
    if (!tv_h.isValidHelper()) {
      ErrorMsg::MissingHelper("LoadingScreen", "TransitionViewController");
      spdlog::error("[LS] TransitionViewController not found — transition background hooks skipped");
    } else {
      if (auto m = tv_h.GetMethod("Awake")) {
        SPUD_STATIC_DETOUR(m, TransitionViewController_Awake_Hook);
        spdlog::info("Loading screen hook installed (TVC.Awake)");
      } else {
        ErrorMsg::MissingMethod("TransitionViewController", "Awake");
      }

      if (auto m = tv_h.GetMethod("AboutToShow")) {
        SPUD_STATIC_DETOUR(m, TransitionViewController_AboutToShow_Hook);
        spdlog::info("Loading screen hook installed (TVC.AboutToShow)");
      } else {
        ErrorMsg::MissingMethod("TransitionViewController", "AboutToShow");
      }

      if (auto m = tv_h.GetMethod("OnAssetBundleDidBeginDownloadEventCallback")) {
        SPUD_STATIC_DETOUR(m, TransitionViewController_OnAssetBundleDidBeginDownload_Hook);
        spdlog::info("Loading screen hook installed (TVC.OnAssetBundleDidBeginDownload)");
      } else {
        ErrorMsg::MissingMethod("TransitionViewController", "OnAssetBundleDidBeingDownload");
      }

      if (auto m = tv_h.GetMethod("DidAssetBundleDownloadCompleteEvent")) {
        SPUD_STATIC_DETOUR(m, TransitionViewController_DidAssetBundleDownloadComplete_Hook);
        spdlog::info("Loading screen hook installed (TVC.DidAssetBundleDownloadComplete)");
      } else {
        ErrorMsg::MissingMethod("TransitionViewController", "DidAssetBundleDownloadComplete");
      }

      auto ss_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.SlideShow", "SlideShowViewController");
      if (!ss_h.isValidHelper()) {
        ErrorMsg::MissingHelper("SlideShow", "SlideShowViewer");
      } else {
        if (auto m = ss_h.GetMethod("ShowCurrentSlide")) {
          SPUD_STATIC_DETOUR(m, SlideShowViewController_ShowCurrentSlide_Hook);
          spdlog::info("Loading screen hook installed (SlideShow.ShowCurrentSlide)");
        } else {
          ErrorMsg::MissingMethod("SlideShowViewer", "ShowCurrentSlide");
        }
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
      if (auto m = ls_h.GetMethod("Awake")) {
        SPUD_STATIC_DETOUR(m, LoginSequence_Awake_Hook);
        spdlog::info("Loading screen hook installed (LoginSequence.Awake)");
      }
    }
  } else {
    spdlog::info("[LS] Login screen background disabled");
  }
}
