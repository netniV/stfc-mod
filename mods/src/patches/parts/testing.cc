#include "config.h"
#include "errormsg.h"

#include "prime/ActionRequirement.h"
#include "prime/BlurController.h"
#include "prime/BookmarksManager.h"
#include "prime/CallbackContainer.h"
#include "prime/ChatManager.h"
#include "prime/ChatMessageListLocalViewController.h"
#include "prime/ClientModifierType.h"
#include "prime/DeploymentManager.h"
#include "prime/FleetLocalViewController.h"
#include "prime/FleetsManager.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/Hub.h"
#include "prime/InventoryForPopup.h"
#include "prime/KeyCode.h"
#include "prime/NavigationSectionManager.h"
#include "prime/ScanTargetViewController.h"
#include "prime/SceneManager.h"
#include "prime/ScreenManager.h"
#include <prime/UIBehaviour.h>

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>
#include <spud/signature.h>

class AppConfig
{
public:
  __declspec(property(get = __get_PlatformSettingsUrl, put = __set_PlatformSettingsUrl))
  Il2CppString*                                                                                  PlatformSettingsUrl;
  __declspec(property(get = __get_PlatformApiKey, put = __set_PlatformApiKey)) Il2CppString*     PlatformApiKey;
  __declspec(property(get = __get_AssetUrlOverride, put = __set_AssetUrlOverride)) Il2CppString* AssetUrlOverride;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "AppConfig");
    return class_helper;
  }

public:
  Il2CppString* __get_PlatformSettingsUrl()
  {
    static auto prop = get_class_helper().GetProperty("PlatformSettingsUrl");
    return prop.GetRaw<Il2CppString>((void*)this);
  }

  void __set_PlatformSettingsUrl(Il2CppString* v)
  {
    static auto prop = get_class_helper().GetProperty("PlatformSettingsUrl");
    return prop.SetRaw((void*)this, *v);
  }

  Il2CppString* __get_PlatformApiKey()
  {
    static auto prop = get_class_helper().GetProperty("PlatformApiKey");
    return prop.GetRaw<Il2CppString>((void*)this);
  }

  void __set_PlatformApiKey(Il2CppString* v)
  {
    static auto prop = get_class_helper().GetProperty("PlatformApiKey");
    return prop.SetRaw((void*)this, *v);
  }

  Il2CppString* __get_AssetUrlOverride()
  {
    static auto prop = get_class_helper().GetProperty("AssetUrlOverride");
    return prop.GetRaw<Il2CppString>((void*)this);
  }

  void __set_AssetUrlOverride(Il2CppString* v)
  {
    static auto prop = get_class_helper().GetProperty("AssetUrlOverride");
    return prop.SetRaw((void*)this, *v);
  }
};

class Model
{
public:
  __declspec(property(get = __get_AppConfig)) AppConfig* AppConfig_;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "Model");
    return class_helper;
  }

public:
  AppConfig* __get_AppConfig()
  {
    static auto field = get_class_helper().GetField("_appConfig");
    return *(AppConfig**)((ptrdiff_t)this + field.offset());
  }
};

void Cursor_SetCursor(auto original, void* _this, ptrdiff_t texture, Vector2* hotspot, int cursorMode)
{
#if _WIN32
  if (!Config::Get().allow_cursor) {
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    ClipCursor(nullptr); // free cursor from any Unity clipping
    return;
  }
#endif

  return original(_this, texture, hotspot, cursorMode);
}

AppConfig* Model_LoadConfigs(auto original, Model* _this)
{
  original(_this);
  auto config = _this->AppConfig_;

  if (!Config::Get().config_settings_url.empty()) {
    auto new_settings_url       = il2cpp_string_new(Config::Get().config_settings_url.c_str());
    config->PlatformSettingsUrl = new_settings_url;
  }

  if (!Config::Get().config_assets_url_override.empty()) {
    auto new_url             = il2cpp_string_new(Config::Get().config_assets_url_override.c_str());
    config->AssetUrlOverride = new_url;
  }

  return config;
}

void SetActive_hook(auto original, void* _this, bool active)
{
  static auto IsActiveSelf = il2cpp_resolve_icall_typed<bool(void*)>("UnityEngine.GameObject::get_activeSelf()");

  if (active && IsActiveSelf(_this)) {
    return;
    // __debugbreak();
  }
  return original(_this, active);
}

bool IsQueueEnabled(auto original, void* _this)
{
  if (Config::Get().queue_enabled) {
    return original(_this);
  }

  return false;
}

void InstallTestPatches()
{
  auto cursorManager = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Cursor");
  if (!cursorManager.isValidHelper()) {
    ErrorMsg::MissingHelper("UnityEngine", "Cursor");
  } else {
    auto cursorMethod = cursorManager.GetMethod("SetCursor_Injected");
    if (cursorMethod == nullptr) {
      ErrorMsg::MissingMethod("Cursor", "SetCursor_Injected");
    } else {
      SPUD_STATIC_DETOUR(cursorMethod, Cursor_SetCursor);
    }
  }

  auto model = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "Model");
  if (!model.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "Model");
  } else {
    auto load_configs_ptr = model.GetMethod("LoadConfigs");
    if (load_configs_ptr == nullptr) {
      ErrorMsg::MissingMethod("Model", "LoadConfigs");
    } else {
      SPUD_STATIC_DETOUR(load_configs_ptr, Model_LoadConfigs);
    }
  }

  auto battle_target_data =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BattleTargetData");
  if (!battle_target_data.isValidHelper()) {
    ErrorMsg::MissingHelper("Models", "BattleTargetData");
  } else {
    static auto SetActive =
        il2cpp_resolve_icall_typed<void(void*, bool)>("UnityEngine.GameObject::SetActive(System.Boolean)");
    if (SetActive == nullptr) {
      ErrorMsg::MissingStaticMethod("GameObject", "SetActive");
    } else {
      SPUD_STATIC_DETOUR(SetActive, SetActive_hook);
    }
  }

  auto queue_manager = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!queue_manager.isValidHelper()) {
    ErrorMsg::MissingHelper("ActionQueue", "ActionQueueManager");
  } else {

    auto is_queue_unlocked = queue_manager.GetMethod("IsQueueUnlocked");
    if (is_queue_unlocked == nullptr) {
      ErrorMsg::MissingStaticMethod("GameObject", "IsQueueUnlocked");
    } else {
      SPUD_STATIC_DETOUR(is_queue_unlocked, IsQueueEnabled);
    }
  }
}
