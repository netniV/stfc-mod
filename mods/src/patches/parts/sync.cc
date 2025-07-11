#include "config.h"
#include "errormsg.h"
#include "file.h"
#include "il2cpp-api-types.h"

#include <il2cpp/il2cpp_helper.h>

#include <Digit.PrimeServer.Models.pb.h>
#include <prime/EntityGroup.h>
#include <prime/HttpResponse.h>
#include <prime/Hub.h>
#include <prime/LanguageManager.h>
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
#endif

#include <curl/curl.h>

#include <condition_variable>
#include <format>
#include <fstream>
#include <ostream>
#include <queue>
#include <string>

#if !__cpp_lib_format
#include <spdlog/fmt/fmt.h>
#endif

namespace http
{

struct CURLClient {
  CURLClient(CURL* handle)
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

#ifdef WIN32
#include <Rpc.h>
#else
#include <uuid/uuid.h>
#endif

static std::wstring instanceSessionId;
static std::wstring gameServerUrl;
static int32_t      instanceId;

static std::string newUUID()
{
#ifdef WIN32
  UUID uuid;
  UuidCreate(&uuid);

  unsigned char* str;
  UuidToStringA(&uuid, &str);

  std::string s((char*)str);

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

static struct curl_slist* sync_slist_append(std::string type, struct curl_slist* list, std::string header,
                                            std::string data, bool mask = false)
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

static void process_curl_response(std::string type, std::string label, long code, bool throw_error = false)
{
  if (code != CURLE_OK) {
    auto text = "Failed to " + label + " - Code " + std::to_string((long)code);
    text      = text;

    sync_log_warn(type, text);
    if (throw_error) {
      throw std::runtime_error("Failed to " + label);
    }
  }
}

static CURL* sync_init(std::string type, std::string url)
{
  CURL* httpClient = curl_easy_init();

  auto proxy = Config::Get().sync_proxy;
  if (!proxy.empty()) {
    process_curl_response(type, "set proxy", curl_easy_setopt(httpClient, CURLOPT_PROXY, proxy.c_str()), true);
    process_curl_response(type, "set verifypeer", curl_easy_setopt(httpClient, CURLOPT_SSL_VERIFYPEER, false));
  }

  // Setting thee HTTP/2 TLS option doesn't esem to work right now...
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

static const std::string CURL_TYPE_UPLOAD   = "UPLOAD";
static const std::string CURL_TYPE_DOWNLOAD = "DOWNLOAD";
static const std::string UNITY_VERSION      = "2020.3.18f1-digit-multiple-fixes-build";
static const std::string PRIME_VERSION      = "1.000.33037";
static const std::string PRIME_API_KEY      = "meh";

static void send_data(std::wstring post_data)
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

      CURLClient httpClient(sync_init(CURL_TYPE_UPLOAD, url));

      struct curl_slist* list = NULL;

      list = sync_slist_append(CURL_TYPE_UPLOAD, list, "Content-Type", "application/json");

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

      if (http_code != 200) {
        process_curl_response(CURL_TYPE_UPLOAD, "communicate with server", http_code, true);
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

static size_t curl_write_to_string(void* contents, size_t size, size_t nmemb, std::string* s)
{
  size_t newLength = size * nmemb;
  s->append((char*)contents, newLength);
  return newLength;
}

static std::wstring get_scopely_data(std::wstring session, std::wstring url, std::wstring path, std::wstring post_data)
{
  static auto loggedUrl = false;

  if (Config::Get().sync_targets.empty() && Config::Get().sync_file.empty()) {
    if (!loggedUrl) {
      loggedUrl = true;
      sync_log_warn(CURL_TYPE_DOWNLOAD, "Not retreiving data, no sync url or file");
    }
    return {};
  }

  std::wstring original_url = url;
  if (url.ends_with(L"/")) {
    url = url.substr(0, url.length() - 1);
    url += path;
  } else {
    url += path;
  }

  CURLClient httpClient(sync_init(CURL_TYPE_DOWNLOAD, to_string(url)));

  struct curl_slist* list = NULL;

  list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "Content-Type", "application/json");

  if (!session.empty()) {
    auto session_id_header = to_string(session);
    auto user_agent        = "UnityPlayer/" + UNITY_VERSION + " (UnityWebRequest / 1.0, libcurl / 7.75.0 - DEV)";

    auto game_server_url = to_string(gameServerUrl);
    game_server_url      = game_server_url.substr(8);

    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "Host", game_server_url.c_str());
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-AUTH-SESSION-ID", session_id_header.c_str(), true);
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-TRANSACTION-ID", newUUID().c_str(), true);
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-Api-Key", PRIME_API_KEY);
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-Unity-Version", UNITY_VERSION);
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-PRIME-VERSION", PRIME_VERSION);
#if __cpp_lib_format
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-Instance-ID", std::format("{:03}", instanceId));
#else
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-Instance-ID", fmt::format("{:03}", instanceId));
#endif
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-PRIME-SYNC", "0");
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "X-Suppress-Codes", "1");
    list = sync_slist_append(CURL_TYPE_DOWNLOAD, list, "User-Agent", user_agent);
  }

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

static void send_data(std::string post_data)
{
  return send_data(to_wstring(post_data));
}

static void write_data(std::string file_data)
{
  if (!Config::Get().sync_file.empty()) {
    std::ofstream sync_file;
    sync_file.open(Config::Get().sync_file, std::ios_base::app);

    sync_file << file_data << "\n\n";
    sync_file.close();
  }
}

} // namespace http

std::mutex              m;
std::condition_variable cv;
std::queue<std::string> sync_data_queue;

std::mutex              m2;
std::condition_variable cv2;
std::queue<uint64_t>    combat_log_data_queue;

void queue_data(std::string data)
{
  if (data == "[]")
    return;

  {
    std::lock_guard lk(m);
    sync_data_queue.push(data);
  }
  cv.notify_all();
}

void HandleEntityGroup(EntityGroup* entity_group);

void MissionsDataContainer_ParseBinaryObject(auto original, void* _this, EntityGroup* group, bool isPlayerData)
{
  HandleEntityGroup(group);
  return original(_this, group, isPlayerData);
}

void GameServerModelRegistry_ProcessResultInternal(auto original, void* _this, HttpResponse* http_response,
                                                   ServiceResponse* service_response, void* callback,
                                                   void* callback_error)
{
  auto entity_groups = service_response->EntityGroups;
  for (int i = 0; i < entity_groups->Count; ++i) {
    auto entity_group = entity_groups->get_Item(i);
    HandleEntityGroup(entity_group);
  }

  return original(_this, http_response, service_response, callback, callback_error);
}
void GameServerModelRegistry_HandleBinaryObjects(auto original, void* _this, ServiceResponse* service_response)
{
  auto entity_groups = service_response->EntityGroups;
  for (int i = 0; i < entity_groups->Count; ++i) {
    auto entity_group = entity_groups->get_Item(i);
    HandleEntityGroup(entity_group);
  }

  return original(_this, service_response);
}

struct ResourceState {
  ResourceState(int64_t amount = -1)
      : amount(amount)
  {
  }

