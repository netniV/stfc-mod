#pragma once

#include "errormsg.h"
#include <il2cpp/il2cpp_helper.h>

#include "MonoSingleton.h"

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

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Bookmarks", "BookmarksManager");
    return class_helper;
  }
};
