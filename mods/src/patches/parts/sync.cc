#include "config.h"
#include "errormsg.h"
#include "file.h"
#include "str_utils.h"
#include "game_state_export.h"
#include "id_mappings.h"

#include <il2cpp-api-types.h>
#include <Digit.PrimeServer.Models.pb.h>
#include <il2cpp/il2cpp_helper.h>
#include <prime/EntityGroup.h>
#include <prime/HttpResponse.h>
#include <prime/ServiceResponse.h>
#include <prime/RealtimeDataPayload.h>
#include <spud/detour.h>

#include <spdlog/spdlog.h>
#if !__cpp_lib_format
#include <spdlog/fmt/fmt.h>
#endif

#if _WIN32
#include <rpc.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.Headers.h>
#else
#include <uuid/uuid.h>
#endif

#include <EASTL/algorithm.h>
#include <EASTL/bonus/ring_buffer.h>
#include <cpr/cpr.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <iomanip>
#include <sstream>

#ifndef STR_FORMAT
#if __cpp_lib_format
#define STR_FORMAT std::format
#else
#define STR_FORMAT fmt::format
#endif
#endif

#ifndef _WIN32
#include <time.h>
#endif

#if _WIN32
// Introduce RAII guard for COM apartments
struct WinRtApartmentGuard {
  WinRtApartmentGuard() { winrt::init_apartment(); }
  ~WinRtApartmentGuard() { winrt::uninit_apartment(); }
};
#endif


namespace http
{

namespace headers
{
  static std::string    gameServerUrl;
  static std::string    instanceSessionId;
  static int32_t        instanceId;
  static std::string    unityVersion{"6000.0.52f1"};
  static std::string    primeVersion{"1.000.48084"};
  static std::string    poweredBy{"stfc community mod/" VER_FILE_VERSION_STR};
} // namespace headers

[[nodiscard]] static std::string newUUID()
{
#ifdef _WIN32
  UUID uuid;
  UuidCreate(&uuid);

  unsigned char* str;
  UuidToStringA(&uuid, &str);

  std::string s(reinterpret_cast<char*>(str));

  RpcStringFreeA(&str);
#else
  uuid_t uuid;
  uuid_generate_random(uuid);
  char s[37];
  uuid_unparse(uuid, s);
#endif
  return s;
}

// Simple URL manipulation class to replace boost::url
class Url
{
public:
  explicit Url(std::string url) : m_url(std::move(url))
  {
    m_handle = curl_url();
    if (m_handle != nullptr) {
      curl_url_set(m_handle, CURLUPART_URL, m_url.data(), 0);
    }
  }

  ~Url()
  {
    if (m_handle != nullptr) {
      curl_url_cleanup(m_handle);
    }
  }

  // Delete copy operations to prevent double-free
  Url(const Url&) = delete;
  Url& operator=(const Url&) = delete;

  Url(Url&& other) noexcept : m_handle(other.m_handle), m_url(std::move(other.m_url))
  {
    other.m_handle = nullptr;
  }

  Url& operator=(Url&& other) noexcept
  {
    if (this != &other) {
      if (m_handle != nullptr) {
        curl_url_cleanup(m_handle);
      }

      m_handle = other.m_handle;
      m_url = std::move(other.m_url);
      other.m_handle = nullptr;
    }

    return *this;
  }
  
  void set_path(const std::string& path)
  {
    if (m_handle == nullptr) {
      return;
    }

    if (CURLUcode rc = curl_url_set(m_handle, CURLUPART_PATH, path.c_str(), 0); rc == CURLUE_OK) {
      char *url = nullptr;
      if (rc = curl_url_get(m_handle, CURLUPART_URL , &url, CURLU_PUNYCODE); rc == CURLUE_OK) {
        m_url = url;
      }

      if (url != nullptr) {
        curl_free(url);
      }
    }
  }
  
  [[nodiscard]] const char* c_str() const
  {
    return m_url.c_str();
  }
  
private:
  CURLU *m_handle;
  std::string m_url;
};

static void sync_log_error(const std::string& type, const std::string& target, const std::string& text)
{
  if (Config::Get().sync_logging) {
    spdlog::error("SYNC-{} - {}: {}", type, target, text);
  }
}

static void sync_log_warn(const std::string& type, const std::string& target, const std::string& text)
{
  if (Config::Get().sync_logging) {
    spdlog::warn("SYNC-{} - {}: {}", type, target, text);
  }
}

static void sync_log_info(const std::string& type, const std::string& target, const std::string& text)
{
  if (Config::Get().sync_logging) {
    spdlog::info("SYNC-{} - {}: {}", type, target, text);
  }
}

static void sync_log_debug(const std::string& type, const std::string& target, const std::string& text)
{
  if (Config::Get().sync_logging && Config::Get().sync_debug) {
    spdlog::debug("SYNC-{} - {}: {}", type, target, text);
  }
}

static void sync_log_trace(const std::string& type, const std::string& target, const std::string& text)
{
  if (Config::Get().sync_logging && Config::Get().sync_debug) {
    spdlog::trace("SYNC-{} - {}: {}", type, target, text);
  }
}

static constexpr std::string CURL_TYPE_UPLOAD   = "UPLOAD";
static constexpr std::string CURL_TYPE_DOWNLOAD = "DOWNLOAD";

// Per-target worker thread infrastructure
struct TargetWorker {
  TargetWorker() = default;
  TargetWorker(const TargetWorker&) = delete;
  TargetWorker& operator=(const TargetWorker&) = delete;

  using request_t = std::tuple<std::string, std::string, bool>;

  std::shared_ptr<cpr::Session> session;
  std::thread                   worker_thread;
  std::atomic_bool              stop_requested{false};
  std::queue<request_t>         request_queue;
  std::mutex                    queue_mtx;
  std::condition_variable       queue_cv;
};

static std::unordered_map<std::string, std::shared_ptr<TargetWorker>> target_workers;
static std::mutex target_workers_mtx;

static void target_worker_thread(std::shared_ptr<TargetWorker> worker)
{
#if _WIN32
  WinRtApartmentGuard apartmentGuard;
#endif

  while (!worker->stop_requested.load(std::memory_order_acquire)) {
    std::string identifier;
    std::string post_data;
    bool is_first_sync = false;

    {
      std::unique_lock lk(worker->queue_mtx);
      worker->queue_cv.wait(lk, [&worker] {
        return worker->stop_requested.load(std::memory_order_acquire) || !worker->request_queue.empty();
      });

      if (worker->stop_requested.load(std::memory_order_acquire) && worker->request_queue.empty()) {
        break;
      }

      if (!worker->request_queue.empty()) {
        auto item = std::move(worker->request_queue.front());
        worker->request_queue.pop();
        identifier = std::move(std::get<0>(item));
        post_data = std::move(std::get<1>(item));
        is_first_sync = std::get<2>(item);
      }
    }

    if (post_data.empty()) {
      continue;
    }

    // Process the request
    try {
      const auto httpClient = worker->session;
      auto& headers = httpClient->GetHeader();

      if (is_first_sync) {
        headers.insert_or_assign("X-PRIME-SYNC", "2");
        sync_log_trace(CURL_TYPE_UPLOAD, identifier, "Adding X-Prime-Sync header for initial sync");
      } else {
        headers.erase("X-PRIME-SYNC");
      }

      httpClient->SetBody(cpr::Body{post_data});

      sync_log_debug(CURL_TYPE_UPLOAD, identifier, "Sending data to " + httpClient->GetFullRequestUrl());

      // Synchronously wait for response
      const auto response = httpClient->Post();

      if (response.status_code == 0) {
        sync_log_error(CURL_TYPE_UPLOAD, identifier, "Failed to send request: " + response.error.message);
      } else if (response.status_code >= 400) {
        sync_log_error(CURL_TYPE_UPLOAD, identifier, STR_FORMAT("Failed to communicate with server: {} (after {:.1f}s)", response.status_line, response.elapsed));
      } else {
        sync_log_debug(CURL_TYPE_UPLOAD, identifier, STR_FORMAT("Response: {} ({:.1f}s elapsed)", response.status_line, response.elapsed));
      }
    } catch (const std::runtime_error& e) {
      ErrorMsg::SyncRuntime(identifier.c_str(), e);
    } catch (const std::exception& e) {
      ErrorMsg::SyncException(identifier.c_str(), e);
#if _WIN32
    } catch (winrt::hresult_error const& ex) {
      ErrorMsg::SyncWinRT(identifier.c_str(), ex);
#endif
    } catch (...) {
      ErrorMsg::SyncMsg(identifier.c_str(), "Unknown error occurred");
    }
  }
}

static std::shared_ptr<TargetWorker> get_curl_client_sync(const std::string& target)
{
  std::scoped_lock lk(target_workers_mtx);

  // Check if a worker already exists
  if (const auto it = target_workers.find(target); it != target_workers.end()) {
    return it->second;
  }

  // Create a new worker for this target
  auto worker = std::make_shared<TargetWorker>();

  // Initialize session
  worker->session = std::make_shared<cpr::Session>();
  const auto& target_config = Config::Get().sync_targets[target];

  worker->session->SetUrl(target_config.url);
  worker->session->SetUserAgent("stfc community mod " VER_FILE_VERSION_STR " (libcurl/" LIBCURL_VERSION ")");
  worker->session->SetAcceptEncoding(cpr::AcceptEncoding{});
  worker->session->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});
  worker->session->SetRedirect(cpr::Redirect{3, true, false, cpr::PostRedirectFlags::POST_ALL});

#ifndef _MODDBG
  worker->session->SetConnectTimeout(cpr::ConnectTimeout{3'000});
  worker->session->SetTimeout(cpr::Timeout{10'000});
#endif

  if (!target_config.proxy.empty()) {
    worker->session->SetProxies({{"http", target_config.proxy}, {"https", target_config.proxy}});

    if (!target_config.verify_ssl) {
      worker->session->SetSslOptions(
        cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false}, cpr::ssl::NoRevoke{true})
      );
    }
  }

  worker->session->SetHeader({
    {"Content-Type", "application/json"},
    {"X-Powered-By", headers::poweredBy},
    {"stfc-sync-token", target_config.token},
  });

  // Start the worker thread
  worker->worker_thread = std::thread(target_worker_thread, worker);
  target_workers[target] = worker;

  return worker;
}

static void send_data(SyncConfig::Type type, const std::string& post_data, bool is_first_sync)
{
  static std::once_flag emit_warning;
  const auto& targets = Config::Get().sync_targets;

  std::call_once(emit_warning, [targets] {
    if (targets.empty()) {
      sync_log_warn(CURL_TYPE_UPLOAD, "GLOBAL", "No target found, will not attempt to send");
    }
  });

  for (const auto& target : targets
       | std::views::filter([type](const auto& t) { return t.second.enabled(type); })
       | std::views::keys) {

    const auto target_identifier = STR_FORMAT("{} ({})", target, to_string(type));

    try {
      const auto worker = get_curl_client_sync(target);

      // Enqueue the request for this target's worker
      {
        std::scoped_lock lk(worker->queue_mtx);
        worker->request_queue.emplace(target_identifier, post_data, is_first_sync);
        sync_log_trace(CURL_TYPE_UPLOAD, target_identifier,
                       STR_FORMAT("Queued request (queue size: {})", worker->request_queue.size()));
      }
      worker->queue_cv.notify_all();

    } catch (const std::runtime_error& e) {
      spdlog::error("Failed to send sync data to target '{}' - Runtime error: {}", target_identifier, e.what());
    } catch (const std::exception& e) {
      spdlog::error("Failed to send sync data to target '{}' - Exception: {}", target_identifier, e.what());
    } catch (...) {
      spdlog::error("Failed to send sync data to target '{}' - Unknown error occurred", target_identifier);
    }
  }
}

static std::shared_ptr<cpr::Session> get_curl_client_scopely()
{
  static std::shared_ptr<cpr::Session> session{nullptr};
  static std::once_flag init_flag;

  // thread-safe initialization
  std::call_once(init_flag, [] {
    session = std::make_shared<cpr::Session>();
    session->SetAcceptEncoding(cpr::AcceptEncoding{});
    session->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});

    if (!Config::Get().sync_options.proxy.empty()) {
      session->SetProxies({{"https", Config::Get().sync_options.proxy}});

      if (!Config::Get().sync_options.verify_ssl) {
        session->SetSslOptions(
          cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false}, cpr::ssl::NoRevoke{true})
        );
      }
    }

    session->SetUserAgent("UnityPlayer/" + headers::unityVersion + " (UnityWebRequest/1.0, libcurl/8.10.1-DEV)");
    session->SetHeader({
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"X-TRANSACTION-ID", newUUID()},
        {"X-AUTH-SESSION-ID", headers::instanceSessionId},
        {"X-PRIME-VERSION", headers::primeVersion},
        {"X-Instance-ID", STR_FORMAT("{:03}", headers::instanceId)},
        {"X-PRIME-SYNC", "0"},
        {"X-Unity-Version", headers::unityVersion},
        {"X-Powered-By", headers::poweredBy},
    });
  });

  return session;
}

static std::string get_game_server_data(const std::string& path, const std::string& post_data);

static std::string get_scopely_data(const std::string& path, const std::string& post_data)
{
  static std::once_flag emit_warning;

  if (Config::Get().sync_targets.empty()) {
    std::call_once(emit_warning, [] {
      sync_log_warn(CURL_TYPE_UPLOAD, "GLOBAL", "No target found, will not attempt to retrieve data");
    });

    return {};
  }

  return get_game_server_data(path, post_data);
}

