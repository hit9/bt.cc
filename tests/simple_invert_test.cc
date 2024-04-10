#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("Simple/Invert/1", "[invert once]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Invert()
    ._().Action<A>()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);

  // Tick#1: A is running
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: A is success.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}

TEST_CASE("Simple/Invert/2", "[invert twice]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Invert()
    ._().Invert()
    ._()._().Action<A>()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);

  // Tick#1: A is running
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: A is success.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}
