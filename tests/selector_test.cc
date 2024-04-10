#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("Selector/1", "[first success]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Selector()
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
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A SUCCESS.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  // A should SUCCESS, and b should not start running
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::UNDEFINED);
  // The whole tree should SUCCESS
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

  // Tick#3: Nothing changes.
  root.Tick(ctx);

  // A should still success, and b should still not started.
  REQUIRE(bb->counterA == 3);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::UNDEFINED);
  // The whole tree should SUCCESS
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}

TEST_CASE("Selector/2", "[first failure and second success]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Selector()
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
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A FAILURE.
  bb->shouldA = bt::Status::FAILURE;
  root.Tick(ctx);
  // A should FAILURE, and b should started
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Makes B SUCCESS
  bb->shouldB = bt::Status::SUCCESS;
  root.Tick(ctx);

  // A should still FAILURE, and b should SUCCESS.
  REQUIRE(bb->counterA == 3);
  REQUIRE(bb->counterB == 2);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::SUCCESS);
  // The whole tree should SUCCESS
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}

TEST_CASE("Selector/3", "[all failure]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Selector()
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
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A FAILURE.
  bb->shouldA = bt::Status::FAILURE;
  root.Tick(ctx);
  // A should FAILURE, and b should started
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Makes B FAILURE
  bb->shouldB = bt::Status::FAILURE;
  root.Tick(ctx);

  // A should still FAILURE, and b should SUCCESS.
  REQUIRE(bb->counterA == 3);
  REQUIRE(bb->counterB == 2);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  // The whole tree should FAILURE
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}