static std::string get_game_server_data(const std::string& path, const std::string& post_data)
{
  Url url(headers::gameServerUrl);
  url.set_path(path);

  const auto        httpClient = get_curl_client_scopely();
  static std::mutex client_mutex;

  std::string response_text;

  {
    std::scoped_lock lk(client_mutex);
    httpClient->SetUrl(url.c_str());

    auto& headers = httpClient->GetHeader();
    headers.insert_or_assign("X-TRANSACTION-ID", newUUID());
    headers.insert_or_assign("X-AUTH-SESSION-ID", headers::instanceSessionId);
    headers.insert_or_assign("X-Instance-ID", STR_FORMAT("{:03}", headers::instanceId));

    httpClient->SetBody(post_data);
    const auto response = httpClient->Post();

    if (response.status_code == 0) {
      sync_log_error(CURL_TYPE_DOWNLOAD, path, "Failed to send request: " + response.error.message);
      return {};
    }

    if (response.status_code >= 400) {
      sync_log_error(CURL_TYPE_DOWNLOAD, path, "Failed to communicate with server: " + response.status_line);
      return {};
    }

    const auto  response_headers = response.header;
    std::string type;

    try {
      type = response_headers.at("Content-Type");
    } catch (const std::out_of_range&) {
      type = "unknown";
    }

    sync_log_debug(CURL_TYPE_DOWNLOAD, path,
                   STR_FORMAT("Response: {} ({}), {:.1f}s elapsed,", response.status_line, type, response.elapsed));
    response_text = response.text;
  }

  return response_text;
}

} // namespace http

NLOHMANN_JSON_NAMESPACE_BEGIN
template <typename T> struct adl_serializer<google::protobuf::RepeatedField<T>> {
  static void to_json(json& j, const google::protobuf::RepeatedField<T>& proto)
  {
    j = json::array();

    for (const auto& v : proto) {
      j.push_back(v);
    }
  }

  static void from_json(const json& j, google::protobuf::RepeatedField<T>& proto)
  {
    if (j.is_array()) {
      for (const auto& v : j) {
        proto.Add(v.get<T>());
      }
    }
  }
};

template <> struct adl_serializer<SyncConfig::Type> {
  static void to_json(json& j, const SyncConfig::Type t)
  {
    for (const auto& opt : SyncOptions) {
      if (opt.type == t) {
        j = opt.type_str;
        return;
      }
    }

    j = nullptr;
  }

  static void from_json(const json& j, SyncConfig::Type& t)
  {
    if (j.is_string()) {
      const auto& s = j.get_ref<const std::string&>();

      for (const auto& opt : SyncOptions) {
        if (opt.type_str == s) {
          t = opt.type;
          return;
        }
      }
    }
  }
};
NLOHMANN_JSON_NAMESPACE_END

std::mutex                                                  sync_data_mtx;
std::condition_variable                                     sync_data_cv;
std::queue<std::tuple<SyncConfig::Type, std::string, bool>> sync_data_queue;

std::mutex              combat_log_data_mtx;
std::condition_variable combat_log_data_cv;
std::queue<uint64_t>    combat_log_data_queue;

struct CachedPlayerData {
  std::string                           name;
  int64_t                               alliance{-1};
  std::chrono::steady_clock::time_point expires_at;
};

std::unordered_map<std::string, CachedPlayerData> player_data_cache;
std::mutex                                        player_data_cache_mtx;

struct CachedAllianceData {
  std::string                           name;
  std::string                           tag;
  int32_t                               level        = 0;
  int32_t                               member_count = 0;
  int64_t                               power        = 0;
  std::chrono::steady_clock::time_point expires_at;
};

std::unordered_map<int64_t, CachedAllianceData> alliance_data_cache;
std::mutex                                      alliance_data_cache_mtx;

void queue_data(SyncConfig::Type type, const std::string& data, bool is_first_sync = false)
{
  {
    std::scoped_lock lk(sync_data_mtx);
    sync_data_queue.emplace(type, data, is_first_sync);
    http::sync_log_debug("QUEUE", to_string(type), "Added data to sync queue");
  }

  sync_data_cv.notify_all();
}

void queue_data(SyncConfig::Type type, const nlohmann::json& data, bool is_first_sync = false)
{
  {
    std::scoped_lock lk(sync_data_mtx);
    sync_data_queue.emplace(type, data.dump(), is_first_sync);
    http::sync_log_debug("QUEUE", to_string(type), STR_FORMAT("Added {} entries to sync queue", data.size()));
  }

  sync_data_cv.notify_all();
}

struct RankLevelState {
  explicit RankLevelState(const int32_t r = -1, const int32_t l = -1)
      : rank(r)
      , level(l)
  {
  }

  bool operator==(const RankLevelState& other) const
  {
    return this->rank == other.rank && this->level == other.level;
  }

private:
  int64_t rank  = -1;
  int64_t level = -1;
};

struct RankLevelShardsState {
  explicit RankLevelShardsState(const int32_t r = -1, const int32_t l = -1, const int32_t s = -1)
      : rank(r)
      , level(l)
      , shards(s)
  {
  }

  bool operator==(const RankLevelShardsState& other) const
  {
    return this->rank == other.rank && this->level == other.level && this->shards == other.shards;
  }

private:
  int32_t rank   = -1;
  int32_t level  = -1;
  int32_t shards = -1;
};

struct ShipState {
  explicit ShipState(const int32_t t = -1, const int32_t l = -1, const double_t lp = -1.0,
                     const std::vector<int64_t>& c = {})
      : tier(t)
      , level(l)
      , level_percentage(lp)
      , components(c)
  {
  }

  bool operator==(const ShipState& other) const
  {
    return this->tier == other.tier && this->level == other.level
           && std::fabs(this->level_percentage - other.level_percentage) < 0.01 && this->components == other.components;
  }

private:
  int32_t              tier             = -1;
  int32_t              level            = -1;
  double_t             level_percentage = -1.0;
  std::vector<int64_t> components;
};

struct pairhash {
  template <typename T, typename U> std::size_t operator()(const std::pair<T, U>& x) const
  {
    return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
  }
};

static eastl::ring_buffer<uint64_t> previously_sent_battlelogs;
static std::mutex                   previously_sent_battlelogs_mtx;

static void load_previously_sent_logs()
{
  using json = nlohmann::json;
  std::scoped_lock lk(previously_sent_battlelogs_mtx);

  previously_sent_battlelogs.set_capacity(300);

  try {
    const std::filesystem::path path(File::MakePath(File::Battles()));
    std::ifstream              file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
      spdlog::warn("Failed to open battles file (not found or not readable); starting with empty cache");
      return;
    }

    const auto battlelogs = json::parse(file);
    for (const auto& v : battlelogs) {
      previously_sent_battlelogs.push_back(v.get<uint64_t>());
    }

    spdlog::debug("Loaded {} previously sent battle logs", previously_sent_battlelogs.size());
  } catch (const std::exception& e) {
    spdlog::error("Failed to parse battles file: {}", e.what());
  } catch (...) {
    spdlog::error("Failed to parse battles file");
  }
}

static void save_previously_sent_logs()
{
  using json           = nlohmann::json;
  auto battlelog_array = json::array();

  {
    std::scoped_lock lk(previously_sent_battlelogs_mtx);
    for (auto id : previously_sent_battlelogs) {
      battlelog_array.push_back(id);
    }
  }

  try {
    const std::filesystem::path path(File::MakePath(File::Battles(), true));
    std::ofstream               file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      spdlog::error("Failed to open battles file for writing");
      return;
    }

    file << battlelog_array.dump();
    spdlog::trace("Saved {} previously sent battle logs", battlelog_array.size());

  } catch (const std::exception& e) {
    spdlog::error("Failed to save battles JSON: {}", e.what());
  } catch (...) {
    spdlog::error("Unknown error while saving battles JSON.");
  }
}

void process_starbase_detailed_scan(std::unique_ptr<std::string>&& bytes)
{
  if (auto response = Digit::PrimeServer::Models::StarbaseDetailedScanResponse(); response.ParseFromString(*bytes)) {
    if (!response.has_starbasedetailedscan()) return;
    const auto& scan = response.starbasedetailedscan();

    // Only capture our own station â€” skip scans of other players' stations
    const auto& owner_id = scan.owneruserid();
    const auto& my_id    = Config::Get().game_state_player_id;
    if (!my_id.empty() && owner_id != my_id) {
      spdlog::debug("StarbaseDetailedScan: skipping scan for owner={} (not our station)", owner_id);
      return;
    }

    int64_t expiry_epoch = 0;
    if (scan.has_playershield() && scan.playershield().has_expirytime()) {
      expiry_epoch = scan.playershield().expirytime().seconds();
    }

    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    spdlog::info("StarbaseDetailedScan: owner={} shield_expiry={} now={} active={}",
                 owner_id, expiry_epoch, now_epoch, expiry_epoch > now_epoch);

    if (Config::Get().game_state_enabled) {
      // token_count stays 0 here â€” tokens come from InventoryResponse (type=8 consumables).
      // We pass -1 to signal "don't update token count" vs a real 0.
      game_state_export::capture_peace_shield(expiry_epoch, -1);
    }
  } else {
    spdlog::debug("StarbaseDetailedScan: failed to parse (may be another message type)");
  }
}

void process_active_missions(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_set<int64_t> active_mission_states;
  static std::mutex                  active_mission_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ActiveMissionsResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "active missions",
                         STR_FORMAT("Processing {} active missions", response.activemissions_size()));

    std::unordered_set<int64_t> active_missions;
    for (const auto& mission : response.activemissions()) {
      active_missions.insert(mission.id());
    }

    bool changed = false;
    {
      std::scoped_lock lk(active_mission_states_mtx);
      if (active_mission_states != active_missions) {
        changed               = true;
        active_mission_states = std::move(active_missions);
      }
    }

    if (changed && !active_mission_states.empty()) {
      if (Config::Get().game_state_enabled) {
        std::vector<std::pair<int64_t, int64_t>> gs_missions;
        for (const auto& mission : response.activemissions()) {
          gs_missions.emplace_back(mission.id(), mission.missionid());
        }
        game_state_export::capture_missions_active(gs_missions);
      }

      auto mission_array = json::array();

      for (const auto mission : active_mission_states) {
        mission_array.push_back({{"type", "active_" + SyncConfig::Type::Missions}, {"mid", mission}});
      }

      queue_data(SyncConfig::Type::Missions, mission_array);
    }
  } else {
    spdlog::error("Failed to parse active missions");
  }
}

void process_completed_missions(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::vector<int64_t> completed_mission_states;
  static std::mutex           completed_mission_states_mtx;

  if (auto response = Digit::PrimeServer::Models::CompletedMissionsResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "completed missions",
                         STR_FORMAT("Processing {} completed missions", response.completedmissions_size()));

    const auto&          missions = response.completedmissions();
    std::vector<int64_t> completed_missions{missions.begin(), missions.end()};
    std::vector<int64_t> diff;

    // Assume the completed missions list is append-only: new entries may be added, but existing ones are never removed.
    std::vector<int64_t> completed_snapshot;
    {
      std::scoped_lock lk(completed_mission_states_mtx);
      std::ranges::set_difference(completed_missions, completed_mission_states, std::back_inserter(diff));

      if (!diff.empty()) {
        completed_mission_states = std::move(completed_missions);
        completed_snapshot       = completed_mission_states;
      }
    }

    if (!diff.empty()) {
      if (Config::Get().game_state_enabled) {
        game_state_export::capture_missions_completed(completed_snapshot);
      }

      auto mission_array = json::array();

      for (const auto mission : diff) {
        mission_array.push_back({{"type", SyncConfig::Type::Missions}, {"mid", mission}});
      }

      queue_data(SyncConfig::Type::Missions, mission_array);
    }
  } else {
    spdlog::error("Failed to parse completed missions");
  }
}

void process_player_inventories(std::unique_ptr<std::string>&& bytes)
{
  using json   = nlohmann::json;
  using item_t = std::underlying_type_t<Digit::PrimeServer::Models::InventoryItemType>;
  static std::unordered_map<std::pair<item_t, int64_t>, int64_t, pairhash> inventory_states;
  static std::mutex                                                        inventory_states_mtx;
  static std::atomic_bool                                                  is_first_sync{true};

  if (auto response = Digit::PrimeServer::Models::InventoryResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "player inventories",
                         STR_FORMAT("Processing {} inventories", response.inventories_size()));

    // Peace shield token counts are read directly from cached_resources at
    // export time using known resource IDs â€” no inventory path needed.
    // Shield expiry is captured separately via process_starbase_detailed_scan.

    auto inventory_items = json::array();
    {
      std::scoped_lock lk(inventory_states_mtx);

      for (const auto& inventory : response.inventories() | std::views::values) {
        for (const auto& item : inventory.items()) {
          if (item.has_commonparams()) {
            const auto item_id = item.commonparams().refid();
            const auto count   = item.count();
            const auto key     = std::make_pair(item.type(), item_id);

            // Route ship unlock BPs to game state export
            if (Config::Get().game_state_enabled &&
                item.type() == Digit::PrimeServer::Models::InventoryItemType::INVENTORYITEMTYPE_INVENTORYBLUEPRINT) {
              game_state_export::capture_ship_blueprints(item_id, count);
            }

            if (const auto& it = inventory_states.find(key); it == inventory_states.end() || it->second != count) {
              inventory_states[key] = count;
              inventory_items.push_back({{"type", SyncConfig::Type::Inventory},
                                         {"item_type", item.type()},
                                         {"refid", item_id},
                                         {"count", count}});
            }
          }
        }
      }
    }

    if (!inventory_items.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      queue_data(SyncConfig::Type::Inventory, inventory_items, first_sync);
    }
  } else {
    spdlog::error("Failed to parse player inventories");
  }
}

