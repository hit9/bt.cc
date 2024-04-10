#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "bt.h"
#include "types.h"

using namespace std::chrono_literals;

TEST_CASE("Retry/1", "[simple retry success]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Retry(3, 30ms)
    ._().Action<A>()
    ;
  // clang-format on

  // Tick#1
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A success.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}

TEST_CASE("Retry/2", "[simple retry final failure]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Retry(3, 30ms)
    ._().Action<A>()
    ;
  // clang-format on

  // Tick#1
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A failure
  bb->shouldA = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Retry 2 more times
  for (int i = 1; i <= 2; i++) {
    std::this_thread::sleep_for(30ms);
    bb->shouldA = bt::Status::RUNNING;
    root.Tick(ctx);
    REQUIRE(root.LastStatus() == bt::Status::RUNNING);
    bb->shouldA = bt::Status::FAILURE;
    root.Tick(ctx);
    REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  }

  // Next the whole tree should failure.
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}

TEST_CASE("Retry/3", "[simple retry final success ]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Retry(3, 30ms)
    ._().Action<A>()
    ;
  // clang-format on

  // Tick#1
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A failure
  bb->shouldA = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Makes A ok.
  std::this_thread::sleep_for(30ms);
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}

TEST_CASE("Retry/4", "[simple retry forever ]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .RetryForever(1ms)
    ._().Action<A>()
    ;
  // clang-format on

  // Tick#1
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A failure
  bb->shouldA = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Makes A ok.
  for (int i = 0; i < 30; i++) {
    std::this_thread::sleep_for(1ms);
    bb->shouldA = bt::Status::FAILURE;
    root.Tick(ctx);
    REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  }
}
