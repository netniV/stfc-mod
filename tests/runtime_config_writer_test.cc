#include "runtime_config_writer.h"
#include <chrono>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace config_edit;
using namespace std::chrono_literals;
namespace
{
std::mutex                            gate;
std::condition_variable               changed;
bool                                  entered = false, released = false;
Outcome                               first_result = Outcome::Saved;
std::vector<Request>                  requests;
std::thread::id                       save_thread;
std::vector<std::chrono::steady_clock::time_point> save_times;
int                                   reports      = 0;
bool                                  block_report = false, release_report = false;
bool                                  fail_thread_start = false;

void Check(bool value, std::source_location location = std::source_location::current())
{
  if (!value)
    throw std::runtime_error("worker fixture failed at line " + std::to_string(location.line()));
}
Outcome Save(TomlEditor&, const std::filesystem::path&, const Request& request)
{
  std::unique_lock lock(gate);
  save_thread = std::this_thread::get_id();
  save_times.push_back(std::chrono::steady_clock::now());
  requests.push_back(request);
  if (requests.size() == 1) {
    entered = true;
    changed.notify_all();
    if (!changed.wait_for(lock, 5s, [] { return released; }))
      throw std::runtime_error("fixture timed out");
    return first_result;
  }
  return Outcome::Saved;
}
void Report(std::string_view, std::string_view, Outcome)
{
  std::unique_lock lock(gate);
  ++reports;
  changed.notify_all();
  if (block_report)
    Check(changed.wait_for(lock, 5s, [] { return release_report; }));
}
void Begin(Outcome result)
{
  entered = released = false;
  requests.clear();
  save_times.clear();
  reports      = 0;
  first_result = result;
  block_report = release_report = false;
}
void AwaitSave()
{
  std::unique_lock lock(gate);
  Check(changed.wait_for(lock, 5s, [] { return entered; }));
}
void Release()
{
  std::lock_guard lock(gate);
  released = true;
  changed.notify_all();
}
void AwaitCompletion(RuntimeConfigWriter& writer, std::uint64_t revision)
{
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (writer.LastCompletion().revision != revision || writer.HasWork()) {
    Check(std::chrono::steady_clock::now() < deadline);
    std::this_thread::yield();
  }
}
template <typename Function, typename Owner> std::thread StartWorker(Function function, Owner* owner)
{
  if (std::exchange(fail_thread_start, false))
    throw std::runtime_error("fixture thread-start failure");
  return std::thread(function, owner);
}
} // namespace

// Block at the real worker's save boundary to exercise scheduling deterministically.
#define CONFIG_EDIT_SAVE(editor, path, request) Save(editor, path, request)
#define CONFIG_EDIT_START_WORKER(...) StartWorker(__VA_ARGS__)
#include "../mods/src/runtime_config_writer.cc"

