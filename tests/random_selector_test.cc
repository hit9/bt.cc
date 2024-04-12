#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

#include "bt.h"
#include "types.h"

TEST_CASE("RandomSelector/1", "[simple random selector]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .RandomSelector()
    ._().Action<H>()
    ._().Action<I>()
    ;
  // clang-format on

  REQUIRE(bb->counterH == 0);
  REQUIRE(bb->counterI == 0);

  // Tick#1
  bb->shouldPriorityI = 0;
  bb->shouldPriorityH = 1;

  for (int i = 0; i < 100; i++) root.Tick(ctx);
  REQUIRE(bb->counterI == 0);  // I should have no chance to get tick
  REQUIRE(bb->counterH == 100);
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
}

TEST_CASE("RandomSelector/2", "[simple random selector - equal weights]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .RandomSelector()
    ._().Action<H>()
    ._().Action<I>()
    ;
  // clang-format on

  REQUIRE(bb->counterH == 0);
  REQUIRE(bb->counterI == 0);

  // Tick#1
  bb->shouldPriorityI = 1;
  bb->shouldPriorityH = 1;

  for (int i = 0; i < 100000; i++) root.Tick(ctx);
  REQUIRE(abs(bb->counterI - bb->counterH) < 3000);  // error < 30%?
  // The whole tree should RUNNING
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
}
