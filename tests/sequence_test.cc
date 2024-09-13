#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEST_CASE("Sequence/1", "[all success]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);
	// clang-format off
    root
    .Sequence()
    ._().Action<A>()
    ._().Action<B>()
    .End()
    ;
	// clang-format on

	Entity e;
	root.BindTreeBlob(e.blob);

	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->counterB == 0);

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	// A is still running, B has not started running.
	REQUIRE(bb->counterA == 1);
	REQUIRE(bb->counterB == 0);
	REQUIRE(bb->statusA == bt::Status::RUNNING);
	REQUIRE(bb->statusB == bt::Status::UNDEFINED);
	// The whole tree should running.
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#2: Makes A success.
	bb->shouldA = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	// A should success, and b should started running
	REQUIRE(bb->counterA == 2);
	REQUIRE(bb->counterB == 1);
	REQUIRE(bb->statusA == bt::Status::SUCCESS);
	REQUIRE(bb->statusB == bt::Status::RUNNING);

	// Tick#3
	++ctx.seq;
	root.Tick(ctx);
	// A should stay success, and b should still running
	REQUIRE(bb->counterA == 3);
	REQUIRE(bb->counterB == 2);
	REQUIRE(bb->statusA == bt::Status::SUCCESS);
	REQUIRE(bb->statusB == bt::Status::RUNNING);

	// Tick#3: Make B success.
	bb->shouldB = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	// A should stay success, and b should success.
	REQUIRE(bb->counterA == 4);
	REQUIRE(bb->counterB == 3);
	REQUIRE(bb->statusA == bt::Status::SUCCESS);
	REQUIRE(bb->statusB == bt::Status::SUCCESS);
	// The whole tree should success.
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

	root.UnbindTreeBlob();
}

TEST_CASE("Sequence/2", "[partial failure - first failure]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);
	// clang-format off
    root
    .Sequence()
    ._().Action<A>()
    ._().Action<B>()
    .End()
    ;
	// clang-format on
	Entity e;
	root.BindTreeBlob(e.blob);

	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->counterB == 0);

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	// A is still running, B has not started running.
	REQUIRE(bb->counterA == 1);
	REQUIRE(bb->counterB == 0);
	REQUIRE(bb->statusA == bt::Status::RUNNING);
	REQUIRE(bb->statusB == bt::Status::UNDEFINED);

	// Tick#2: Makes A failure
	bb->shouldA = bt::Status::FAILURE;
	++ctx.seq;
	root.Tick(ctx);
	// A should failure, and b should not started
	REQUIRE(bb->counterA == 2);
	REQUIRE(bb->counterB == 0);
	REQUIRE(bb->statusA == bt::Status::FAILURE);
	REQUIRE(bb->statusB == bt::Status::UNDEFINED);
	// The whole tree should failure.
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);
	root.UnbindTreeBlob();
}

TEST_CASE("Sequence/3", "[partial failure - last failure]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);
	// clang-format off
    root
    .Sequence()
    ._().Action<A>()
    ._().Action<B>()
    .End()
    ;
	// clang-format on
	Entity e;
	root.BindTreeBlob(e.blob);

	REQUIRE(bb->counterA == 0);
	REQUIRE(bb->counterB == 0);

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	// A is still running, B has not started running.
	REQUIRE(bb->counterA == 1);
	REQUIRE(bb->counterB == 0);
	REQUIRE(bb->statusA == bt::Status::RUNNING);
	REQUIRE(bb->statusB == bt::Status::UNDEFINED);

	// Tick#2: Makes A SUCCESS.
	bb->shouldA = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	// A should failure, and b should start running
	REQUIRE(bb->counterA == 2);
	REQUIRE(bb->counterB == 1);
	REQUIRE(bb->statusA == bt::Status::SUCCESS);
	REQUIRE(bb->statusB == bt::Status::RUNNING);
	// The whole tree should running
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#3: Makes B FAILURE
	bb->shouldB = bt::Status::FAILURE;
	++ctx.seq;
	root.Tick(ctx);

	// A should still success, and b should FAILURE
	REQUIRE(bb->counterA == 3);
	REQUIRE(bb->counterB == 2);
	REQUIRE(bb->statusA == bt::Status::SUCCESS);
	REQUIRE(bb->statusB == bt::Status::FAILURE);
	// The whole tree should FAILURE
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);
	root.UnbindTreeBlob();
}

