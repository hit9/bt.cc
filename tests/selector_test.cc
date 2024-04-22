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
    .End()
    ;
  // clang-format on
  Entity e;
  root.BindTreeBlob(e.blob);

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
  root.UnbindTreeBlob();
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
    .End()
    ;
  // clang-format on
  Entity e;
  root.BindTreeBlob(e.blob);

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
  root.UnbindTreeBlob();
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
    .End()
    ;
  // clang-format on
  Entity e;
  root.BindTreeBlob(e.blob);

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
  root.UnbindTreeBlob();
}

TEST_CASE("Selector/4", "[priority selector final success]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Selector()
    ._().Action<G>()
    ._().Action<H>()
    .End()
    ;
  // clang-format on

  Entity e;
  root.BindTreeBlob(e.blob);
  REQUIRE(bb->counterH == 0);
  REQUIRE(bb->counterG == 0);

  // Tick#1
  bb->shouldPriorityG = 1;
  bb->shouldPriorityH = 2;

  root.Tick(ctx);
  REQUIRE(bb->counterG == 0);
  REQUIRE(bb->counterH == 1);
  REQUIRE(bb->statusG == bt::Status::UNDEFINED);
  REQUIRE(bb->statusH == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: makes H failure.
  bb->shouldH = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 1);  // G now started.
  REQUIRE(bb->counterH == 2);
  REQUIRE(bb->statusG == bt::Status::RUNNING);
  REQUIRE(bb->statusH == bt::Status::FAILURE);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: one more tick
  root.Tick(ctx);
  REQUIRE(bb->counterG == 2);
  REQUIRE(bb->counterH == 3);

  // Tick#4: make G success.
  bb->shouldG = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 3);
  REQUIRE(bb->counterH == 4);
  REQUIRE(bb->statusG == bt::Status::SUCCESS);
  REQUIRE(bb->statusH == bt::Status::FAILURE);
  // The whole tree should SUCCESS
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
  root.UnbindTreeBlob();
}

TEST_CASE("Selector/4", "[priority selector final failure]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Selector()
    ._().Action<G>()
    ._().Action<H>()
    .End()
    ;
  // clang-format on
  Entity e;
  root.BindTreeBlob(e.blob);

  REQUIRE(bb->counterH == 0);
  REQUIRE(bb->counterG == 0);

  // Tick#1
  bb->shouldPriorityG = 1;
  bb->shouldPriorityH = 2;

  root.Tick(ctx);
  REQUIRE(bb->counterG == 0);
  REQUIRE(bb->counterH == 1);
  REQUIRE(bb->statusG == bt::Status::UNDEFINED);
  REQUIRE(bb->statusH == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: makes H failure.
  bb->shouldH = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 1);  // G now started.
  REQUIRE(bb->counterH == 2);
  REQUIRE(bb->statusG == bt::Status::RUNNING);
  REQUIRE(bb->statusH == bt::Status::FAILURE);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: make G failure.
  bb->shouldG = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 2);
  REQUIRE(bb->counterH == 3);
  REQUIRE(bb->statusG == bt::Status::FAILURE);
  REQUIRE(bb->statusH == bt::Status::FAILURE);
  // The whole tree should SUCCESS
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
  root.UnbindTreeBlob();
}

TEST_CASE("Selector/5", "[priority selector - dynamic]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .Selector()
    ._().Action<G>()
    ._().Action<H>()
    .End()
    ;
  // clang-format on

  Entity e;
  root.BindTreeBlob(e.blob);
  REQUIRE(bb->counterH == 0);
  REQUIRE(bb->counterG == 0);

  // Tick#1
  bb->shouldPriorityG = 1;
  bb->shouldPriorityH = 2;

  root.Tick(ctx);
  REQUIRE(bb->counterG == 0);
  REQUIRE(bb->counterH == 1);
  REQUIRE(bb->statusG == bt::Status::UNDEFINED);
  REQUIRE(bb->statusH == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: increase G's priority
  bb->shouldPriorityG = 2;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 1);  // G now started
  REQUIRE(bb->counterH == 1);  // H should not ticked
  REQUIRE(bb->statusG == bt::Status::RUNNING);
  REQUIRE(bb->statusH == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: increase G's priority
  bb->shouldPriorityG = 3;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 2);  // G now started
  REQUIRE(bb->counterH == 1);  // H should not ticked
  REQUIRE(bb->statusG == bt::Status::RUNNING);
  REQUIRE(bb->statusH == bt::Status::RUNNING);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#4: makes H success.
  bb->shouldH = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 3);  // G now started
  REQUIRE(bb->counterH == 1);  // H still blocked
  REQUIRE(bb->statusG == bt::Status::RUNNING);
  REQUIRE(bb->statusH == bt::Status::RUNNING);  // h is not ticked.
  // The whole tree should still RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick$5 makes H's priority highest
  bb->shouldPriorityH = 99;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 3);  // G should be skipped.
  REQUIRE(bb->counterH == 2);
  REQUIRE(bb->statusG == bt::Status::RUNNING);
  REQUIRE(bb->statusH == bt::Status::SUCCESS);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
  root.UnbindTreeBlob();
}
