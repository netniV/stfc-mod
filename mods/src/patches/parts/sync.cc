#include "config.h"
#include "errormsg.h"
#include "file.h"
#include "il2cpp-api-types.h"

#include <il2cpp/il2cpp_helper.h>

#include <Digit.PrimeServer.Models.pb.h>
#include <prime/EntityGroup.h>
#include <prime/HttpResponse.h>
#include <prime/ServiceResponse.h>
#include <str_utils.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <EASTL/algorithm.h>
#include <EASTL/bonus/ring_buffer.h>

#if _WIN32
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <rpc.h>
#else
#include <uuid/uuid.h>
#endif

#include <curl/curl.h>
#include <boost/url.hpp>

#include <condition_variable>
#include <format>
#include <fstream>
#include <future>
#include <ostream>
#include <queue>
#include <string>

#if !__cpp_lib_format
#include <spdlog/fmt/fmt.h>
#endif

namespace http
{

namespace headers
{
  static std::string gameServerUrl;
  static std::string instanceSessionId;
  static int32_t     instanceId;
  static std::string unityVersion{"2021.3.44f1"};
  static std::string primeVersion{"1.000.44068"};
  static std::string apiKey{"meh"};
  static constexpr char poweredBy[] = "stfc community patch " VER_FILE_VERSION_STR " (libcurl/" LIBCURL_VERSION ")";
} // namespace headers

struct CURLClient {
  explicit CURLClient(CURL* handle)
      : handle_(handle)
  {
  }

  operator CURL*() const
  {
    return this->handle_;
  }

