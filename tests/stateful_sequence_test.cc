#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("StatefulSequence/1", "[all success]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .StatefulSequence()
    ._().Action<A>()
    ._().Action<B>()
    ._().Action<E>()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->counterB == 0);

  // Tick#1
  root.Tick(ctx);
  // A is still running, B & E has not started running.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::UNDEFINED);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Make A success.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  // A should success, and B should started running, E should still not started.
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3
  root.Tick(ctx);
  // A should stay success, but without ticked.
  // B should still running, got one more tick.
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 2);
  // E still cooling down.
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#4: Makes B success.
  bb->shouldB = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 3);  // got one more tick.
  // E started running
  REQUIRE(bb->counterE == 1);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::SUCCESS);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#5: Makes E success
  bb->shouldE = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked.
  REQUIRE(bb->counterB == 3);  // not ticked.
  REQUIRE(bb->counterE == 2);  // got one more tick.
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::SUCCESS);
  REQUIRE(bb->statusE == bt::Status::SUCCESS);
  // The whole tree should SUCCESS.
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}

TEST_CASE("StatefulSequence/2", "[paritial failure]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .StatefulSequence()
    ._().Action<A>()
    ._().Action<B>()
    ._().Action<E>()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->counterB == 0);

  // Tick#1
  root.Tick(ctx);
  // A is still running, B & E has not started running.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::UNDEFINED);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Make A success.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  // A should success, and B should started running, E should still not started.
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Make B failure
  bb->shouldB = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked
  REQUIRE(bb->counterB == 2);  // got one more tick
  REQUIRE(bb->counterE == 0);  // still not started.
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should FAILURE.
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);

  // Tick#4: The next tick will starts from first child.
  root.Tick(ctx);
  REQUIRE(bb->counterA == 3);  // restarts from here, got ticked
}

TEST_CASE("StatefulSequence/3", "[priority StatefulSequence]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .StatefulSequence()
    ._().Action<G>()
    ._().Action<H>()
    ._().Action<I>()
    ;
  // clang-format on

  bb->shouldPriorityG = 1;
  bb->shouldPriorityH = 2;
  bb->shouldPriorityI = 3;

  // Tick#1
  root.Tick(ctx);
  REQUIRE(bb->counterG == 0);
  REQUIRE(bb->counterH == 0);
  REQUIRE(bb->counterI == 1);
  REQUIRE(bb->statusI == bt::Status::RUNNING);
  REQUIRE(bb->statusH == bt::Status::UNDEFINED);
  REQUIRE(bb->statusG == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: makes I success.
  bb->shouldI = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 0);
  REQUIRE(bb->counterH == 1);
  REQUIRE(bb->counterI == 2);
  REQUIRE(bb->statusI == bt::Status::SUCCESS);
  REQUIRE(bb->statusH == bt::Status::RUNNING);
  REQUIRE(bb->statusG == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  // Tick#3
  root.Tick(ctx);
  REQUIRE(bb->counterG == 0);
  REQUIRE(bb->counterH == 2);
  REQUIRE(bb->counterI == 2);
  REQUIRE(bb->statusI == bt::Status::SUCCESS);
  REQUIRE(bb->statusH == bt::Status::RUNNING);
  REQUIRE(bb->statusG == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  // Tick#4: Makes G priority highest.
  bb->shouldPriorityG = 9999;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 1);
  REQUIRE(bb->counterH == 2);
  REQUIRE(bb->counterI == 2);
  REQUIRE(bb->statusI == bt::Status::SUCCESS);
  REQUIRE(bb->statusH == bt::Status::RUNNING);
  REQUIRE(bb->statusG == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  // Tick#5: Makes G success.
  bb->shouldG = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 2);
  REQUIRE(bb->counterH == 3);
  REQUIRE(bb->counterI == 2);
  REQUIRE(bb->statusI == bt::Status::SUCCESS);
  REQUIRE(bb->statusH == bt::Status::RUNNING);
  REQUIRE(bb->statusG == bt::Status::SUCCESS);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  // Tick#6: Makes H success.
  bb->shouldH = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterG == 2);
  REQUIRE(bb->counterH == 4);
  REQUIRE(bb->counterI == 2);
  REQUIRE(bb->statusI == bt::Status::SUCCESS);
  REQUIRE(bb->statusH == bt::Status::SUCCESS);
  REQUIRE(bb->statusG == bt::Status::SUCCESS);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}
