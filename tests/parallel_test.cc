#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("Parallel/1", "[all success]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Parallel()
    ._().Action<A>()
    ._().Action<B>()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterB == 0);

  // Tick#1
  root.Tick(ctx);
  // both A and B should RUNNING
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A SUCCESS.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  // A should SUCCESS, and b should still running
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 2);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Makes B success.
  bb->shouldB = bt::Status::SUCCESS;
  root.Tick(ctx);

  REQUIRE(bb->counterA == 3);
  REQUIRE(bb->counterB == 3);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::SUCCESS);
  // The whole tree should SUCCESS
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}

TEST_CASE("Parallel/2", "[partial success (2nd failure)]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Parallel()
    ._().Action<A>()
    ._().Action<B>()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterB == 0);

  // Tick#1
  root.Tick(ctx);
  // both A and B should RUNNING
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A SUCCESS.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  // A should SUCCESS, and b should still running
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 2);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Makes B failure.
  bb->shouldB = bt::Status::FAILURE;
  root.Tick(ctx);

  REQUIRE(bb->counterA == 3);
  REQUIRE(bb->counterB == 3);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  // The whole tree should FAILURE
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}

TEST_CASE("Parallel/3", "[partial success (1st failure)]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Parallel()
    ._().Action<A>()
    ._().Action<B>()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterB == 0);

  // Tick#1
  root.Tick(ctx);
  // both A and B should RUNNING
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A FAILURE.
  bb->shouldA = bt::Status::FAILURE;
  root.Tick(ctx);
  // A should FAILURE b should still running
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 2);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  // The whole tree should FAILURE
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}

TEST_CASE("Parallel/4", "[all failure]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Parallel()
    ._().Action<A>()
    ._().Action<B>()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterB == 0);

  // Tick#1
  root.Tick(ctx);
  // both A and B should RUNNING
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A and B FAILURE.
  bb->shouldA = bt::Status::FAILURE;
  bb->shouldB = bt::Status::FAILURE;
  root.Tick(ctx);
  // A and B shuold both FAILURE
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 2);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  // The whole tree should FAILURE
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}

TEST_CASE("Parallel/5", "[priority partial]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Parallel()
    ._().Action<G>()
    ._().Action<H>()
    ;
  // clang-format on

  REQUIRE(bb->counterG == 0);
  REQUIRE(bb->counterH == 0);

  bb->shouldPriorityG = 3;
  bb->shouldPriorityH = 2;

  // Tick#1
  root.Tick(ctx);
  REQUIRE(bb->counterG == 1);
  REQUIRE(bb->counterH == 1);
  REQUIRE(bb->statusG == bt::Status::RUNNING);
  REQUIRE(bb->statusH == bt::Status::RUNNING);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2
  bb->shouldG = bt::Status::FAILURE;
  bb->shouldH = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 2);
  REQUIRE(bb->counterH == 2);
  REQUIRE(bb->statusG == bt::Status::FAILURE);
  REQUIRE(bb->statusH == bt::Status::SUCCESS);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}
