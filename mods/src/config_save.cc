#include "config_save.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <stdexcept>

#if _WIN32
#include <Windows.h>
#include <sddl.h>
#include <fcntl.h>
#include <io.h>
#pragma comment(lib, "advapi32.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#endif

// Compile-time substitutions are used only by the isolated failure fixture.
#ifndef CONFIG_SAVE_WRITE
#define CONFIG_SAVE_WRITE std::fwrite
#endif
#ifndef CONFIG_SAVE_CLOSE
#define CONFIG_SAVE_CLOSE std::fclose
#endif

void SaveConfigDocument(const toml::table& config, const std::filesystem::path& path, std::string_view header)
{
  // Serialize and validate before opening any file. Values are encoded by toml++,
  // never interpolated into TOML source. Validate the header too.
  std::ostringstream output;
  output.exceptions(std::ios::badbit | std::ios::failbit);
  output << header << config;
  const auto bytes = output.str();
  (void)toml::parse(bytes);

  // weakly_canonical alone leaves a dangling final symlink unresolved.
  // Follow it before staging, preserving the former ofstream behavior.
  auto destination = std::filesystem::weakly_canonical(path);
  unsigned links = 0;
  while (std::filesystem::is_symlink(std::filesystem::symlink_status(destination))) {
    if (++links > 40)
      throw std::filesystem::filesystem_error("config symlink cycle", path,
                                              std::make_error_code(std::errc::too_many_symbolic_link_levels));
    auto target = std::filesystem::read_symlink(destination);
    destination = std::filesystem::weakly_canonical(target.is_absolute() ? target : destination.parent_path() / target);
  }
  static std::atomic<unsigned long long> sequence{0};
  auto                                   temporary = destination;
  temporary += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-"
               + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));

  // Configs may contain tokens. Protect staging at creation, before any bytes,
  // even if the destination is private beneath a more permissive directory.
  std::FILE* file = nullptr;
#if _WIN32
  PSECURITY_DESCRIPTOR security = nullptr;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
          L"D:P(A;;FA;;;OW)(A;;FA;;;SY)", SDDL_REVISION_1, &security, nullptr))
    throw std::system_error(GetLastError(), std::system_category(), "could not protect temporary config file");
  SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), security, FALSE};
  const auto handle = CreateFileW(temporary.c_str(), GENERIC_WRITE | READ_CONTROL, 0, &attributes,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  const auto create_error = GetLastError();
  LocalFree(security);
  if (handle == INVALID_HANDLE_VALUE)
    throw std::system_error(create_error, std::system_category(), "could not create temporary config file");
  const auto descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_WRONLY | _O_BINARY);
  if (descriptor == -1) {
    CloseHandle(handle);
  } else {
    file = _fdopen(descriptor, "wb");
    if (!file)
      _close(descriptor);
  }
#else
  const auto descriptor = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (descriptor == -1)
    throw std::system_error(errno, std::generic_category(), "could not create temporary config file");
  file = ::fdopen(descriptor, "wb");
  if (!file)
    ::close(descriptor);
#endif
  if (!file) {
    const auto error = errno;
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw std::system_error(error, std::generic_category(), "could not open temporary config stream");
  }

  bool replacing = false;
  try {
    if (CONFIG_SAVE_WRITE(bytes.data(), 1, bytes.size(), file) != bytes.size()) {
      throw std::system_error(errno, std::generic_category(), "could not write temporary config file");
    }
    const auto closed = CONFIG_SAVE_CLOSE(file); // Includes flushing; failure prevents replacement.
    file              = nullptr;
    if (closed != 0) {
      throw std::system_error(errno, std::generic_category(), "could not close temporary config file");
    }
#if _WIN32
    // Let Windows retain the existing file's permissions and streams. A backup
    // protects the old contents in ReplaceFile's documented partial-failure cases.
    auto backup = temporary;
    backup += ".bak";
    if (!ReplaceFileW(destination.c_str(), temporary.c_str(), backup.c_str(), 0, nullptr, nullptr)) {
      auto error = GetLastError();
      if (error == ERROR_FILE_NOT_FOUND) {
        // Missing-destination fallback: do not overwrite a file appearing before
        // this move. The caller's earlier existence check is not a create-only transaction.
        error = MoveFileExW(temporary.c_str(), destination.c_str(), 0) ? ERROR_SUCCESS : GetLastError();
      }
      if (error != ERROR_SUCCESS) {
        replacing = error == ERROR_UNABLE_TO_MOVE_REPLACEMENT || error == ERROR_UNABLE_TO_MOVE_REPLACEMENT_2;
        throw std::filesystem::filesystem_error(
            replacing ? "config replacement failed; retain temporary/backup for recovery" : "config replacement failed",
            temporary, destination, std::error_code(error, std::system_category()));
      }
    }
    std::error_code ignored;
    std::filesystem::remove(backup, ignored);
#else
    // Preserve ordinary permission bits when replacing an existing config.
    if (std::filesystem::exists(destination)) {
      std::filesystem::permissions(temporary, std::filesystem::status(destination).permissions());
    }
    std::filesystem::rename(temporary, destination);
#endif
  } catch (...) {
    if (file) {
      std::fclose(file);
    }
    std::error_code ignored;
    if (!replacing) {
      std::filesystem::remove(temporary, ignored);
    }
    throw;
  }
}
