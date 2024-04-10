#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "bt.h"
#include "types.h"

using namespace std::chrono_literals;

TEST_CASE("Simple/Timeout/1", "[simple timeout success]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Timeout(100ms)
    ._().Action<A>()
    ;
  // clang-format on

  // Tick#1: A is not started.
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A success.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}

TEST_CASE("Simple/Timeout/2", "[simple timeout failure]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Timeout(100ms)
    ._().Action<A>()
    ;
  // clang-format on

  // Tick#1: A is not started.
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A failure.
  bb->shouldA = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}

TEST_CASE("Simple/Timeout/3", "[simple timeout timedout]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Timeout(100ms)
    ._().Action<A>()
    ;
  // clang-format on

  // Tick#1: A is not started.
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: should timeout
  std::this_thread::sleep_for(110ms);
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}
