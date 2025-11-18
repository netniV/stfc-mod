#include "file.h"
#include "windowtitle.h"

#if _WIN32
std::string ConvertWStringToString(const std::wstring& wstr)
{
  if (wstr.empty())
    return std::string();

  int         sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
  std::string str(sizeNeeded, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], sizeNeeded, nullptr, nullptr);

  return str;
}
#endif
 
std::filesystem::path File::Path()
{
  if (!File::initialized) {
    File::Init();
  }

  return configPath;
}

const char* File::Default()
{
  if (!File::initialized) {
    File::Init();
  }

  return cacheNameDefault.c_str();
}

const char* File::Config()
{
  if (!File::initialized) {
    File::Init();
  }

  return cacheNameConfig.c_str();
}

const char* File::Vars()
{
  if (!File::initialized) {
    File::Init();
  }

  return cacheNameVar.c_str();
}

const char* File::Log()
{
  if (!File::initialized) {
    File::Init();
  }

  return cacheNameLog.c_str();
}

const char* File::Battles()
{
  if (!File::initialized) {
    File::Init();
  }

  return cacheNameBattles.c_str();
}

std::wstring File::Title()
{
  if (!File::initialized) {
    File::Init();
  }

  if (cacheNameTitle.empty()) {
    std::wstring title = WindowTitle::Get();

    if (File::override && !title.empty()) {
      cacheNameTitle = L"[" + configPath.filename().replace_extension().wstring() + L"] " + title;
    }
  }

  return cacheNameTitle;
}

#if !_WIN32
std::u8string File::MakePath(std::string_view filename, bool create_dir, bool old_path)
{
  const std::filesystem::path libraryPath =
      fm::FolderManager::pathForDirectory(fm::NSLibraryDirectory, fm::NSUserDomainMask);
  const auto packageName = old_path ? "com.tashcan.startrekpatch" : "com.stfcmod.startrekpatch";
  const auto config_dir  = libraryPath / "Preferences" / packageName;

  if (create_dir) {
    std::error_code ec;
    std::filesystem::create_directories(config_dir, ec);
  }
  std::filesystem::path config_path = config_dir / filename;
  return config_path.u8string();
}
#else
std::string_view File::MakePath(std::string_view filename, bool create_dir, bool old_path)
{
  return filename;
}
#endif

void File::Init()
{
  if (!File::initialized) {
    File::override = false;

    /*******************************
     *
     * Set the default config file name
     *
     *******************************/

    cacheNameDefault = std::filesystem::path(FILE_DEF_CONFIG).string();

    /*******************************
     *
     * Check for command line argument
     * override and set config path
     *
     *******************************/
#if _WIN32
    // Get the command line
    LPCWSTR cmdLine = GetCommandLineW();

    // Parse command line into individual arguments
    int     argc;
    LPWSTR* argv = CommandLineToArgvW(cmdLine, &argc);

    // If we have some arguments, lets see what we got
    std::wstring argValue;
    if (argv != nullptr) {
      // Output the arguments (for example purposes, we'll just print them)
      for (int i = 0; i < argc - 1; ++i) {
        if (std::wstring(argv[i]) == L"-debug") {
          File::debug = true;
        }

        if (std::wstring(argv[i]) == L"-trace") {
          File::trace = true;
        }

        if (std::wstring(argv[i]) == L"-ccm" && i + 1 < argc) {
          // Found "-ccm", so take the next argument as the value
          argValue = argv[i + 1];
          break;
        }
      }

      if (!argValue.empty()) {
        File::override = true;
        configPath     = std::filesystem::path(ConvertWStringToString(argValue));
      }

      // Clean up
      LocalFree(argv);
    }
#endif

    // Second check here is because on windows, it may still be
    // unset at this point.  On the mac, we do not currently
    // support multiple configuration files
    if (configPath.empty()) {
      File::override = false;
      configPath     = std::filesystem::path(cacheNameDefault);
    }

    /*******************************
     *
     * Set the battle log file name
     *
     *******************************/
    if (File::override) {
      cacheNameBattles = configPath.replace_extension(FILE_EXT_JSON).string();
    } else {
      cacheNameBattles = std::string(FILE_DEF_BL);
    }

    /*******************************
     *
     * Set the log file name
     *
     *******************************/
    if (File::override) {
      cacheNameLog = configPath.replace_extension(FILE_EXT_LOG).string();
    } else {
      cacheNameLog = std::string(FILE_DEF_LOG);
    }

    /*******************************
     *
     * Set the vars file name
     *
     *******************************/

    if (File::override) {
      cacheNameVar = configPath.replace_extension(FILE_EXT_VARS).string();
    } else {
      cacheNameVar = std::string(FILE_DEF_VARS);
    }

    /*******************************
     *
     * Set the config file name
     *
     *******************************/
    if (File::override) {
      cacheNameConfig = configPath.replace_extension(FILE_EXT_TOML).string();
    } else {
      cacheNameConfig = std::string(FILE_DEF_CONFIG);
    }

    File::initialized = true;
  }
}

bool File::hasCustomNames()
{
  return File::override;
}

bool File::hasDebug()
{
  return File::debug;
}

bool File::hasTrace()
{
  return File::trace;
}

#ifdef _MODDBG
bool File::debug = true;
#else
bool File::debug = false;
#endif

bool File::trace       = false;
bool File::override    = false;
bool File::initialized = false;

std::wstring File::cacheNameTitle = L"";

std::string File::cacheNameBattles = "";
std::string File::cacheNameLog     = "";
std::string File::cacheNameVar     = "";
std::string File::cacheNameConfig  = "";
std::string File::cacheNameDefault = "";

std::filesystem::path File::configPath = "";
