#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("ForceSuccess", "[simple force success test]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
  root
  .ForceSuccess()
  ._().Action<A>()
  .End()
  ;
  // clang-format on

  Entity e;
  root.BindTreeBlob(e.blob);

  //  Tick#1
  ++ctx.seq;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);  // report running as is

  // Tick#2: Makes A failed.
  bb->shouldA = bt::Status::FAILURE;
  ++ctx.seq;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);  // still success.

  // Tick#3: Makes A success.
  bb->shouldA = bt::Status::SUCCESS;
  ++ctx.seq;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 3);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);  // still success.
}

TEST_CASE("ForceFailure", "[simple force failure test]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
  root
  .ForceFailure()
  ._().Action<A>()
  .End()
  ;
  // clang-format on

  Entity e;
  root.BindTreeBlob(e.blob);

  //  Tick#1
  ++ctx.seq;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);  // report running as is

  // Tick#2: Makes A failed.
  bb->shouldA = bt::Status::FAILURE;
  ++ctx.seq;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);  // should also failure.

  // Tick#3: Makes A success.
  bb->shouldA = bt::Status::SUCCESS;
  ++ctx.seq;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 3);
  REQUIRE(bb->statusA == bt::Status::SUCCESS);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);  // still failure.
}

