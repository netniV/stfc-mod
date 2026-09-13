#define CONFIG_RUNTIME_TEST "runtime_config_fixture.h"
#include "../mods/src/patches/parts/runtime_config.cc"
#include <cassert>
#include <string_view>
#include <thread>

namespace
{
config_edit::RuntimeConfigWriter fixture;
unsigned                         resumes = 0;
void                             Reset()
{
  fixture   = {};
  writer    = &fixture;
  available = true;
  owner     = GetCurrentThreadId();
  forcing   = false;
  persistence_unavailable = false;
  reported_save_failure   = false;
  save_status_changed     = nullptr;
  fixture_update_callback = nullptr;
  draining = stopped = resume = false;
  vote                        = 0;
  resumes                     = 0;
  request_quit                = [](int) { ++resumes; };
}
} // namespace
int main(int argc, char** argv)
{
  Reset();
  if (argc == 2) {
    const std::string_view mode(argv[1]);
    fixture.work         = mode != "idle";
    fixture.block_cancel = mode == "deadline";
    if (mode != "missing-handle")
      fixture.handle = CreateEventW(nullptr, TRUE, mode == "finished", nullptr);
    runtime_config::ForceClose();
    Sleep(10000); // Parent kills this fixture if the independent deadline fails.
    return 9;
  }
  unsigned         notices     = 0;
  static unsigned* noticeCount = &notices;
  assert(runtime_config::SetSaveStatusObserver([] { ++*noticeCount; }));
  fixture.failures = true;
  std::thread foreignNotice([] { Update(); });
  foreignNotice.join();
  assert(notices == 0 && runtime_config::HasSaveFailures());
  Update();
  Update();
  assert(notices == 1); // One UI callback on a transition, never on each frame.
  fixture.failures = false;
  Update();
  assert(notices == 2 && !runtime_config::HasSaveFailures());
  available               = false;
  writer                  = nullptr; // Configure/Install never supplied the normal update path.
  fixture_update_callback = nullptr;
  assert(runtime_config::SetSaveStatusObserver(save_status_changed));
  assert(fixture_update_callback);
  runtime_config::SaveWarpMode("warp");
  fixture_update_callback();
  assert(notices == 3 && runtime_config::HasSaveFailures());
  available = true;
  writer    = &fixture;
  runtime_config::SaveWarpMode("jump");
  Update();
  assert(notices == 3 && runtime_config::HasSaveFailures()); // Rejected edits remain session-only.
  Reset();
  fixture.failures = true; // Transient thread-start failure is tracked by its key.
  runtime_config::SaveWarpMode("warp");
  assert(runtime_config::HasSaveFailures() && !persistence_unavailable);
  fixture.failures = false;
  runtime_config::SaveWarpMode("jump");
  assert(!runtime_config::HasSaveFailures());
  Reset();
  runtime_config::SaveWarpMode("invalid");
  assert(fixture.submissions == 0); // Generic submission must retain the mode wrapper's domain check.
  assert(WantsQuit([] { return true; }));
  assert(fixture.stopped && stopped && !draining);
  runtime_config::SaveWarpMode("warp");
  assert(fixture.submissions == 0);
  Update();
  assert(resumes == 0);

  Reset();
  fixture.work = true;
  assert(!WantsQuit([] { return false; }));
  assert(!fixture.stopped && !draining);

  Reset();
  fixture.work = true;
  assert(!WantsQuit([] { return true; }));
  assert(fixture.stopped && draining);
  Update();
  assert(resumes == 0);
  fixture.work = false; // Disk work done is not yet native worker termination.
  Update();
  assert(resumes == 0);
  fixture.finished = true;
  std::thread foreign([] {
    Update();
    runtime_config::SaveWarpMode("jump");
  });
  foreign.join();
  assert(resumes == 0 && fixture.submissions == 0);
  request_quit = [](int) {
    ++resumes;
    assert(!WantsQuit([] { return false; })); // A genuine resumed veto is final.
  };
  Update();
  Update();
  assert(resumes == 1);

  Reset();
  fixture.work = true;
  assert(!WantsQuit([] {
    assert(!WantsQuit([] { return false; }));
    return true; // Older outer vote must not replace the later veto.
  }));
  fixture.finished = true;
  Update();
  assert(resumes == 0);

  Reset();
  assert(!WantsQuit([] { return false; }));
  runtime_config::SaveWarpMode("warp");
  assert(fixture.submissions == 1); // Veto leaves ordinary save admission open.
  std::puts("Native adapter idle/drain/veto/owner fixtures passed");
}
