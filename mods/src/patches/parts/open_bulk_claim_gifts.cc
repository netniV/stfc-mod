#include "config.h"
#include "errormsg.h"
#include "prime/Hub.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>

#include <cstddef>
#include <cstring>
#include <string>

namespace
{
constexpr auto kGiftsCategoryKey    = "chests";
constexpr int  kAutoOpenRetryFrames = 300;

bool g_pending_auto_open   = false;
bool g_auto_open_attempted = false;
int  g_retry_frames_left   = 0;

class DrawerContext
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerContext");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  {
    return get_class_helper();
  }

  bool Enabled()
  {
    static auto field = get_class_helper().GetField("Enabled");
    return field.isValidHelper() && *(bool*)((ptrdiff_t)this + field.offset());
  }
};

class DrawerWidget
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerWidget");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  {
    return get_class_helper();
  }

  DrawerContext* Context()
  {
    static auto widget_base = get_class_helper().GetParent("Widget`1");
    static auto field       = widget_base.GetProperty("Context");
    return field.GetRaw<DrawerContext>(this);
  }

  bool ContextEnabled()
  {
    auto context = Context();
    return context != nullptr && context->Enabled();
  }

  void OpenViaButtonCallback()
  {
    static auto method = get_class_helper().GetMethod<void(DrawerWidget*)>("OnOpenButtonClicked");
    if (method != nullptr) {
      method(this);
    }
  }
};

class ShopCategory
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Prime.Shop", "ShopCategory");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  {
    return get_class_helper();
  }

  static std::string KeyForValue(int value)
  {
    static auto method = get_class_helper().GetMethod<Il2CppString*(int)>("EnumToKey");
    auto        key    = method != nullptr ? method(value) : nullptr;
    return key != nullptr ? to_string(key) : std::string{};
  }
};

class BundleGroupConfig
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "BundleGroupConfig");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  {
    return get_class_helper();
  }

  int Category()
  {
    static auto field = get_class_helper().GetField("_category");
    return field.isValidHelper() ? *(int*)((ptrdiff_t)this + field.offset()) : -1;
  }
};

class ShopSectionContext
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopSectionContext");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  {
    return get_class_helper();
  }
};

class ShopListScrollerViewController
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopListScrollerViewController");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  {
    return get_class_helper();
  }

  DrawerWidget* SelectionDrawer()
  {
    // Lower Gifts << drawer; scroller category can describe upper shop chrome such as latinum.
    static auto field = get_class_helper().GetField("_selectionDrawerWidget");
    return field.isValidHelper() ? *(DrawerWidget**)((ptrdiff_t)this + field.offset()) : nullptr;
  }
};

bool IsClassNamed(void* object, const char* namespaze, const char* name)
{
  if (object == nullptr) {
    return false;
  }

  const auto klass = ((Il2CppObject*)object)->klass;
  if (klass == nullptr) {
    return false;
  }

  const auto object_namespace = klass->namespaze != nullptr ? klass->namespaze : "";
  return strcmp(object_namespace, namespaze) == 0 && strcmp(klass->name, name) == 0;
}

void ClearAutoOpen()
{
  g_pending_auto_open = false;
  g_retry_frames_left = 0;
}

void ArmAutoOpen()
{
  g_pending_auto_open   = true;
  g_auto_open_attempted = false;
  g_retry_frames_left   = kAutoOpenRetryFrames;
}

bool IsGiftsShopPayload(int section, void* args)
{
  if (section != (int)SectionID::Shop_List || !IsClassNamed(args, "Digit.Prime.Shop", "BundleGroupConfig")) {
    return false;
  }

  const auto category = ((BundleGroupConfig*)args)->Category();
  return ShopCategory::KeyForValue(category) == kGiftsCategoryKey;
}

bool IsBulkClaimTabSection(int section)
{
  return section == (int)SectionID::Shop_List || section == (int)SectionID::Shop_AllianceChests
         || section == (int)SectionID::Consumables || section == (int)SectionID::Missions_AwayTeamsList;
}

void TryAutoOpenDrawer(ShopListScrollerViewController* controller)
{
  if (!Config::Get().auto_open_bulk_claim_flyout || !g_pending_auto_open || g_auto_open_attempted
      || controller == nullptr) {
    return;
  }

  if (g_retry_frames_left-- <= 0) {
    ClearAutoOpen();
    return;
  }

  auto drawer = controller->SelectionDrawer();
  if (drawer == nullptr || !drawer->ContextEnabled()) {
    return;
  }

  g_auto_open_attempted = true;
  ClearAutoOpen();
  drawer->OpenViaButtonCallback();
}

void ShopListViewController_AboutToHide(auto original, void* _this)
{
  ClearAutoOpen();
  original(_this);
}

void ShopListScrollerViewController_Update(auto original, ShopListScrollerViewController* _this)
{
  original(_this);
  TryAutoOpenDrawer(_this);
}