  ~CURLClient()
  {
    curl_easy_cleanup(handle_);
  }

private:
  CURL* handle_;
};

static std::string newUUID()
{
#ifdef WIN32
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

static void sync_log_error(std::string type, std::string text)
{
  if (Config::Get().sync_logging) {
    spdlog::error("SYNC-{}: {}", type, text);
  }
}

static void sync_log_warn(std::string type, std::string text)
{
  if (Config::Get().sync_logging) {
    spdlog::warn("SYNC-{}: {}", type, text);
  }
}

static void sync_log_info(std::string type, std::string text)
{
  if (Config::Get().sync_logging) {
    spdlog::info("SYNC-{}: {}", type, text);
  }
}

static void sync_log_debug(std::string type, std::string text)
{
  if (Config::Get().sync_logging) {
    spdlog::debug("SYNC-{}: {}", type, text);
  }
}

static struct curl_slist* sync_slist_append(const std::string &type, struct curl_slist* list, const std::string& header,
                                            const std::string& data, const bool mask = false)
{
  auto combined = header + ": " + data;
  if (Config::Get().sync_logging) {
    if (mask) {
      if (spdlog::get_level() == spdlog::level::trace) {
        spdlog::debug("SYNC-{}: Adding header - '{}' [not redacted]", type, combined);
      } else {
        spdlog::debug("SYNC-{}: Adding header - '{}: {}' [redacted]", type, header, "******");
      }
    } else {
      spdlog::debug("SYNC-{}: Adding header - '{}'", type, combined);
    }
  }

  return curl_slist_append(list, combined.c_str());
}

static void process_curl_response(const std::string& type, const std::string& label, const long code,
                                  const bool throw_error = false)
{
  if (code != CURLE_OK) {
    const auto text = "Failed to " + label + " - Code " + std::to_string(code);
    sync_log_warn(type, text);

    if (throw_error) {
      throw std::runtime_error(text);
    }
  }
}

static const std::string CURL_TYPE_UPLOAD   = "UPLOAD";
static const std::string CURL_TYPE_DOWNLOAD = "DOWNLOAD";

static CURL* get_curl_client_sync(const std::string& target)
{
  CURL* httpClient = curl_easy_init();

  auto proxy = Config::Get().sync_proxy;
  if (!proxy.empty()) {
    process_curl_response(type, "set proxy", curl_easy_setopt(httpClient, CURLOPT_PROXY, proxy.c_str()), true);
    process_curl_response(type, "set verifypeer", curl_easy_setopt(httpClient, CURLOPT_SSL_VERIFYPEER, false));
  }

  // Setting the HTTP/2 TLS option doesn't seem to work right now...
  // process_curl_response(type, "set TLS", curl_easy_setopt(httpClient, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS));

  process_curl_response(type, "set UserAgent", curl_easy_setopt(httpClient, CURLOPT_USERAGENT, "stfc community patch"));

  if (spdlog::get_level() == spdlog::level::trace) {
    sync_log_warn(type, "Sending data to " + url);
  } else {
    sync_log_info(type, "Sending data to " + url);
  }

  process_curl_response(type, "set url", curl_easy_setopt(httpClient, CURLOPT_URL, url.c_str()));

  return httpClient;
}

static size_t curl_write_to_string(void* contents, size_t size, size_t nmemb, std::string* s)
{
  size_t newLength = size * nmemb;
  s->append(static_cast<char*>(contents), newLength);
  return newLength;
}

static void send_data(const std::wstring& post_data)
{
  static auto loggedUrl = false;
  if (Config::Get().sync_targets.empty()) {
    if (!loggedUrl) {
      loggedUrl = true;
      sync_log_warn(CURL_TYPE_UPLOAD, "No url found, will not attempt to send");
    }
    return;
  }

  for (const auto& sync_target : Config::Get().sync_targets) {
    try {
      const auto& url   = sync_target.first;
      const auto& token = sync_target.second;

      CURLClient httpClient(get_curl_client_sync(CURL_TYPE_UPLOAD, url));

      struct curl_slist* list = nullptr;

      list = sync_slist_append(CURL_TYPE_UPLOAD, list, "Content-Type", "application/json");
      list = sync_slist_append(CURL_TYPE_UPLOAD, list, "X-Powered-By", headers::poweredBy);

      if (!token.empty()) {
        list = sync_slist_append(CURL_TYPE_UPLOAD, list, "stfc-sync-token", token, true);
      }

      if (list) {
        process_curl_response(CURL_TYPE_UPLOAD, "set headers", curl_easy_setopt(httpClient, CURLOPT_HTTPHEADER, list));
      }

      auto post_data_str = to_string(post_data);
      process_curl_response(CURL_TYPE_UPLOAD, "set data",
                            curl_easy_setopt(httpClient, CURLOPT_POSTFIELDS, post_data_str.c_str()));

      process_curl_response(CURL_TYPE_UPLOAD, "send data", curl_easy_perform(httpClient), true);

      long http_code = 0;
      process_curl_response(CURL_TYPE_UPLOAD, "get response code",
                            curl_easy_getinfo(httpClient, CURLINFO_RESPONSE_CODE, &http_code));

      if (http_code < 200 || http_code >= 400) {
        process_curl_response(CURL_TYPE_UPLOAD, "communicate with server", http_code, true);
      } else if (http_code != 200 && Config::Get().sync_debug) {
        process_curl_response(CURL_TYPE_UPLOAD, "INFO:", http_code);
      }
#if _WIN32
    } catch (winrt::hresult_error const& ex) {
      ErrorMsg::SyncWinRT(sync_target.first.c_str(), ex);
    } catch (const std::wstring& sz) {
      ErrorMsg::SyncMsg(sync_target.first.c_str(), sz);
    }
#else
    } catch (const std::wstring& sz) {
      ErrorMsg::SyncMsg(sync_target.first.c_str(), sz);
    }
#endif
    catch (const std::runtime_error& e) {
      ErrorMsg::SyncRuntime(sync_target.first.c_str(), e);
    } catch (...) {
      ErrorMsg::SyncMsg(sync_target.first.c_str(), L"Unknown error");
    }
  }
}

static void send_data(const std::string& post_data)
{
  return send_data(to_wstring(post_data));
}

static void write_data(const std::string& file_data)
{
  const auto& file = Config::Get().sync_file;
  if (file.empty()) {
    return;
  }

  static std::mutex file_mutex;
  std::lock_guard lock(file_mutex);

  const auto& path = std::filesystem::path(file);
  std::ofstream sync_file(path, std::ios::app);
  if (!sync_file.is_open()) {
    sync_log_error("FILE", "Failed to open sync_file for append: " + path.string());
    return;
  }

  if (!file_data.empty()) {
    sync_file << file_data << std::endl;
  }

  if (!sync_file.good()) {
    sync_log_error("FILE", "Failed to write/flush to sync_file: " + path.string());
  }
}

static CURL* get_curl_client_scopely()
{

}

static std::wstring get_scopely_data(const std::string& path, const std::string& post_data)
{
  static auto loggedUrl = false;

  if (Config::Get().sync_targets.empty() && Config::Get().sync_file.empty()) {
    if (!loggedUrl) {
      loggedUrl = true;
      sync_log_warn(CURL_TYPE_DOWNLOAD, "Not retrieving data, no sync url or file");
    }
    return {};
  }

  boost::url url(headers::gameServerUrl);
  url.set_path(path);

  CURLClient httpClient(get_curl_client_sync(url.buffer()));
  process_curl_response(CURL_TYPE_DOWNLOAD, "set accept encoding", curl_easy_setopt(httpClient, CURLOPT_ACCEPT_ENCODING, ""));

  struct curl_slist* list = nullptr;
  list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "Content-Type", "application/json");

