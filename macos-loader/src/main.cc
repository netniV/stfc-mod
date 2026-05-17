#include "folder_manager.h"

#include <inicpp.h>

#include <cstdio>
#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <CoreServices/CoreServices.h>

#define LD_LIBRARY_PATH_ENV "DYLD_LIBRARY_PATH"
#define LD_PRELOAD_ENV "DYLD_INSERT_LIBRARIES"

static char **appArgs = NULL;

static void die(const char *op)
{
  fprintf(stderr, "cliloader Error: %s\n", op);
  exit(1);
}

int main()
{
  constexpr auto kAppPath = R"(Star Trek Fleet Command.app/Contents/MacOS/Star Trek Fleet Command)";

  auto ApplicationSupportPath =
      (char *)fm::FolderManager::pathForDirectory(fm::NSApplicationSupportDirectory, fm::NSUserDomainMask);
  auto LibraryPath = (char *)fm::FolderManager::pathForDirectory(fm::NSLibraryDirectory, fm::NSUserDomainMask);

  std::filesystem::path launcher_settings_ini_path =
      std::filesystem::path(LibraryPath) / "Preferences" / "Star Trek Fleet Command" / "launcher_settings.ini";

  std::ifstream launcher_settings_ini_stream(launcher_settings_ini_path);
  ini::IniFile  launcher_settings_ini(launcher_settings_ini_stream);

  std::filesystem::path game_install_path = launcher_settings_ini["General"]["152033..GAME_PATH"].as<std::string>();
  game_install_path /= kAppPath;
  auto game_install_path_str = game_install_path.string();

  char     buf[PATH_MAX];
  uint32_t bufsize = PATH_MAX;
  _NSGetExecutablePath(buf, &bufsize);

  std::string path = dirname(buf);

#define SETENV(_name, _value) setenv(_name, _value, 1)
  std::string ld_library_path     = path;
  const char *old_ld_library_path = getenv(LD_LIBRARY_PATH_ENV);
  if (old_ld_library_path) {
    ld_library_path += ":";
    ld_library_path += old_ld_library_path;
  }
  std::string ld_preload     = path + "/libstfc-community-mod.dylib";
  const char *old_ld_preload = getenv(LD_PRELOAD_ENV);
  if (old_ld_preload) {
    ld_preload += ":";
    ld_preload += old_ld_preload;
  }

  SETENV(LD_PRELOAD_ENV, ld_preload.c_str());
  SETENV(LD_LIBRARY_PATH_ENV, ld_library_path.c_str());

  char *const argument_list[] = {(char *)game_install_path_str.c_str(), NULL};

  if (execvp(argument_list[0], argument_list) == -1) {
    printf("Failed to launch app %d %s\n", errno, game_install_path_str.c_str());
    return -1;
  }

  return 0;
}
