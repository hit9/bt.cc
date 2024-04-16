#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("StatefulRandomSelector/1", "[simple stateful random selector]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .StatefulRandomSelector()
    ._().Action<H>()
    ._().Action<I>()
    .End()
    ;
  // clang-format on

  REQUIRE(bb->counterH == 0);
  REQUIRE(bb->counterI == 0);

  // Tick#1
  bb->shouldPriorityI = 1;
  bb->shouldPriorityH = 1;

  for (int i = 0; i < 10; i++) root.Tick(ctx);

  // Makes I failure.
  bb->shouldI = bt::Status::FAILURE;

  for (int i = 0; i < 100; i++) root.Tick(ctx);
  REQUIRE(bb->counterI <= 11);  // at most get 11 tick
  REQUIRE(bb->counterH >= 99);  // at least 99 tick

  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
}
