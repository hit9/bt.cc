#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("Simple/Sequence/1", "[all success]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Sequence()
    ._().Action<A>()
    ._().Action<B>()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterB == 0);

  // Tick#1
  root.Tick(ctx);
  // A is still running, B has not started running.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A success.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  // A should success, and b should started running
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);

  // Tick#3
  root.Tick(ctx);
  // A should stay success, and b should still running
  REQUIRE(bb->counterA == 3);
  REQUIRE(bb->counterB == 2);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);

  // Tick#3: Make B success.
  bb->shouldB = bt::Status::SUCCESS;
  root.Tick(ctx);
  // A should stay success, and b should success.
  REQUIRE(bb->counterA == 4);
  REQUIRE(bb->counterB == 3);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::SUCCESS);
  // The whole tree should success.
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}

TEST_CASE("Simple/Sequence/2", "[partial failure - first failure]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Sequence()
    ._().Action<A>()
    ._().Action<B>()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterB == 0);

  // Tick#1
  root.Tick(ctx);
  // A is still running, B has not started running.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::UNDEFINED);

  // Tick#2: Makes A failure
  bb->shouldA = bt::Status::FAILURE;
  root.Tick(ctx);
  // A should failure, and b should not started
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::UNDEFINED);
  // The whole tree should failure.
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}

TEST_CASE("Simple/Sequence/3", "[partial failure - last failure]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Sequence()
    ._().Action<A>()
    ._().Action<B>()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterB == 0);

  // Tick#1
  root.Tick(ctx);
  // A is still running, B has not started running.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::UNDEFINED);

  // Tick#2: Makes A SUCCESS.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  // A should failure, and b should start running
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  // The whole tree should running
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Makes B FAILURE
  bb->shouldB = bt::Status::FAILURE;
  root.Tick(ctx);

  // A should still success, and b should FAILURE
  REQUIRE(bb->counterA == 3);
  REQUIRE(bb->counterB == 2);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  // The whole tree should FAILURE
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}

