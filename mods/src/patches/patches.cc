#include "patches.h"
#include "file.h"
#include "version.h"

#include <il2cpp/il2cpp-functions.h>

#include <spud/detour.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#if _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#endif

void InstallUiScaleHooks();
void InstallZoomHooks();
void InstallBuffFixHooks();
#if _WIN32
void InstallFreeResizeHooks();
#endif
void InstallToastBannerHooks();
void InstallPanHooks();
void InstallImproveResponsivenessHooks();
void InstallHotkeyHooks();
void InstallTestPatches();
void InstallMiscPatches();
void InstallChatPatches();
void InstallResolutionListFix();
void InstallTempCrashFixes();
void InstallSyncPatches();
void InstallObjectTrackers();

__int64 il2cpp_init_hook(auto original, const char* domain_name)
{
  struct PatchEntry {
    const char*                  name;
    std::pair<void (*)(), bool*> fnAndEnabled;
  };

#if _WIN32
#ifndef NDEBUG
  AllocConsole();
  FILE* fp;
  freopen_s(&fp, "CONOUT$", "w", stdout);
#endif
#endif

  File::Init();

  auto file_logger = spdlog::basic_logger_mt("default", File::Log(), true);
  auto sink        = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  file_logger->sinks().push_back(sink);
  spdlog::set_default_logger(file_logger);

  const auto log_level =
      File::hasTrace() ? spdlog::level::trace : (File::hasDebug() ? spdlog::level::debug : spdlog::level::info);

  spdlog::set_level(log_level);
  spdlog::flush_on(log_level);

  spdlog::info("Initializing STFC Community Patch ({})", VER_PRODUCT_VERSION_STR);
  spdlog::info("");
  if (File::hasCustomNames()) {
    spdlog::info("Using custom names");
  } else {
    spdlog::info("Using standard names");
  }

  spdlog::info("  Log: {}", File::Log());
  spdlog::info("  Cfg: {}", File::Config());
  spdlog::info("  Var: {}", File::Vars());
  spdlog::info("   BL: {}", File::Battles());
  spdlog::info("");

#if VERSION_PATCH
  spdlog::warn("*** NOTE: Beta versions may have unexpected bugs and issues");
  spdlog::info("");
#endif

  spdlog::info("Please see https://github.com/netniv/stfc-mod for latest configuration help,");
  spdlog::info("examples and future releases, or visit the STFC Community Mod discord server");
  spdlog::info("at https://discord.gg/PrpHgs7Vjs");
  spdlog::info("");
  spdlog::info("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
  spdlog::info("");
  spdlog::info("Loading Configuration...");
  spdlog::info("");

  static auto& cfg = Config::Get();

  spdlog::info("");
  spdlog::info("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
  spdlog::info("");

  spdlog::info("Initializing code hooks:");
  const PatchEntry patches[] = {
      {"UiScaleHooks", {InstallUiScaleHooks, &cfg.installUiScaleHooks}},
      {"ZoomHooks", {InstallZoomHooks, &cfg.installZoomHooks}},
      {"BuffFixHooks", {InstallBuffFixHooks, &cfg.installBuffFixHooks}},
      {"ToastBannerHooks", {InstallToastBannerHooks, &cfg.installToastBannerHooks}},
      {"PanHooks", {InstallPanHooks, &cfg.installPanHooks}},
      {"ImproveResponsivenessHooks", {InstallImproveResponsivenessHooks, &cfg.installImproveResponsivenessHooks}},
      {"HotkeyHooks", {InstallHotkeyHooks, &cfg.installHotkeyHooks}},
#if _WIN32
      {"FreeResizeHooks", {InstallFreeResizeHooks, &cfg.installFreeResizeHooks}},
#endif
      {"TempCrashFixes", {InstallTempCrashFixes, &cfg.installTempCrashFixes}},
      {"TestPatches", {InstallTestPatches, &cfg.installTestPatches}},
      {"MiscPatches", {InstallMiscPatches, &cfg.installMiscPatches}},
      {"ChatPatches", {InstallChatPatches, &cfg.installChatPatches}},
      {"ResolutionListFix", {InstallResolutionListFix, &cfg.installResolutionListFix}},
      {"SyncPatches", {InstallSyncPatches, &cfg.installSyncPatches}},
      {"ObjectTracker", {InstallObjectTrackers, &cfg.installObjectTracker}},
  };
  printf("il2cpp_init_hook(%s)\n", domain_name);

  auto r = original(domain_name);

  auto patch_count = 0;
  auto patch_total = sizeof(patches) / sizeof(patches[0]);

  for (const auto& patch : patches) {
    patch_count++;
    const auto [patch_func, patch_enabled] = patch.fnAndEnabled;
    const auto patch_install               = (patch_enabled && *patch_enabled);
    const auto patch_mode                  = patch_install ? "+ Patch" : "x Skipp";
    spdlog::info(" {}ing {:>2} of {} ({})", patch_mode, patch_count, patch_total, patch.name);

    if (patch_install) {
      patch_func();
    }
  }

  spdlog::info("");

#if VERSION_PATCH
  spdlog::info("Installed beta version {}.{}.{} (Patch {})", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION,
               VERSION_PATCH);
#else
  spdlog::info("Installed release version {}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
#endif

  spdlog::info("");
  spdlog::info("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
  spdlog::info("");

  return r;
}

void ApplyPatches()
{
#if _WIN32
  auto assembly = LoadLibraryA("GameAssembly.dll");
#else
  char     buf[PATH_MAX];
  uint32_t bufsize = PATH_MAX;
  _NSGetExecutablePath(buf, &bufsize);

  char assembly_path[PATH_MAX];
  snprintf(assembly_path, sizeof(assembly_path), "%s/%s", dirname(buf), "../Frameworks/GameAssembly.dylib");
  printf("Loading %s\n", assembly_path);
  auto assembly = dlopen(assembly_path, RTLD_LAZY | RTLD_GLOBAL);

  init_il2cpp_pointers();
#endif

  if (assembly == nullptr) {
    spdlog::error("Failed to load GameAssembly");
    return;
  } else {
    try {
#if _WIN32
      auto n = GetProcAddress(assembly, "il2cpp_init");
#else
      auto n = dlsym(assembly, "il2cpp_init");
#endif
      printf("Got il2cpp_init %p\n", n);

      SPUD_STATIC_DETOUR(n, il2cpp_init_hook);
    } catch (...) {
      // Failed to Apply at least some patches
    }
  }
}