  if (!headers::instanceSessionId.empty()) {
    auto user_agent        = "UnityPlayer/" + headers::primeVersion + " (UnityWebRequest/1.0, libcurl/8.5.0-DEV)";
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "Host", url.host());
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-AUTH-SESSION-ID", headers::instanceSessionId, true);
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-TRANSACTION-ID", newUUID(), true);
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-Api-Key", headers::apiKey, true);
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-Unity-Version", headers::unityVersion);
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-PRIME-VERSION", headers::primeVersion);
#if __cpp_lib_format
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-Instance-ID", std::format("{:03}", headers::instanceId));
#else
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-Instance-ID", fmt::format("{:03}", instanceId));
#endif
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "User-Agent", user_agent);
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-PRIME-SYNC", "0");
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-Suppress-Codes", "1");

  }

  list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-Powered-By", "stfc community patch " VER_FILE_VERSION_STR " (libcurl/" LIBCURL_VERSION ")");

  if (list) {
    process_curl_response(CURL_TYPE_DOWNLOAD, "set headers", curl_easy_setopt(httpClient, CURLOPT_HTTPHEADER, list));
  }

  auto post_data_str = to_string(post_data);
  process_curl_response(CURL_TYPE_DOWNLOAD, "set data",
                        curl_easy_setopt(httpClient, CURLOPT_POSTFIELDS, post_data_str.c_str()));
  if (spdlog::get_level() == spdlog::level::trace) {
    sync_log_warn(CURL_TYPE_DOWNLOAD, "Message body - " + post_data_str);
  }

  std::string s;

  process_curl_response(CURL_TYPE_DOWNLOAD, "set write func",
                        curl_easy_setopt(httpClient, CURLOPT_WRITEFUNCTION, curl_write_to_string));
  process_curl_response(CURL_TYPE_DOWNLOAD, "set write var", curl_easy_setopt(httpClient, CURLOPT_WRITEDATA, &s));

  auto log_text = "Getting data for " + to_string(path);
  if (spdlog::get_level() == spdlog::level::trace) {
    log_text = log_text + " at " + to_string(original_url);
    sync_log_warn(CURL_TYPE_DOWNLOAD, log_text);
  } else {
    sync_log_info(CURL_TYPE_DOWNLOAD, log_text);
  }

  process_curl_response(CURL_TYPE_DOWNLOAD, "send data", curl_easy_perform(httpClient), true);

  long http_code = 0;
  process_curl_response(CURL_TYPE_DOWNLOAD, "get response code",
                        curl_easy_getinfo(httpClient, CURLINFO_RESPONSE_CODE, &http_code));

  if (http_code != 200) {
    process_curl_response(CURL_TYPE_DOWNLOAD, "communicate with server", http_code, true);
  }

  return to_wstring(s);

  return {};
}



} // namespace http

std::mutex              m;
std::condition_variable cv;
std::queue<std::string> sync_data_queue;

std::mutex              m2;
std::condition_variable cv2;
std::queue<uint64_t>    combat_log_data_queue;

NLOHMANN_JSON_NAMESPACE_BEGIN
template <typename T>
struct adl_serializer<google::protobuf::RepeatedField<T>> {
  static void to_json(json& j, const google::protobuf::RepeatedField<T>& proto) {
    j = json::array();

    for (const auto& v : proto) {
      j.push_back(v);
    }
  }

  static void from_json(const json& j, google::protobuf::RepeatedField<T>& proto) {
    if (j.is_array()) {
      for (const auto& v : j) {
        proto.Add(v.get<T>());
      }
    }
  }
};
NLOHMANN_JSON_NAMESPACE_END

void queue_data(const std::string& data)
{
  {
    std::lock_guard lk(m);
    sync_data_queue.push(data);
  }

  cv.notify_all();
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
  int64_t rank = -1;
  int64_t level = -1;
};

struct RankLevelShardsState {
  explicit RankLevelShardsState(const int32_t r = -1, const int32_t l = -1, const int32_t s = -1)
      : rank(l)
      , level(r)
      , shards(s)
  {
  }

  bool operator==(const RankLevelShardsState& other) const
  {
    return this->rank == other.rank && this->level == other.level && this->shards == other.shards;
  }

private:
  int32_t rank = -1;
  int32_t level = -1;
  int32_t shards = -1;
};

struct ShipState {
  explicit ShipState(const int32_t t = -1, const int32_t l = -1, const double_t lp = -1.0, const std::vector<int64_t>& c = {})
      : tier(t)
      , level(l)
      , level_percentage(lp)
      , components(c)
  {
  }

  bool operator==(const ShipState& other) const
  {
    return this->tier == other.tier && this->level == other.level && std::fabs(this->level_percentage - other.level_percentage) < 0.01 && this->components == other.components;
  }

private:
  int32_t              tier = -1;
  int32_t              level = -1;
  double_t             level_percentage = -1.0;
  std::vector<int64_t> components = {};
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
  std::lock_guard lock(previously_sent_battlelogs_mtx);

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
    std::lock_guard lock(previously_sent_battlelogs_mtx);
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

  } catch (const std::exception& e) {
    spdlog::error("Failed to save battles JSON: {}", e.what());
  } catch (...) {
    spdlog::error( "Unknown error while saving battles JSON.");
  }
}

using pmsg_buff_t = std::unique_ptr<std::string>;

void process_active_missions(pmsg_buff_t&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_set<int64_t> active_mission_states;
  static std::mutex active_mission_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ActiveMissionsResponse();
         response.ParseFromString(*bytes)) {

    std::unordered_set<int64_t> active_missions;
    for (const auto& mission : response.activemissions()) {
      active_missions.insert(mission.id());
    }

    bool changed = false;
    {
      std::lock_guard lock(active_mission_states_mtx);
      if (active_mission_states != active_missions) {
        changed = true;
        active_mission_states = std::move(active_missions);
      }
    }

    if (changed) {
      auto mission_array = json::array();
      for (const auto& mission : active_missions) {
        mission_array.push_back({{"type", "active_mission"}, {"mid", mission}});
      }

      if (!mission_array.empty()) {
        queue_data(mission_array.dump());
      }
    }
  } else {
    spdlog::error("Failed to parse active missions");
  }
}

