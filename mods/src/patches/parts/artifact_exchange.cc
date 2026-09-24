#include "config.h"

#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

namespace
{
ptrdiff_t convert_all_offset    = 0;
void* (*get_game_object)(void*) = nullptr;
void (*set_active)(void*, bool) = nullptr;

void InventoryUsePopup_Bind_Hook(auto original, void* controller)
{
  original(controller);
  if (!controller)
    return;
  auto* button = *reinterpret_cast<void**>(static_cast<char*>(controller) + convert_all_offset);
  if (button) {
    if (auto* object = get_game_object(button))
      set_active(object, false);
  }
}
} // namespace

void InstallArtifactExchangeHooks()
{
  // This dedicated field belongs to artifact bulk conversion, not the inventory
  // entry button or the individual exchange controls. Hide it after normal setup.
  auto controller =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Inventories", "InventoryUsePopupViewController");
  if (!controller.isValidHelper()) {
    spdlog::warn("[ArtifactExchange] popup class unavailable; leaving game unchanged");
    return;
  }
  auto field = controller.GetField("_convertAllButton");
  auto bind  = controller.GetMethod("OnDidBindCanvasContext", 0);
  // Current Unity exposes injected native entry points; use the managed wrappers
  // that accept component/GameObject instances, as the other UI patches do.
  auto component   = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
  auto game_object = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  get_game_object  = component.GetMethod<void*(void*)>("get_gameObject", 0);
  set_active       = game_object.GetMethod<void(void*, bool)>("SetActive", 1);
  if (!field.isValidHelper() || field.offset() < sizeof(Il2CppObject) || !bind || !get_game_object || !set_active) {
    spdlog::warn("[ArtifactExchange] required popup API unavailable; leaving game unchanged");
    return;
  }
  convert_all_offset = field.offset();
  SPUD_STATIC_DETOUR(bind, InventoryUsePopup_Bind_Hook);
  spdlog::info("[ArtifactExchange] hiding artifact Exchange All button");
}
