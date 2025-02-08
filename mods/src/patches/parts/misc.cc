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
#include <prime/InterstitialViewController.h>

int64_t InventoryForPopup_set_MaxItemsToUse(auto original, InventoryForPopup* a1, int64_t a2)
{
  if (a1->IsDonationUse && a2 == 50 && Config::Get().extend_donation_slider) {
    const auto max = Config::Get().extend_donation_max;
    if (max) {
      a2 = max;
    } else {
      return 0;
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
  auto h   = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Inventories", "InventoryForPopup");
  auto ptr = h.GetMethod("set_MaxItemsToUse");
  if (!ptr) {
    return;
  }
  SPUD_STATIC_DETOUR(ptr, InventoryForPopup_set_MaxItemsToUse);

  auto bundle_data_widget = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "BundleDataWidget");
  ptr                     = bundle_data_widget.GetMethod("OnActionButtonPressedCallback");
  if (!ptr) {
    return;
  }
  SPUD_STATIC_DETOUR(ptr, BundleDataWidget_OnActionButtonPressedCallback);
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
  SPUD_STATIC_DETOUR(il2cpp_resolve_icall_typed<ResolutionArray*()>("UnityEngine.Screen::get_resolutions()"),
                     GetResolutions_Hook);
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

SectionID ShopSummaryDirectorGoBackBehavior(auto original, ShopSummaryDirector* _this, void* status,
                                            SectionNavHistory* history)
{
  if (Config::Get().stay_in_bundle_after_summary
      && strcmp(((Il2CppObject*)(_this))->klass->name, "ShopSummaryDirector") == 0) {
    auto section_data = (ShopSectionContext*)Hub::get_SectionManager()->_sectionStorage->GetState(SectionID::Shop_List);
    section_data      = section_data;

    if (!section_data) {
      section_data =
          (ShopSectionContext*)Hub::get_SectionManager()->_sectionStorage->GetState(SectionID::Shop_Refining_List);
      section_data = section_data;
    }

    auto suppress_go_back = false;
    if (section_data) {
      auto bundle_config = section_data->_bundleConfig;
      if (bundle_config) {
        auto f           = bundle_config->_category;
        f                = f;
        suppress_go_back = (f == 3 || f == 10 || f == 22 || f == 29);
      }
    }

    if (suppress_go_back) {
      if (auto id_array = _this->_backLogicSkipSectionIds) {
        auto ids     = (SectionID*)id_array->vector;
        auto ids_len = id_array->max_length;
        for (size_t i = 0; i < ids_len; ++i) {
          auto id = ids[i];
          if (id == SectionID::Shop_Showcase) {
            ids[i] = SectionID::Navigation_Combat_Debug;
          }
        }
      }

      if (auto cache_id_array = _this->backlogicCache; cache_id_array) {
        auto cache_ids = (SectionID*)cache_id_array->vector;
        auto cache_len = cache_id_array->max_length;
        for (size_t i = 0; i < cache_len; ++i) {
          auto id = cache_ids[i];
          if (id == SectionID::Shop_Showcase) {
            cache_ids[i] = SectionID::Navigation_Combat_Debug;
          }
        }
      }

      auto sectionID = original(_this, status, history);

      if (auto id_array = _this->_backLogicSkipSectionIds) {
        auto ids     = (SectionID*)id_array->vector;
        auto ids_len = id_array->max_length;

        for (size_t i = 0; i < ids_len; ++i) {
          auto id = ids[i];
          if (id == SectionID::Navigation_Combat_Debug) {
            ids[i] = SectionID::Shop_Showcase;
          }
        }
      }

      if (auto cache_id_array = _this->backlogicCache; cache_id_array) {
        auto cache_ids = (SectionID*)cache_id_array->vector;
        auto cache_len = cache_id_array->max_length;

        for (size_t i = 0; i < cache_len; ++i) {
          auto id = cache_ids[i];
          if (id == SectionID::Navigation_Combat_Debug) {
            cache_ids[i] = SectionID::Shop_Showcase;
          }
        }
      }

      return sectionID;
    }
  }
  return original(_this, status, history);
}

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

//   const auto section_data = Hub::get_SectionManager()->_sectionStorage->GetState(sectionID);

void InstallTempCrashFixes()
{
  auto BuffService_helper =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService");
  if (!BuffService_helper.HasClass()) {
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
  if (!shop_scene_manager.HasClass()) {
    ErrorMsg::MissingHelper("Shop", "ShopSceneManager");
  } else {
    auto reveal_show = shop_scene_manager.GetMethod("ShouldShowRevealSequence");
    if (reveal_show == nullptr) {
      ErrorMsg::MissingMethod("ShopSceneManager", "ShouldShowRevealSequence");
    } else {
      SPUD_STATIC_DETOUR(reveal_show, ShouldShowRevealHook);
    }
  }

  auto shop_summary_director = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopSummaryDirector");
  if (!shop_summary_director.HasClass()) {
    ErrorMsg::MissingHelper("Shop", "ShopSummaryDirector");
  } else {
    shop_method = shop_summary_director.GetMethod("GoBackBehaviour");
    if (shop_method == nullptr) {
      ErrorMsg::MissingMethod("ShopSummaryDirectory", "GoBackBehaviour");
    } else {
      SPUD_STATIC_DETOUR(shop_method, ShopSummaryDirectorGoBackBehavior);
    }
  }

  static auto interstitial_controller =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Interstitial", "InterstitialViewController");
  if (!interstitial_controller.HasClass()) {
    ErrorMsg::MissingHelper("Interstitial", "InterstitialViewController");
  } else {
    auto interstitial_show = interstitial_controller.GetMethod("AboutToShow");
    if (interstitial_show == nullptr) {
      ErrorMsg::MissingMethod("InterstitialViewController", "AboutToShow");
    } else {
      SPUD_STATIC_DETOUR(interstitial_show, InterstitialViewController_AboutToShow);
    }
  }
}