TEST_CASE("Sequence/4", "[priority sequence - final success]")
{
	bt::Tree root;
	auto	 bb = std::make_shared<Blackboard>();

	bt::Context ctx(bb);
	// clang-format off
    root
    .Sequence()
    ._().Action<G>()
    ._().Action<H>()
    ._().Action<I>()
    .End()
    ;
	// clang-format on
	Entity e;
	root.BindTreeBlob(e.blob);
	REQUIRE(bb->counterG == 0);
	REQUIRE(bb->counterH == 0);
	REQUIRE(bb->counterI == 0);

	bb->shouldPriorityG = 1;
	bb->shouldPriorityH = 2;
	bb->shouldPriorityI = 3;

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterI == 1);
	REQUIRE(bb->counterH == 0);
	REQUIRE(bb->counterG == 0);
	REQUIRE(bb->statusI == bt::Status::RUNNING);
	REQUIRE(bb->statusH == bt::Status::UNDEFINED);
	REQUIRE(bb->statusG == bt::Status::UNDEFINED);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#2: makes I success
	bb->shouldI = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterI == 2);
	REQUIRE(bb->counterH == 1);
	REQUIRE(bb->counterG == 0);
	REQUIRE(bb->statusI == bt::Status::SUCCESS);
	REQUIRE(bb->statusH == bt::Status::RUNNING);
	REQUIRE(bb->statusG == bt::Status::UNDEFINED);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#3: makes H success
	bb->shouldH = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterI == 3);
	REQUIRE(bb->counterH == 2);
	REQUIRE(bb->counterG == 1);
	REQUIRE(bb->statusI == bt::Status::SUCCESS);
	REQUIRE(bb->statusH == bt::Status::SUCCESS);
	REQUIRE(bb->statusG == bt::Status::RUNNING);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#4: makes G success
	bb->shouldG = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterI == 4);
	REQUIRE(bb->counterH == 3);
	REQUIRE(bb->counterG == 2);
	REQUIRE(bb->statusI == bt::Status::SUCCESS);
	REQUIRE(bb->statusH == bt::Status::SUCCESS);
	REQUIRE(bb->statusG == bt::Status::SUCCESS);
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
	root.UnbindTreeBlob();
}

TEST_CASE("Sequence/5", "[priority sequence - partial success/failure]")
{
	bt::Tree root;
	auto	 bb = std::make_shared<Blackboard>();

	bt::Context ctx(bb);
	// clang-format off
    root
    .Sequence()
    ._().Action<G>()
    ._().Action<H>()
    ._().Action<I>()
    .End()
    ;
	// clang-format on
	Entity e;
	root.BindTreeBlob(e.blob);
	REQUIRE(bb->counterG == 0);
	REQUIRE(bb->counterH == 0);
	REQUIRE(bb->counterI == 0);

	bb->shouldPriorityG = 1;
	bb->shouldPriorityH = 2;
	bb->shouldPriorityI = 3;

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterI == 1);
	REQUIRE(bb->counterH == 0);
	REQUIRE(bb->counterG == 0);
	REQUIRE(bb->statusI == bt::Status::RUNNING);
	REQUIRE(bb->statusH == bt::Status::UNDEFINED);
	REQUIRE(bb->statusG == bt::Status::UNDEFINED);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#2: makes I success
	bb->shouldI = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterI == 2);
	REQUIRE(bb->counterH == 1);
	REQUIRE(bb->counterG == 0);
	REQUIRE(bb->statusI == bt::Status::SUCCESS);
	REQUIRE(bb->statusH == bt::Status::RUNNING);
	REQUIRE(bb->statusG == bt::Status::UNDEFINED);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#3: makes H FAILURE
	bb->shouldH = bt::Status::FAILURE;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterI == 3);
	REQUIRE(bb->counterH == 2);
	REQUIRE(bb->counterG == 0);
	REQUIRE(bb->statusI == bt::Status::SUCCESS);
	REQUIRE(bb->statusH == bt::Status::FAILURE);
	REQUIRE(bb->statusG == bt::Status::UNDEFINED);
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);
	root.UnbindTreeBlob();
}

TEST_CASE("Sequence/5", "[priority sequence - dynamic]")
{
	bt::Tree root;
	auto	 bb = std::make_shared<Blackboard>();

	bt::Context ctx(bb);
	// clang-format off
    root
    .Sequence()
    ._().Action<G>()
    ._().Action<H>()
    ._().Action<I>()
    .End()
    ;
	// clang-format on
	Entity e;
	root.BindTreeBlob(e.blob);
	REQUIRE(bb->counterG == 0);
	REQUIRE(bb->counterH == 0);
	REQUIRE(bb->counterI == 0);

	bb->shouldPriorityG = 1;
	bb->shouldPriorityH = 2;
	bb->shouldPriorityI = 3;

	// Tick#1
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterI == 1);
	REQUIRE(bb->counterH == 0);
	REQUIRE(bb->counterG == 0);
	REQUIRE(bb->statusI == bt::Status::RUNNING);
	REQUIRE(bb->statusH == bt::Status::UNDEFINED);
	REQUIRE(bb->statusG == bt::Status::UNDEFINED);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#2: makes G' and H' s priority higher
	bb->shouldPriorityG = 3;
	bb->shouldPriorityH = 3;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterI == 1);
	REQUIRE(bb->counterH == 0);
	REQUIRE(bb->counterG == 1);
	REQUIRE(bb->statusI == bt::Status::RUNNING);
	REQUIRE(bb->statusH == bt::Status::UNDEFINED);
	REQUIRE(bb->statusG == bt::Status::RUNNING);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#2: makes I' s priority higher
	bb->shouldPriorityI = 999;
	bb->shouldI = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterI == 2);
	REQUIRE(bb->counterH == 0);
	REQUIRE(bb->counterG == 2);
	REQUIRE(bb->statusI == bt::Status::SUCCESS);
	REQUIRE(bb->statusH == bt::Status::UNDEFINED);
	REQUIRE(bb->statusG == bt::Status::RUNNING);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);
	root.UnbindTreeBlob();
}
