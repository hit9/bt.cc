#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEMPLATE_TEST_CASE("SubTree/1", "[subtree test]", Entity, (EntityFixedBlob<32>))
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	bt::Tree subtree;

	// clang-format off
  subtree
  .Sequence()
  ._().Action<A>()
  ._().If<C>()
  ._()._().Action<B>()
  .End()
  ;
	// clang-format on

	// clang-format off
  root
  .Selector()
  ._().Action<E>()
  ._().Subtree(std::move(subtree))
  .End()
  ;
	// clang-format on

	TestType e;
	root.BindTreeBlob(e.blob);

	// Tick#1: Make Action E Failure.
	bb->shouldE = bt::Status::FAILURE;
	++ctx.seq;
	root.Tick(ctx);
	// A should running.
	REQUIRE(bb->counterA == 1);
	REQUIRE(bb->counterE == 1);
	REQUIRE(bb->counterB == 0);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#2: Make C true.
	bb->shouldC = true;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterA == 2);
	REQUIRE(bb->counterE == 2);
	REQUIRE(bb->counterB == 0); // B is blocked
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#3: Make A success.
	bb->shouldA = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterA == 3);
	REQUIRE(bb->counterE == 3);
	REQUIRE(bb->counterB == 1); // B is started
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#4: Make B success.
	bb->shouldB = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterA == 4);
	REQUIRE(bb->counterE == 4);
	REQUIRE(bb->counterB == 2); // B ok
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

	root.UnbindTreeBlob();
}
