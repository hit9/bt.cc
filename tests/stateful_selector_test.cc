#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("StatefulSelector/1", "[final success]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .StatefulSelector()
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

  // Tick#2: Make A failure.
  bb->shouldA = bt::Status::FAILURE;
  root.Tick(ctx);
  // A should failure, and B should started running, E should still not started.
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3
  root.Tick(ctx);
  // A should stay failure, but without ticked.
  // B should still running, got one more tick.
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 2);
  // E still cooling down.
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#4: Makes B failure
  bb->shouldB = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked
  REQUIRE(bb->counterB == 3);  // got one more tick.
  // E started running
  REQUIRE(bb->counterE == 1);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#5: Makes E success
  bb->shouldE = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked.
  REQUIRE(bb->counterB == 3);  // not ticked.
  REQUIRE(bb->counterE == 2);  // got one more tick.
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  REQUIRE(bb->statusE == bt::Status::SUCCESS);
  // The whole tree should SUCCESS.
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

  // Tick#6: One more tick should restart from the first
  root.Tick(ctx);
  REQUIRE(bb->counterA == 3);  // got ticked.
}

TEST_CASE("StatefulSelector/2", "[final failure]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .StatefulSelector()
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

  // Tick#2: Make A failure.
  bb->shouldA = bt::Status::FAILURE;
  root.Tick(ctx);
  // A should failure, and B should started running, E should still not started.
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3
  root.Tick(ctx);
  // A should stay failure, but without ticked.
  // B should still running, got one more tick.
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 2);
  // E still cooling down.
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#4: Makes B failure
  bb->shouldB = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked
  REQUIRE(bb->counterB == 3);  // got one more tick.
  // E started running
  REQUIRE(bb->counterE == 1);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#5: Makes E failure
  bb->shouldE = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked.
  REQUIRE(bb->counterB == 3);  // not ticked.
  REQUIRE(bb->counterE == 2);  // got one more tick.
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  REQUIRE(bb->statusE == bt::Status::FAILURE);
  // The whole tree should SUCCESS.
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);

  // Tick#6: One more tick should restart from the first
  root.Tick(ctx);
  REQUIRE(bb->counterA == 3);  // got ticked.
}
