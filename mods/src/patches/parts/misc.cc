#include "config.h"
#include "errormsg.h"

#include <prime/BundleDataWidget.h>
#include <prime/ClientModifierType.h>
#include <prime/Hub.h>
#include <prime/IList.h>
#include <prime/InventoryForPopup.h>
#include <prime/ShopSummaryDirector.h>

#include <il2cpp/il2cpp_helper.h>

#include <spud/detour.h>

#if _WIN32
#include <Windows.h>
#endif

#include <algorithm>
#include <prime/ActionQueueManager.h>
#include <prime/InterstitialViewController.h>

int64_t InventoryForPopup_set_MaxItemsToUse(auto original, InventoryForPopup* a1, int64_t a2)
{
  if (a1->IsDonationUse && a2 == 50 && Config::Get().extend_donation_slider) {
    const auto max = Config::Get().extend_donation_max;
    if (max > 0) {
      a2 = max;
    } else {
      return -1;
    }
  }

  int64_t standard = original(a1, a2);
  return standard;
}

void BundleDataWidget_OnActionButtonPressedCallback(auto original, BundleDataWidget* _this)
{
  if (_this->CurrentState & BundleDataWidget::ItemState::CooldownTimerOn) {
    _this->AuxViewButtonPressedHandler();
  } else {
    original(_this);
  }
}

void InstallMiscPatches()
{
#if _WIN32
  auto h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Inventories", "InventoryForPopup");
  if (!h.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Inventories", "InventoryForPopup");
  } else {
    auto ptr = h.GetMethod("set_MaxItemsToUse");
    if (!ptr) {
      ErrorMsg::MissingMethod("InventoryForPopup", "set_MaxItemsToUse");
    } else {
      SPUD_STATIC_DETOUR(ptr, InventoryForPopup_set_MaxItemsToUse);
    }
  }
#endif

  auto bundle_data_widget = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "BundleDataWidget");
  if (!bundle_data_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Shop", "BundleDataWidget");
  } else {
    auto ptr = bundle_data_widget.GetMethod("OnActionButtonPressedCallback");
    if (!ptr) {
      ErrorMsg::MissingMethod("BundleDataWidget", "OnActionButtonPressedCallback");
    } else
      SPUD_STATIC_DETOUR(ptr, BundleDataWidget_OnActionButtonPressedCallback);
  }
}

struct Resolution {
  int m_Width;
  int m_Height;
  int m_RefreshRate;

  bool operator==(const Resolution& other) const
  {
    return this->m_Height == other.m_Height && this->m_Width == other.m_Width;
  }
};

struct ResolutionArray {
  Il2CppObject obj;
  void*        bounds;
  size_t       maxlength;
  Resolution   data[1];
};

ResolutionArray* GetResolutions_Hook(auto original)
{
  auto resolutions = original();

#if _WIN32
  // Modify
  auto screenWidth  = GetSystemMetrics(SM_CXSCREEN);
  auto screenHeight = GetSystemMetrics(SM_CYSCREEN);

  int targetRefreshRate = 0;
  for (int i = 0; i < resolutions->maxlength; ++i) {
    auto ores = resolutions->data[i];
    if (ores.m_Width == screenWidth && ores.m_Height == screenHeight) {
      targetRefreshRate = std::max(ores.m_RefreshRate, targetRefreshRate);
    }
  }

  std::vector<Resolution> res;
  for (int i = 0; i < resolutions->maxlength; ++i) {
    if (Config::Get().show_all_resolutions)
      resolutions->data[i].m_RefreshRate = targetRefreshRate;

    auto ores = resolutions->data[i];
    if (Config::Get().show_all_resolutions || (ores.m_RefreshRate == targetRefreshRate || targetRefreshRate == 0)) {
      res.push_back(ores);
    }
  }

  res.erase(unique(res.begin(), res.end()), res.end());

  int i = 0;
  for (const auto& resultRes : res) {
    resolutions->data[i] = resultRes;
    ++i;
  }
  resolutions->maxlength = res.size();
#endif

  return resolutions;
}

void InstallResolutionListFix()
{
  auto get_resolutions = il2cpp_resolve_icall_typed<ResolutionArray*()>("UnityEngine.Screen::get_resolutions()");
  if (!get_resolutions) {
    ErrorMsg::MissingMethod("UnityEngine.Screen", "get_resolutions");
  } else {
    SPUD_STATIC_DETOUR(get_resolutions, GetResolutions_Hook);
  }
}

IList* ExtractBuffsOfType_Hook(auto original, ClientModifierType modifier, IList* list)
{
  if (list) {
    for (int i = 0; i < list->Count; ++i) {
      auto item = list->Get(i);
      if (item == 0) {
        return nullptr;
      }
    }
  }
  return original(modifier, list);
}

