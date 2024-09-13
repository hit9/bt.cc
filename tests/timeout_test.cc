#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "bt.h"
#include "types.h"

using namespace std::chrono_literals;

TEMPLATE_TEST_CASE("Timeout/1", "[simple timeout success]", Entity,
	(EntityFixedBlob<16, sizeof(bt::TimeoutNode::Blob)>))
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Timeout(100ms)
    ._().Action<A>()
    .End()
    ;
	// clang-format on

	TestType e;
	root.BindTreeBlob(e.blob);
	// Tick#1: A is not started.
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterA == 1);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#2: Makes A success.
	bb->shouldA = bt::Status::SUCCESS;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterA == 2);
	REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
	root.UnbindTreeBlob();
}

TEMPLATE_TEST_CASE("Timeout/2", "[simple timeout failure]", Entity,
	(EntityFixedBlob<16, sizeof(bt::TimeoutNode::Blob)>))
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Timeout(100ms)
    ._().Action<A>()
    .End()
    ;
	// clang-format on

	TestType e;
	root.BindTreeBlob(e.blob);
	// Tick#1: A is not started.
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterA == 1);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#2: Makes A failure.
	bb->shouldA = bt::Status::FAILURE;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterA == 2);
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);
	root.UnbindTreeBlob();
}

TEMPLATE_TEST_CASE("Timeout/3", "[simple timeout timedout]", Entity,
	(EntityFixedBlob<16, sizeof(bt::TimeoutNode::Blob)>))
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);

	// clang-format off
    root
    .Timeout(100ms)
    ._().Action<A>()
    .End()
    ;
	// clang-format on

	TestType e;
	root.BindTreeBlob(e.blob);
	// Tick#1: A is not started.
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterA == 1);
	REQUIRE(root.LastStatus() == bt::Status::RUNNING);

	// Tick#2: should timeout
	std::this_thread::sleep_for(110ms);
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->counterA == 1);
	REQUIRE(root.LastStatus() == bt::Status::FAILURE);
	root.UnbindTreeBlob();
}