void process_completed_missions(pmsg_buff_t&& bytes)
{
  using json = nlohmann::json;
  static std::vector<int64_t> completed_mission_states;
  static std::mutex completed_mission_states_mtx;

  if (auto response = Digit::PrimeServer::Models::CompletedMissionsResponse();
      response.ParseFromString(*bytes)) {

    const auto& missions = response.completedmissions();
    std::vector<int64_t> completed_missions{missions.begin(), missions.end()};
    std::vector<int64_t> diff;

    // Assume the completed missions list is append-only: new entries may be added, but existing ones are never removed.
    {
      std::lock_guard lock(completed_mission_states_mtx);
      std::ranges::set_difference(completed_missions, completed_mission_states, std::back_inserter(diff));

      if (!diff.empty()) {
        completed_mission_states = std::move(completed_missions);
      }
    }

    if (!diff.empty()) {
      auto mission_array = json::array();

      for (const auto mission : diff) {
        mission_array.push_back({{"type", "mission"}, {"mid", mission}});
      }

      queue_data(mission_array.dump());
    }
  } else {
    spdlog::error("Failed to parse completed missions");
  }
}

void process_player_inventories(pmsg_buff_t&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<std::pair<std::underlying_type_t<Digit::PrimeServer::Models::InventoryItemType>, int64_t>, int64_t, pairhash> inventory_states;
  static std::mutex inventory_states_mtx;

  if (auto response = Digit::PrimeServer::Models::InventoryResponse();
      response.ParseFromString(*bytes)) {

    auto inventory_items = json::array();
    {
      std::lock_guard lock(inventory_states_mtx);

      for (const auto& inventory : response.inventories() | std::views::values) {
        for (const auto& item : inventory.items()) {
          if (item.has_commonparams()) {
            const auto item_id = item.commonparams().refid();
            const auto count   = item.count();
            const auto key = std::make_pair(item.type(), item_id);

            if (const auto& it = inventory_states.find(key); it == inventory_states.end() || it->second != count) {
              inventory_states[key] = count;
              inventory_items.push_back({{"type", "inventory"}, {"item_type", item.type()}, {"refid", item_id}, {"count", count}});
            }
          }
        }
      }
    }

    if (!inventory_items.empty()) {
      queue_data(inventory_items.dump());
    }
  } else {
    spdlog::error("Failed to parse player inventories");
  }
}

void process_research_trees_state(pmsg_buff_t&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int32_t> research_states;
  static std::mutex research_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ResearchTreesState();
      response.ParseFromString(*bytes)) {

    auto research_array = json::array();
    {
      std::lock_guard lock(research_states_mtx);

      for (const auto& [id, level] : response.researchprojectlevels()) {
        if (const auto& it = research_states.find(id); it == research_states.end() || it->second != level) {
          research_states[id] = level;
          research_array.push_back({{"type", "research"}, {"rid", id}, {"level", level}});
        }
      }
    }

    if (!research_array.empty()) {
      queue_data(research_array.dump());
    }
  } else {
    spdlog::error("Failed to parse research trees state");
  }
}

void process_officers(pmsg_buff_t&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<uint64_t, RankLevelShardsState> officer_states;
  static std::mutex officer_states_mtx;

  if (auto response = Digit::PrimeServer::Models::OfficersResponse();
      response.ParseFromString(*bytes)) {

    auto officers_array = json::array();
    {
      std::lock_guard lock(officer_states_mtx);

      for (const auto& officer : response.officers()) {
        const RankLevelShardsState officer_state{officer.rankindex(), officer.level(), officer.shardcount()};

        if (const auto& it = officer_states.find(officer.id()); it == officer_states.end() || it->second != officer_state) {
          officer_states[officer.id()] = officer_state;
          officers_array.push_back({{"type", "officer"},
                                    {"oid", officer.id()},
                                    {"rank", officer.rankindex()},
                                    {"level", officer.level()},
                                    {"shard_count", officer.shardcount()}});
        }
      }
    }

    if (!officers_array.empty()) {
      queue_data(officers_array.dump());
    }
  } else {
    spdlog::error("Failed to parse officers");
  }
}

void process_forbidden_techs(pmsg_buff_t&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<uint64_t, RankLevelShardsState> tech_states;
  static std::mutex tech_states_mtx;

  if (auto response = Digit::PrimeServer::Models::ForbiddenTechsResponse();
      response.ParseFromString(*bytes)) {

    auto tech_array = json::array();
    {
      std::lock_guard lock(tech_states_mtx);

      for (const auto& tech : response.forbiddentechs()) {
        const RankLevelShardsState tech_state{tech.tier(), tech.level(), tech.shardcount()};

        if (const auto& it = tech_states.find(tech.id()); it == tech_states.end() || it->second != tech_state) {
          tech_states[tech.id()] = tech_state;
          tech_array.push_back({{"type", "tech"},
                                {"tid", tech.id()},
                                {"tier", tech.tier()},
                                {"level", tech.level()},
                                {"shard_count", tech.shardcount()}});
        }
      }
    }

    if (!tech_array.empty()) {
      queue_data(tech_array.dump());
    }
  } else {
    spdlog::error("Failed to parse forbidden techs");
  }
}

