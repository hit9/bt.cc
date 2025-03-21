#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("Condition/1", "[simplest condition - constructed from template]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Sequence()
    ._().Condition<C>()
    ._().Action<A>()
    .End()
    ;
	// clang-format on

	Entity e;

	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->shouldC == false);

	// Tick#1
	root.BindTreeBlob(e.blob);
	++ctx.seq;
	root.Tick(ctx);
	// A should not started.
	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->statusA == bt::Status::UNDEFINED);
	root.UnbindTreeBlob();

	// Tick#2: Make C true.
	root.BindTreeBlob(e.blob);
	bb->shouldC = true;
	++ctx.seq;
	root.Tick(ctx);
	// A should started running.
	REQUIRE(bb->counterA == 1);
	REQUIRE(bb->statusA == bt::Status::RUNNING);
	// The whole tree should running.
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);
	root.UnbindTreeBlob();

	// Tick#3: Make A Success.
	root.BindTreeBlob(e.blob);
	bb->shouldA = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	// The whole tree should Success.
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

	root.UnbindTreeBlob();
}

TEST_CASE("Condition/2", "[simplest condition - constructed from lambda]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Sequence()
    ._().Condition([&](const bt::Context& ctx) ->auto { return bb->shouldC; })
    ._().Action<A>()
    .End()
    ;
	// clang-format on

	Entity e;

	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->shouldC == false);

	// Tick#1
	root.BindTreeBlob(e.blob);

	++ctx.seq;
	root.Tick(ctx);
	// A should not started.
	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->statusA == bt::Status::UNDEFINED);

	// Tick#2: Make C true.
	bb->shouldC = true;
	++ctx.seq;
	root.Tick(ctx);
	// A should started running.
	REQUIRE(bb->counterA == 1);
	REQUIRE(bb->statusA == bt::Status::RUNNING);
	// The whole tree should running.
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	root.UnbindTreeBlob();
}

TEST_CASE("Condition/3", "[simplest condition - if]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Sequence()
    ._().If<C>()
    ._()._().Action<A>()
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
	// A should not started.
	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->statusA == bt::Status::UNDEFINED);

	// Tick#2: Make C true.
	bb->shouldC = true;
	++ctx.seq;
	root.Tick(ctx);
	// A should started running.
	REQUIRE(bb->counterA == 1);
	REQUIRE(bb->statusA == bt::Status::RUNNING);
	// The whole tree should running.
	REQUIRE(root.GetNodeBlob()->running);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#2: Make C false.
	bb->shouldC = false;
	++ctx.seq;
	root.Tick(ctx);
	// A should not change.
	REQUIRE(bb->counterA == 1);
	REQUIRE(bb->statusA == bt::Status::RUNNING);
	// The whole tree should failure.
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);

	root.UnbindTreeBlob();
}

TEST_CASE("Condition/4", "[simplest condition - and]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
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

	Entity e;
	root.BindTreeBlob(e.blob);

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);

	// Tick#2: Make C true.
	bb->shouldC = true;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);

	// Tick#2: Make All false.
	bb->shouldC = true;
	bb->shouldD = true;
	bb->shouldF = true;
	++ctx.seq;
	root.Tick(ctx);
	// The whole tree should Success
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

	root.UnbindTreeBlob();
}

TEST_CASE("Condition/5", "[simplest condition - or]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
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

	Entity e;
	root.BindTreeBlob(e.blob);

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);

	// Tick#2: Make C true.
	bb->shouldC = true;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

	// Tick#3: Make only D true.
	bb->shouldC = false;
	bb->shouldD = true;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

	// Tick#4: Make All false.
	bb->shouldC = true;
	bb->shouldD = true;
	bb->shouldF = true;
	++ctx.seq;
	root.Tick(ctx);
	// The whole tree should Success
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

	root.UnbindTreeBlob();
}

TEST_CASE("Condition/6", "[simplest condition - not]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Not()
    ._().Condition<C>()
    .End()
    ;
	// clang-format on

	Entity e;
	root.BindTreeBlob(e.blob);

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

	// Tick#2: Make C true.
	bb->shouldC = true;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);

	root.UnbindTreeBlob();
}

TEST_CASE("Condition/7", "[simplest condition - not2]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Sequence()
    ._().Not<C>()
    ._().Action<A>()
    .End()
    ;
	// clang-format on

	Entity e;
	root.BindTreeBlob(e.blob);

	REQUIRE(!bb->shouldC);

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);
	REQUIRE(bb->counterA == 1);

	// Tick#2: Make A Success.
	bb->shouldA = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
	REQUIRE(bb->counterA == 2);

	// Tick#3: Make C true.
	bb->shouldC = true;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);
	REQUIRE(bb->counterA == 2); // not ticked

	root.UnbindTreeBlob();
}

TEST_CASE("Condition/8", "[simple condition - bt::not/and/or]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Parallel()
    ._().If<bt::Not<C>>()
    ._()._().Action<A>()
    ._().IfNot<C>()
    ._()._().Action<B>()
    ._().If<bt::And<C, D>>()
    ._()._().Action<E>()
    ._().If<bt::Or<C, D>>()
    ._()._().Action<G>()
    ._().If<bt::And<C, bt::Or<D,F>>>()
    ._()._().Action<H>()
    .End()
    ;
	// clang-format on

	Entity e;
	root.BindTreeBlob(e.blob);

	REQUIRE(!bb->shouldC);
	REQUIRE(!bb->shouldD);
	REQUIRE(!bb->shouldF);

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);
	REQUIRE(bb->counterA == 1);
	REQUIRE(bb->counterB == 1);
	REQUIRE(bb->counterE == 0);
	REQUIRE(bb->counterG == 0);
	REQUIRE(bb->counterH == 0);

	// Tick#2: Make C true
	bb->shouldC = true;

	++ctx.seq;
	root.Tick(ctx);

	REQUIRE(root.LastStatus() == bt::Status::FAILURE);
	REQUIRE(bb->counterA == 1); // A not ticked, not changed
	REQUIRE(bb->counterB == 1); // B not ticked, not changed
	REQUIRE(bb->counterE == 0); // C && D still false, E  not ticked, not changed
	REQUIRE(bb->counterG == 1); // C || D changes to true, G ticked
	REQUIRE(bb->counterH == 0); // C && (D || F) still fail

	// Tick#3: Make D true
	bb->shouldD = true;

	++ctx.seq;
	root.Tick(ctx);

	REQUIRE(root.LastStatus() == bt::Status::FAILURE);
	REQUIRE(bb->counterA == 1); // A not ticked, not changed, but returns failure
	REQUIRE(bb->counterB == 1); // B not ticked, not changed, but returns failure
	REQUIRE(bb->counterE == 1); // C && D changes to true, E  ticked
	REQUIRE(bb->counterG == 2); // C || D still true, G ticked
	REQUIRE(bb->counterH == 1); // C && (D || F) changes to true, H ticked

	root.UnbindTreeBlob();
}

TEST_CASE("Condition/9", "[simple condition - True/False]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Selector()
    ._().If<bt::False>()
    ._()._().Action<A>()
    ._().If<bt::True>()
    ._()._().Action<B>()
    .End()
    ;
	// clang-format on

	Entity e;
	root.BindTreeBlob(e.blob);

	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->counterB == 0);

	// Tick#1:
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);
	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->counterB == 1);

	// Tick#2: make B Success.
	bb->shouldB = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->counterB == 2);

	root.UnbindTreeBlob();
}
