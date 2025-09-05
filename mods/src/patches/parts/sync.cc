#include "config.h"
#include "errormsg.h"
#include "file.h"
#include "str_utils.h"

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
#include <boost/url.hpp>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <fstream>
#include <ostream>
#include <queue>
#include <semaphore>
#include <string>

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
  static std::string    primeVersion{"1.000.44468"};
  static constexpr char poweredBy[] = "stfc community patch/" VER_FILE_VERSION_STR;
} // namespace headers

static std::string newUUID()
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

static const std::string CURL_TYPE_UPLOAD   = "UPLOAD";
static const std::string CURL_TYPE_DOWNLOAD = "DOWNLOAD";

std::mutex http_responses_mtx;
std::condition_variable http_responses_cv;
using response_queue_item_t = std::tuple<std::string, cpr::Response>;
std::queue<response_queue_item_t> http_responses_queue;


static std::shared_ptr<cpr::Session> get_curl_client_sync(const std::string& target)
{
  static std::unordered_map<std::string, std::shared_ptr<cpr::Session>> sessions_map;
  static std::mutex                                                     sessions_map_mutex;

  std::shared_ptr<cpr::Session> session;
  {
    std::lock_guard lk(sessions_map_mutex);

    if (!sessions_map.contains(target)) {
      session                   = std::make_shared<cpr::Session>();
      const auto& target_config = Config::Get().sync_targets[target];

      session->SetUrl(target_config.url);
      session->SetUserAgent("stfc community patch " VER_FILE_VERSION_STR " (libcurl/" LIBCURL_VERSION ")");
      session->SetAcceptEncoding(cpr::AcceptEncoding{});
      session->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});
      session->SetRedirect(cpr::Redirect{3, true, false, cpr::PostRedirectFlags::POST_ALL});

      if (!target_config.proxy.empty()) {
        session->SetProxies({{"http", target_config.proxy}, {"https", target_config.proxy}});

        // TODO: Is this too permissive?
        session->SetSslOptions(cpr::Ssl(
          cpr::ssl::VerifyHost{false},
          cpr::ssl::VerifyPeer{false},
          cpr::ssl::NoRevoke{true})
        );
      }

      session->SetHeader({
          {"Content-Type", "application/json"},
          {"X-Powered-By", headers::poweredBy},
          {"stfc-sync-token", target_config.token},
      });

      sessions_map[target] = session;
    } else {
      session = sessions_map[target];
    }
  }

  return session;
}

