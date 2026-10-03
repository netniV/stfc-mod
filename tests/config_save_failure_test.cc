#include <cerrno>
#include <cstdio>
#include <cassert>
#include <filesystem>
#if _WIN32
#include <Windows.h>
#include <Aclapi.h>
#include <sddl.h>
#include <io.h>
#else
#include <sys/stat.h>
#endif

static std::filesystem::path staging;
static void CheckPrivateStaging(std::FILE* file)
{
#if _WIN32
  PACL acl = nullptr;
  PSECURITY_DESCRIPTOR security = nullptr;
  const auto handle = file ? reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file))) : nullptr;
  const auto error = file ? GetSecurityInfo(handle, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                           nullptr, nullptr, &acl, nullptr, &security)
                          : GetNamedSecurityInfoW(const_cast<wchar_t*>(staging.c_str()), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                                  nullptr, nullptr, &acl, nullptr, &security);
  assert(error == ERROR_SUCCESS && acl && acl->AceCount == 2);
  SECURITY_DESCRIPTOR_CONTROL control;
  DWORD revision;
  assert(GetSecurityDescriptorControl(security, &control, &revision) && (control & SE_DACL_PROTECTED));
  for (DWORD i = 0; i < acl->AceCount; ++i) {
    void* entry = nullptr;
    assert(GetAce(acl, i, &entry));
    const auto* ace = static_cast<ACCESS_ALLOWED_ACE*>(entry);
    assert(ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE && !(ace->Header.AceFlags & INHERITED_ACE));
    wchar_t* sid = nullptr;
    assert(ConvertSidToStringSidW(const_cast<DWORD*>(&ace->SidStart), &sid));
    assert(std::wstring_view(sid) == L"S-1-3-4" || std::wstring_view(sid) == L"S-1-5-18");
    LocalFree(sid);
  }
  LocalFree(security);
#else
  struct stat status;
  assert((file ? ::fstat(fileno(file), &status) : ::stat(staging.c_str(), &status)) == 0);
  assert((status.st_mode & 0777) == 0600);
#endif
}

static bool failClose = false;

static std::size_t ShortWrite(const void* data, std::size_t size, std::size_t count, std::FILE* file)
{
  for (const auto& entry : std::filesystem::directory_iterator(staging.parent_path()))
    if (entry.path().filename().string().find(".tmp-") != std::string::npos)
      staging = entry.path();
  CheckPrivateStaging(file);
  const auto written = std::fwrite(data, size, failClose ? count : count / 2, file);
  CheckPrivateStaging(file);
  if (failClose)
    return written;
  errno              = ENOSPC;
  return written;
}

static int FailedClose(std::FILE* file)
{
  std::fclose(file);
  CheckPrivateStaging(nullptr);
  errno = ENOSPC;
  return EOF;
}

// Exercise the production cleanup path without adding runtime injection controls.
#define CONFIG_SAVE_WRITE ShortWrite
#define CONFIG_SAVE_CLOSE FailedClose
#include "../mods/src/config_save.cc"

#include <cassert>
#include <fstream>
#include <iostream>

int main(int argc, char** argv)
{
  assert(argc == 2);
  const std::filesystem::path root(argv[1]);
  std::filesystem::create_directories(root);
  const auto        path     = root / "settings.toml";
  staging = path;
  const std::string original = "# keep this exactly\nenabled = false\n";
  {
    std::ofstream out(path, std::ios::binary);
    out << original;
  }
#if !_WIN32
  ::umask(0022);
  std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
#endif
  for (bool closeFailure : {false, true}) {
    failClose   = closeFailure;
    bool failed = false;
    try {
      SaveConfigDocument(toml::table{{"enabled", true}}, path);
    } catch (const std::system_error&) {
      failed = true;
    }
    assert(failed);
    std::ifstream     input(path, std::ios::binary);
    const std::string actual(std::istreambuf_iterator<char>{input}, {});
    assert(actual == original);
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
      assert(entry.path() == path);
    }
  }
  std::cout << "Short-write and failed-close fixtures passed\n";
}