  inline operator int64_t() const
  {
    return amount;
  }

private:
  int64_t amount = -1;
};

struct RankLevelState {
  RankLevelState(int64_t a = -1, int64_t b = -1)
      : a(a)
      , b(b)
  {
  }

  bool operator==(const RankLevelState& other) const
  {
    return this->a == other.a && this->b == other.b;
  }

private:
  int64_t a = -1;
  int64_t b = -1;
};

struct pairhash {
public:
  template <typename T, typename U> std::size_t operator()(const std::pair<T, U>& x) const
  {
    return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
  }
};

static std::unordered_map<uint64_t, ResourceState>                               resource_states;
static std::unordered_map<uint64_t, ResourceState>                               module_states;
static std::unordered_map<uint64_t, RankLevelState>                              officer_states;
static std::unordered_map<uint64_t, RankLevelState>                              ft_states;
static std::unordered_map<std::pair<int64_t, int64_t>, RankLevelState, pairhash> trait_states;
static std::unordered_set<int64_t>                                               mission_completed;
static std::unordered_set<int64_t>                                               mission_active;
static std::unordered_set<uint64_t>                                              battlelog_states;

static eastl::ring_buffer<uint64_t> previously_sent_battlelogs;
static eastl::ring_buffer<uint64_t> previously_sent_missions;

static void load_previously_sent_logs()
{
  previously_sent_battlelogs.set_capacity(300);
  using json = nlohmann::json;
  try {
    std::ifstream file(File::Battles());
    std::string   battlelog_json;
    file >> battlelog_json;
    const auto battlelogs = json::parse(battlelog_json);
    for (auto v : battlelogs) {
      previously_sent_battlelogs.push_back(v.get<uint64_t>());
    }
  } catch (...) {
  }
}

static void save_previously_sent_logs()
{
  using json           = nlohmann::json;
  auto battlelog_array = json::array();
  for (auto id : previously_sent_battlelogs) {
    battlelog_array.push_back(id);
  }
  std::ofstream file(File::Battles());
  file << battlelog_array.dump();
  file.close();
}

void sync_active_missions()
{
  using json = nlohmann::json;

  auto mission_array = json::array();

  for (const auto id : mission_active) {
    mission_array.push_back({{"type", "active_mission"}, {"mid", id}});
  }

  queue_data(mission_array.dump());
}

void HandleEntityGroup(EntityGroup* entity_group)
{
  using json = nlohmann::json;

  auto mission_sync = false;

  auto bytes = entity_group->Group;
  auto type  = entity_group->Type_;

  if (type == EntityGroup::Type::ActiveMissions) {
    auto response = Digit::PrimeServer::Models::ActiveMissionsResponse();
    if (response.ParseFromArray(bytes->bytes->m_Items, bytes->bytes->max_length)) {

      for (const auto& mission : response.activemissions()) {
        auto mission_update = !mission_active.contains(mission.id());
        if (!mission_update) {
          mission_sync = true;
          mission_active.insert(mission.id());
        }
      }
    }
  } else if (type == EntityGroup::Type::CompletedMissions) {
    auto response = Digit::PrimeServer::Models::CompletedMissionsResponse();
    if (response.ParseFromArray(bytes->bytes->m_Items, bytes->bytes->max_length)) {
      auto mission_array  = json::array();
      auto mission_update = false;
      for (const auto& mission : response.completedmissions()) {
        if (!mission_completed.contains(mission)) {
          if (mission_active.contains(mission)) {
            mission_sync = true;
            mission_active.erase(mission);
          }

          mission_update = true;
          mission_completed.insert(mission);
          mission_array.push_back({{"type", "mission"}, {"mid", mission}});
        }
      }

      if (Config::Get().sync_missions && mission_update) {
        queue_data(mission_array.dump());
      }
    }
  } else if (type == EntityGroup::Type::PlayerInventories) {
    auto response = Digit::PrimeServer::Models::InventoryResponse();
    if (response.ParseFromArray(bytes->bytes->m_Items, bytes->bytes->max_length)) {
      auto inventory_object = json();
      for (const auto& inventory : response.inventories()) {
        for (const auto& item : inventory.second.items()) {
          auto type = item.type();
          if (type == Digit::PrimeServer::Models::INVENTORYITEMTYPE_INVENTORYRESOURCE)
            inventory_object[std::to_string(item.commonparams().refid())] = item.count();
        }
      }
    }
  } else if (type == EntityGroup::Type::ResearchTreesState) {
    auto response = Digit::PrimeServer::Models::ResearchTreesState();
    if (response.ParseFromArray(bytes->bytes->m_Items, bytes->bytes->max_length)) {
      auto research_array = json::array();
      for (const auto& research : response.researchprojectlevels()) {
        research_array.push_back({{"type", "research"}, {"rid", research.first}, {"level", research.second}});
      }

      if (Config::Get().sync_research) {
        queue_data(research_array.dump());
      }
    }
  } else if (type == EntityGroup::Type::Officers) {
    auto response = Digit::PrimeServer::Models::OfficersResponse();
    if (response.ParseFromArray(bytes->bytes->m_Items, bytes->bytes->max_length)) {
      auto officers_array = json::array();
      for (const auto& officer : response.officers()) {
        if (officer.rankindex() == 0) {
          continue;
        }
        if (officer_states[officer.id()] != RankLevelState{officer.rankindex(), officer.level()}) {
          officer_states[officer.id()] = RankLevelState{officer.rankindex(), officer.level()};
          officers_array.push_back({{"type", "officer"},
                                    {"oid", officer.id()},
                                    {"rank", officer.rankindex()},
                                    {"level", officer.level()},
                                    {"shard_count", officer.shardcount()}});
        }
      }
      if (Config::Get().sync_officer) {
        queue_data(officers_array.dump());
      }
    }
  } else if (type == EntityGroup::Type::ForbiddenTechs) {
    auto response = Digit::PrimeServer::Models::ForbiddenTechsResponse();
    if (response.ParseFromArray(bytes->bytes->m_Items, bytes->bytes->max_length)) {
      auto ft_array = json::array();
      for (const auto& ft : response.forbiddentechs()) {
        if (ft_states[ft.id()] != RankLevelState{ft.tier(), ft.level()}) {
          ft_states[ft.id()] = RankLevelState{ft.tier(), ft.level()};
          ft_array.push_back({{"type", "ft"},
                              {"fid", ft.id()},
                              {"tier", ft.tier()},
                              {"level", ft.level()},
                              {"shard_count", ft.shardcount()}});
        }
      }
      if (Config::Get().sync_tech) {
        queue_data(ft_array.dump());
      }
    }
  } else if (type == EntityGroup::Type::ActiveOfficerTraits) {
    auto response = Digit::PrimeServer::Models::OfficerTraitsResponse();
    if (response.ParseFromArray(bytes->bytes->m_Items, bytes->bytes->max_length)) {
      auto trait_array = json::array();
      for (const auto& trait_outer : response.activeofficertraits()) {
        for (const auto& trait : trait_outer.second.activetraits()) {
          const auto key = std::make_pair(trait_outer.first, trait.first);
          if (trait_states[key] != trait.second.level()) {
            trait_states[key] = trait.second.level();
            trait_array.push_back(
                {{"type", "trait"}, {"oid", trait_outer.first}, {"tid", trait.first}, {"level", trait.second.level()}});
          }
        }
      }
      if (Config::Get().sync_traits) {
        queue_data(trait_array.dump());
      }
    }
  } else if (type == EntityGroup::Type::Json) {
    try {
      using json = nlohmann::json;

      auto text   = bytes->bytes->m_Items;
      auto result = json::parse(text);

      if (result.contains("battle_result_headers")) {
        auto headers  = result["battle_result_headers"];
        auto loadData = battlelog_states.empty();

        if (Config::Get().sync_battlelogs) {
          std::lock_guard lk(m2);
          if (loadData) {
            load_previously_sent_logs();
          }

          for (const auto header : headers) {
            const auto id       = header["id"].get<uint64_t>();
            bool       pushData = false;

            if (loadData) {
              pushData = (eastl::find(previously_sent_battlelogs.begin(), previously_sent_battlelogs.end(), id)
                          == previously_sent_battlelogs.end());
            } else {
              pushData = !battlelog_states.contains(id);
            }

            if (pushData) {
              battlelog_states.insert(id);
              combat_log_data_queue.push(id);
              previously_sent_battlelogs.push_back(id);
              if (!loadData) {
                save_previously_sent_logs();
              }
            }
          }

          if (loadData) {
            save_previously_sent_logs();
          }
        }
        cv2.notify_all();
      }

      if (result.contains("resources")) {
        auto resource_array = json::array();
        for (const auto& resource : result["resources"].get<json::object_t>()) {
          auto id     = std::stoll(resource.first);
          auto amount = resource.second["current_amount"].get<int64_t>();

          const auto prevResourceAmountIter = resource_states.find(id);
          const auto hadResource =
              (prevResourceAmountIter != resource_states.end() && prevResourceAmountIter->second != 0);
          const auto amountChanged =
              amount > 0
              && (prevResourceAmountIter == resource_states.end() || prevResourceAmountIter->second != amount);
          const auto resourceDepleted = hadResource && amount == 0;

          if (resourceDepleted || amountChanged) {
            resource_states[id] = amount;
            resource_array.push_back({{"type", "resource"}, {"rid", id}, {"amount", amount}});
          }
        }
        if (Config::Get().sync_resources) {
          queue_data(resource_array.dump());
        }
      }
      if (result.contains("starbase_modules")) {
        auto starbase_array = json::array();
        for (const auto& resource : result["starbase_modules"].get<json::object_t>()) {
          auto id    = resource.second["id"].get<uint64_t>();
          auto level = resource.second["level"].get<int64_t>();
          if (module_states[id] != level) {
            module_states[id] = level;
            starbase_array.push_back({{"type", "module"}, {"bid", id}, {"level", level}});
          }
        }
        if (Config::Get().sync_buildings) {
          queue_data(starbase_array.dump());
        }
      }
      if (result.contains("ships")) {
        auto ship_array = json::array();
        for (const auto& resource : result["ships"].get<json::object_t>()) {
          ship_array.push_back({{"type", "ship"},
                                {"psid", resource.second["id"]},
                                {"level", resource.second["level"]},
                                {"tier", resource.second["tier"]},
                                {"hull_id", resource.second["hull_id"]},
                                {"components", resource.second["components"]}});
        }
        if (Config::Get().sync_ships) {
          queue_data(ship_array.dump());
        }
      }
    } catch (json::exception e) {
    }
  }

  if (Config::Get().sync_missions && mission_sync) {
    sync_active_missions();
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
#if _WIN32
  winrt::init_apartment();
#endif

  for (;;) {
    {
      std::unique_lock lk(m2);
      cv2.wait(lk, []() { return !combat_log_data_queue.empty(); });
    }

    try {
      const auto sync_data = ([&] {
        std::lock_guard lk(m2);
        auto            data = combat_log_data_queue.front();
        combat_log_data_queue.pop();
        return data;
      })();

      auto body       = L"{\"journal_id\":" + std::to_wstring(sync_data) + L"}";
      auto battle_log = http::get_scopely_data(http::instanceSessionId, http::gameServerUrl, L"/journals/get", body);

      using json = nlohmann::json;

      auto ship_array  = json::array();
      auto battle_json = json::parse(battle_log);

      const auto journal = battle_json["journal"];

      const auto target_fleet_data    = journal["target_fleet_data"];
      const auto initiator_fleet_data = journal["initiator_fleet_data"];

      std::vector<std::string> profiles_to_fetch;

      if (target_fleet_data["ref_ids"].is_null()) {
        for (const auto& fleet : target_fleet_data["deployed_fleets"]) {
          const auto player_id = fleet["uid"].get<std::string>();
          profiles_to_fetch.push_back(std::string("\"") + player_id + "\"");
        }
      }

      if (initiator_fleet_data["ref_ids"].is_null()) {
        for (const auto& fleet : initiator_fleet_data["deployed_fleets"]) {
          const auto player_id = fleet["uid"].get<std::string>();
          profiles_to_fetch.push_back(std::string("\"") + player_id + "\"");
        }
      }

      std::string profiles_joined = std::accumulate(profiles_to_fetch.begin(), profiles_to_fetch.end(), std::string(),
                                                    [](const std::string& a, const std::string& b) -> std::string {
                                                      return a + (a.length() > 0 ? "," : "") + b;
                                                    });
      auto        profiles_body   = "{\"user_ids\":[" + profiles_joined + "]}";
      auto profiles = http::get_scopely_data(http::instanceSessionId, http::gameServerUrl, L"/user_profile/profiles",
                                             to_wstring(profiles_body));
      auto profiles_json = json::parse(profiles);
      auto names         = json::object();
      for (const auto& profile : profiles_json["user_profiles"].get<json::object_t>()) {
        names[profile.first] = profile.second["name"];
      }
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

void PrimeApp_InitPrimeServer(auto original, void* _this, Il2CppString* gameServerUrl, Il2CppString* gatewayServerUrl,
                              Il2CppString* sessionId)
{
  original(_this, gameServerUrl, gatewayServerUrl, sessionId);
  http::instanceSessionId = to_wstring(sessionId);
  http::gameServerUrl     = to_wstring(gameServerUrl);
}

void GameServer_SetInstanceIdHeader(auto original, void* _this, int32_t instanceId)
{
  original(_this, instanceId);
  http::instanceId = instanceId;
}

void InstallSyncPatches()
{
  curl_global_init(CURL_GLOBAL_ALL);

  auto missions_data_container =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "MissionsDataContainer");
  if (!missions_data_container.HasClass()) {
    ErrorMsg::MissingHelper("Models", "MissionsDataContainer");
  } else {
    auto ptr = missions_data_container.GetMethod("ParseBinaryObject");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("MissionsDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, MissionsDataContainer_ParseBinaryObject);
    }
  }

  auto inventory_data_container =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "InventoryDataContainer");
  if (!inventory_data_container.HasClass()) {
    ErrorMsg::MissingHelper("Services", "InventoryDataContainer");
  } else {
    auto ptr = inventory_data_container.GetMethod("ParseBinaryObject");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("InventoryDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, MissionsDataContainer_ParseBinaryObject);
    }
  }

  auto research_data_container =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "ResearchDataContainer");
  if (!research_data_container.HasClass()) {
    ErrorMsg::MissingHelper("Services", "ResearchDataContainer");
  } else {
    auto ptr = research_data_container.GetMethod("ParseBinaryObject");
    if (ptr == nullptr) {
      ErrorMsg::MissingHelper("ResearchDataContainer", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, MissionsDataContainer_ParseBinaryObject);
    }
  }

  auto research_service =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "ResearchService");
  if (!research_service.HasClass()) {
    ErrorMsg::MissingHelper("Services", "ResearchService");
  } else {
    auto ptr = research_service.GetMethod("ParseBinaryObject");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("ResearchService", "ParseBinaryObject");
    } else {
      SPUD_STATIC_DETOUR(ptr, MissionsDataContainer_ParseBinaryObject);
    }
  }