static void send_data(SyncConfig::Type type, const std::string& post_data)
{
  using sem_ptr = std::shared_ptr<std::counting_semaphore<1>>;
  static std::unordered_map<std::string, sem_ptr> client_semaphores;
  static std::mutex                               client_sync_map_guard;
  static std::once_flag                           init_once;

  std::call_once(init_once, []() {
    const auto& targets = Config::Get().sync_targets;
    if (targets.empty()) {
      sync_log_warn(CURL_TYPE_UPLOAD, "GLOBAL", "No target found, will not attempt to send");
    } else {
      client_semaphores.reserve(targets.size());
      for (const auto& target : targets | std::views::keys) {
        client_semaphores.emplace(target, std::make_shared<std::counting_semaphore<1>>(1));
      }
    }
  });

  for (const auto& target : Config::Get().sync_targets | std::views::filter([type](const auto& t) { return t.second.enabled(type); }) | std::views::keys) {
    // Get/create the per-target semaphore in a thread-safe manner.
    sem_ptr target_sem;
    {
      std::lock_guard guard(client_sync_map_guard);
      auto            it = client_semaphores.find(target);
      if (it == client_semaphores.end()) {
        it = client_semaphores.emplace(target, std::make_shared<std::counting_semaphore<1>>(1)).first;
      }
      target_sem = it->second;
    }

    const auto target_identifier = STR_FORMAT("{} ({})", target, to_string(type));
    bool acquired = false;

    try {
      // Ensure only one in-flight operation per target.
      target_sem->acquire();
      acquired = true;

      const auto httpClient = get_curl_client_sync(target);
#ifndef _MODDBG
      httpClient->SetConnectTimeout(cpr::ConnectTimeout{3000});
      httpClient->SetTimeout(cpr::Timeout{10000});
#endif
      httpClient->SetBody(cpr::Body{post_data});

      sync_log_debug(CURL_TYPE_UPLOAD, target_identifier, "Sending data to " + httpClient->GetFullRequestUrl());
      auto asyncResponse = httpClient->PostAsync();

      // Detach a waiter that enqueues the completed response and releases the semaphore.
      std::thread([target_identifier, target_sem, ar = std::move(asyncResponse)]() mutable {
        try {
          auto response = ar.get();
          {
            std::lock_guard lk(http_responses_mtx);
            http_responses_queue.emplace(target_identifier, std::move(response));
          }
          http_responses_cv.notify_all();
        } catch (const std::runtime_error& e) {
          ErrorMsg::SyncRuntime(target_identifier.c_str(), e);
        } catch (const std::exception& e) {
          ErrorMsg::SyncException(target_identifier.c_str(), e);
#if _WIN32
        } catch (winrt::hresult_error const& ex) {
          ErrorMsg::SyncWinRT(target_identifier.c_str(), ex);
#endif
        } catch (...) {
          ErrorMsg::SyncMsg(target_identifier.c_str(), "Unknown error occurred");
        }
        // Always release to allow the next send for this target.
        try { target_sem->release(); } catch (...) {}
      }).detach();

    } catch (const std::runtime_error& e) {
      spdlog::error("Failed to send sync data to target '{}' - Runtime error: {}", target_identifier, e.what());
      if (acquired) { try { target_sem->release(); } catch (...) {} }
    } catch (const std::exception& e) {
      spdlog::error("Failed to send sync data to target '{}' - Exception: {}", target_identifier, e.what());
      if (acquired) { try { target_sem->release(); } catch (...) {} }
    } catch (...) {
      spdlog::error("Failed to send sync data to target '{}' - Unknown error occurred", target_identifier);
      if (acquired) { try { target_sem->release(); } catch (...) {} }
    }
  }
}

static std::shared_ptr<cpr::Session> get_curl_client_scopely()
{
  static std::shared_ptr<cpr::Session> session{nullptr};

  if (!session) {
    session = std::make_shared<cpr::Session>();
    session->SetAcceptEncoding(cpr::AcceptEncoding{});
    session->SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});

    if (!Config::Get().sync_options.proxy.empty()) {
      session->SetProxies({{"https", Config::Get().sync_options.proxy}});

      // TODO: Is this too permissive?
      session->SetSslOptions(cpr::Ssl(
        cpr::ssl::VerifyHost{false},
        cpr::ssl::VerifyPeer{false},
        cpr::ssl::NoRevoke{true}
      ));
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
  }

  return session;
}

static std::string get_scopely_data(const std::string& path, const std::string& post_data)
{
  static std::once_flag init_once;
  std::call_once(init_once, []() {
    if (Config::Get().sync_targets.empty()) {
      sync_log_warn(CURL_TYPE_UPLOAD, "GLOBAL", "No target found, will not attempt to retrieve data");
    }
  });

  boost::url url(headers::gameServerUrl);
  url.set_path(path);

  const auto        httpClient = get_curl_client_scopely();
  static std::mutex client_mutex;

  {
    std::lock_guard lk(client_mutex);
    httpClient->SetUrl(url.buffer().data());

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
    return response.text;
  }

  return {};
}

static void handle_response_data()
{
#if _WIN32
  WinRtApartmentGuard apartmentGuard;
#endif

  for (;;) {
    std::string   target_identifier;
    cpr::Response response;

    {
      std::unique_lock lk(http_responses_mtx);
      http_responses_cv.wait(lk, []() { return !http_responses_queue.empty(); });

      auto& tuple       = http_responses_queue.front();
      target_identifier = std::move(std::get<0>(tuple));
      response          = std::move(std::get<1>(tuple));
      http_responses_queue.pop();
    }

    // Process a completed response
    if (response.status_code == 0) {
      sync_log_error(CURL_TYPE_UPLOAD, target_identifier, "Failed to send request: " + response.error.message);
    } else if (response.status_code >= 400) {
      sync_log_error(CURL_TYPE_UPLOAD, target_identifier, STR_FORMAT("Failed to communicate with server: {} (after {:.1f}s)", response.status_line, response.elapsed));
    } else {
      sync_log_debug(CURL_TYPE_UPLOAD, target_identifier, STR_FORMAT("Response: {} ({:.1f}s elapsed)", response.status_line, response.elapsed));
    }
  }
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

std::mutex                                           sync_data_mtx;
std::condition_variable                              sync_data_cv;
std::queue<std::pair<SyncConfig::Type, std::string>> sync_data_queue;

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
  std::chrono::steady_clock::time_point expires_at;
};