void process_research_trees_state(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int32_t> research_states;
  static std::mutex                           research_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ResearchTreesState(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "research trees state",
                         STR_FORMAT("Processing {} research projects", response.researchprojectlevels_size()));

    auto research_array = json::array();
    {
      std::scoped_lock lk(research_states_mtx);

      for (const auto& [id, level] : response.researchprojectlevels()) {
        // Capture for gamestate export
        if (Config::Get().game_state_enabled) {
          game_state_export::capture_research(id, level);
        }

        if (const auto& it = research_states.find(id); it == research_states.end() || it->second != level) {
          research_states[id] = level;
          research_array.push_back({{"type", SyncConfig::Type::Research}, {"rid", id}, {"level", level}});
        }
      }
    }

    if (!research_array.empty() && Config::Get().sync_options.research) {
      queue_data(SyncConfig::Type::Research, research_array);
    }
  } else {
    spdlog::error("Failed to parse research trees state");
  }
}

void process_officers(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<uint64_t, RankLevelShardsState> officer_states;
  static std::mutex                                         officer_states_mtx;

  if (auto response = Digit::PrimeServer::Models::OfficersResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "officers", STR_FORMAT("Processing {} officers", response.officers_size()));

    auto officers_array = json::array();
    {
      std::scoped_lock lk(officer_states_mtx);

      for (const auto& officer : response.officers()) {
        const RankLevelShardsState officer_state{officer.rankindex(), officer.level(), officer.shardcount()};

        // Capture for gamestate export
        if (Config::Get().game_state_enabled) {
          game_state_export::capture_officers(officer.id(), officer.rankindex(), officer.level(), officer.shardcount());
        }

        if (const auto& it = officer_states.find(officer.id());
            it == officer_states.end() || it->second != officer_state) {
          officer_states[officer.id()] = officer_state;
          officers_array.push_back({{"type", SyncConfig::Type::Officer},
                                    {"oid", officer.id()},
                                    {"rank", officer.rankindex()},
                                    {"level", officer.level()},
                                    {"shard_count", officer.shardcount()}});
        }
      }
    }

    if (!officers_array.empty() && Config::Get().sync_options.officer) {
      queue_data(SyncConfig::Type::Officer, officers_array);
    }
  } else {
    spdlog::error("Failed to parse officers");
  }
}

void process_forbidden_techs(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<uint64_t, RankLevelShardsState> tech_states;
  static std::mutex                                         tech_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ForbiddenTechsResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "techs",
                         STR_FORMAT("Processing {} forbidden/chaos techs", response.forbiddentechs_size()));

    auto tech_array = json::array();
    {
      std::scoped_lock lk(tech_states_mtx);

      for (const auto& tech : response.forbiddentechs()) {
        const RankLevelShardsState tech_state{tech.tier(), tech.level(), tech.shardcount()};

        if (const auto& it = tech_states.find(tech.id()); it == tech_states.end() || it->second != tech_state) {
          tech_states[tech.id()] = tech_state;
          tech_array.push_back({{"type", SyncConfig::Type::Tech},
                                {"fid", tech.id()},
                                {"tier", tech.tier()},
                                {"level", tech.level()},
                                {"shard_count", tech.shardcount()}});
        }
      }
    }

    if (!tech_array.empty()) {
      queue_data(SyncConfig::Type::Tech, tech_array);
    }
  } else {
    spdlog::error("Failed to parse forbidden techs");
  }
}

void process_active_officer_traits(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<std::pair<int64_t, int64_t>, int32_t, pairhash> trait_states;
  static std::mutex                                                         trait_states_mtx;

  if (auto response = Digit::PrimeServer::Models::OfficerTraitsResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "active officer traits",
                         STR_FORMAT("Processing {} active officer traits", response.activeofficertraits_size()));

    auto trait_array = json::array();
    {
      std::scoped_lock lk(trait_states_mtx);

      for (const auto& [officer_id, officer_traits] : response.activeofficertraits()) {
        for (const auto& trait : officer_traits.activetraits() | std::views::values) {
          const auto& key = std::make_pair(officer_id, trait.traitid());

          if (const auto& it = trait_states.find(key); it == trait_states.end() || it->second != trait.level()) {
            trait_states[key] = trait.level();
            trait_array.push_back({{"type", SyncConfig::Type::Traits},
                                   {"oid", officer_id},
                                   {"tid", trait.traitid()},
                                   {"level", trait.level()}});
          }

          // Capture for gamestate export (always, not just on change)
          if (Config::Get().game_state_enabled) {
            game_state_export::capture_officer_trait(
              static_cast<uint64_t>(officer_id),
              static_cast<uint64_t>(trait.traitid()),
              trait.level()
            );
          }
        }
      }
    }

    if (!trait_array.empty()) {
      queue_data(SyncConfig::Type::Traits, trait_array);
    }
  } else {
    spdlog::error("Failed to parse active officer traits");
  }
}

void process_blueprint_specs(std::unique_ptr<std::string>&& bytes)
{
  if (!Config::Get().game_state_enabled) return;

  if (auto response = Digit::PrimeServer::Models::StaticSyncBlueprintSpecsResponse(); response.ParseFromString(*bytes)) {
    std::vector<game_state_export::BlueprintSpecEntry> specs;
    specs.reserve(response.blueprintspecs_size());
    for (const auto& [id, spec] : response.blueprintspecs()) {
      game_state_export::BlueprintSpecEntry e;
      e.spec_id      = spec.id();
      e.name         = spec.name();
      e.parts_needed = spec.partsneeded();
      e.hull_id      = spec.outputreference();
      specs.push_back(std::move(e));
    }
    spdlog::info("GameState: process_blueprint_specs: {} specs", specs.size());
    game_state_export::capture_blueprint_specs(std::move(specs));
  } else {
    spdlog::error("Failed to parse blueprint specs");
  }
}

void process_faction_specs(std::unique_ptr<std::string>&& bytes)
{
  if (!Config::Get().game_state_enabled) return;

  if (auto response = Digit::PrimeServer::Models::StaticSyncFactionSpecsResponse(); response.ParseFromString(*bytes)) {
    std::vector<std::pair<int64_t, std::string>> specs;
    std::unordered_map<int64_t, std::string> tree_faction_map; // researchTreeId -> faction name
    specs.reserve(static_cast<size_t>(response.factionspecs_size()));
    for (const auto& [id, spec] : response.factionspecs()) {
      if (!spec.name().empty()) {
        specs.emplace_back(id, spec.name());
        // Each researchTreeId on this faction spec maps to this faction's name
        for (const auto tree_id : spec.researchtreeids())
          tree_faction_map[tree_id] = spec.name();
      }
    }
    spdlog::info("GameState: FactionSpecs: {} factions, {} research tree mappings",
                 specs.size(), tree_faction_map.size());
    game_state_export::capture_faction_specs(specs);
    game_state_export::capture_tree_faction_map(std::move(tree_faction_map));
  } else {
    spdlog::error("GameState: Failed to parse FactionSpecs");
  }
}

void process_research_specs(std::unique_ptr<std::string>&& bytes)
{
  if (!Config::Get().game_state_enabled) return;

  if (auto response = Digit::PrimeServer::Models::StaticSyncResearchTreeSpecsResponse(); response.ParseFromString(*bytes)) {
    // Get the researchTreeId -> faction_name map built from FactionSpec.researchTreeIds
    const auto tree_faction_map = game_state_export::get_research_tree_faction_map();

    // Map each researchProjectId -> faction_name via its researchTreeId
    std::unordered_map<int64_t, int64_t> research_faction_map;
    int32_t matched = 0;
    for (const auto& [project_id, project] : response.researchprojects()) {
      if (tree_faction_map.count(project.researchtreeid())) {
        // Store tree_id as proxy; actual name resolution in export via cached_tree_faction_map
        research_faction_map[project_id] = project.researchtreeid();
        ++matched;
      }
    }
    spdlog::info("GameState: ResearchSpecs: {} trees, {} projects, {} matched to faction",
                 response.researchtrees_size(), response.researchprojects_size(), matched);
    game_state_export::capture_research_specs(std::move(research_faction_map));
  } else {
    spdlog::error("GameState: Failed to parse ResearchSpecs");
  }
}

void process_consumable_specs(std::unique_ptr<std::string>&& bytes)
{
  if (!Config::Get().game_state_enabled) return;

  if (auto response = Digit::PrimeServer::Models::StaticSyncConsumableSpecsResponse(); response.ParseFromString(*bytes)) {
    std::vector<game_state_export::FavorSpecEntry> specs;
    for (const auto& s : response.spec()) {
      // Faction favors are RESEARCH_UNLOCK consumables with name pattern Consumable_{prefix}_{favor}_{tier}
      if (s.consumabletype() != Digit::PrimeServer::Models::ConsumableType::CONSUMABLETYPE_UNLOCK) continue;
      if (!s.has_consumableunlockspecparams()) continue;
      const auto& unlock = s.consumableunlockspecparams();
      if (unlock.unlocktype() != "RESEARCH_UNLOCK" || !unlock.has_researchproject()) continue;

      // Parse name: "Consumable_{prefix}_{favor_body}_{tier}"
      const std::string& name = s.name();
      static const std::string consumable_pfx = "Consumable_";
      if (name.rfind(consumable_pfx, 0) != 0) continue;
      const std::string body = name.substr(consumable_pfx.size()); // e.g. "Baj_Weapon_Damage_3"

      // Split into prefix (first segment), favor body (middle), tier (last segment)
      auto first_us = body.find('_');
      auto last_us  = body.rfind('_');
      if (first_us == std::string::npos || last_us == first_us) continue;

      const std::string faction_prefix = body.substr(0, first_us);
      const std::string tier_str       = body.substr(last_us + 1);
      const std::string favor_name     = body.substr(first_us + 1, last_us - first_us - 1);

      int32_t tier = 0;
      try { tier = std::stoi(tier_str); } catch (...) { continue; }
      if (tier <= 0) continue;

      specs.push_back({
        unlock.researchproject().researchid(),
        tier,
        faction_prefix,
        favor_name
      });
    }
    spdlog::info("GameState: ConsumableSpecs: {} faction favor specs parsed", specs.size());
    game_state_export::capture_favor_specs(specs);
  } else {
    spdlog::error("GameState: Failed to parse ConsumableSpecs");
  }
}


void process_base_ship_tier_specs(std::unique_ptr<std::string>&& bytes)
{
  if (!Config::Get().game_state_enabled) return;

  if (auto r = Digit::PrimeServer::Models::StaticSyncBaseShipTierSpecsResponse(); r.ParseFromString(*bytes)) {
    std::unordered_map<int32_t, int32_t> tier_level_caps;
    for (const auto& [tier, spec] : r.baseshiptierspecs())
      tier_level_caps[tier] = spec.maxshiplevel();

    // hull_tier_durations comes from ShipTierSpecs; pass empty map here â€” combined in process_ship_tier_specs
    spdlog::info("GameState: BaseShipTierSpecs: {} tiers parsed", tier_level_caps.size());
    // Store just level caps â€” hull durations and cargo stats handled by process_ship_tier_specs
    game_state_export::capture_ship_tier_specs(tier_level_caps, {}, {});
  } else {
    spdlog::error("GameState: Failed to parse BaseShipTierSpecs");
  }
}

void process_ship_tier_specs(std::unique_ptr<std::string>&& bytes)
{
  if (!Config::Get().game_state_enabled) return;

  if (auto r = Digit::PrimeServer::Models::StaticSyncShipTierSpecsResponse(); r.ParseFromString(*bytes)) {
    std::unordered_map<int64_t, int32_t> hull_durations;
    std::unordered_map<int64_t, std::unordered_map<int32_t, game_state_export::CargoStats>> hull_cargo;

    for (const auto& [hull_id, spec] : r.shiptierspecs()) {
      const int32_t dur = static_cast<int32_t>(spec.costdurationfactor());
      if (dur > 0)
        hull_durations[hull_id] = dur;

      for (const auto& [tier_key, tier_mod] : spec.tierstatmodifiers()) {
        const int32_t tier = tier_mod.tier();
        game_state_export::CargoStats stats;
        for (const auto& [stat_code, value] : tier_mod.statmodifiers()) {
          if (stat_code == 66) stats.capacity   = value; // CLIENTMODIFIERTYPE_MODCARGOCAPACITY
          if (stat_code == 67) stats.protection = value; // CLIENTMODIFIERTYPE_MODCARGOPROTECTION
        }
        if (stats.capacity > 0.f || stats.protection > 0.f)
          hull_cargo[hull_id][tier] = stats;
      }
    }
    spdlog::info("GameState: ShipTierSpecs: {} hull durations, {} hulls with cargo stats",
                 hull_durations.size(), hull_cargo.size());
    game_state_export::capture_ship_tier_specs({}, hull_durations, hull_cargo);
  } else {
    spdlog::error("GameState: Failed to parse ShipTierSpecs");
  }
}

