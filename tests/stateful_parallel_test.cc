#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("StatefulParallel/1", "[all success]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .StatefulParallel()
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
  // A/B/E all started running
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->counterE == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Make A SUCCESS
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 2);
  REQUIRE(bb->counterE == 2);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // A should skip this tick
  REQUIRE(bb->counterB == 3);
  REQUIRE(bb->counterE == 3);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#4: Makes B SUCCESS
  bb->shouldB = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked
  REQUIRE(bb->counterB == 4);  // got one more tick.
  REQUIRE(bb->counterE == 4);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::SUCCESS);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#5: Makes E success
  bb->shouldE = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked.
  REQUIRE(bb->counterB == 4);  // not ticked.
  REQUIRE(bb->counterE == 5);  // got one more tick.
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::SUCCESS);
  REQUIRE(bb->statusE == bt::Status::SUCCESS);
  // The whole tree should SUCCESS.
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

  // Tick#6: One more tick should restart from all children
  root.Tick(ctx);
  REQUIRE(bb->counterA == 3);  // got ticked.
  REQUIRE(bb->counterB == 5);  // got ticked.
  REQUIRE(bb->counterE == 6);  // got ticked.
}

TEST_CASE("StatefulParallel/2", "[final failure]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .StatefulParallel()
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
  // A/B/E all started running
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->counterE == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Make A SUCCESS
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 2);
  REQUIRE(bb->counterE == 2);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // A should skip this tick
  REQUIRE(bb->counterB == 3);
  REQUIRE(bb->counterE == 3);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#4: Makes B failure
  bb->shouldB = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked
  REQUIRE(bb->counterB == 4);  // got one more tick.
  REQUIRE(bb->counterE == 4);  // go ticked
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);

  // Tick#5: One more tick should restart from all children
  root.Tick(ctx);
  REQUIRE(bb->counterA == 3);  // got ticked.
  REQUIRE(bb->counterB == 5);  // got ticked.
  REQUIRE(bb->counterE == 5);  // got ticked.
}