  auto game_server_model_registry =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServerModelRegistry");
  if (!game_server_model_registry.HasClass()) {
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

  auto platform_model_registry =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Core", "PlatformModelRegistry");
  if (!platform_model_registry.HasClass()) {
    ErrorMsg::MissingHelper("Core", "PlatformModelRegistry");
  } else {
    auto ptr = platform_model_registry.GetMethod("ProcessResultInternal");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("PlatformModelRegistry", "ProcessResultInterval");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServerModelRegistry_ProcessResultInternal);
    }
  }

  auto authentication_service = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "PrimeApp");
  if (!authentication_service.HasClass()) {
    ErrorMsg::MissingHelper("Core", "PrimeApp");
  } else {
    auto ptr = authentication_service.GetMethod("InitPrimeServer");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("PrimeApp", "InitPrimeServer");
    } else {
      SPUD_STATIC_DETOUR(ptr, PrimeApp_InitPrimeServer);
    }
  }

  auto game_server = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Core", "GameServer");
  if (!game_server.HasClass()) {
    ErrorMsg::MissingHelper("Core", "GameServer");
  } else {
    auto ptr = game_server.GetMethod("SetInstanceIdHeader");
    if (ptr == nullptr) {
      ErrorMsg::MissingMethod("GameServer", "SetInstanceIdHeader");
    } else {
      SPUD_STATIC_DETOUR(ptr, GameServer_SetInstanceIdHeader);
    }
  }

  std::thread(ship_sync_data).detach();
  std::thread(ship_combat_log_data).detach();
}
