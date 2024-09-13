#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "bt.h"
#include "types.h"

// build a large tree.
void build(bt::Tree& root, int n = 1000)
{
	root.Sequence();
	for (int i = 0; i < n; i++)
	{
		// clang-format off
    root
    ._().Action<A>()
    ._().Action<B>()
    ._().Action<G>()
    ._().Action<H>()
    ._().Action<I>();
		// clang-format on
	}
	root.End();
}

// build a large tree.
void buildStateful(bt::Tree& root)
{
	root.StatefulSequence();
	for (int i = 0; i < 1000; i++)
	{
		// clang-format off
    root
    ._().Action<A>()
    ._().Action<B>()
    ._().Action<G>()
    ._().Action<H>()
    ._().Action<I>();
		// clang-format on
	}
	root.End();
}

TEMPLATE_TEST_CASE("Tick/1", "[simple small tree traversal benchmark - 60 nodes ]", Entity,
	(EntityFixedBlob<62>))
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);
	build(root, 10);
	TestType e;
	bb->shouldA = bt::Status::SUCCESS;
	bb->shouldB = bt::Status::SUCCESS;
	bb->shouldG = bt::Status::SUCCESS;
	bb->shouldH = bt::Status::SUCCESS;
	bb->shouldI = bt::Status::SUCCESS;
	BENCHMARK("bench tick without priorities  small tree - 60 nodes ")
	{
		root.BindTreeBlob(e.blob);
		++ctx.seq;
		root.Tick(ctx);
		root.UnbindTreeBlob();
	};
}

TEMPLATE_TEST_CASE("Tick/1", "[simple traversal benchmark ]", Entity, (EntityFixedBlob<6002>))
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);
	build(root);
	TestType e;
	bb->shouldA = bt::Status::SUCCESS;
	bb->shouldB = bt::Status::SUCCESS;
	bb->shouldG = bt::Status::SUCCESS;
	bb->shouldH = bt::Status::SUCCESS;
	bb->shouldI = bt::Status::SUCCESS;
	BENCHMARK("bench tick without priorities - 6000 nodes ")
	{
		root.BindTreeBlob(e.blob);
		++ctx.seq;
		root.Tick(ctx);
		root.UnbindTreeBlob();
	};
}

TEST_CASE("Tick/2", "[simple traversal benchmark - priority ]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);
	build(root);
	Entity e;
	bb->shouldA = bt::Status::SUCCESS;
	bb->shouldB = bt::Status::SUCCESS;
	bb->shouldG = bt::Status::SUCCESS;
	bb->shouldH = bt::Status::SUCCESS;
	bb->shouldI = bt::Status::SUCCESS;
	bb->shouldPriorityI = 4;
	bb->shouldPriorityH = 3;
	bb->shouldPriorityG = 2;
	BENCHMARK("bench tick with priorities - 6000 nodes")
	{
		root.BindTreeBlob(e.blob);
		++ctx.seq;
		root.Tick(ctx);
		root.UnbindTreeBlob();
	};
}

TEST_CASE("Tick/3", "[simple traversal benchmark - stateful ]")
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);
	buildStateful(root);
	Entity e;
	bb->shouldA = bt::Status::SUCCESS;
	bb->shouldB = bt::Status::SUCCESS;
	bb->shouldG = bt::Status::SUCCESS;
	bb->shouldH = bt::Status::SUCCESS;
	bb->shouldI = bt::Status::SUCCESS;
	BENCHMARK("bench tick without priorities - stateful - 6000 nodes ")
	{
		root.BindTreeBlob(e.blob);
		++ctx.seq;
		root.Tick(ctx);
		root.UnbindTreeBlob();
	};
}

TEMPLATE_TEST_CASE("Tick/4", "[lots of entities]", Entity, (EntityFixedBlob<302>))
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);
	build(root, 50);

	std::vector<TestType> entities(1000);
	bb->shouldA = bt::Status::SUCCESS;
	bb->shouldB = bt::Status::SUCCESS;
	bb->shouldG = bt::Status::SUCCESS;
	bb->shouldH = bt::Status::SUCCESS;
	bb->shouldI = bt::Status::SUCCESS;
	BENCHMARK("bench lots of entities - 1000 entities x 300 nodes")
	{
		for (auto& e : entities)
		{
			root.BindTreeBlob(e.blob);
			++ctx.seq;
			root.Tick(ctx);
			root.UnbindTreeBlob();
		}
	};
}
