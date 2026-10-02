#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>
#include <spdlog/spdlog.h>

#include <cstddef>

namespace
{
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
  { return get_class_helper(); }

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
  { return get_class_helper(); }

  DrawerContext* Context()
  {
    static auto widget_base = get_class_helper().GetParent("Widget`1");
    static auto field       = widget_base.GetProperty("Context");
    return field.GetRaw<DrawerContext>(this);
  }

  void OpenViaButtonCallback()
  {
    static auto method = get_class_helper().GetMethod<void(DrawerWidget*)>("OnOpenButtonClicked");
    if (method != nullptr) {
      method(this);
    }
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
  { return get_class_helper(); }

  DrawerWidget* SelectionDrawer()
  {
    static auto field = get_class_helper().GetField("_selectionDrawerWidget");
    return field.isValidHelper() ? *(DrawerWidget**)((ptrdiff_t)this + field.offset()) : nullptr;
  }
};

ShopListScrollerViewController* g_active_scroller    = nullptr;
bool                            g_opened_for_landing = false;

void TryAutoOpenDrawer(ShopListScrollerViewController* controller)
{
  if (!Config::Get().auto_open_bulk_claim_flyout || controller == nullptr || g_opened_for_landing) {
    return;
  }

  auto* drawer  = controller->SelectionDrawer();
  auto* context = drawer != nullptr ? drawer->Context() : nullptr;
  if (context == nullptr || !context->Enabled()) {
    return;
  }

  // Mark first so a native callback cannot cause a nested lifecycle update to open the drawer twice.
  g_opened_for_landing = true;
  drawer->OpenViaButtonCallback();
}

void ShopListScrollerViewController_AboutToShow(auto original, ShopListScrollerViewController* self)
{
  g_active_scroller    = self;
  g_opened_for_landing = false;

  original(self);
  TryAutoOpenDrawer(self);
}

void ShopListScrollerViewController_AboutToHide(auto original, ShopListScrollerViewController* self)
{
  if (g_active_scroller == self) {
    g_active_scroller    = nullptr;
    g_opened_for_landing = false;
  }

  original(self);
}

void ShopListScrollerViewController_UpdateDrawer(auto original, ShopListScrollerViewController* self)
{
  original(self);

  if (g_active_scroller == self) {
    TryAutoOpenDrawer(self);
  }
}
} // namespace

void InstallGiftsBulkClaimHooks()
{
  bool can_install = true;

  auto drawer_context = DrawerContext::ClassHelper();
  if (!drawer_context.isValidHelper()) {
    ErrorMsg::MissingHelper("Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerContext");
    can_install = false;
  } else if (!drawer_context.GetField("Enabled").isValidHelper()) {
    ErrorMsg::MissingMethod("DrawerContext", "Enabled");
    can_install = false;
  }

  auto drawer_widget = DrawerWidget::ClassHelper();
  if (!drawer_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerWidget");
    can_install = false;
  } else {
    auto widget_base = drawer_widget.GetParent("Widget`1");
    if (!widget_base.isValidHelper() || !widget_base.GetProperty("Context").isValidHelper()) {
      ErrorMsg::MissingMethod("DrawerWidget", "Widget<DrawerContext>.Context");
      can_install = false;
    }
    if (drawer_widget.GetMethod("OnOpenButtonClicked") == nullptr) {
      ErrorMsg::MissingMethod("DrawerWidget", "OnOpenButtonClicked");
      can_install = false;
    }
  }

  auto shop_list_scroller = ShopListScrollerViewController::ClassHelper();
  if (!shop_list_scroller.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Shop", "ShopListScrollerViewController");
    spdlog::warn("Skipping GiftsBulkClaimHooks: required IL2CPP dependencies are unavailable; no hooks installed.");
    return;
  }

  if (!shop_list_scroller.GetField("_selectionDrawerWidget").isValidHelper()) {
    ErrorMsg::MissingMethod("ShopListScrollerViewController", "_selectionDrawerWidget");
    can_install = false;
  }

  auto about_to_show = shop_list_scroller.GetMethod("AboutToShow");
  if (about_to_show == nullptr) {
    ErrorMsg::MissingMethod("ShopListScrollerViewController", "AboutToShow");
    can_install = false;
  }

  auto about_to_hide = shop_list_scroller.GetMethod("AboutToHide");
  if (about_to_hide == nullptr) {
    ErrorMsg::MissingMethod("ShopListScrollerViewController", "AboutToHide");
    can_install = false;
  }

  auto update_drawer = shop_list_scroller.GetMethod("UpdateDrawer");
  if (update_drawer == nullptr) {
    ErrorMsg::MissingMethod("ShopListScrollerViewController", "UpdateDrawer");
    can_install = false;
  }

  if (!can_install) {
    spdlog::warn("Skipping GiftsBulkClaimHooks: required IL2CPP dependencies are unavailable; no hooks installed.");
    return;
  }

  SPUD_STATIC_DETOUR(about_to_show, ShopListScrollerViewController_AboutToShow);
  SPUD_STATIC_DETOUR(about_to_hide, ShopListScrollerViewController_AboutToHide);
  SPUD_STATIC_DETOUR(update_drawer, ShopListScrollerViewController_UpdateDrawer);
}