std::unordered_map<int64_t, CachedAllianceData> alliance_data_cache;
std::mutex                                      alliance_data_cache_mtx;

void queue_data(SyncConfig::Type type, const std::string& data)
{
  {
    std::lock_guard lk(sync_data_mtx);
    sync_data_queue.emplace(type, data);
    http::sync_log_debug("QUEUE", to_string(type), "Added data to sync queue");
  }

  sync_data_cv.notify_all();
}

void queue_data(SyncConfig::Type type, const nlohmann::json& data)
{
  {
    std::lock_guard lk(sync_data_mtx);
    sync_data_queue.emplace(type, data.dump());
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
  std::vector<int64_t> components       = {};
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
  std::lock_guard lk(previously_sent_battlelogs_mtx);

  previously_sent_battlelogs.set_capacity(300);

  try {
    std::ifstream file(File::Battles(), std::ios::in | std::ios::binary);
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
    std::lock_guard lk(previously_sent_battlelogs_mtx);
    for (auto id : previously_sent_battlelogs) {
      battlelog_array.push_back(id);
    }
  }

  try {
    std::ofstream file(File::Battles(), std::ios::out | std::ios::binary | std::ios::trunc);
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
      std::lock_guard lk(active_mission_states_mtx);
      if (active_mission_states != active_missions) {
        changed               = true;
        active_mission_states = std::move(active_missions);
      }
    }

    if (changed && !active_mission_states.empty()) {
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
    {
      std::lock_guard lk(completed_mission_states_mtx);
      std::ranges::set_difference(completed_missions, completed_mission_states, std::back_inserter(diff));

      if (!diff.empty()) {
        completed_mission_states = std::move(completed_missions);
      }
    }

    if (!diff.empty()) {
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
  using json = nlohmann::json;
  static std::unordered_map<std::pair<std::underlying_type_t<Digit::PrimeServer::Models::InventoryItemType>, int64_t>,
                            int64_t, pairhash>
                    inventory_states;
  static std::mutex inventory_states_mtx;

  if (auto response = Digit::PrimeServer::Models::InventoryResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "player inventories",
                         STR_FORMAT("Processing {} inventories", response.inventories_size()));

    auto inventory_items = json::array();
    {
      std::lock_guard lk(inventory_states_mtx);

      for (const auto& inventory : response.inventories() | std::views::values) {
        for (const auto& item : inventory.items()) {
          if (item.has_commonparams()) {
            const auto item_id = item.commonparams().refid();
            const auto count   = item.count();
            const auto key     = std::make_pair(item.type(), item_id);

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
      queue_data(SyncConfig::Type::Inventory, inventory_items);
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
      std::lock_guard lk(research_states_mtx);

      for (const auto& [id, level] : response.researchprojectlevels()) {
        if (const auto& it = research_states.find(id); it == research_states.end() || it->second != level) {
          research_states[id] = level;
          research_array.push_back({{"type", SyncConfig::Type::Research}, {"rid", id}, {"level", level}});
        }
      }
    }

    if (!research_array.empty()) {
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
      std::lock_guard lk(officer_states_mtx);

      for (const auto& officer : response.officers()) {
        const RankLevelShardsState officer_state{officer.rankindex(), officer.level(), officer.shardcount()};

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

    if (!officers_array.empty()) {
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
      std::lock_guard lk(tech_states_mtx);

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
      std::lock_guard lk(trait_states_mtx);

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

void process_global_active_buffs(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, std::pair<int32_t, int64_t>> buff_states;
  static std::mutex                                               buff_states_mtx;

  if (auto response = Digit::PrimeServer::Models::GlobalActiveBuffsResponse(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "global buffs",
                         STR_FORMAT("Processing {} active buffs", response.globalactivebuffs_size()));

    auto buff_array = json::array();
    {
      std::lock_guard lk(buff_states_mtx);

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
      queue_data(SyncConfig::Type::Buffs, buff_array);
    }
  } else {
    spdlog::error("Failed to parse global active buffs");
  }
}

static std::unordered_map<int64_t, int64_t> slot_states;
static std::mutex                           slot_states_mtx;

inline std::chrono::time_point<std::chrono::system_clock> parse_timestamp(const std::string& timestamp)
{
#ifdef _WIN32
  std::istringstream ss(timestamp);
  std::chrono::system_clock::time_point time_point;
  std::chrono::from_stream(ss, "%Y-%m-%dT%H:%M:%S", time_point);
  return time_point;
#else
  std::tm tm = {};
  strptime(timestamp.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
  auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
  return time_point;
#endif
}

void process_entity_slots(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;

  if (auto response = Digit::PrimeServer::Models::EntitySlots(); response.ParseFromString(*bytes)) {

    http::sync_log_trace("PROCESS", "entity slots", STR_FORMAT("Processing {} slots", response.entityslots__size()));

    auto slot_array = json::array();
    {
      std::lock_guard lk(slot_states_mtx);

      for (const auto& slot : response.entityslots_()) {
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
          default:
            continue;
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
    }

    if (!slot_array.empty()) {
      queue_data(SyncConfig::Type::Slots, slot_array);
    }
  } else {
    spdlog::error("Failed to parse entity slots");
  }
}

void process_entity_slots_rtc(std::unique_ptr<std::string>&& json_payload)
{
  using json = nlohmann::json;

  try {
    auto data = json::parse(*json_payload);
    http::sync_log_trace("PROCESS", "entity slots (RTC)", "Processing entity slot update");

    const auto item = data["item_id"];
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
          slot_params["expiry_time"] = timestamp.time_since_epoch().count();
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
            slot_params["cooldown_expiration"] = timestamp.time_since_epoch().count();
          }
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
      std::lock_guard lk(slot_states_mtx);

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
        std::lock_guard lk(jobs_active_mtx);
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
      std::lock_guard lk(jobs_active_mtx);
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
      queue_data(SyncConfig::Type::Jobs, job_array);
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
    std::lock_guard lk(previously_sent_battlelogs_mtx);

    for (const auto id : battle_ids | std::views::reverse) {
      if (eastl::find(previously_sent_battlelogs.begin(), previously_sent_battlelogs.end(), id)
          == previously_sent_battlelogs.end()) {
        previously_sent_battlelogs.push_back(id);
        ;
        to_enqueue.push_back(id);
      }
    }
  }

  if (!to_enqueue.empty()) {
    http::sync_log_trace("PROCESS", "battle headers",
                         STR_FORMAT("Queuing {} battles for background processing", to_enqueue.size()));

    {
      std::lock_guard lk(combat_log_data_mtx);
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

  http::sync_log_trace("PROCESS", "resources", STR_FORMAT("Processing {} resources", section.size()));

  auto resource_array = json::array();
  {
    std::lock_guard lk(resource_states_mtx);

    for (const auto& [str_id, resource] : section.get<json::object_t>()) {
      auto id     = std::stoll(str_id);
      auto amount = resource["current_amount"].get<int64_t>();

      if (const auto& it = resource_states.find(id); it == resource_states.end() || it->second != amount) {
        resource_states[id] = amount;
        resource_array.push_back({{"type", SyncConfig::Type::Resources}, {"rid", id}, {"amount", amount}});
      }
    }
  }

  if (!resource_array.empty()) {
    queue_data(SyncConfig::Type::Resources, resource_array);
  }
}

void process_starbase_modules(const nlohmann::json& section)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int32_t> module_states;
  static std::mutex                           module_states_mtx;

  http::sync_log_trace("PROCESS", "starbase modules", STR_FORMAT("Processing {} buildings", section.size()));

  auto starbase_array = json::array();
  {
    std::lock_guard lk(module_states_mtx);

    for (const auto& module : section.get<json::object_t>() | std::views::values) {
      const auto id    = module["id"].get<int64_t>();
      const auto level = module["level"].get<int32_t>();

      if (const auto& it = module_states.find(id); it == module_states.end() || it->second != level) {
        module_states[id] = level;
        starbase_array.push_back({{"type", SyncConfig::Type::Buildings}, {"bid", id}, {"level", level}});
      }
    }
  }

  if (!starbase_array.empty()) {
    queue_data(SyncConfig::Type::Buildings, starbase_array);
  }
}

void process_ships(const nlohmann::json& section)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, ShipState> ship_states;
  static std::mutex                             ship_states_mtx;

  http::sync_log_trace("PROCESS", "ships", STR_FORMAT("Processing {} ships", section.size()));

  auto ship_array = json::array();
  {
    std::lock_guard lk(ship_states_mtx);

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
                              {"tier", tier},
                              {"hull_id", ship["hull_id"].get<int64_t>()},
                              {"components", components}});
      }
    }
  }

  if (!ship_array.empty()) {
    queue_data(SyncConfig::Type::Ships, ship_array);
  }
}

void process_json(std::unique_ptr<std::string>&& bytes)
{
  using json = nlohmann::json;

  try {
    const auto result = json::parse(bytes->begin(), bytes->end());

    for (const auto& [key, section] : result.items()) {
      if (key == "battle_result_headers") {
        if (!Config::Get().sync_options.battlelogs) {
          continue;
        }

        process_battle_headers(section);

      } else if (key == "resources") {
        if (!Config::Get().sync_options.resources) {
          continue;
        }

        process_resources(section);

      } else if (key == "starbase_modules") {
        if (!Config::Get().sync_options.buildings) {
          continue;
        }

        process_starbase_modules(section);

      } else if (key == "ships") {
        if (!Config::Get().sync_options.ships) {
          continue;
        }

        process_ships(section);
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
      names.insert_or_assign(profile.userid(), CachedPlayerData{profile.name(), profile.allianceid(), expires_at});
    }

    {
      std::lock_guard lk(player_data_cache_mtx);
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
        names.insert_or_assign(alliance.id(), CachedAllianceData{alliance.name(), alliance.tag(), expires_at});
      }
    }

    {
      std::lock_guard lk(alliance_data_cache_mtx);
      alliance_data_cache.insert(names.begin(), names.end());
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


  for (;;) {
    std::pair<SyncConfig::Type, std::string> sync_data;
    {
      std::unique_lock lock(sync_data_mtx);
      sync_data_cv.wait(lock, []() { return !sync_data_queue.empty(); });
      // Move the item out while holding the lock to avoid races/UB
      sync_data = std::move(sync_data_queue.front());
      sync_data_queue.pop();
    }

    try {
      http::send_data(sync_data.first, sync_data.second);
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
  std::lock_guard lk(player_data_cache_mtx);

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
  std::lock_guard lk(alliance_data_cache_mtx);

  for (const auto& alliance_id : alliance_ids) {
    const auto it = alliance_data_cache.find(alliance_id);
    if (it != alliance_data_cache.end()) {
      if (it->second.expires_at > now) {
        for (auto& [player_id, entry] : out_names.items()) {
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
      auto       battle_log = http::get_scopely_data("/journals/get", journals_body.dump());
      json       battle_json;

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

          std::lock_guard lk(player_data_cache_mtx);

          try {
            for (const auto& [player_id, profile] : profiles_json["user_profiles"].get<json::object_t>()) {
              const auto& name        = profile["name"].get<std::string>();
              const auto& alliance_id = profile["alliance_id"].get<int64_t>();

              names[player_id] = {
                  {"name", name}, {"alliance_id", alliance_id}, {"alliance_name", nullptr}, {"alliance_tag", nullptr}};
              player_data_cache[player_id] = {name, alliance_id, expires_at};
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

          std::lock_guard lk(alliance_data_cache_mtx);

          try {
            for (const auto& [alliance_id_str, profile] : profiles_json["alliances_info"].get<json::object_t>()) {
              const auto  id   = profile["id"].get<int64_t>();
              const auto& name = profile["name"].get<std::string>();
              const auto& tag  = profile["tag"].get<std::string>();

              alliance_data_cache[id] = {name, tag, expires_at};
            }
          } catch (json::exception& e) {
            spdlog::error("Failed to parse alliance profiles: {}", e.what());
          }

          for (auto& [player_id, entry] : names.items()) {
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
        http::send_data(SyncConfig::Type::Battles, ship_data);
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
  auto       bytesPtr  = reinterpret_cast<const char*>(entity_group->Group->bytes->m_Items);

  // Helper to run processing asynchronously with exception handling
  auto submit_async = [bytesPtr, byteCount]<typename T>(T&& func) {
    auto payload = std::make_unique<std::string>(bytesPtr, byteCount);

    std::thread([f = std::forward<T>(func), p = std::move(payload)]() mutable {
      try {
        f(std::move(p));
      } catch (const std::exception& e) {
        spdlog::error("Exception in HandleEntityGroup: {}", e.what());
      } catch (...) {
        spdlog::error("Unknown exception in HandleEntityGroup");
      }
    }).detach();
  };

  switch (entity_group->Type_) {
    case EntityGroup::Type::ActiveMissions:
      if (Config::Get().sync_options.missions) {
        submit_async(process_active_missions);
      }
      break;
    case EntityGroup::Type::CompletedMissions:
      if (Config::Get().sync_options.missions) {
        submit_async(process_completed_missions);
      }
      break;
    case EntityGroup::Type::PlayerInventories:
      if (Config::Get().sync_options.inventory) {
        submit_async(process_player_inventories);
      }
      break;
    case EntityGroup::Type::ResearchTreesState:
      if (Config::Get().sync_options.research) {
        submit_async(process_research_trees_state);
      }
      break;
    case EntityGroup::Type::Officers:
      if (Config::Get().sync_options.officer) {
        submit_async(process_officers);
      }
      break;
    case EntityGroup::Type::ForbiddenTechs:
      if (Config::Get().sync_options.tech) {
        submit_async(process_forbidden_techs);
      }
      break;
    case EntityGroup::Type::ActiveOfficerTraits:
      if (Config::Get().sync_options.traits) {
        submit_async(process_active_officer_traits);
      }
      break;
    case EntityGroup::Type::Json:
      if (const auto& o = Config::Get().sync_options; o.battlelogs || o.resources || o.ships || o.buildings) {
        submit_async(process_json);
      }
      break;
    case EntityGroup::Type::Jobs:
      if (Config::Get().sync_options.jobs) {
        submit_async(process_jobs);
      }
      break;
    case EntityGroup::Type::GlobalActiveBuffs:
      if (Config::Get().sync_options.buffs) {
        submit_async(process_global_active_buffs);
      }
      break;
    case EntityGroup::Type::EntitySlots:
      if (Config::Get().sync_options.slots) {
        submit_async(process_entity_slots);
      }
      break;
    case EntityGroup::Type::AllianceGetGameProperties:
      if (Config::Get().sync_options.buffs) {
        submit_async(process_alliance_games_props);
      }
    case EntityGroup::Type::UserProfiles:
      if (Config::Get().sync_options.battlelogs) {
        submit_async(cache_player_names);
      }
    case EntityGroup::Type::AllianceProfiles:
      if (Config::Get().sync_options.battlelogs) {
        submit_async(cache_alliance_names);
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

void DataContainer_ParseRtcPayload(auto original, void* _this, bool incrementalJsonParsing, RealtimeDataPayload* data)
{
  original(_this, incrementalJsonParsing, data);

  if (data == nullptr || data->Target == nullptr || data->DataType == nullptr || data->Data == nullptr) {
    return;
  }

  const auto target = to_string(data->Target);
  if (target != "slot:assign" && target != "slot:clear") {
    return;
  }

  const auto type_string = to_string(data->DataType);
  if (std::stoi(type_string) != DataType::JSON) {
    return;
  }

  const auto rtcData = to_string(data->Data);
  auto payload = std::make_unique<std::string>(rtcData);

  std::thread([p = std::move(payload)]() mutable {
    try {
      process_entity_slots_rtc(std::move(p));
    } catch (const std::exception& e) {
      spdlog::error("Exception in ParseRtcPayload: {}", e.what());
    } catch (...) {
      spdlog::error("Unknown exception in ParseRtcPayload");
    }
  }).detach();
}

void GameServerModelRegistry_ProcessResultInternal(auto original, void* _this, HttpResponse* http_response,
                                                   ServiceResponse* service_response, void* callback,
                                                   void* callback_error)
{
  const auto entity_groups = service_response->EntityGroups;
  for (int i = 0; i < entity_groups->Count; ++i) {
    const auto entity_group = entity_groups->get_Item(i);
    HandleEntityGroup(entity_group);
  }

  return original(_this, http_response, service_response, callback, callback_error);
}

void GameServerModelRegistry_HandleBinaryObjects(auto original, void* _this, ServiceResponse* service_response)
{
  const auto entity_groups = service_response->EntityGroups;
  for (int i = 0; i < entity_groups->Count; ++i) {
    const auto entity_group = entity_groups->get_Item(i);
    HandleEntityGroup(entity_group);
  }

  return original(_this, service_response);
}

void PrimeApp_InitPrimeServer(auto original, void* _this, Il2CppString* gameServerUrl, Il2CppString* gatewayServerUrl,
                              Il2CppString* sessionId)
{
  original(_this, gameServerUrl, gatewayServerUrl, sessionId);
  http::headers::instanceSessionId = to_string(to_wstring(sessionId));
  http::headers::gameServerUrl     = to_string(to_wstring(gameServerUrl));
}

void GameServer_Initialise(auto original, void* _this, Il2CppString* sessionId, Il2CppString* gameVersion,
                           bool encryptRequests)
{
  original(_this, sessionId, gameVersion, encryptRequests);
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
    auto ptr = game_server_model_registry.GetMethod("ProcessResultInternal");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServerModelRegistry", "ProcessResultInterval");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ProcessResultInternal);
    }

    ptr = game_server_model_registry.GetMethod("HandleBinaryObjects");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServerModelRegsitry", "HandleBinaryObjects");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_HandleBinaryObjects);
    }
  }

  if (auto platform_model_registry =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Core", "PlatformModelRegistry");
      !platform_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PlatformModelRegistry");
  } else {
    if (const auto ptr = platform_model_registry.GetMethod("ProcessResultInternal"); ptr == nullptr) {
      ErrorMsg::MissingMethod("PlatformModelRegistry", "ProcessResultInterval");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ProcessResultInternal);
    }
  }

  if (auto buff_data_container =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffDataContainer");
      !buff_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffDataContainer");
  } else {
    if (const auto ptr = buff_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
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
    if (const auto ptr = buff_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
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
    if (const auto ptr = inventory_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
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
    if (const auto ptr = job_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
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
    if (const auto ptr = job_service_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
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
    if (const auto ptr = missions_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
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
    if (const auto ptr = research_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
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
    if (const auto ptr = research_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
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
    if (const auto ptr = slot_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseBinaryObject);
    }

    if (const auto ptr = slot_data_container.GetMethod("ParseSlotUpdatedJson"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseSlotUpdatedJson");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseRtcPayload);
    }

    if (const auto ptr = slot_data_container.GetMethod("ParseSlotRemovedJson"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseSlotRemovedJson");
    } else {
      SPUD_STATIC_DETOUR(ptr, DataContainer_ParseRtcPayload);
    }
  }

  if (auto authentication_service = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "PrimeApp");
      !authentication_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PrimeApp");
  } else {
    if (const auto ptr = authentication_service.GetMethod("InitPrimeServer"); ptr == nullptr) {
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
    if (const auto ptr = game_server.GetMethod("Initialise"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "Initialise");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServer_Initialise);
    }

    if (const auto ptr = game_server.GetMethod("SetInstanceIdHeader"); ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "SetInstanceIdHeader");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServer_SetInstanceIdHeader);
    }
  }

  std::thread(ship_sync_data).detach();
  std::thread(ship_combat_log_data).detach();
  std::thread(http::handle_response_data).detach();
}