void ShopSectionContext_InjectTabData(auto original, ShopSectionContext* _this, int section,
                                      Il2CppArraySize* tab_locale_contexts,
                                      Il2CppArraySize* additional_tab_locale_contexts,
                                      Il2CppArraySize* tab_icon_identifiers, Il2CppArraySize* pip_types,
                                      Il2CppArraySize* hide_if_no_content, bool set_current_section,
                                      bool override_existing_tabs)
{
  original(_this, section, tab_locale_contexts, additional_tab_locale_contexts, tab_icon_identifiers, pip_types,
           hide_if_no_content, set_current_section, override_existing_tabs);

  if (!Config::Get().auto_open_bulk_claim_flyout || !set_current_section) {
    return;
  }

  if (IsBulkClaimTabSection(section)) {
    ArmAutoOpen();
  } else {
    ClearAutoOpen();
  }
}

bool SectionManager_TriggerSectionChange(auto original,
                                         void* _this,
                                         int next_section,
                                         void* args,
                                         bool forced_section_change,
                                         bool is_go_back_step,
                                         bool allow_same_section)
{
  const auto changed = original(_this, next_section, args, forced_section_change, is_go_back_step, allow_same_section);
  if (changed && Config::Get().auto_open_bulk_claim_flyout && IsGiftsShopPayload(next_section, args)) {
    ArmAutoOpen();
  }
  return changed;
}
} // namespace

void InstallGiftsBulkClaimHooks()
{
  auto drawer_context = DrawerContext::ClassHelper();
  if (!drawer_context.isValidHelper()) {
    ErrorMsg::MissingHelper("Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerContext");
  } else if (!drawer_context.GetField("Enabled").isValidHelper()) {
    ErrorMsg::MissingMethod("DrawerContext", "Enabled");
  }

  auto drawer_widget = DrawerWidget::ClassHelper();
  if (!drawer_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerWidget");
  } else {
    auto widget_base = drawer_widget.GetParent("Widget`1");
    if (!widget_base.isValidHelper() || !widget_base.GetProperty("Context").isValidHelper()) {
      ErrorMsg::MissingMethod("DrawerWidget", "Widget<DrawerContext>.Context");
    }
    if (drawer_widget.GetMethod("OnOpenButtonClicked") == nullptr) {
      ErrorMsg::MissingMethod("DrawerWidget", "OnOpenButtonClicked");
    }
  }

  auto shop_category = ShopCategory::ClassHelper();
  if (!shop_category.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Shop", "ShopCategory");
  } else if (shop_category.GetMethod("EnumToKey") == nullptr) {
    ErrorMsg::MissingMethod("ShopCategory", "EnumToKey");
  }

  auto bundle_group_config = BundleGroupConfig::ClassHelper();
  if (!bundle_group_config.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Shop", "BundleGroupConfig");
  } else if (!bundle_group_config.GetField("_category").isValidHelper()) {
    ErrorMsg::MissingMethod("BundleGroupConfig", "_category");
  }

  auto shop_section_context = ShopSectionContext::ClassHelper();
  if (!shop_section_context.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Shop", "ShopSectionContext");
  } else if (auto ptr = shop_section_context.GetMethod("InjectTabData"); ptr == nullptr) {
    ErrorMsg::MissingMethod("ShopSectionContext", "InjectTabData");
  } else {
    SPUD_STATIC_DETOUR(ptr, ShopSectionContext_InjectTabData);
  }

  auto shop_list_controller = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopListViewController");
  if (!shop_list_controller.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Shop", "ShopListViewController");
  } else if (auto ptr = shop_list_controller.GetMethod("AboutToHide"); ptr == nullptr) {
    ErrorMsg::MissingMethod("ShopListViewController", "AboutToHide");
  } else {
    SPUD_STATIC_DETOUR(ptr, ShopListViewController_AboutToHide);
  }

  auto shop_list_scroller = ShopListScrollerViewController::ClassHelper();
  if (!shop_list_scroller.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Shop", "ShopListScrollerViewController");
  } else {
    if (!shop_list_scroller.GetField("_selectionDrawerWidget").isValidHelper()) {
      ErrorMsg::MissingMethod("ShopListScrollerViewController", "_selectionDrawerWidget");
    }
    if (auto ptr = shop_list_scroller.GetMethod("Update"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ShopListScrollerViewController", "Update");
    } else {
      SPUD_STATIC_DETOUR(ptr, ShopListScrollerViewController_Update);
    }
  }

  auto section_manager = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Sections", "SectionManager");
  if (!section_manager.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Client.Sections", "SectionManager");
  } else if (auto ptr = section_manager.GetMethod("TriggerSectionChange", 5); ptr == nullptr) {
    ErrorMsg::MissingMethod("SectionManager", "TriggerSectionChange(section,args)");
  } else {
    SPUD_STATIC_DETOUR(ptr, SectionManager_TriggerSectionChange);
  }
}
