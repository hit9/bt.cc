#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("Action/1", "[empty action]")
{
	bt::Tree root;

	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Selector()
    ._().If<C>()
    ._()._().Action<bt::Empty>()
    ._().Action<A>()
    .End()
    ;
	// clang-format on

	Entity e;
	root.BindTreeBlob(e.blob);

	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->shouldC == false);

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterA == 1);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#2: make C returns true
	bb->shouldC = true;

	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterA == 1);						   // not ticked
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS); // empty returns SUCCESS.

	root.UnbindTreeBlob();
}
