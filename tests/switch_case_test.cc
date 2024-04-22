#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("SwitchCase/1", "[simplest switch/case]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
  root
  .Switch()
  ._().Case<C>()
  ._()._().Sequence()
  ._()._()._().Action<A>()
  ._()._()._().Action<E>()
  ._().Case<D>()
  ._()._().Action<B>()
  .End()
  ;
  // clang-format on

  Entity e;
  root.BindTreeBlob(e.blob);
  // Tick#1
  root.Tick(ctx);
  // All should unstarted.
  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->counterB == 0);
  // The whole tree should FAILURE.
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);

  // Tick#2: Make D success.
  bb->shouldD = true;
  root.Tick(ctx);
  // only B should started
  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  // The whole tree should RUNNING.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Make D FAILURE and C ok
  bb->shouldD = false;
  bb->shouldC = true;
  root.Tick(ctx);
  // A should started
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);  // E is blocking by A
  REQUIRE(bb->statusB == bt::Status::RUNNING);    // B wont change, it's not ticked.
  // The whole tree should RUNNING.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#4: Make A success and E success
  bb->shouldA = bt::Status::SUCCESS;
  bb->shouldE = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterE == 1);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(bb->statusE == bt::Status::SUCCESS);
  REQUIRE(bb->statusB == bt::Status::RUNNING);  // B wont change, it's not ticked.
  // The whole tree should SUCCESS
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
  root.UnbindTreeBlob();
}
