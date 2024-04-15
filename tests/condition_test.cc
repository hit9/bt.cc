#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("Condition/1", "[simplest condition - constructed from template]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Sequence()
    ._().Condition<C>()
    ._().Action<A>()
    .End()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->shouldC == false);

  // Tick#1
  root.Tick(ctx);
  // A should not started.
  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->statusA == bt::Status::UNDEFINED);

  // Tick#2: Make C true.
  bb->shouldC = true;
  root.Tick(ctx);
  // A should started running.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Make A Success.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  // The whole tree should Success.
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}

TEST_CASE("Condition/2", "[simplest condition - constructed from lambda]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Sequence()
    ._().Condition([&](const bt::Context& ctx) ->auto { return bb->shouldC; })
    ._().Action<A>()
    .End()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->shouldC == false);

  // Tick#1
  root.Tick(ctx);
  // A should not started.
  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->statusA == bt::Status::UNDEFINED);

  // Tick#2: Make C true.
  bb->shouldC = true;
  root.Tick(ctx);
  // A should started running.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
}

TEST_CASE("Condition/3", "[simplest condition - if]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Sequence()
    ._().If<C>()
    ._()._().Action<A>()
    .End()
    ;
  // clang-format on

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->shouldC == false);

  // Tick#1
  root.Tick(ctx);
  // A should not started.
  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->statusA == bt::Status::UNDEFINED);

  // Tick#2: Make C true.
  bb->shouldC = true;
  root.Tick(ctx);
  // A should started running.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Make C false.
  bb->shouldC = false;
  root.Tick(ctx);
  // A should not change.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  // The whole tree should failure.
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}

TEST_CASE("Condition/4", "[simplest condition - and]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Sequence()
    ._().Condition<C>()
    ._().Condition<D>()
    ._().Condition<F>()
    .End()
    ;
  // clang-format on

  // Tick#1
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);

  // Tick#2: Make C true.
  bb->shouldC = true;
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);

  // Tick#2: Make All false.
  bb->shouldC = true;
  bb->shouldD = true;
  bb->shouldF = true;
  root.Tick(ctx);
  // The whole tree should Success
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}

TEST_CASE("Condition/5", "[simplest condition - or]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Selector()
    ._().Condition<C>()
    ._().Condition<D>()
    ._().Condition<F>()
    .End()
    ;
  // clang-format on

  // Tick#1
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);

  // Tick#2: Make C true.
  bb->shouldC = true;
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

  // Tick#3: Make only D true.
  bb->shouldC = false;
  bb->shouldD = true;
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

  // Tick#4: Make All false.
  bb->shouldC = true;
  bb->shouldD = true;
  bb->shouldF = true;
  root.Tick(ctx);
  // The whole tree should Success
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}

TEST_CASE("Condition/6", "[simplest condition - not]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Not()
    ._().Condition<C>()
    .End()
    ;
  // clang-format on

  // Tick#1
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

  // Tick#2: Make C true.
  bb->shouldC = true;
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
}