void process_ship_bonus_buff_specs(std::unique_ptr<std::string>&& bytes)
{
  if (!Config::Get().game_state_enabled) return;

  if (auto response = Digit::PrimeServer::Models::StaticSyncShipBonusSpecsResponse(); response.ParseFromString(*bytes)) {
    std::vector<game_state_export::BuffSpecEntry> specs;
    specs.reserve(static_cast<size_t>(response.shipbonusspecs_size()));
    for (const auto& [id, spec] : response.shipbonusspecs()) {
      game_state_export::BuffSpecEntry entry;
      entry.buff_id       = spec.buffid();
      entry.modifier_code = spec.modifiercode();
      entry.operation     = static_cast<int32_t>(spec.op());
      entry.faction_id    = spec.has_attributes() ? spec.attributes().factionid() : 0;
      entry.show_percentage = spec.showpercentage();
      for (const auto v : spec.rankedbuffvalues())
        entry.ranked_values.push_back(v);
      if (entry.ranked_values.empty()) {
        for (const auto v : spec.rankedvalues())
          entry.ranked_values.push_back(static_cast<double>(v));
      }
      specs.push_back(std::move(entry));
    }
    spdlog::info("GameState: ShipBonusBuffSpecs: {} buff specs", specs.size());
    game_state_export::capture_buff_specs(specs);
  } else {
    spdlog::error("GameState: Failed to parse ShipBonusBuffSpecs");
  }
}

void process_territory_static_data(std::unique_ptr<std::string>&& bytes)
{
  if (!Config::Get().game_state_enabled) return;

  if (auto r = Digit::PrimeServer::Models::TerritoriesStaticDataResponse(); r.ParseFromString(*bytes)) {
    std::unordered_map<int64_t, game_state_export::TerritorySpec> specs;
    for (const auto& [tid, t] : r.territories()) {
      game_state_export::TerritorySpec spec;
      spec.territory_id = tid;
      spec.tier         = t.tier();
      for (const auto& p : t.takeoverperiods()) {
        spec.takeover_windows.push_back({
          .weekday      = p.weekday(),
          .start_hour   = p.starthour(),
          .duration_mins = p.duration(),
        });
      }
      for (int64_t nid : t.nodeids()) {
        spec.node_ids.push_back(nid);
      }
      specs[tid] = std::move(spec);
    }
    spdlog::info("GameState: TerritoryStaticData: {} territory specs", specs.size());
    game_state_export::capture_territory_specs(std::move(specs));
  } else {
    spdlog::error("GameState: Failed to parse TerritoryStaticData");
  }
}

void process_territory_alliance_slots(std::unique_ptr<std::string>&& bytes)
{
  if (!Config::Get().game_state_enabled) return;

  if (auto r = Digit::PrimeServer::Models::AllianceTerritorySlotsResponse(); r.ParseFromString(*bytes)) {
    const auto& slots = r.allianceterritoryslots();
    std::vector<game_state_export::TerritorySlot> held;
    for (const auto& [slot_idx, slot] : slots.assignedslots()) {
      using State = Digit::PrimeServer::Models::AllianceAssignedTerritorySlot_TerritorySlotState;
      held.push_back({
        .territory_id = slot.territoryid(),
        .state        = (slot.state() == State::AllianceAssignedTerritorySlot_TerritorySlotState_TERRITORYSLOTSTATE_OWNED)
                          ? "owned" : "takeover",
      });
    }
    spdlog::info("GameState: TerritoryAllianceSlots: {} held / {} total slots",
                 held.size(), slots.slotsize());
    game_state_export::capture_territory_slots(std::move(held), static_cast<int32_t>(slots.slotsize()));
  } else {
    spdlog::error("GameState: Failed to parse TerritoryAllianceSlots");
  }
}

void process_loyalty_specs(std::unique_ptr<std::string>&& bytes)
{
  if (!Config::Get().game_state_enabled) return;

  if (auto response = Digit::PrimeServer::Models::StaticSyncLoyaltyResponse(); response.ParseFromString(*bytes)) {
    const auto& specs = response.loyaltyspecs();

    // This is the global Syndicate Loyalty track (Resource_Loyalty_Points).
    // All tiers belong to the same single track â€” not per-faction.
    std::vector<game_state_export::LoyaltyBuffEntry> entries;
    std::vector<int64_t>                             buff_ids;

    const int32_t max_tiers = specs.loyaltytiers_size();
    for (const auto& tier : specs.loyaltytiers()) {
      for (const auto& collection : tier.buffrewardcollections()) {
        for (const auto& reward : collection.buffrewards()) {
          entries.push_back({"Syndicate", tier.tier(), max_tiers});
          buff_ids.push_back(reward.buffid());
        }
      }
    }

    if (!buff_ids.empty()) {
      spdlog::info("GameState: LoyaltySpecs (Syndicate) tiers={} buffs={}", max_tiers, buff_ids.size());
      game_state_export::capture_loyalty_specs(entries, buff_ids);
    }
  } else {
    spdlog::error("GameState: Failed to parse LoyaltySpecs");
  }
}

void process_global_active_buffs(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, std::pair<int32_t, int64_t>> buff_states;
  static std::mutex                                               buff_states_mtx;
  static std::atomic_bool                                         is_first_sync{true};


  if (auto response = Digit::PrimeServer::Models::GlobalActiveBuffsResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "global buffs",
                         STR_FORMAT("Processing {} active buffs", response.globalactivebuffs_size()));

    auto buff_array = json::array();
    {
      std::scoped_lock lk(buff_states_mtx);

      // Track all buff ids present in the response to detect removals.
      std::unordered_set<int64_t> present_ids;
      present_ids.reserve(static_cast<size_t>(response.globalactivebuffs_size()));

      for (const auto& buff : response.globalactivebuffs()) {
        present_ids.insert(buff.buffid());
        const bool expires = buff.has_activebuff() && buff.activebuff().has_expirytime();
        const auto state   = std::make_pair(buff.level(), expires ? buff.activebuff().expirytime().seconds() : -1);

        if (const auto& it = buff_states.find(buff.buffid()); it == buff_states.end() || it->second != state) {
          buff_states[buff.buffid()] = state;
          buff_array.push_back({{"type", SyncConfig::Type::Buffs},
                                {"bid", buff.buffid()},
                                {"level", state.first},
                                {"expiry_time", expires ? json(state.second) : json(nullptr)}});
        }
      }

      // Remove buffs that are no longer present and record each removal.
      for (auto it = buff_states.begin(); it != buff_states.end(); ) {
        if (!present_ids.contains(it->first)) {
          buff_array.push_back({
            {"type", "expired_" + SyncConfig::Type::Buffs},
            {"bid", it->first},
          });
          it = buff_states.erase(it);
        } else {
          ++it;
        }
      }
    }

    if (!buff_array.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      queue_data(SyncConfig::Type::Buffs, buff_array, first_sync);
    }

    // Capture full buff snapshot for faction favors cross-reference
    if (Config::Get().game_state_enabled) {
      std::vector<std::pair<int64_t, int32_t>> all_buffs;
      all_buffs.reserve(static_cast<size_t>(response.globalactivebuffs_size()));
      for (const auto& buff : response.globalactivebuffs()) {
        all_buffs.emplace_back(buff.buffid(), buff.level());
      }
      game_state_export::capture_active_buffs(all_buffs);
    }
  } else {
    spdlog::error("Failed to parse global active buffs");
  }
}

static std::unordered_map<int64_t, int64_t> slot_states;
static std::mutex                           slot_states_mtx;

inline std::optional<std::chrono::time_point<std::chrono::system_clock>> parse_timestamp(const std::string& timestamp)
{
#ifdef _WIN32
  std::istringstream ss(timestamp);
  std::chrono::system_clock::time_point time_point;

  if (!std::chrono::from_stream(ss, "%Y-%m-%dT%H:%M:%S", time_point)) {
    spdlog::error("Failed to parse timestamp: {}", timestamp);
    return std::nullopt;
  }

  return time_point;
#else
  std::tm tm = {};
  if (strptime(timestamp.c_str(), "%Y-%m-%dT%H:%M:%S", &tm) == nullptr) {
    spdlog::error("Failed to parse timestamp: {}", timestamp);
    return std::nullopt;
  }

  return std::chrono::system_clock::from_time_t(std::mktime(&tm));
#endif
}

void process_single_slot(const Digit::PrimeServer::Models::EntitySlot& slot, nlohmann::json& slot_array)
{
  using json = nlohmann::json;

  json    slot_params;
  int64_t state_value = slot.has_slotitemid() ? slot.slotitemid().value() : -1;

  switch (slot.slottype()) {
    case Digit::PrimeServer::Models::SLOTTYPE_CONSUMABLE:
      if (slot.has_consumableslotparams()) {
        const auto& consumable = slot.consumableslotparams();
        slot_params["expiry_time"] =
            consumable.has_expirytime() ? json(consumable.expirytime().seconds()) : json(nullptr);
      }
      break;
    case Digit::PrimeServer::Models::SLOTTYPE_OFFICERPRESET:
      if (slot.has_officerpresetslotparams()) {
        const auto& preset = slot.officerpresetslotparams();
        slot_params = {{"name", preset.name()}, {"order", preset.order()}, {"officer_ids", preset.officerids()}};
        state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
      }
      break;
    case Digit::PrimeServer::Models::SLOTTYPE_FLEETCOMMANDER:
      if (slot.has_fleetcommanderslotparams()) {
        slot_params["order"] = slot.fleetcommanderslotparams().order();
      }
      break;
    case Digit::PrimeServer::Models::SLOTTYPE_SELECTABLESKILL:
      if (slot.has_selectableskillslotparams()) {
        const auto& skill = slot.selectableskillslotparams();
        slot_params["cooldown_expiration"] =
            skill.has_cooldownexpiration() ? json(skill.cooldownexpiration().seconds()) : json(nullptr);
      }
      break;
    case Digit::PrimeServer::Models::SLOTTYPE_FORBIDDENTECH:
      if (slot.has_forbiddentechslotparams()) {
        const auto& tech = slot.forbiddentechslotparams();
        slot_params      = {{"owner_id", tech.ownerid()}, {"tier", tech.tier()}, {"level", tech.level()}};
        state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
      }
      break;
    case Digit::PrimeServer::Models::SLOTTYPE_FLEETPRESET:
      if (slot.has_fleetpresetslotparams()) {
        const auto& preset     = slot.fleetpresetslotparams();
        auto        setup_json = json::array();

        for (const auto& setup : preset.setups()) {
          setup_json.push_back({{"drydock_id", setup.drydockid()},
                                {"ship_id", setup.shipids()[0]},
                                {"officer_ids", setup.officerids()}});
        }

        slot_params = {{"name", preset.name()}, {"order", preset.order()}, {"setup", setup_json}};
        state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
      }
      break;
    default:
      return;
  }

  if (const auto& it = slot_states.find(slot.id()); it == slot_states.end() || it->second != state_value) {
    slot_states[slot.id()] = state_value;
    slot_array.push_back({{"type", SyncConfig::Type::Slots},
                          {"sid", slot.id()},
                          {"slot_type", slot.slottype()},
                          {"spec_id", slot.slotspecid()},
                          {"item_id", slot.has_slotitemid() ? json(slot.slotitemid().value()) : json(nullptr)},
                          {"params", slot_params}});
  }
}

void process_entity_slots(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;

  if (auto response = Digit::PrimeServer::Models::EntitySlots(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "entity slots", STR_FORMAT("Processing {} slots", response.entityslots__size()));

    // Capture drydock assignments for gamestate export
    if (Config::Get().game_state_enabled) {
      std::vector<game_state_export::DrydockEntry> assignments;
      for (const auto& slot : response.entityslots_()) {
        spdlog::debug("EntitySlots: slot type={} id={} has_fleetpreset={}",
                      static_cast<int>(slot.slottype()), slot.id(),
                      slot.has_fleetpresetslotparams());
        if (slot.slottype() == Digit::PrimeServer::Models::SLOTTYPE_FLEETPRESET &&
            slot.has_fleetpresetslotparams()) {
          for (const auto& setup : slot.fleetpresetslotparams().setups()) {
            spdlog::info("EntitySlots: FLEETPRESET drydock_id={} ship_count={}",
                         setup.drydockid(), setup.shipids_size());
            if (setup.drydockid() > 0 && !setup.shipids().empty()) {
              game_state_export::DrydockEntry e;
              e.drydock_id = setup.drydockid();
              e.ship_id    = setup.shipids(0);
              assignments.push_back(e);
            }
          }
        }
      }
      if (!assignments.empty()) {
        game_state_export::capture_drydock_assignments(assignments);
      }
    }

    auto slot_array = json::array();
    {
      std::scoped_lock lk(slot_states_mtx);

      for (const auto& slot : response.entityslots_()) {
        process_single_slot(slot, slot_array);
      }
    }

    if (!slot_array.empty()) {
      queue_data(SyncConfig::Type::Slots, slot_array);
    }
  } else {
    spdlog::error("Failed to parse entity slots");
  }
}

