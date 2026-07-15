#pragma once

#include "errormsg.h"
#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>

#include "MonoSingleton.h"
#include "Hub.h"

struct BookmarksManager : MonoSingleton<BookmarksManager> {
  friend struct MonoSingleton<BookmarksManager>;

public:
  void ViewBookmarks()
  {
    static auto ViewBookmarksMethod = get_class_helper().GetMethod<void(BookmarksManager*)>("ViewBookmarks");
    static auto ViewBookmarksWarn   = true;

    if (ViewBookmarksMethod) {
      ViewBookmarksMethod(this);
    } else if (ViewBookmarksWarn) {
      ViewBookmarksWarn = false;
      ErrorMsg::MissingMethod("BookmarksManager", "ViewBookmarks");
    }
  }

  void ViewCoordinateSearch()
  {
    auto* context = CreateCoordinateSearchContext();
    if (!context) {
      spdlog::error("[CoordSearch] Failed to create CoordinateSearchContext, falling back to bookmarks");
      ViewBookmarks();
      return;
    }

    Hub::get_SectionManager()->TriggerSectionChange(SectionID::Bookmarks_Search_Coordinates, context, false, false, true);
  }

private:
  void* CreateCoordinateSearchContext()
  {
    static auto coord_class = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Bookmarks", "CoordinateSearchContext");
    if (!coord_class.isValidHelper()) {
      spdlog::error("[CoordSearch] Failed to get CoordinateSearchContext class");
      return nullptr;
    }

    auto* context = coord_class.New<void>();
    if (!context) {
      spdlog::error("[CoordSearch] Failed to create CoordinateSearchContext instance");
      return nullptr;
    }

    static auto ctor_method = coord_class.GetMethod<void(void*)>(".ctor");
    if (ctor_method) {
      ctor_method(context);
    } else {
      spdlog::warn("[CoordSearch] CoordinateSearchContext .ctor() method not found");
    }

    auto* zero_str = il2cpp_string_new("0");
    static auto set_x_method = coord_class.GetMethod<void(void*, void*)>("set_DefaultXCoordinate");
    if (set_x_method && zero_str) {
      set_x_method(context, zero_str);
    }

    static auto set_y_method = coord_class.GetMethod<void(void*, void*)>("set_DefaultYCoordinate");
    if (set_y_method && zero_str) {
      set_y_method(context, zero_str);
    }

    auto* empty_str = il2cpp_string_new("");
    static auto set_system_method = coord_class.GetMethod<void(void*, void*)>("set_DefaultSystemInput");
    if (set_system_method && empty_str) {
      set_system_method(context, empty_str);
    }

    return context;
  }

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Bookmarks", "BookmarksManager");
    return class_helper;
  }
};