void process_active_officer_traits(pmsg_buff_t&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<std::pair<int64_t, int64_t>, int32_t, pairhash> trait_states;
  static std::mutex trait_states_mtx;

  if (auto response = Digit::PrimeServer::Models::OfficerTraitsResponse();
      response.ParseFromString(*bytes)) {

    auto trait_array = json::array();
    {
      std::lock_guard lock(trait_states_mtx);

      for (const auto& [officer_id, officer_traits] : response.activeofficertraits()) {
        for (const auto& trait : officer_traits.activetraits() | std::views::values) {
          const auto& key = std::make_pair(officer_id, trait.traitid());

          if (const auto& it = trait_states.find(key); it == trait_states.end() || it->second != trait.level()) {
            trait_states[key] = trait.level();
            trait_array.push_back({{"type", "trait"}, {"oid", officer_id}, {"tid", trait.traitid()}, {"level", trait.level()}});
          }
        }
      }
    }

    if (!trait_array.empty()) {
      queue_data(trait_array.dump());
    }
  } else {
    spdlog::error("Failed to parse active officer traits");
  }
}

void process_global_active_buffs(pmsg_buff_t&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int32_t> buff_states;
  static std::mutex buff_states_mtx;

  if (auto response = Digit::PrimeServer::Models::GlobalActiveBuffsResponse();
        response.ParseFromString(*bytes)) {

    auto buff_array = json::array();
    {
      std::lock_guard lock(buff_states_mtx);

      for (const auto& buff : response.globalactivebuffs()) {
        if (const auto& it = buff_states.find(buff.buffid()); it == buff_states.end() || it->second != buff.level()) {
          buff_states[buff.buffid()] = buff.level();
          buff_array.push_back({{"type", "active_buff"}, {"bid", buff.buffid()}, {"level", buff.level()}});
        }
      }
    }

    if (!buff_array.empty()) {
      queue_data(buff_array.dump());
    }
  } else {
    spdlog::error("Failed to parse global active buffs");
  }
}

void process_entity_slots(pmsg_buff_t&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int64_t> slot_states;
  static std::mutex slot_states_mtx;

  if (auto response = Digit::PrimeServer::Models::EntitySlots();
        response.ParseFromString(*bytes)) {

    auto slot_array = json::array();
    {
      std::lock_guard lock(slot_states_mtx);

      for (const auto& slot : response.entityslots_()) {
        json slot_params;

        switch (slot.slottype()) {
          case Digit::PrimeServer::Models::SLOTTYPE_CONSUMABLE:
            if (slot.has_consumableslotparams()) {
              slot_params["expiry_time"] = slot.consumableslotparams().expirytime().seconds();
            }
            break;
          case Digit::PrimeServer::Models::SLOTTYPE_OFFICERPRESET:
            if (slot.has_officerpresetslotparams()) {
              const auto& preset = slot.officerpresetslotparams();

              slot_params = {
                {"name", preset.name()},
                {"order", preset.order()},
                {"officer_ids", preset.officerids()}
              };
            }
            break;
          case Digit::PrimeServer::Models::SLOTTYPE_FLEETCOMMANDER:
            if (slot.has_fleetcommanderslotparams()) {
              slot_params["order"] = slot.fleetcommanderslotparams().order();
            }
            break;
          case Digit::PrimeServer::Models::SLOTTYPE_SELECTABLESKILL:
            if (slot.has_selectableskillslotparams()) {
              slot_params["cooldown_expiration"] = slot.selectableskillslotparams().cooldownexpiration().seconds();
            }
            break;
          case Digit::PrimeServer::Models::SLOTTYPE_FLEETPRESET:
            if (slot.has_fleetpresetslotparams()) {
              const auto& preset = slot.fleetpresetslotparams();
              auto setup_json = json::array();

              for (const auto& setup : preset.setups()) {
                setup_json.push_back({
                  {"drydock_id", setup.drydockid()},
                  {"ship_id", setup.shipids()[0]},
                  {"officer_ids", setup.officerids()}
                });
              }

              slot_params = {
                {"name", preset.name()},
                {"order", preset.order()},
                {"setup", setup_json}
              };
            }
          default:
            continue;
        }

        if (const auto& it = slot_states.find(slot.id()); it == slot_states.end() || it->second != slot.slotitemid()) {
          slot_states[slot.id()] = slot.slotitemid();
          slot_array.push_back({
            {"type", "slot"},
            {"slot_type", slot.slottype()},
            {"sid", slot.id()},
            {"spec_id", slot.slotspecid()},
            {"item_id", slot.has_slotitemid() ? json(slot.slotitemid()) : json(nullptr)},
            {"params", slot_params}
          });
        }
      }
    }

    if (!slot_array.empty()) {
      queue_data(slot_array.dump());
    }
  } else {
    spdlog::error("Failed to parse entity slots");
  }
}