void process_entity_slots_data(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;

  if (auto response = Digit::PrimeServer::Models::EntitySlotsData(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "entity slots data", STR_FORMAT("Processing {} slots", response.entityslots_size()));

    // Capture drydock assignments for gamestate export
    if (Config::Get().game_state_enabled) {
      std::vector<game_state_export::DrydockEntry> assignments;
      for (const auto& slots : response.entityslots()) {
        spdlog::debug("EntitySlotsData: entity_type={} slot_count={}",
                      static_cast<int>(slots.entitytype()), slots.slots_size());
        for (const auto& slot : slots.slots()) {
          spdlog::debug("EntitySlotsData: slot type={} id={} has_fleetpreset={}",
                        static_cast<int>(slot.slottype()), slot.id(),
                        slot.has_fleetpresetslotparams());
          if (slot.slottype() == Digit::PrimeServer::Models::SLOTTYPE_FLEETPRESET &&
              slot.has_fleetpresetslotparams()) {
            for (const auto& setup : slot.fleetpresetslotparams().setups()) {
              spdlog::info("EntitySlotsData: FLEETPRESET drydock_id={} ship_count={}",
                           setup.drydockid(), setup.shipids_size());
              if (setup.drydockid() > 0 && !setup.shipids().empty()) {
                game_state_export::DrydockEntry e;
                e.drydock_id = setup.drydockid();
                e.ship_id    = setup.shipids(0);
                assignments.push_back(e);
              }
            }
          }
        }
      }
      if (!assignments.empty()) {
        game_state_export::capture_drydock_assignments(assignments);
      }
    }

    auto slot_array = json::array();
    {
      std::scoped_lock lk(slot_states_mtx);

      for (const auto& slots : response.entityslots()) {
        http::sync_log_trace("PROCESS", "entity slots data", STR_FORMAT("Processing {} slots for entity {}", slots.slots_size(), static_cast<int32_t>(slots.entitytype())));

        for (const auto& slot : slots.slots()) {
          process_single_slot(slot, slot_array);
        }
      }
    }

    if (!slot_array.empty()) {
      queue_data(SyncConfig::Type::Slots, slot_array);
    }
  } else {
    spdlog::error("Failed to parse entity slots data");
  }
}

void process_entity_slots_rtc(std::unique_ptr<std::string>&& json_payload)
{
  using json = nlohmann::json;

  try {
    auto data = json::parse(*json_payload);
    http::sync_log_trace("PROCESS", "entity slots (RTC)", "Processing entity slot update");

    const auto item = data["slot_item_id"];
    const auto item_id = item.is_null() ? -1 : item.get<int64_t>();

    json    slot_params;
    int64_t state_value = item_id;

    const auto type = data["slot_type"].get<int32_t>();
    switch (type) {
      case Digit::PrimeServer::Models::SLOTTYPE_CONSUMABLE:
        if (const auto& expiry_time = data["consumable_slot_params"]["expiry_time"]; expiry_time.is_null()) {
          slot_params["expiry_time"] = nullptr;
        } else {
          const auto timestamp = parse_timestamp(data["consumable_slot_params"]["expiry_time"].get_ref<const std::string&>());
          if (timestamp.has_value()) {
            slot_params["expiry_time"] = timestamp.value().time_since_epoch().count();
          } else {
            slot_params["expiry_time"] = nullptr;
          }
        }
        break;
      case Digit::PrimeServer::Models::SLOTTYPE_OFFICERPRESET:
        if (const auto& preset = data["officer_preset_slot_params"]; !preset.is_null()) {
          slot_params = preset;
          state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
        }
        break;
      case Digit::PrimeServer::Models::SLOTTYPE_FLEETCOMMANDER:
        if (const auto& params = data["fleet_commander_slot_params"]; !params.is_null()) {
          slot_params["order"] = params["order"];
        }
        break;
      case Digit::PrimeServer::Models::SLOTTYPE_SELECTABLESKILL:
        if (const auto& params = data["selectable_skill_slot_params"]; !params.is_null()) {
          if (const auto& expiry_time = params["cooldown_expiration"]; expiry_time.is_null()) {
            slot_params["cooldown_expiration"] = nullptr;
          } else {
            const auto timestamp = parse_timestamp(params["cooldown_expiration"].get_ref<const std::string&>());
            if (timestamp.has_value()) {
              slot_params["cooldown_expiration"] = timestamp.value().time_since_epoch().count();
            } else {
              slot_params["cooldown_expiration"] = nullptr;
            }
          }
        }
        break;
      case Digit::PrimeServer::Models::SLOTTYPE_FORBIDDENTECH:
        if (const auto& tech = data["forbidden_tech_slot_params"]; !tech.is_null()) {
          slot_params = tech;
          state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
        }
        break;
      case Digit::PrimeServer::Models::SLOTTYPE_FLEETPRESET:
        if (const auto& params = data["fleet_preset_slot_params"]; !params.is_null()) {
          auto setup_json = json::array();
          for (const auto& setup : params["setups"]) {
            setup_json.push_back({{"drydock_id", setup["d"]}, {"ship_id", setup["s"][0]}, {"officer_ids", setup["o"]}});
          }

          slot_params = {{"name", params["name"]}, {"order", params["order"]}, {"setup", setup_json}};
          state_value = static_cast<int64_t>(std::hash<json>{}(slot_params));
        }
        break;
      default:
        return;
    }

    const auto id = data["slot_id"].get<int64_t>();
    {
      std::scoped_lock lk(slot_states_mtx);

      if (const auto& it = slot_states.find(id); it == slot_states.end() || it->second != state_value) {
        slot_states[id] = state_value;

        auto slot_array = json::array();
        slot_array.push_back({{"type", SyncConfig::Type::Slots},
                              {"sid", id},
                              {"slot_type", type},
                              {"spec_id", data["slot_spec_id"]},
                              {"item_id", item},
                              {"params", slot_params}});

        queue_data(SyncConfig::Type::Slots, slot_array);
      }
    }
  } catch (json::exception& e) {
    spdlog::error("Failed to parse slots JSON: {}", e.what());
  }
}