bool ShouldShowRevealHook(auto original, void* _this, bool ignore)
{
  auto result = original(_this, ignore);
  if (Config::Get().always_skip_reveal_sequence) {
    return false;
  }
  return result;
}

struct ShopCategory {
public:
  __declspec(property(get = __get__flagValue)) int Value;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Prime.Shop", "ShopCategory");
    return class_helper;
  }

public:
  int __get__flagValue()
  {
    static auto field = get_class_helper().GetProperty("Value");
    return *field.GetUnboxedSelf<int>(this);
  }
};

struct CurrencyType {
public:
  __declspec(property(get = __get__flagValue)) int Value;
  //

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Content", "CurrencyType");
    return class_helper;
  }

public:
  int __get__flagValue()
  {
    static auto field = get_class_helper().GetProperty("Value");
    return *field.GetUnboxedSelf<int>(this);
  }
};

struct BundleGroupConfig {
public:
  __declspec(property(get = __get__category)) int _category;
  __declspec(property(get = __get__currency)) int _currency;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "BundleGroupConfig");
    return class_helper;
  }

public:
  int __get__category()
  {
    static auto field = get_class_helper().GetField("_category");
    return *(int*)((ptrdiff_t)this + field.offset());
  }

  int __get__currency()
  {
    static auto field = get_class_helper().GetField("_currency");
    return *(int*)((ptrdiff_t)this + field.offset());
  }
};

class ShopSectionContext
{
public:
  __declspec(property(get = __get__bundleConfig)) BundleGroupConfig* _bundleConfig;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopSectionContext");
    return class_helper;
  }

public:
  BundleGroupConfig* __get__bundleConfig()
  {
    static auto field = get_class_helper().GetProperty("BundleGroup");
    return field.GetRaw<BundleGroupConfig>(this);
  }
};

bool isFirstInterstitial = true;

void InterstitialViewController_AboutToShow(auto original, InterstitialViewController* _this)
{
  if (Config::Get().disable_first_popup && isFirstInterstitial && _this != nullptr) {
    isFirstInterstitial = false;
    _this->CloseWhenReady();
  } else {
    original(_this);
  }
}

void ActionQueueManager_AddActionToQueue(auto original, ActionQueueManager* _this, long fleet_id)
{
  spdlog::warn("ActionQueueManager_AddActionToQueue({})", fleet_id);
  original(_this, fleet_id);
}

//   const auto section_data = Hub::get_SectionManager()->_sectionStorage->GetState(sectionID);

void InstallTempCrashFixes()
{
  auto BuffService_helper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService");
  if (!BuffService_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffService");
  } else {
    auto ptr_extract_buffs_of_type = BuffService_helper.GetMethod("ExtractBuffsOfType");
    if (ptr_extract_buffs_of_type == nullptr) {
      ErrorMsg::MissingMethod("BuffService", "ExtractBuffsOfType");
    } else {
      SPUD_STATIC_DETOUR(ptr_extract_buffs_of_type, ExtractBuffsOfType_Hook);
    }
  }

  auto shop_scene_manager = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopSceneManager");
  if (!shop_scene_manager.isValidHelper()) {
    ErrorMsg::MissingHelper("Shop", "ShopSceneManager");
  } else {
    auto reveal_show = shop_scene_manager.GetMethod("ShouldShowRevealSequence");
    if (reveal_show == nullptr) {
      ErrorMsg::MissingMethod("ShopSceneManager", "ShouldShowRevealSequence");
    } else {
      SPUD_STATIC_DETOUR(reveal_show, ShouldShowRevealHook);
    }
  }

  static auto interstitial_controller =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Interstitial", "InterstitialViewController");
  if (!interstitial_controller.isValidHelper()) {
    ErrorMsg::MissingHelper("Interstitial", "InterstitialViewController");
  } else {
    auto interstitial_show = interstitial_controller.GetMethod("AboutToShow");
    if (interstitial_show == nullptr) {
      ErrorMsg::MissingMethod("InterstitialViewController", "AboutToShow");
    } else {
      SPUD_STATIC_DETOUR(interstitial_show, InterstitialViewController_AboutToShow);
    }
  }

  static auto actionqueue_manager =
      il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!actionqueue_manager.isValidHelper()) {
    ErrorMsg::MissingHelper("ActionQueue", "ActionQueueMaanger");
  } else {
    auto addtoqueue_method = actionqueue_manager.GetMethod("AddActionToQueue");
    if (addtoqueue_method == nullptr) {
      ErrorMsg::MissingMethod("ActionQueueManager", "AddActionToQueue");
    } else {
      // SPUD_STATIC_DETOUR(addtoqueue_method, ActionQueueManager_AddActionToQueue);
    }
  }
}
