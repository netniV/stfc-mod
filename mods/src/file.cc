#include "file.h"

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
  static std::filesystem::path configPath = "";

  if (configPath.empty()) {
    File::override = false;

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
      configPath     = std::filesystem::path(File::Default());
    }
  }

  return configPath;
}

const char* File::Default()
{
  static std::string cacheNameDefault = "";
  if (cacheNameDefault.empty()) {
    cacheNameDefault = std::filesystem::path(FILE_DEF_CONFIG).string();
  }

  return cacheNameDefault.c_str();
}

const char* File::Config()
{
  static std::string cacheNameConfig = "";

  if (cacheNameConfig.empty()) {
    if (File::override) {
      cacheNameConfig = File::Path().replace_extension(FILE_EXT_TOML).string();
    } else {
      cacheNameConfig = std::string(FILE_DEF_CONFIG);
    }
  }

  return cacheNameConfig.c_str();
}

const char* File::Vars()
{
  static std::string cacheNameVar = "";

  if (cacheNameVar.empty()) {
    if (File::override) {
      cacheNameVar = File::Path().replace_extension(FILE_EXT_VARS).string();
    } else {
      cacheNameVar = std::string(FILE_DEF_VARS);
    }
  }

  return cacheNameVar.c_str();
}

const char* File::Log()
{
  static std::string cacheNameLog = "";

  if (cacheNameLog.empty()) {
    if (File::override) {
      cacheNameLog = File::Path().replace_extension(FILE_EXT_LOG).string();
    } else {
      cacheNameLog = std::string(FILE_DEF_LOG);
    }
  }

  return cacheNameLog.c_str();
}

const char* File::Battles()
{
  static std::string cacheNameBattles = "";

  if (cacheNameBattles.empty()) {
    if (File::override) {
      cacheNameBattles = File::Path().replace_extension(FILE_EXT_JSON).string();
    } else {
      cacheNameBattles = std::string(FILE_DEF_BL);
    }
  }

  return cacheNameBattles.c_str();
}

std::wstring File::Title()
{
  static std::wstring cacheNameTitle = L"";
  if (cacheNameTitle.empty()) {

#ifdef _WIN32
    HWND         hwnd = Config::WindowHandle();
    std::wstring title;
    title.reserve(GetWindowTextLength(hwnd) + 1);
    title.resize(title.capacity());
    GetWindowTextW(hwnd, const_cast<WCHAR*>(title.c_str()), title.capacity());
#else
    std::wstring title = std::wstring(FILE_DEF_TITLE);
#endif

    if (File::override && !title.empty()) {
      cacheNameTitle = L"[" + File::Path().filename().replace_extension().wstring() + L"] " + title;
    }
  }

  return cacheNameTitle;
}

#if !_WIN32
std::u8string File::MakePath(std::string_view filename, bool create_dir)
{
  auto ApplicationSupportPath =
      (char*)fm::FolderManager::pathForDirectory(fm::NSApplicationSupportDirectory, fm::NSUserDomainMask);
  auto LibraryPath = (char*)fm::FolderManager::pathForDirectory(fm::NSLibraryDirectory, fm::NSUserDomainMask);

  const auto config_dir = std::filesystem::path(LibraryPath) / "Preferences" / "com.stfcmod.startrekpatch";

  if (create_dir) {
    std::error_code ec;
    std::filesystem::create_directories(config_dir, ec);
  }
  std::filesystem::path config_path = config_dir / filename;
  return config_path.u8string();
}
#else
std::string_view File::MakePath(std::string_view filename, bool create_dir)
{
  return filename;
}
#endif

void File::Init()
{
  File::Path();
}

bool File::hasCustomNames()
{
  return File::override;
}

bool File::override = false;