void process_jobs(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_set<std::string> jobs_active;
  static std::mutex                      jobs_active_mtx;
  static std::atomic_bool                is_first_sync{true};

  if (auto response = Digit::PrimeServer::Models::JobResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "jobs", STR_FORMAT("Processing {} jobs", response.jobs_size()));

    std::unordered_set<std::string> uuids_in_response;
    uuids_in_response.reserve(response.jobs_size());
    auto job_array = json::array();

    for (const auto& job : response.jobs()) {
      const std::string& uuid = job.uuid();
      uuids_in_response.insert(uuid);

      bool emit = false;
      {
        std::scoped_lock lk(jobs_active_mtx);
        emit = jobs_active.insert(uuid).second;
      }

      if (!emit) {
        continue;
      }

      json job_params;

      switch (job.type()) {
        case Digit::PrimeServer::Models::JOBTYPE_RESEARCH: {
          const auto& research = job.researchparams();
          job_params           = {{"rid", research.projectid()}, {"level", research.level()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_STARBASECONSTRUCTION: {
          const auto& construction = job.starbaseconstructionparams();
          job_params               = {{"bid", construction.moduleid()}, {"level", construction.level()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_SHIPTIERUP: {
          const auto& upgrade = job.tierupshipparams();
          job_params          = {{"psid", upgrade.shipid()}, {"tier", upgrade.newtier()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_SHIPSCRAP: {
          const auto& scrap = job.scrapyardparams();
          job_params        = {{"psid", scrap.shipid()}, {"hull_id", scrap.hullid()}, {"level", scrap.level()}};
        } break;
        default:
          continue;
      }

      json job_data = json::object({{"type", SyncConfig::Type::Jobs},
                                    {"job_type", job.type()},
                                    {"uuid", job.uuid()},
                                    {"start_time", job.starttime().seconds()},
                                    {"duration", job.duration()},
                                    {"reduction", job.reductioninseconds()}});

      job_data.update(job_params);
      job_array.push_back(std::move(job_data));
    }

    // Prune entries that are no longer present to prevent unbounded growth
    {
      std::scoped_lock lk(jobs_active_mtx);
      for (auto it = jobs_active.begin(); it != jobs_active.end();) {
        if (!uuids_in_response.contains(*it)) {
          job_array.push_back({
            {"type", "completed_" + SyncConfig::Type::Jobs},
            {"uuid", *it}
          });
          it = jobs_active.erase(it);
        } else {
          ++it;
        }
      }
    }

    if (!job_array.empty()) {
      const bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
      queue_data(SyncConfig::Type::Jobs, job_array, first_sync);
    }

    // Game state queue snapshot — always replace the full snapshot so that
    // completed jobs are cleared even when the UUID-based legacy emit skips them.
    if (Config::Get().game_state_enabled) {
      std::vector<game_state_export::QueueJobEntry> queue_jobs;
      for (const auto& job : response.jobs()) {
        game_state_export::QueueJobEntry e;
        e.start_epoch    = job.has_starttime() ? static_cast<int64_t>(job.starttime().seconds()) : 0;
        e.duration_secs  = job.duration();
        e.reduction_secs = job.reductioninseconds();

        switch (job.type()) {
          case Digit::PrimeServer::Models::JOBTYPE_RESEARCH:
            e.job_type = "research";
            e.ref_id   = job.researchparams().projectid();
            e.level    = job.researchparams().level();
            break;
          case Digit::PrimeServer::Models::JOBTYPE_STARBASECONSTRUCTION:
            e.job_type = "build";
            e.ref_id   = job.starbaseconstructionparams().moduleid();
            e.level    = job.starbaseconstructionparams().level();
            break;
          case Digit::PrimeServer::Models::JOBTYPE_SHIPSCRAP:
            e.job_type = "scrap";
            e.ref_id   = job.scrapyardparams().hullid();
            e.ref_id2  = job.scrapyardparams().shipid();
            e.level    = job.scrapyardparams().level();
            break;
          case Digit::PrimeServer::Models::JOBTYPE_REPAIRFLEET:
            e.job_type = "repair";
            e.ref_id   = job.repairfleetparams().fleetid();
            break;
          default:
            continue;
        }
        queue_jobs.push_back(std::move(e));
      }
      game_state_export::capture_queues(std::move(queue_jobs));
    }
  } else {
    spdlog::error("Failed to parse jobs");
  }
}

void process_alliance_games_props(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::atomic_int32_t emerald_chain_level{-1};

  if (auto response = Digit::PrimeServer::Models::AllianceGamePropertiesResponse(); response.ParseFromString(*bytes)) {

    for (const auto& prop : response.properties()) {
      if (prop.propertyname() == "claimed_loyalty_tiers") {
        const auto& claimed_ec_levels = prop.valuelist();
        const auto& max_element =
            std::ranges::max_element(claimed_ec_levels, {}, [](const auto& level) { return std::stoi(level); });
        const auto ec_level = max_element != claimed_ec_levels.end() ? std::stoi(*max_element) : -1;

        int32_t current_level = emerald_chain_level.load();
        if (ec_level != current_level && emerald_chain_level.compare_exchange_strong(current_level, ec_level)) {
          auto ag_array = json::array();
          ag_array.push_back({{"type", SyncConfig::Type::EmeraldChain}, {"level", ec_level}});
          queue_data(SyncConfig::Type::EmeraldChain, ag_array);
        }

        break;
      }
    }
  } else {
    spdlog::error("Failed to parse alliance games properties");
  }
}

void process_battle_headers(const nlohmann::json& section)
{
  http::sync_log_trace("PROCESS", "battle headers", STR_FORMAT("Processing {} battle headers", section.size()));

  std::vector<uint64_t> battle_ids;
  battle_ids.reserve(section.size());

  for (const auto& battle : section) {
    const auto id = battle["id"].get<uint64_t>();
    battle_ids.push_back(id);
  }

  std::vector<uint64_t> to_enqueue;
  {
    std::scoped_lock lk(previously_sent_battlelogs_mtx);

    for (const auto id : battle_ids | std::views::reverse) {
      if (eastl::find(previously_sent_battlelogs.begin(), previously_sent_battlelogs.end(), id)
          == previously_sent_battlelogs.end()) {
        previously_sent_battlelogs.push_back(id);
        to_enqueue.push_back(id);
      }
    }
  }

  if (!to_enqueue.empty()) {
    http::sync_log_trace("PROCESS", "battle headers",
                         STR_FORMAT("Queuing {} battles for background processing", to_enqueue.size()));

    {
      std::scoped_lock lk(combat_log_data_mtx);
      for (const auto id : to_enqueue) {
        combat_log_data_queue.push(id);
      }
    }

    save_previously_sent_logs();
    combat_log_data_cv.notify_all();
  }
}

void process_resources(const nlohmann::json& section)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int64_t> resource_states;
  static std::mutex                           resource_states_mtx;
  static std::atomic_bool                     is_first_sync{true};

  http::sync_log_trace("PROCESS", "resources", STR_FORMAT("Processing {} resources", section.size()));
  spdlog::info("GameState: Processing {} resources", section.size());

  // Capture for gamestate export
  if (Config::Get().game_state_enabled) {
    game_state_export::capture_resources(section);
  }

  auto resource_array = json::array();
  {
    std::scoped_lock lk(resource_states_mtx);

    for (const auto& [str_id, resource] : section.get<json::object_t>()) {
      auto id     = std::stoll(str_id);
      auto amount = resource["current_amount"].get<int64_t>();

      if (const auto& it = resource_states.find(id); it == resource_states.end() || it->second != amount) {
        resource_states[id] = amount;
        resource_array.push_back({{"type", SyncConfig::Type::Resources}, {"rid", id}, {"amount", amount}});
      }
    }
  }

  if (!resource_array.empty() && Config::Get().sync_options.resources) {
    bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
    queue_data(SyncConfig::Type::Resources, resource_array, first_sync);
  }
}

void process_starbase_modules(const nlohmann::json& section)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int32_t> module_states;
  static std::mutex                           module_states_mtx;

  http::sync_log_trace("PROCESS", "starbase modules", STR_FORMAT("Processing {} buildings", section.size()));
  spdlog::info("GameState: Processing {} starbase modules (buildings)", section.size());

  // Capture for gamestate export
  if (Config::Get().game_state_enabled) {
    game_state_export::capture_buildings(section);
    spdlog::info("GameState: Captured {} buildings for export", section.size());
  }

  auto starbase_array = json::array();
  {
    std::scoped_lock lk(module_states_mtx);

    for (const auto& module : section.get<json::object_t>() | std::views::values) {
      const auto id    = module["id"].get<int64_t>();
      const auto level = module["level"].get<int32_t>();

      if (const auto& it = module_states.find(id); it == module_states.end() || it->second != level) {
        module_states[id] = level;
        starbase_array.push_back({{"type", SyncConfig::Type::Buildings}, {"bid", id}, {"level", level}});
      }
    }
  }

  if (!starbase_array.empty() && Config::Get().sync_options.buildings) {
    queue_data(SyncConfig::Type::Buildings, starbase_array);
  }
}

void process_ships(const nlohmann::json& section)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, ShipState> ship_states;
  static std::mutex                             ship_states_mtx;
  static std::atomic_bool                       is_first_sync{true};

  http::sync_log_trace("PROCESS", "ships", STR_FORMAT("Processing {} ships", section.size()));
  spdlog::info("GameState: Processing {} ships", section.size());

  // Capture for gamestate export
  if (Config::Get().game_state_enabled) {
    game_state_export::capture_ships(section);
    spdlog::info("GameState: Captured {} ships for export", section.size());
  }

  auto ship_array = json::array();
  {
    std::scoped_lock lk(ship_states_mtx);

    for (const auto& ship : section.get<json::object_t>() | std::views::values) {
      const auto      id               = ship["id"].get<int64_t>();
      const auto      tier             = ship["tier"].get<int32_t>();
      const auto      level            = ship["level"].get<int32_t>();
      const auto      level_percentage = ship["level_percentage"].get<double_t>();
      const auto      components       = ship["components"].get<std::vector<int64_t>>();
      const ShipState state{tier, level, level_percentage, components};

      if (const auto& it = ship_states.find(id); it == ship_states.end() || it->second != state) {
        ship_states[id] = state;
        ship_array.push_back({{"type", SyncConfig::Type::Ships},
                              {"psid", id},
                              {"level", level},
                              {"level_percentage", level_percentage},
                              {"tier", tier},
                              {"hull_id", ship["hull_id"].get<int64_t>()},
                              {"components", components}});
      }
    }
  }

  if (!ship_array.empty() && Config::Get().sync_options.ships) {
    bool first_sync = is_first_sync.exchange(false, std::memory_order_acq_rel);
    queue_data(SyncConfig::Type::Ships, ship_array, first_sync);
  }
}

void process_json(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;

  try {
    const auto result = json::parse(bytes->begin(), bytes->end());

    const bool export_gs = Config::Get().game_state_enabled;

    // First pass: build ship_id -> status map from my_deployed_fleets so it is
    // available when we process the fleets key (key order is not guaranteed).
    struct FleetStatus { int32_t state = 0; int32_t system_id = 0; bool is_damaged = false; bool is_mining = false; };
    std::unordered_map<int64_t, FleetStatus> fleet_status_by_ship;
    if (export_gs && result.contains("my_deployed_fleets") &&
        result["my_deployed_fleets"].is_object()) {
      for (const auto& [fk, fv] : result["my_deployed_fleets"].items()) {
        if (!fv.contains("ship_ids") || !fv["ship_ids"].is_array()) continue;
        FleetStatus fs;
        fs.state     = fv.value("state", 0);
        fs.is_mining = fv.value("is_mining", false);
        if (fv.contains("node_address") && fv["node_address"].contains("system") &&
            !fv["node_address"]["system"].is_null()) {
          fs.system_id = fv["node_address"]["system"].get<int32_t>();
        }
        for (const auto& sid_json : fv["ship_ids"]) {
          int64_t sid = sid_json.get<int64_t>();
          const std::string sid_str = std::to_string(sid);
          fs.is_damaged = fv.contains("ship_dmg") && fv["ship_dmg"].contains(sid_str)
                          && fv["ship_dmg"][sid_str].get<float>() > 0.f;
          fleet_status_by_ship[sid] = fs;
        }
        // Also capture fleet_id -> ship_ids mapping so repair jobs referencing
        // a fleet can be resolved to specific ships.
        if (fv.contains("fleet_id")) {
          try {
            int64_t fleet_id = fv.value("fleet_id", 0LL);
            if (fleet_id != 0) {
              std::vector<int64_t> ship_ids;
              for (const auto& sid_json : fv["ship_ids"]) ship_ids.push_back(sid_json.get<int64_t>());
              game_state_export::capture_fleet_ships(fleet_id, ship_ids);
            }
          } catch (...) {
            spdlog::warn("GameState: failed to capture fleet->ships mapping");
          }
        }
      }

      // When my_deployed_fleets arrives WITHOUT the fleets key (mid-session
      // updates), push status changes into the existing cached drydock entries
      // so exports reflect the current state without requiring a relog.
      if (!result.contains("fleets") && !fleet_status_by_ship.empty()) {
        std::unordered_map<int64_t, game_state_export::FleetStatusUpdate> updates;
        for (const auto& [sid, fs] : fleet_status_by_ship) {
          updates[sid] = {fs.state, fs.system_id, fs.is_damaged, fs.is_mining};
        }
        game_state_export::update_drydock_status(updates);
      }
    }
    for (const auto& [key, section] : result.items()) {
      if (key == "battle_result_headers") {
        if (!Config::Get().sync_options.battlelogs && !Config::Get().game_state_enabled) {
          continue;
        }

        process_battle_headers(section);

      } else if (key == "resources") {
        if (!Config::Get().sync_options.resources && !export_gs) {
          continue;
        }

        process_resources(section);

      } else if (key == "starbase_modules") {
        if (!Config::Get().sync_options.buildings && !export_gs) {
          continue;
        }

        process_starbase_modules(section);

      } else if (key == "ships") {
        if (!Config::Get().sync_options.ships && !export_gs) {
          continue;
        }

        process_ships(section);

      } else if (key == "fleets") {
        if (!export_gs) continue;

        // Each entry has drydock_id (server-side) and ship_ids.
        // Sort by drydock_id ascending and re-index 1-based so the export
        // layer maps 1=A, 2=B ... 26=Z, 27=AA etc. Players can have more than 5.
        if (section.is_object() && !section.empty()) {
          std::vector<game_state_export::DrydockEntry> assignments;
          for (const auto& [fleet_key, fleet] : section.items()) {
            if (!fleet.contains("drydock_id") || !fleet.contains("ship_ids")) continue;
            if (!fleet["ship_ids"].is_array() || fleet["ship_ids"].empty()) continue;

            game_state_export::DrydockEntry e;
            e.drydock_id = fleet["drydock_id"].get<int32_t>();
            e.ship_id    = fleet["ship_ids"][0].get<int64_t>();
            // Enrich with live status from my_deployed_fleets
            auto status_it = fleet_status_by_ship.find(e.ship_id);
            if (status_it != fleet_status_by_ship.end()) {
              e.state      = status_it->second.state;
              e.system_id  = status_it->second.system_id;
              e.is_damaged = status_it->second.is_damaged;
              e.is_mining  = status_it->second.is_mining;
            }

            assignments.push_back(e);
          }
          if (!assignments.empty()) {
            // Sort by raw drydock_id so letters A-E follow server ordering
            std::sort(assignments.begin(), assignments.end(),
                      [](const auto& a, const auto& b) { return a.drydock_id < b.drydock_id; });
            // Re-index to 1-based sequential IDs so the export layer maps 1=A, 2=B etc.
            for (int i = 0; i < static_cast<int>(assignments.size()); ++i) {
              assignments[i].drydock_id = i + 1;
            }
            spdlog::info("process_json fleets: captured {} drydock assignments", assignments.size());
            game_state_export::capture_drydock_assignments(assignments);
          }
        }
      } else if (key == "starbase") {
        if (export_gs) {
          try {
            // home system ID
            int32_t home_system_id = 0;
            if (section.contains("location") && section["location"].is_object())
              home_system_id = section["location"].value("system", 0);

            // last_relocation: ISO-8601 UTC string -> epoch
            int64_t last_relocation_epoch = 0;
            const std::string reloc_str = section.value("last_relocation", "");
            if (!reloc_str.empty()) {
              std::tm tm{};
              std::istringstream ss(reloc_str);
              ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
              if (!ss.fail())
                last_relocation_epoch = static_cast<int64_t>(
                  std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::from_time_t(_mkgmtime(&tm))
                      .time_since_epoch()).count());
            }

            game_state_export::capture_station_info(home_system_id, last_relocation_epoch, -1);

            // Also update peace shield from the inline peace_shield field if present;
            // this is the same data as my_shield_state but available earlier at login.
            if (section.contains("peace_shield") && section["peace_shield"].is_object()) {
              const auto& ps = section["peace_shield"];
              const std::string expiry_str = ps.value("expiry_time", "");
              if (!expiry_str.empty()) {
                std::tm tm{};
                std::istringstream ss(expiry_str);
                ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
                if (!ss.fail()) {
                  int64_t expiry_epoch = static_cast<int64_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::system_clock::from_time_t(_mkgmtime(&tm))
                        .time_since_epoch()).count());
                  game_state_export::capture_peace_shield(expiry_epoch, -1);
                }
              }
            } else if (section.value("state", -1) == 0) {
              // state=0 with no peace_shield object — log only; don't reset.
              // Mid-session starbase blob updates omit peace_shield even when a shield
              // is active — treating absence as "no shield" causes false resets.
              spdlog::debug("starbase blob: state=0 with no peace_shield (ignoring — not authoritative)");
            }
          } catch (const json::exception& je) {
            spdlog::warn("GameState: starbase parse error: {}", je.what());
          }
        }
      } else if (key == "my_deployed_fleets") {
        // handled in first pass above — skip here
      } else if (key == "alliance" || key == "alliance_members") {
        // alliance profile / member roster — no useful game state to capture here
      } else if (key == "alliance_container" || key == "my_shield_state") {
        // Both keys carry peace shield state. alliance_container arrives at login;
        // my_shield_state arrives on station navigation and as mid-session updates.
        // Null/empty = no active shield. Object with expiry_time = shield active.
        if (!export_gs) continue;
        try {
          if (section.is_null() || section.empty()) {
            game_state_export::capture_peace_shield(0, -1);
            spdlog::info("GameState: {}: no active shield", key);
          } else {
            const std::string expiry_str = section.value("expiry_time", "");
            if (!expiry_str.empty()) {
              std::tm tm{};
              std::istringstream ss(expiry_str);
              ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
              int64_t expiry_epoch = 0;
              if (!ss.fail()) {
                expiry_epoch = static_cast<int64_t>(
                  std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::from_time_t(_mkgmtime(&tm))
                      .time_since_epoch()).count());
              }
              game_state_export::capture_peace_shield(expiry_epoch, -1);
              spdlog::info("GameState: {}: expiry={} epoch={}", key, expiry_str, expiry_epoch);
            }
          }
        } catch (const json::exception& je) {
          spdlog::warn("GameState: {} parse error: {}", key, je.what());
        }
      }
    }
  } catch (const json::exception& e) {
    spdlog::error("Error parsing json: {}", e.what());
  }
}

void cache_player_names(std::unique_ptr<std::string>&& bytes)
{
  if (auto response = Digit::PrimeServer::Models::UserProfilesResponse(); response.ParseFromString(*bytes)) {

    std::unordered_map<std::string, CachedPlayerData> names;
    const auto                                        expires_at =
        std::chrono::steady_clock::now() + std::chrono::seconds(Config::Get().sync_resolver_cache_ttl);

    for (const auto& profile : response.userprofiles()) {
      names.insert_or_assign(profile.userid(), CachedPlayerData{.name=profile.name(), .alliance=profile.allianceid(), .expires_at=expires_at});
    }

    if (Config::Get().game_state_enabled) {
      const auto& player_id = Config::Get().game_state_player_id;
      if (player_id.empty()) {
        // Log each unique userid once so the user can identify their own and set player_id in config
        static std::unordered_set<std::string> logged_userids;
        for (const auto& profile : response.userprofiles()) {
          if (logged_userids.insert(profile.userid()).second) {
            spdlog::info("GameState: UserProfile seen: userid={} name='{}' â€” set [gamestate_export] player_id in config to enable name/power export",
                         profile.userid(), profile.name());
          }
        }
      } else {
        for (const auto& profile : response.userprofiles()) {
          if (profile.userid() == player_id) {
            nlohmann::json player_data;
            player_data["name"]        = profile.name();
            player_data["ops_level"]   = 0; // preserved from OPERATIONS building
            player_data["power"]       = static_cast<int64_t>(profile.militarymight());
            player_data["server"]      = http::headers::instanceId;
            player_data["alliance_id"] = profile.allianceid();
            player_data["alliance"]    = "";
            game_state_export::capture_player_data(player_data);
            spdlog::info("GameState: Captured own player name={}, power={}", profile.name(), profile.militarymight());
            // Try to resolve alliance name immediately from cache (if AllianceProfiles
            // already arrived this session); cache_alliance_names handles the reverse order
            {
              std::scoped_lock lk(alliance_data_cache_mtx);
              auto it = alliance_data_cache.find(profile.allianceid());
              if (it != alliance_data_cache.end()) {
                spdlog::info("GameState: Resolved alliance from cache: '{}' [{}]",
                             it->second.name, it->second.tag);
                game_state_export::capture_player_alliance(
                  it->second.name, it->second.tag,
                  it->second.level, it->second.member_count, it->second.power);
              }
            }
            break;
          }
        }
      }
    }

    {
      std::scoped_lock lk(player_data_cache_mtx);
      player_data_cache.insert(names.begin(), names.end());
    }
  } else {
    spdlog::error("Failed to parse user profile");
  }
}

void cache_alliance_names(std::unique_ptr<std::string>&& bytes)
{
  if (auto response = Digit::PrimeServer::Models::GetAllianceProfilesResponse(); response.ParseFromString(*bytes)) {

    std::unordered_map<int64_t, CachedAllianceData> names;
    const auto                                      expires_at =
        std::chrono::steady_clock::now() + std::chrono::seconds(Config::Get().sync_resolver_cache_ttl);

    for (const auto& alliance : response.allianceprofiles()) {
      if (alliance.id() > 0) {
        names.insert_or_assign(alliance.id(), CachedAllianceData{
          .name         = alliance.name(),
          .tag          = alliance.tag(),
          .level        = alliance.level(),
          .member_count = alliance.metadata().membercount(),
          .power        = static_cast<int64_t>(alliance.metadata().militarymight()),
          .expires_at   = expires_at
        });
      }
    }

    {
      std::scoped_lock lk(alliance_data_cache_mtx);
      alliance_data_cache.insert(names.begin(), names.end());
    }

    // Enrich captured player data with alliance name now that we have it
    if (Config::Get().game_state_enabled) {
      const auto& player_id = Config::Get().game_state_player_id;
      if (!player_id.empty()) {
        std::scoped_lock lk(player_data_cache_mtx);
        auto player_it = player_data_cache.find(player_id);
        if (player_it != player_data_cache.end()) {
          auto alliance_it = names.find(player_it->second.alliance);
          if (alliance_it != names.end()) {
            spdlog::info("GameState: Enriching alliance for player={} -> alliance='{}' [{}]",
                         player_id, alliance_it->second.name, alliance_it->second.tag);
            game_state_export::capture_player_alliance(
              alliance_it->second.name, alliance_it->second.tag,
              alliance_it->second.level, alliance_it->second.member_count,
              alliance_it->second.power);
          }
        }
      }
    }

  } else {
    spdlog::error("Failed to parse alliance profile");
  }
}

void ship_sync_data()
{
#if _WIN32
  WinRtApartmentGuard apartmentGuard;
#endif

  try {
    for (;;) {
      std::tuple<SyncConfig::Type, std::string, bool> sync_data;
      {
        std::unique_lock lock(sync_data_mtx);
        sync_data_cv.wait(lock, []() { return !sync_data_queue.empty(); });
        // Move the item out while holding the lock to avoid races/UB
        sync_data = std::move(sync_data_queue.front());
        sync_data_queue.pop();
      }

      try {
        auto& [type, data, is_first_sync] = sync_data;
        http::send_data(type, data, is_first_sync);
      } catch (const std::runtime_error& e) {
        ErrorMsg::SyncRuntime("ship", e);
      } catch (const std::exception& e) {
        ErrorMsg::SyncMsg("ship", e.what());
      } catch (const std::wstring& sz) {
        ErrorMsg::SyncMsg("ship", sz);
#if _WIN32
      } catch (winrt::hresult_error const& ex) {
        ErrorMsg::SyncWinRT("ship", ex);
#endif
      } catch (...) {
        ErrorMsg::SyncMsg("ship", "Unknown error during sending of sync data");
      }
    }
  } catch (const std::exception& e) {
    spdlog::critical("ship_sync_data thread terminated: {}", e.what());
  } catch (...) {
    spdlog::critical("ship_sync_data thread terminated: unknown exception");
  }
}

inline void collect_user_ids_from_fleet(const nlohmann::json& fleet_data, std::unordered_set<std::string>& user_ids)
{
  if (!fleet_data.contains("ref_ids") || fleet_data["ref_ids"].is_null()) {
    for (const auto& fleet : fleet_data["deployed_fleets"]) {
      const auto& player_id = fleet["uid"].get<std::string>();
      user_ids.insert(player_id);
    }
  }
}

inline void collect_alliance_ids(const nlohmann::json& names, std::unordered_set<int64_t>& alliance_ids)
{
  for (const auto& [player_id, entry] : names.items()) {
    try {
      const auto alliance_id = entry["alliance_id"].get<int64_t>();
      alliance_ids.insert(alliance_id);
    } catch (const nlohmann::json::exception&) {
    }
  }
}

void resolve_player_names(const std::unordered_set<std::string>& user_ids, nlohmann::json& out_names,
                          nlohmann::json& out_request_ids, const std::chrono::time_point<std::chrono::steady_clock> now)
{
  std::scoped_lock lk(player_data_cache_mtx);

  for (const auto& user_id : user_ids) {
    const auto it = player_data_cache.find(user_id);
    if (it != player_data_cache.end()) {
      if (it->second.expires_at > now) {
        out_names[user_id] = {{"name", it->second.name},
                              {"alliance_id", it->second.alliance},
                              {"alliance_name", nullptr},
                              {"alliance_tag", nullptr}};
      } else {
        // expired entry: erase and queue for fetch
        player_data_cache.erase(it);
        out_request_ids.push_back(user_id);
      }
    } else {
      // cache miss: queue for fetch
      out_request_ids.push_back(user_id);
    }
  }
}

void resolve_alliance_names(const std::unordered_set<int64_t>& alliance_ids, nlohmann::json& out_names,
                            nlohmann::json&                                          out_request_ids,
                            const std::chrono::time_point<std::chrono::steady_clock> now)
{
  std::scoped_lock lk(alliance_data_cache_mtx);

  for (const auto& alliance_id : alliance_ids) {
    const auto it = alliance_data_cache.find(alliance_id);
    if (it != alliance_data_cache.end()) {
      if (it->second.expires_at > now) {
        for (const auto& [player_id, entry] : out_names.items()) {
          try {
            if (entry["alliance_id"].get<int64_t>() == alliance_id) {
              entry["alliance_name"] = it->second.name;
              entry["alliance_tag"]  = it->second.tag;
              entry.erase("alliance_id");
            }
          } catch (const nlohmann::json::exception&) {
          }
        }
      } else {
        // expired entry: erase and queue for fetch
        alliance_data_cache.erase(it);
        out_request_ids.push_back(alliance_id);
      }
    }
  }
}

void ship_combat_log_data()
{
  using json = nlohmann::json;

#if _WIN32
  WinRtApartmentGuard apartmentGuard;
#endif


  for (;;) {
    uint64_t journal_id;
    {
      std::unique_lock lock(combat_log_data_mtx);
      combat_log_data_cv.wait(lock, [] { return !combat_log_data_queue.empty(); });
      // Move the item out while holding the lock to avoid races/UB
      journal_id = combat_log_data_queue.front();
      combat_log_data_queue.pop();
    }

    try {
      http::sync_log_trace("PROCESS", "combat log", STR_FORMAT("Fetching combat log for battle {}", journal_id));

      const json journals_body{{"journal_id", journal_id}};
      auto       battle_log = (Config::Get().sync_targets.empty() && Config::Get().game_state_enabled)
                                  ? http::get_game_server_data("/journals/get", journals_body.dump())
                                  : http::get_scopely_data("/journals/get", journals_body.dump());
      json       battle_json;

      if (battle_log.empty()) {
        continue;
      }

      try {
        battle_json = std::move(json::parse(battle_log));
      } catch (const json::exception& e) {
        spdlog::error("Error parsing journal response from game server: {}", e.what());
        continue;
      }

      const auto& journal              = battle_json["journal"];
      const auto& target_fleet_data    = journal["target_fleet_data"];
      const auto& initiator_fleet_data = journal["initiator_fleet_data"];

      auto       names      = json::object();
      const auto now        = std::chrono::steady_clock::now();
      const auto expires_at = now + std::chrono::seconds(Config::Get().sync_resolver_cache_ttl);

      {
        std::unordered_set<std::string> user_ids;
        collect_user_ids_from_fleet(target_fleet_data, user_ids);
        collect_user_ids_from_fleet(initiator_fleet_data, user_ids);

        json profiles_request{{"user_ids", json::array()}};
        resolve_player_names(user_ids, names, profiles_request["user_ids"], now);

        const auto fetch_count = profiles_request["user_ids"].size();
        if (fetch_count > 0) {
          http::sync_log_trace("PROCESS", "combat log", STR_FORMAT("Fetching {} player profiles", fetch_count));

          auto profiles      = http::get_scopely_data("/user_profile/profiles", profiles_request.dump());
          auto profiles_json = json::parse(profiles);

          std::scoped_lock lk(player_data_cache_mtx);

          try {
            for (const auto& [player_id, profile] : profiles_json["user_profiles"].get<json::object_t>()) {
              const auto& name        = profile["name"].get<std::string>();
              const auto& alliance_id = profile["alliance_id"].get<int64_t>();

              names[player_id] = {
                  {"name", name}, {"alliance_id", alliance_id}, {"alliance_name", nullptr}, {"alliance_tag", nullptr}};
              player_data_cache[player_id] = {.name=name, .alliance=alliance_id, .expires_at=expires_at};
            }
          } catch (const json::exception& e) {
            spdlog::error("Failed to parse user profiles: {}", e.what());
          }
        }
      }

      {
        std::unordered_set<int64_t> alliance_ids;
        json alliances_request{{"user_current_rank", 0}, {"alliance_id", 0}, {"alliance_ids", json::array()}};

        collect_alliance_ids(names, alliance_ids);
        resolve_alliance_names(alliance_ids, names, alliances_request["alliance_ids"], now);

        const auto fetch_count = alliances_request["alliance_ids"].size();
        if (fetch_count > 0) {
          http::sync_log_trace("PROCESS", "combat log", STR_FORMAT("Fetching {} alliance profiles", fetch_count));

          auto profiles      = http::get_scopely_data("/alliance/get_alliances_public_info", alliances_request.dump());
          auto profiles_json = json::parse(profiles);

          std::scoped_lock lk(alliance_data_cache_mtx);

          try {
            for (const auto& [alliance_id_str, profile] : profiles_json["alliances_info"].get<json::object_t>()) {
              const auto  id   = profile["id"].get<int64_t>();
              const auto& name = profile["name"].get<std::string>();
              const auto& tag  = profile["tag"].get<std::string>();

              alliance_data_cache[id] = {.name=name, .tag=tag, .expires_at=expires_at};
            }
          } catch (json::exception& e) {
            spdlog::error("Failed to parse alliance profiles: {}", e.what());
          }

          for (const auto& [player_id, entry] : names.items()) {
            try {
              if (entry.contains("alliance_id")) {
                const auto alliance_id = entry["alliance_id"].get<int64_t>();
                const auto it          = alliance_data_cache.find(alliance_id);
                if (it != alliance_data_cache.end()) {
                  entry["alliance_name"] = it->second.name;
                  entry["alliance_tag"]  = it->second.tag;
                  entry.erase("alliance_id");
                }
              }
            } catch (json::exception& e) {
              spdlog::error("Failed to update cached player data: {}", e.what());
            }
          }
        }
      }

      auto battle_array = json::array();
      battle_array.push_back(
          {{"type", SyncConfig::Type::Battles}, {"names", names}, {"journal", battle_json["journal"]}});

      try {
        auto ship_data = battle_array.dump();
        http::send_data(SyncConfig::Type::Battles, ship_data, false);
      } catch (const std::runtime_error& e) {
        ErrorMsg::SyncRuntime("combat", e);
      } catch (const std::exception& e) {
        ErrorMsg::SyncException("combat", e);
      } catch (const std::wstring& sz) {
        ErrorMsg::SyncMsg("combat", sz);
#if _WIN32
      } catch (winrt::hresult_error const& ex) {
        ErrorMsg::SyncWinRT("combat", ex);
#endif
      } catch (...) {
        ErrorMsg::SyncMsg("combat", "Unknown error during sending of sync data");
      }

    } catch (json::exception& e) {
      spdlog::error("Error parsing combat log or profiles: {}", e.what());
    } catch (std::exception& e) {
      spdlog::error("Error processing combat log: {}", e.what());
    } catch (...) {
      spdlog::error("Unknown error during processing of combat log data");
    }
  }
}

void HandleEntityGroup(EntityGroup* entity_group)
{
  if (entity_group == nullptr || entity_group->Group == nullptr || entity_group->Group->bytes == nullptr
      || entity_group->Group->Length <= 0) {
    return;
  }

  const auto byteCount = static_cast<size_t>(entity_group->Group->Length);
  const auto *bytesPtr = reinterpret_cast<const char*>(entity_group->Group->bytes->m_Items);

  // Deduplicate: the same entity group can arrive via multiple hooks in the same pipeline
  // pass (e.g. ProcessResultInternal then ParseBinaryObjectsHelper for the same response).
  // Skip processing if we have already handled identical bytes for this entity type.
  {
    // FNV-1a 64-bit hash of the payload bytes
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < byteCount; ++i) {
      hash ^= static_cast<uint8_t>(bytesPtr[i]);
      hash *= 1099511628211ULL;
    }

    static std::mutex                              dedup_mutex;
    static std::unordered_map<int32_t, uint64_t>  last_hash;
    const int32_t type_key = static_cast<int32_t>(entity_group->Type_);
    {
      std::scoped_lock lk(dedup_mutex);
      auto [it, inserted] = last_hash.emplace(type_key, hash);
      if (!inserted) {
        if (it->second == hash) {
          spdlog::debug("HandleEntityGroup: skipping duplicate type={} bytes={}", type_key, byteCount);
          return;
        }
        it->second = hash;
      }
    }
  }

  spdlog::debug("HandleEntityGroup called with type: {}", static_cast<int>(entity_group->Type_));

  // Helper to run processing asynchronously with exception handling
  auto submit_async = [bytesPtr, byteCount]<typename T>(T&& func) {
    auto payload = std::make_unique<std::string>(bytesPtr, byteCount);

    try {
      std::thread([f = std::forward<T>(func), p = std::move(payload)]() mutable {
        try {
          f(std::move(p));
        } catch (const std::exception& e) {
          spdlog::error("Exception in HandleEntityGroup: {}", e.what());
        } catch (...) {
          spdlog::error("Unknown exception in HandleEntityGroup");
        }
      }).detach();
    } catch (const std::exception& e) {
      spdlog::error("Failed to spawn async task: {}", e.what());
    } catch (...) {
      spdlog::error("Failed to spawn async task: unknown exception");
    }
  };

  switch (entity_group->Type_) {
    case EntityGroup::Type::ActiveMissions:
      if (Config::Get().sync_options.missions || Config::Get().game_state_enabled) {
        submit_async(process_active_missions);
      }
      break;
    case EntityGroup::Type::CompletedMissions:
      if (Config::Get().sync_options.missions || Config::Get().game_state_enabled) {
        submit_async(process_completed_missions);
      }
      break;
    case EntityGroup::Type::PlayerInventories:
      if (Config::Get().sync_options.inventory || Config::Get().game_state_enabled) {
        submit_async(process_player_inventories);
      }
      break;
    case EntityGroup::Type::ResearchTreesState:
      if (Config::Get().sync_options.research || Config::Get().game_state_enabled) {
        submit_async(process_research_trees_state);
      }
      break;
    case EntityGroup::Type::Officers:
      if (Config::Get().sync_options.officer || Config::Get().game_state_enabled) {
        submit_async(process_officers);
      }
      break;
    case EntityGroup::Type::ForbiddenTechs:
      if (Config::Get().sync_options.tech) {
        submit_async(process_forbidden_techs);
      }
      break;
    case EntityGroup::Type::ActiveOfficerTraits:
      if (Config::Get().sync_options.traits || Config::Get().game_state_enabled) {
        submit_async(process_active_officer_traits);
      }
      break;
    case EntityGroup::Type::Json:
      if (const auto& o = Config::Get().sync_options; o.battlelogs || o.resources || o.ships || o.buildings || Config::Get().game_state_enabled) {
        submit_async(process_json);
      }
      break;
    case EntityGroup::Type::Jobs:
      if (Config::Get().sync_options.jobs || Config::Get().game_state_enabled) {
        submit_async(process_jobs);
      }
      break;
    case EntityGroup::Type::GlobalActiveBuffs:
      if (Config::Get().sync_options.buffs || Config::Get().game_state_enabled) {
        submit_async(process_global_active_buffs);
      }
      break;
    case EntityGroup::Type::LoyaltySpecs:
      if (Config::Get().game_state_enabled) {
        submit_async(process_loyalty_specs);
      }
      break;
    case EntityGroup::Type::EntitySlots:
      if (Config::Get().sync_options.slots || Config::Get().game_state_enabled) {
        submit_async(process_entity_slots);
      }
      break;
    case EntityGroup::Type::EntitySlotsData:
      if (Config::Get().sync_options.slots || Config::Get().game_state_enabled) {
        submit_async(process_entity_slots_data);
      }
      break;
    case EntityGroup::Type::StarbaseDetailedScan:
      if (Config::Get().game_state_enabled) {
        submit_async(process_starbase_detailed_scan);
      }
      break;
    case EntityGroup::Type::FactionSpecs:
      if (Config::Get().game_state_enabled) {
        submit_async(process_faction_specs);
      }
      break;
    case EntityGroup::Type::BlueprintSpecs:
      if (Config::Get().game_state_enabled) {
        submit_async(process_blueprint_specs);
      }
      break;
    case EntityGroup::Type::ResearchSpecs:
      if (Config::Get().game_state_enabled) {
        submit_async(process_research_specs);
      }
      break;
    case EntityGroup::Type::ConsumableSpecs:
      if (Config::Get().game_state_enabled) {
        submit_async(process_consumable_specs);
      }
      break;
    case EntityGroup::Type::ShipBonusBuffSpecs:
      if (Config::Get().game_state_enabled) {
        submit_async(process_ship_bonus_buff_specs);
      }
      break;
    case EntityGroup::Type::BaseShipTierSpecs:
      if (Config::Get().game_state_enabled) {
        submit_async(process_base_ship_tier_specs);
      }
      break;
    case EntityGroup::Type::ShipTierSpecs:
      if (Config::Get().game_state_enabled) {
        submit_async(process_ship_tier_specs);
      }
      break;
    case EntityGroup::Type::AllianceGetGameProperties:
      if (Config::Get().sync_options.buffs) {
        submit_async(process_alliance_games_props);
      }
      break;
    case EntityGroup::Type::TerritoryStaticData:
      if (Config::Get().game_state_enabled) {
        submit_async(process_territory_static_data);
      }
      break;
    case EntityGroup::Type::TerritoryAllianceSlots:
      if (Config::Get().game_state_enabled) {
        submit_async(process_territory_alliance_slots);
      }
      break;
    case EntityGroup::Type::UserProfiles:
      if (Config::Get().sync_options.battlelogs || Config::Get().game_state_enabled) {
        submit_async(cache_player_names);
      }
      break;
    case EntityGroup::Type::AllianceProfiles:
      if (Config::Get().sync_options.battlelogs || Config::Get().game_state_enabled) {
        submit_async(cache_alliance_names);
      }
      break;
    case EntityGroup::Type::AllianceStarbaseConfig:
      if (Config::Get().game_state_enabled) {
        submit_async([](std::unique_ptr<std::string>&& bytes) {
          if (auto r = Digit::PrimeServer::Models::StaticSyncAllianceStarbaseConfResponse();
              r.ParseFromString(*bytes)) {
            const int32_t system_id = static_cast<int32_t>(r.alliancestarbaseconfig().originsystemid());
            game_state_export::capture_alliance_starbase_system(system_id);
          }
        });
      }
      break;
    default:
      break;
  }
}

