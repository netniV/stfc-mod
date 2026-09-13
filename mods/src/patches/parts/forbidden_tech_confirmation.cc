#include "config.h"
#include "errormsg.h"
#include "settings/forbidden_tech.h"
#include "settings/windows_hook_extent.h"

#include <il2cpp/il2cpp_helper.h>
#include <il2cpp-tabledefs.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <cstring>

namespace
{
constexpr int32_t ConfirmButtonResult = 1;

const FieldInfo* on_selection_field = nullptr;
bool             installed          = false;

bool IsForbiddenTechConfirmation(const Il2CppDelegate* callback)
{
  if (callback == nullptr || callback->target == nullptr || callback->method == nullptr) {
    return false;
  }

  const auto* target_class = callback->target->klass;
  const auto* owner_class  = target_class == nullptr ? nullptr : target_class->declaringType;

  return owner_class != nullptr && strcmp(owner_class->name, "ForbiddenTechManager") == 0
         && strcmp(callback->method->name, "<RequestAction>b__0") == 0;
}

bool ConfirmForbiddenTechUpgrade(Il2CppDelegate* callback)
{
  if (!Config::Get().auto_confirm_ft_upgrade || !IsForbiddenTechConfirmation(callback)) {
    return false;
  }

  int32_t          result    = ConfirmButtonResult;
  void*            args[]    = {&result};
  Il2CppException* exception = nullptr;
  il2cpp_runtime_invoke(callback->method, callback->target, args, &exception);

  if (exception != nullptr) {
    spdlog::warn("ForbiddenTechConfirmation: failed to invoke the confirmation callback");
    return false;
  }

  return true;
}

Il2CppDelegate* GetSelectionCallback(void* context)
{
  if (context == nullptr || on_selection_field == nullptr) {
    return nullptr;
  }

  return *reinterpret_cast<Il2CppDelegate**>(reinterpret_cast<char*>(context) + on_selection_field->offset);
}

void MessageBox_Show_Hook(auto original, void* context)
{
  if (!ConfirmForbiddenTechUpgrade(GetSelectionCallback(context))) {
    original(context);
  }
}

void MessageBox_ShowWithCallback_Hook(auto original, void* context, Il2CppDelegate* callback)
{
  if (!ConfirmForbiddenTechUpgrade(callback)) {
    original(context, callback);
  }
}
} // namespace

bool ForbiddenTechControlsAvailable()
{ return installed; }

void InstallForbiddenTechConfirmationHooks()
{
  if (installed)
    return;
  auto message_box_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "MessageBox");
  if (!message_box_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Client.UI", "MessageBox");
    return;
  }

  auto context_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "MessageBoxContext");
  if (!context_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Client.UI", "MessageBoxContext");
    return;
  }

  on_selection_field = il2cpp_class_get_field_from_name(context_helper.get_cls(), "OnSelection");
  if (on_selection_field == nullptr || (on_selection_field->type->attrs & FIELD_ATTRIBUTE_STATIC)
      || on_selection_field->type->byref
      || (on_selection_field->type->type != IL2CPP_TYPE_CLASS
          && on_selection_field->type->type != IL2CPP_TYPE_GENERICINST)) {
    spdlog::error("Unable to find field 'MessageBoxContext->OnSelection'");
    return;
  }

  const auto* show               = message_box_helper.GetMethodInfo("Show", 1);
  const auto* show_with_callback = message_box_helper.GetMethodInfo("Show", 2);
  if (!show || !show->methodPointer || !show_with_callback || !show_with_callback->methodPointer) {
    ErrorMsg::MissingMethod("MessageBox", "Show");
    return;
  }
#if defined(_WIN32) && defined(_M_X64)
  for (auto* method : {show, show_with_callback}) {
    bool valid = (method->flags & METHOD_ATTRIBUTE_STATIC) && !method->is_generic && !method->is_inflated
                 && !method->has_full_generic_sharing_signature && method->return_type
                 && method->return_type->type == IL2CPP_TYPE_VOID && !method->return_type->byref
                 && mod_settings::WindowsHookFits(method->methodPointer);
    for (int i = 0; valid && i < method->parameters_count; ++i)
      valid = method->parameters[i] && !method->parameters[i]->byref
              && (method->parameters[i]->type == IL2CPP_TYPE_CLASS
                  || method->parameters[i]->type == IL2CPP_TYPE_GENERICINST);
    if (!valid || show->methodPointer == show_with_callback->methodPointer) {
      spdlog::warn("Forbidden Tech confirmation unavailable: hook metadata/extent");
      return;
    }
  }
#endif
  SPUD_STATIC_DETOUR(show->methodPointer, MessageBox_Show_Hook);
  SPUD_STATIC_DETOUR(show_with_callback->methodPointer, MessageBox_ShowWithCallback_Hook);
  installed = true;
}