int main()
{
  try {
    for (auto outcome : {Outcome::Saved, Outcome::AlreadySaved, Outcome::Conflict, Outcome::IoError}) {
      Begin(outcome);
      RuntimeConfigWriter writer("unused", Value{std::string("none")}, Report);
      Check(writer.Submit("invalid") == 0);
      Check(writer.Submit("warp") == 1);
      AwaitSave();
      Check(writer.HasWork());
      Check(writer.Submit("jump") == 2);
      Check(writer.Submit("none") == 3);
      writer.Stop(false);
      Check(!writer.PollStopped());
      Check(writer.Submit("warp") == 0);
      Release();
      auto deadline = std::chrono::steady_clock::now() + 5s;
      while (!writer.PollStopped()) {
        Check(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
      }
      Check(!writer.HasWork());
      Check(requests.size() == 2);
      Check(requests[1].desired == Value{std::string("none")});
      const bool success = outcome == Outcome::Saved || outcome == Outcome::AlreadySaved;
      Check(requests[1].expected == std::optional<Value>{std::string(success ? "warp" : "none")});
      Check(reports == (success ? 0 : 1));
      Check(save_thread != std::this_thread::get_id());
      Check(writer.LastCompletion().revision == 3);
      Check(writer.LastCompletion().outcome == Outcome::Saved);
    }
    Begin(Outcome::Saved);
    {
      RuntimeConfigWriter writer("unused", Value{std::string("none")});
      writer.Submit("warp");
      AwaitSave();
      writer.Submit("jump");
      writer.Stop(false); // Force-close may arrive during an orderly drain.
      writer.RequestCancelPending();
      Check(writer.Submit("none") == 0);
      // Hold the force-close caller before Stop(true), while the active save
      // completes. Publication alone must prevent the queued save from starting.
      Release();
      auto deadline = std::chrono::steady_clock::now() + 5s;
      while (!writer.PollStopped()) {
        Check(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
      }
      Check(requests.size() == 1);
      Check(writer.LastCompletion().revision == 2);
      Check(writer.LastCompletion().outcome == Outcome::Cancelled);
      writer.Stop(true);
    }
    Begin(Outcome::IoError);
    {
      RuntimeConfigWriter writer("unused", Value{std::string("none")}, Report);
      Check(writer.Register("graphics", "threshold", Value{0.5}));
      const auto failed = writer.Submit("warp");
      AwaitSave();
      Release();
      AwaitCompletion(writer, failed);
      Check(writer.HasFailures());
      AwaitCompletion(writer, writer.Submit("graphics", "threshold", 0.7));
      Check(writer.HasFailures()); // Saving B cannot hide A's failed save.
      AwaitCompletion(writer, writer.Submit("jump"));
      Check(!writer.HasFailures()); // A later successful save of A clears it.
      Check(reports == 1);
    }
    Begin(Outcome::Saved);
    {
      RuntimeConfigWriter writer("unused", Value{std::string("none")});
      fail_thread_start = true;
      Check(!writer.Submit("warp"));
      Check(writer.HasFailure("ui", "auto_confirm_instant_warp") && writer.HasFailures() && !writer.HasWork());
      Check(!writer.HasFailure("other", "unregistered"));
      const auto retry = writer.Submit("jump");
      AwaitSave();
      Release();
      AwaitCompletion(writer, retry);
      Check(!writer.HasFailures() && !writer.HasFailure("ui", "auto_confirm_instant_warp"));
    }
    Begin(Outcome::IoError);
    {
      block_report = true;
      RuntimeConfigWriter writer("unused", Value{std::string("none")}, Report);
      writer.Submit("warp");
      AwaitSave();
      Release();
      {
        std::unique_lock lock(gate);
        Check(changed.wait_for(lock, 5s, [] { return reports == 1; }));
        Check(writer.HasWork()); // Logging still executing must not look idle.
        writer.Stop(false);
        Check(!writer.PollStopped());
        release_report = true;
        changed.notify_all();
      }
    }
    Begin(Outcome::Saved);
    {
      RuntimeConfigWriter writer("unused", Value{std::string("none")});
      Check(writer.Register("graphics", "threshold", Value{0.5}));
      Check(!writer.Submit("other", "unregistered", true));
      writer.Submit("warp");
      AwaitSave();
      Check(!writer.Register("graphics", "late", Value{true}));
      writer.Submit("graphics", "threshold", 0.6, 150ms);
      writer.Submit("jump");
      writer.Submit("graphics", "threshold", 0.7, 150ms);
      writer.Stop(false); // Drain also flushes a slider whose delay has not expired.
      Release();
      const auto deadline = std::chrono::steady_clock::now() + 5s;
      while (!writer.PollStopped()) {
        Check(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
      }
      Check(requests.size() == 3);
      Check(requests[1].key == "auto_confirm_instant_warp" && requests[1].desired == Value{std::string("jump")});
      Check(requests[1].expected == std::optional<Value>{std::string("warp")});
      Check(requests[2].key == "threshold" && requests[2].desired == Value{0.7});
      Check(requests[2].expected == std::optional<Value>{0.5});
    }
    Begin(Outcome::Saved);
    std::chrono::steady_clock::time_point initial_submitted, replacement_submitted;
    {
      RuntimeConfigWriter writer("unused", std::nullopt);
      Check(writer.Register("graphics", "threshold", Value{0.5}));
      initial_submitted = std::chrono::steady_clock::now();
      writer.Submit("graphics", "threshold", 0.6, 300ms);
      {
        std::unique_lock lock(gate);
        changed.wait_for(lock, 75ms, [] { return entered; });
      }
      replacement_submitted = std::chrono::steady_clock::now();
      const auto revision   = writer.Submit("graphics", "threshold", 0.7, 300ms);
      Release();
      AwaitCompletion(writer, revision); // Ordinary expiration, without Stop/quit flushing the delay.
    }
    // The test thread may resume after the first deadline on a busy runner.
    // Validate actual entry times, not a negative assertion made by a late observer.
    // Both an already-active first write and a coalesced replacement are valid;
    // the gated test above independently requires queued same-key coalescing.
    Check(requests.size() == 1 || requests.size() == 2);
    Check(requests.back().desired == Value{0.7});
    if (requests.size() == 2)
      Check(requests.front().desired == Value{0.6});
    for (std::size_t i = 0; i < requests.size(); ++i) {
      const auto submitted = requests[i].desired == Value{0.6} ? initial_submitted : replacement_submitted;
      Check(save_times[i] >= submitted + 300ms);
    }
    Begin(Outcome::Saved);
    {
      RuntimeConfigWriter writer("unused", std::nullopt);
      Check(writer.Register("graphics", "threshold", Value{0.5}));
      writer.Submit("graphics", "threshold", 0.9, 10s);
      writer.Stop(true); // Cancellation must wake a delayed writer promptly.
      const auto deadline = std::chrono::steady_clock::now() + 5s;
      while (!writer.PollStopped()) {
        Check(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
      }
      Check(requests.empty());
    }
    RuntimeConfigWriter idle("unused", std::nullopt);
    idle.Stop(false);
    Check(idle.PollStopped());
    std::cout << "Runtime writer coalescing/drain/cancel fixtures passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