void DataContainer_ParseBinaryObject(auto original, void* _this, EntityGroup* group, bool isPlayerData)
{
  HandleEntityGroup(group);
  return original(_this, group, isPlayerData);
}

void DataContainer_ParseEntitySlotsData(auto original, void* _this, EntityGroup* group)
{
  HandleEntityGroup(group);
  return original(_this, group);
}

void DataContainer_ParseSlotData(auto original, void* _this, void* entity_slot, Il2CppString* channel_id)
{
  // TODO: figure out what is in this entity_slot struct.
  return original(_this, entity_slot, channel_id);
}

// This is unused after sync changes in v48084
// void DataContainer_ParseRtcPayload(auto original, void* _this, bool incrementalJsonParsing, RealtimeDataPayload* data)
// {
//   original(_this, incrementalJsonParsing, data);

//   if (data == nullptr || data->Target == nullptr || data->DataType == nullptr || data->Data == nullptr) {
//     return;
//   }

//   const auto target = to_string(data->Target);
//   if (target != "slot:assign" && target != "slot:clear") {
//     return;
//   }

//   const auto type_string = to_string(data->DataType);
//   if (std::stoi(type_string) != DataType::JSON) {
//     return;
//   }

//   const auto rtcData = to_string(data->Data);
//   auto payload = std::make_unique<std::string>(rtcData);

