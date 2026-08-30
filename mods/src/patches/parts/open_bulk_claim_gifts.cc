#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>

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

void DrawerWidget_OnDidBindContext(auto original, DrawerWidget* self)
{
  original(self);

  if (g_active_scroller != nullptr && g_active_scroller->SelectionDrawer() == self) {
    // A new native selection-drawer binding represents a new in-place tab landing.
    g_opened_for_landing = false;
    TryAutoOpenDrawer(g_active_scroller);
  }
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
    if (auto ptr = drawer_widget.GetMethod("OnDidBindContext"); ptr == nullptr) {
      ErrorMsg::MissingMethod("DrawerWidget", "OnDidBindContext");
    } else {
      SPUD_STATIC_DETOUR(ptr, DrawerWidget_OnDidBindContext);
    }
  }

  auto shop_list_scroller = ShopListScrollerViewController::ClassHelper();
  if (!shop_list_scroller.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Shop", "ShopListScrollerViewController");
    return;
  }

  if (!shop_list_scroller.GetField("_selectionDrawerWidget").isValidHelper()) {
    ErrorMsg::MissingMethod("ShopListScrollerViewController", "_selectionDrawerWidget");
  }

  if (auto ptr = shop_list_scroller.GetMethod("AboutToShow"); ptr == nullptr) {
    ErrorMsg::MissingMethod("ShopListScrollerViewController", "AboutToShow");
  } else {
    SPUD_STATIC_DETOUR(ptr, ShopListScrollerViewController_AboutToShow);
  }

  if (auto ptr = shop_list_scroller.GetMethod("AboutToHide"); ptr == nullptr) {
    ErrorMsg::MissingMethod("ShopListScrollerViewController", "AboutToHide");
  } else {
    SPUD_STATIC_DETOUR(ptr, ShopListScrollerViewController_AboutToHide);
  }

  if (auto ptr = shop_list_scroller.GetMethod("UpdateDrawer"); ptr == nullptr) {
    ErrorMsg::MissingMethod("ShopListScrollerViewController", "UpdateDrawer");
  } else {
    SPUD_STATIC_DETOUR(ptr, ShopListScrollerViewController_UpdateDrawer);
  }
}
