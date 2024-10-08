#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEMPLATE_TEST_CASE("Repeat/1", "[simple repeat]", Entity,
	(EntityFixedBlob<16, sizeof(bt::RepeatNode::Blob)>))
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Parallel()
    ._().Action<E>()
    ._().Repeat(2)
    ._()._().Sequence()
    ._()._()._().Action<A>()
    ._()._()._().Action<B>()
    .End()
    ;
	// clang-format on
	TestType e;
	root.BindTreeBlob(e.blob);

	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->counterB == 0);
	REQUIRE(bb->counterE == 0);

	// Tick1
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterE == 1);
	REQUIRE(bb->counterA == 1);
	REQUIRE(bb->counterB == 0); // blocked by A
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick2: Make A, B, E success.
	bb->shouldA = bt::Status::SUCCESS;
	bb->shouldB = bt::Status::SUCCESS;
	bb->shouldE = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterE == 2);
	REQUIRE(bb->counterA == 2);
	REQUIRE(bb->counterB == 1);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING); // Repeat is still RUNNING

	// Tick3: A, B goes into RUNNING again.
	bb->shouldA = bt::Status::RUNNING;
	bb->shouldB = bt::Status::RUNNING;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterE == 3);
	REQUIRE(bb->counterA == 3);
	REQUIRE(bb->counterB == 1);						   // blocked by A
	REQUIRE(root.LastStatus() == bt::Status::RUNNING); // still running

	// Tick4: A, B both gose into SUCCESS again.
	bb->shouldA = bt::Status::SUCCESS;
	bb->shouldB = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterE == 4);
	REQUIRE(bb->counterA == 4);
	REQUIRE(bb->counterB == 2);						   // blocked by A
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS); // repeat => success.

	root.UnbindTreeBlob();
}