//   std::thread([p = std::move(payload)]() mutable {
//     try {
//       process_entity_slots_rtc(std::move(p));
//     } catch (const std::exception& e) {
//       spdlog::error("Exception in ParseRtcPayload: {}", e.what());
//     } catch (...) {
//       spdlog::error("Unknown exception in ParseRtcPayload");
//     }
//   }).detach();
// }

void GameServerModelRegistry_ProcessResultInternal(auto original, void* _this, void* parsing_context,
                                                   ServiceResponse* service_response, MethodInfo* method)
{
  auto *const entity_groups = service_response->EntityGroups;
  for (int i = 0; i < entity_groups->Count; ++i) {
    auto *const entity_group = entity_groups->get_Item(i);
    HandleEntityGroup(entity_group);
  }

  return original(_this, parsing_context, service_response, method);
}

void GameServerModelRegistry_ParseBinaryObjectsHelper(auto original, void* _this, void* parsing_context,
                                                       ServiceResponse* service_response, MethodInfo* method)
{
  auto *const entity_groups = service_response->EntityGroups;
  for (int i = 0; i < entity_groups->Count; ++i) {
    auto *const entity_group = entity_groups->get_Item(i);
    HandleEntityGroup(entity_group);
  }

  return original(_this, parsing_context, service_response, method);
}

void PrimeApp_InitPrimeServer(auto original, void* _this, Il2CppString* gameServerUrl, Il2CppString* gatewayServerUrl,
                              Il2CppString* sessionId, Il2CppString* serverRegion)
{
  original(_this, gameServerUrl, gatewayServerUrl, sessionId, serverRegion);
  http::headers::instanceSessionId = to_string(to_wstring(sessionId));
  http::headers::gameServerUrl     = to_string(to_wstring(gameServerUrl));
}

void GameServer_Initialise(auto original, void* _this, Il2CppString* sessionId, Il2CppString* gameVersion,
                           bool encryptRequests, Il2CppString* serverRegion)
{
  original(_this, sessionId, gameVersion, encryptRequests, serverRegion);
  http::headers::primeVersion = to_string(to_wstring(gameVersion));
}

void GameServer_SetInstanceIdHeader(auto original, void* _this, int32_t instanceId)
{
  original(_this, instanceId);
  http::headers::instanceId = instanceId;
}

void InstallSyncPatches()
{
  load_previously_sent_logs();

  if (auto game_server_model_registry =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServerModelRegistry");
      !game_server_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "GameServerModelRegistry");
  } else {
    auto *ptr = game_server_model_registry.GetMethod("ProcessResultInternal");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServerModelRegistry", "ProcessResultInternal");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ProcessResultInternal);
    }

    ptr = game_server_model_registry.GetMethod("ParseBinaryObjectsHelper");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServerModelRegistry", "ParseBinaryObjectsHelper");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ParseBinaryObjectsHelper);
    }
  }
#if 0
  if (auto platform_model_registry =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Core", "PlatformModelRegistry");
      !platform_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PlatformModelRegistry");
  } else {
    if (auto *const ptr = platform_model_registry.GetMethod("ProcessResultInternal"); ptr == nullptr) {
      ErrorMsg::MissingMethod("PlatformModelRegistry", "ProcessResultInternal");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ProcessResultInternal);
    }
  }
#endif

  if (auto buff_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffDataContainer");
      !buff_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffDataContainer");
  } else {
    if (auto *const ptr = buff_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("BuffDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto buff_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService");
      !buff_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffService");
  } else {
    if (auto *const ptr = buff_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("BuffService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto inventory_data_container = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime",
                                                              "Digit.PrimeServer.Services", "InventoryDataContainer");
      !inventory_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "InventoryDataContainer");
  } else {
    if (auto *const ptr = inventory_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("InventoryDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto job_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "JobService");
      !job_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "JobService");
  } else {
    if (auto *const ptr = job_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("JobService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto job_service_data_container = il2cpp_get_class_helper(
          "Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "JobServiceDataContainer");
      !job_service_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "JobServiceDataContainer");
  } else {
    if (auto *const ptr = job_service_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("JobServiceDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto missions_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "MissionsDataContainer");
      !missions_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Models", "MissionsDataContainer");
  } else {
    if (auto *const ptr = missions_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("MissionsDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto research_data_container = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime",
                                                             "Digit.PrimeServer.Services", "ResearchDataContainer");
      !research_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "ResearchDataContainer");
  } else {
    if (auto *const ptr = research_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingHelper("ResearchDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto research_service =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "ResearchService");
      !research_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "ResearchService");
  } else {
    if (auto *const ptr = research_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ResearchService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }
  }

  if (auto slot_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "SlotDataContainer");
      !slot_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "SlotDataContainer");
  } else {
    if (auto *const ptr = slot_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }

    if (auto *const ptr = slot_data_container.GetMethod("ParseEntitySlotsData"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseEntitySlotsData");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseEntitySlotsData);
    }

    // v48084: ParseSlotUpdatedJson/ParseSlotRemovedJson renamed to UpdateSlotData/RemoveSlotData
    // with new signature (EntitySlot, String channelId) â€” hook needs rewrite to match.
    if (auto *const ptr = slot_data_container.GetMethod("UpdateSlotData"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "UpdateSlotData");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseSlotData);
    }

    if (auto *const ptr = slot_data_container.GetMethod("RemoveSlotData"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "RemoveSlotData");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseSlotData);
    }
  }

  if (auto prime_app = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "PrimeApp");
      !prime_app.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PrimeApp");
  } else {
    if (auto *const ptr = prime_app.GetMethod("InitPrimeServer"); ptr == nullptr) {
      ErrorMsg::MissingMethod("PrimeApp", "InitPrimeServer");
    } else {
      SPUD_STATIC_DETOUR(ptr, PrimeApp_InitPrimeServer);
    }
  }

  if (auto game_server =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServer");
      !game_server.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "GameServer");
  } else {
    if (auto *const ptr = game_server.GetMethod("Initialise"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "Initialise");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServer_Initialise);
    }

    if (auto *const ptr = game_server.GetMethod("SetInstanceIdHeader"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "SetInstanceIdHeader");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServer_SetInstanceIdHeader);
    }
  }

  std::thread(ship_sync_data).detach();
  std::thread(ship_combat_log_data).detach();
}