void process_jobs(pmsg_buff_t&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_set<std::string> jobs_active;
  static std::mutex                      jobs_active_mtx;

  if (auto response = Digit::PrimeServer::Models::JobResponse();
    response.ParseFromString(*bytes)) {

    std::unordered_set<std::string> uuids_in_response;
    uuids_in_response.reserve(response.jobs_size());
    auto job_array = json::array();

    for (const auto& job : response.jobs()) {
      const std::string& uuid = job.uuid();
      uuids_in_response.insert(uuid);

      bool emit = false;
      {
        std::lock_guard lock(jobs_active_mtx);
        emit = jobs_active.insert(uuid).second;
      }

      if (!emit) {
        continue;
      }

      json job_params;

      switch (job.type()) {
        case Digit::PrimeServer::Models::JOBTYPE_RESEARCH: {
          const auto& research = job.researchparams();
          job_params= {{"rid", research.projectid()}, {"level", research.level()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_STARBASECONSTRUCTION: {
          const auto& construction = job.starbaseconstructionparams();
          job_params= {{"bid", construction.moduleid()}, {"level", construction.level()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_SHIPTIERUP: {
          const auto& upgrade = job.tierupshipparams();
          job_params= {{"psid", upgrade.shipid()}, {"tier", upgrade.newtier()}};
        } break;
        case Digit::PrimeServer::Models::JOBTYPE_SHIPSCRAP: {
          const auto& scrap = job.scrapyardparams();
          job_params = {{"psid", scrap.shipid()}, {"hull_id", scrap.hullid()}, {"level", scrap.level()}};
        } break;
        default:
          continue;
      }

      json job_data = json::object({{"type", "job"},
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
          it = jobs_active.erase(it);
        } else {
          ++it;
        }
      }
    }

    if (!job_array.empty()) {
      queue_data(job_array.dump());
    }
  } else {
    spdlog::error("Failed to parse jobs");
  }
}

void process_json(pmsg_buff_t&& bytes)
{
  using json = nlohmann::json;
  static std::unordered_map<int64_t, int64_t> resource_states;
  static std::unordered_map<int64_t, int32_t> module_states;
  static std::unordered_map<int64_t, ShipState> ship_states;
  static std::mutex resource_states_mtx;
  static std::mutex module_states_mtx;
  static std::mutex ship_states_mtx;

  try {
    const auto result = json::parse(bytes->begin(), bytes->end());

    for (const auto& [key, section] : result.items()) {
      if (key == "battle_result_headers") {
        if (!Config::Get().sync_battlelogs) {
          continue;
        }

        std::vector<uint64_t> battle_ids;
        battle_ids.reserve(section.size());

        for (const auto& battle : section) {
          const auto id = battle["id"].get<uint64_t>();
          battle_ids.push_back(id);
        }

        std::vector<uint64_t> to_enqueue;
        {
          std::lock_guard lock(previously_sent_battlelogs_mtx);

          for (const auto id : battle_ids | std::views::reverse) {
            if (eastl::find(previously_sent_battlelogs.begin(), previously_sent_battlelogs.end(), id) == previously_sent_battlelogs.end()) {
              previously_sent_battlelogs.push_back(id);;
              to_enqueue.push_back(id);
            }
          }
        }

        if (!to_enqueue.empty()) {
          {
            std::lock_guard lock(m2);
            for (const auto id : to_enqueue) {
              combat_log_data_queue.push(id);
            }
          }

          save_previously_sent_logs();
          cv2.notify_all();
        }

      } else if (key == "resources") {
        if (!Config::Get().sync_resources) {
          continue;
        }

        auto resource_array = json::array();
        {
          std::lock_guard lock(resource_states_mtx);

          for (const auto& [str_id, resource] : section.get<json::object_t>()) {
            auto id = std::stoll(str_id);
            auto amount = resource["current_amount"].get<int64_t>();

            const auto prevResourceAmountIter = resource_states.find(id);
            const auto hadResource = (prevResourceAmountIter != resource_states.end() && prevResourceAmountIter->second != 0);
            const auto amountChanged = amount > 0 && (prevResourceAmountIter == resource_states.end() || prevResourceAmountIter->second != amount);
            const auto resourceDepleted = hadResource && amount == 0;

            if (resourceDepleted || amountChanged) {
              resource_states[id] = amount;
              resource_array.push_back({{"type", "resource"}, {"rid", id}, {"amount", amount}});
            }
          }
        }

        if (!resource_array.empty()) {
          queue_data(resource_array.dump());
        }

      } else if (key == "starbase_modules") {
        if (!Config::Get().sync_buildings) {
          continue;
        }

        auto starbase_array = json::array();
        {
          std::lock_guard lock(module_states_mtx);

          for (const auto& module : section.get<json::object_t>() | std::views::values) {
            const auto id = module["id"].get<int64_t>();
            const auto level = module["level"].get<int32_t>();

            if (const auto &it = module_states.find(id); it == module_states.end() || it->second != level) {
              module_states[id] = level;
              starbase_array.push_back({{"type", "module"}, {"bid", id}, {"level", level}});
            }
          }
        }

        if (!starbase_array.empty()) {
          queue_data(starbase_array.dump());
        }

      } else if (key == "ships") {
        if (!Config::Get().sync_ships) {
          continue;
        }

        auto ship_array = json::array();
        {
          std::lock_guard lock(ship_states_mtx);

          for (const auto& ship : section.get<json::object_t>() | std::views::values) {
            const auto id= ship["id"].get<int64_t>();
            const auto tier = ship["tier"].get<int32_t>();
            const auto level = ship["level"].get<int32_t>();
            const auto level_percentage = ship["level_percentage"].get<double_t>();
            const auto components = ship["components"].get<std::vector<int64_t>>();
            const ShipState state{tier, level, level_percentage, components};

            if (const auto& it = ship_states.find(id); it == ship_states.end() || it->second != state) {
              ship_states[id] = state;
              ship_array.push_back({{"type", "ship"},
                                    {"psid", id},
                                    {"level", level},
                                    {"tier", tier},
                                    {"hull_id", ship["hull_id"].get<int64_t>()},
                                    {"components", components}});
            }
          }
        }

        if (!ship_array.empty()) {
          queue_data(ship_array.dump());
        }
      }
    }
  } catch (const json::exception& e) {
    spdlog::error("Error parsing json: %s", e.what());
  }
}



void ship_sync_data()
{
#if _WIN32
  winrt::init_apartment();
#endif

  for (;;) {
    {
      std::unique_lock lk(m);
      cv.wait(lk, []() { return !sync_data_queue.empty(); });
    }
    const auto sync_data = ([&] {
      std::lock_guard lk(m);
      auto            data = sync_data_queue.front();
      sync_data_queue.pop();
      return data;
    })();
    try {
      http::write_data(sync_data);
      http::send_data(sync_data);
#if _WIN32
    } catch (winrt::hresult_error const& ex) {
      ErrorMsg::SyncWinRT("ship", ex);
    } catch (const std::wstring& sz) {
      ErrorMsg::SyncMsg("ship", sz);
    }
#else
    } catch (const std::wstring& sz) {
      ErrorMsg::SyncMsg("ship", sz);
    }
#endif
    catch (const std::runtime_error& e) {
      ErrorMsg::SyncRuntime("ship", e);
    }
  }
#if _WIN32
  winrt::uninit_apartment();
#endif
}

void ship_combat_log_data()
{
  using json = nlohmann::json;

#if _WIN32
  winrt::init_apartment();
#endif

  for (;;) {
    {
      std::unique_lock lk(m2);
      cv2.wait(lk, [] { return !combat_log_data_queue.empty(); });
    }

    try {
      const auto sync_data = [&] {
        std::lock_guard lk(m2);
        const auto      data = combat_log_data_queue.front();
        combat_log_data_queue.pop();
        return data;
      }();

      const json journals_body{{"journal_id", sync_data}};
      auto battle_log = http::get_scopely_data("/journals/get", journals_body.dump());

      auto battle_json = json::parse(battle_log);
      const auto& journal = battle_json["journal"];
      const auto& target_fleet_data    = journal["target_fleet_data"];
      const auto& initiator_fleet_data = journal["initiator_fleet_data"];

      json profiles_body{{"user_ids", json::array()}};
      auto& user_ids = profiles_body["user_ids"];

      if (target_fleet_data["ref_ids"].is_null()) {
        for (const auto& fleet : target_fleet_data["deployed_fleets"]) {
          const auto& player_id = fleet["uid"];
          user_ids.push_back(player_id);
        }
      }

      if (initiator_fleet_data["ref_ids"].is_null()) {
        for (const auto& fleet : initiator_fleet_data["deployed_fleets"]) {
          const auto& player_id = fleet["uid"];
          user_ids.push_back(player_id);
        }
      }

      auto profiles = http::get_scopely_data("/user_profile/profiles", profiles_body.dump());
      auto profiles_json = json::parse(profiles);
      auto names = json::object();

      for (const auto& [player_id, profile] : profiles_json["user_profiles"].get<json::object_t>()) {
        names[player_id] = profile["name"];
      }

      auto ship_array  = json::array();
      ship_array.push_back({{"type", "battlelog"}, {"names", names}, {"journal", battle_json["journal"]}});

      try {
        auto ship_data = ship_array.dump();
        http::write_data(ship_data);
        http::send_data(ship_data);
#if _WIN32
      } catch (winrt::hresult_error const& ex) {
        ErrorMsg::SyncWinRT("combat", ex);
      } catch (const std::wstring& sz) {
        ErrorMsg::SyncMsg("combat", sz);
      }
#else
      } catch (const std::wstring& sz) {
        ErrorMsg::SyncMsg("combat", sz);
      }
#endif
      catch (const std::runtime_error& e) {
        ErrorMsg::SyncRuntime("combat", e);
      }
    } catch (...) {
    }
  }

#if _WIN32
  winrt::uninit_apartment();
#endif
}

void HandleEntityGroup(EntityGroup* entity_group)
{
  if (entity_group == nullptr || entity_group->Group == nullptr || entity_group->Group->bytes == nullptr || entity_group->Group->Length <= 0) {
    return;
  }

  const auto byteCount = static_cast<size_t>(entity_group->Group->Length);
  auto bytesPtr = reinterpret_cast<const char*>(entity_group->Group->bytes->m_Items);

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
      if (Config::Get().sync_missions) {
        submit_async(process_active_missions);
      }
      break;
    case EntityGroup::Type::CompletedMissions:
      if (Config::Get().sync_missions) {
        submit_async(process_completed_missions);
      }
      break;
    case EntityGroup::Type::PlayerInventories:
      if (Config::Get().sync_inventory) {
        submit_async(process_player_inventories);
      }
      break;
    case EntityGroup::Type::ResearchTreesState:
      if (Config::Get().sync_research) {
        submit_async(process_research_trees_state);
      }
      break;
    case EntityGroup::Type::Officers:
      if (Config::Get().sync_officer) {
        submit_async(process_officers);
      }
      break;
    case EntityGroup::Type::ForbiddenTechs:
      if (Config::Get().sync_tech) {
        submit_async(process_forbidden_techs);
      }
      break;
    case EntityGroup::Type::ActiveOfficerTraits:
      if (Config::Get().sync_traits) {
        submit_async(process_active_officer_traits);
      }
      break;
    case EntityGroup::Type::Json:
      if (const auto& c = Config::Get(); c.sync_battlelogs || c.sync_buildings || c.sync_resources || c.sync_ships) {
        submit_async(process_json);
      }
      break;
    case EntityGroup::Type::Jobs:
      if (Config::Get().sync_jobs) {
        submit_async(process_jobs);
      }
      break;
    case EntityGroup::Type::GlobalActiveBuffs:
      if (Config::Get().sync_buffs) {
        submit_async(process_global_active_buffs);
      }
      break;
    case EntityGroup::Type::EntitySlots:
      if (Config::Get().sync_slots) {
        submit_async(process_entity_slots);
      }
      break;
    default:
      break;
  }
}

void MissionsDataContainer_ParseBinaryObject(auto original, void* _this, EntityGroup* group, bool isPlayerData)
{
  HandleEntityGroup(group);
  return original(_this, group, isPlayerData);
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

void GameServer_Initialise(auto original, void* _this, Il2CppString* sessionId, Il2CppString* gameVersion, bool encryptRequests)
{
  original(_this, sessionId, gameVersion, encryptRequests);
  http::headers::primeVersion = to_string(to_wstring(gameVersion));
}

void GameServer_SetInstanceIdHeader(auto original, void* _this, int32_t instanceId)
{
  original(_this, instanceId);
  http::headers::instanceId = instanceId;
}

void SetPlatformAPIKey(Il2CppString* platformApiKey)
{
  http::headers::apiKey = to_string(to_wstring(platformApiKey));
}

void InstallSyncPatches()
{
  curl_global_init(CURL_GLOBAL_ALL);
  load_previously_sent_logs();

  if (auto missions_data_container = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "MissionsDataContainer");
    !missions_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Models", "MissionsDataContainer");
  } else {
    if (const auto ptr = missions_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("MissionsDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, MissionsDataContainer_ParseBinaryObject);
    }
  }

  auto inventory_data_container =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "InventoryDataContainer");
  if (!inventory_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "InventoryDataContainer");
  } else {
    if (const auto ptr = inventory_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("InventoryDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, MissionsDataContainer_ParseBinaryObject);
    }
  }

  if (auto research_data_container = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "ResearchDataContainer");
    !research_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "ResearchDataContainer");
  } else {
    if (const auto ptr = research_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingHelper("ResearchDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, MissionsDataContainer_ParseBinaryObject);
    }
  }

  if (auto research_service = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "ResearchService");
      !research_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "ResearchService");
  } else {
    if (const auto ptr = research_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("ResearchService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, MissionsDataContainer_ParseBinaryObject);
    }
  }

  if (auto game_server_model_registry = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServerModelRegistry");
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

  if (auto platform_model_registry = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Core", "PlatformModelRegistry");
    !platform_model_registry.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "PlatformModelRegistry");
  } else {
    if (const auto ptr = platform_model_registry.GetMethod("ProcessResultInternal"); ptr == nullptr) {
      ErrorMsg::MissingMethod("PlatformModelRegistry", "ProcessResultInterval");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ProcessResultInternal);
    }
  }

  if (auto slot_data_container = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "SlotDataContainer");
    !slot_data_container.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "SlotDataContainer");
  } else {
    if (const auto ptr = slot_data_container.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("SlotDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, MissionsDataContainer_ParseBinaryObject);
    }
  }

  if ( auto buff_service = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService");
      !buff_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "BuffService");
  } else {
    if (const auto ptr = buff_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("BuffService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, MissionsDataContainer_ParseBinaryObject);
    }
  }

  if (auto job_service = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "JobService");
      !job_service.isValidHelper()) {
    ErrorMsg::MissingHelper("Services", "JobService");
  } else {
    if (const auto ptr = job_service.GetMethod("ParseBinaryObject"); ptr == nullptr) {
      ErrorMsg::MissingMethod("JobService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, MissionsDataContainer_ParseBinaryObject);
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

  if (auto game_server = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServer");
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
}
