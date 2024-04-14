#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("Pool/1", "[pool - subtree test]") {
  bt::Tree root;
  auto pool = std::make_shared<bt::NodePool>(10240); // 10KB
  root.BindPool(pool);
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  auto st = [&]() {
    bt::Tree subtree;
    subtree.BindPool(pool);

    // clang-format off

    subtree
    .Sequence()
    ._().Action<A>()
    ._().If<C>()
    ._()._().Action<B>()
    ;
    // clang-format on

    return subtree;
  };

  // clang-format off
  root
  .Selector()
  ._().Action<E>()
  ._().Subtree(st());
  ;
  // clang-format on

  // Tick#1: Make Action E Failure.
  bb->shouldE = bt::Status::FAILURE;
  root.Tick(ctx);
  // A should running.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterE == 1);
  REQUIRE(bb->counterB == 0);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Make C true.
  bb->shouldC = true;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterE == 2);
  REQUIRE(bb->counterB == 0);  // B is blocked
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Make A success.
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 3);
  REQUIRE(bb->counterE == 3);
  REQUIRE(bb->counterB == 1);  // B is started
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#4: Make B success.
  bb->shouldB = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 4);
  REQUIRE(bb->counterE == 4);
  REQUIRE(bb->counterB == 2);  // B ok
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
}
