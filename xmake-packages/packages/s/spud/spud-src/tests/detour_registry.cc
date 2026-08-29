#include <spud/detour.h>

#include "detour/target_registry.h"

#include <atomic>
#include <barrier>
#include <thread>
#include <utility>

#include <catch2/catch_test_macros.hpp>

namespace
{

#if defined(_MSC_VER)
#define SPUD_TEST_NOINLINE __declspec(noinline)
#else
#define SPUD_TEST_NOINLINE __attribute__((noinline))
#endif

SPUD_TEST_NOINLINE int duplicate_target(int value)
{ return value + 1; }

SPUD_TEST_NOINLINE int move_target(int value)
{ return value + 2; }

SPUD_TEST_NOINLINE int reinstall_target(int value)
{ return value + 3; }

SPUD_TEST_NOINLINE int detached_target(int value)
{ return value + 4; }

int add_ten(int (*original)(int), int value)
{ return original(value) + 10; }

int add_twenty(int (*original)(int), int value)
{ return original(value) + 20; }

} // namespace

TEST_CASE("target registry rejects exact and canonical aliases")
{
  spud::detail::target_registry    registry;
  const spud::detail::target_owner first = {
      .requested_address   = 0x1000,
      .canonical_address   = 0x2000,
      .owner_token         = 0x3000,
      .replacement_address = 0x4000,
  };

  REQUIRE(registry.claim(first).status == spud::detail::target_claim_status::claimed);
  REQUIRE(registry.size() == 2);
  REQUIRE(registry.claim(first).status == spud::detail::target_claim_status::already_owned);

  const auto exact = registry.claim({
      .requested_address   = first.requested_address,
      .canonical_address   = 0x5000,
      .owner_token         = 0x6000,
      .replacement_address = 0x7000,
  });
  REQUIRE(exact.status == spud::detail::target_claim_status::conflict);
  REQUIRE(exact.incumbent.owner_token == first.owner_token);

  const auto alias = registry.claim({
      .requested_address   = 0x8000,
      .canonical_address   = first.canonical_address,
      .owner_token         = 0x9000,
      .replacement_address = 0xA000,
  });
  REQUIRE(alias.status == spud::detail::target_claim_status::conflict);
  REQUIRE(alias.incumbent.owner_token == first.owner_token);

  registry.release(first);
  REQUIRE(registry.size() == 0);
}

TEST_CASE("target registry admits exactly one concurrent owner")
{
  spud::detail::target_registry registry;
  std::barrier                  start(3);
  std::atomic_size_t            claimed   = 0;
  std::atomic_size_t            conflicts = 0;

  const auto contender = [&](uintptr_t owner) {
    start.arrive_and_wait();
    const auto result = registry.claim({
        .requested_address   = 0x1000,
        .canonical_address   = 0x2000,
        .owner_token         = owner,
        .replacement_address = owner + 1,
    });
    if (result.status == spud::detail::target_claim_status::claimed) {
      claimed.fetch_add(1, std::memory_order_relaxed);
    } else if (result.status == spud::detail::target_claim_status::conflict) {
      conflicts.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::thread first(contender, 0x3000);
  std::thread second(contender, 0x4000);
  start.arrive_and_wait();
  first.join();
  second.join();

  REQUIRE(claimed.load(std::memory_order_relaxed) == 1);
  REQUIRE(conflicts.load(std::memory_order_relaxed) == 1);
  REQUIRE(registry.size() == 2);
}

TEST_CASE("duplicate detour preserves the first installed hook")
{
  int (*volatile call_target)(int) = duplicate_target;
  REQUIRE(call_target(1) == 2);

  {
    auto first = spud::create_detour(&duplicate_target, &add_ten);
    first.install();
    REQUIRE(first.last_install_status() == spud::detour_install_status::installed);
    REQUIRE(call_target(1) == 12);

    first.install();
    REQUIRE(first.last_install_status() == spud::detour_install_status::already_installed);
    REQUIRE(call_target(1) == 12);

    auto second = spud::create_detour(&duplicate_target, &add_twenty);
    second.install();
    REQUIRE(second.last_install_status() == spud::detour_install_status::duplicate_target);
    REQUIRE(second.trampoline() == nullptr);
    REQUIRE(call_target(1) == 12);
  }
  REQUIRE(call_target(1) == 2);
}

TEST_CASE("moving an installed detour preserves ownership and cleanup")
{
  int (*volatile call_target)(int) = move_target;
  REQUIRE(call_target(1) == 3);
  {
    auto source = spud::create_detour(&move_target, &add_ten);
    source.install();
    auto moved = std::move(source);
    REQUIRE(moved.last_install_status() == spud::detour_install_status::installed);
    REQUIRE(call_target(1) == 13);
  }
  REQUIRE(call_target(1) == 3);
}

TEST_CASE("normal destruction releases a target for later installation")
{
  int (*volatile call_target)(int) = reinstall_target;
  {
    auto first = spud::create_detour(&reinstall_target, &add_ten);
    first.install();
    REQUIRE(first.last_install_status() == spud::detour_install_status::installed);
    REQUIRE(call_target(1) == 14);
  }
  REQUIRE(call_target(1) == 4);
  {
    auto second = spud::create_detour(&reinstall_target, &add_twenty);
    second.install();
    REQUIRE(second.last_install_status() == spud::detour_install_status::installed);
    REQUIRE(call_target(1) == 24);
  }
  REQUIRE(call_target(1) == 4);
}

TEST_CASE("detached hooks retain their target claim")
{
  int (*volatile call_target)(int) = detached_target;
  {
    auto first = spud::create_detour(&detached_target, &add_ten);
    first.install().detach();
    REQUIRE(call_target(1) == 15);
  }

  auto second = spud::create_detour(&detached_target, &add_twenty);
  second.install();
  REQUIRE(second.last_install_status() == spud::detour_install_status::duplicate_target);
  REQUIRE(call_target(1) == 15);
}
