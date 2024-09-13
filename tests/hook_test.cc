#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

static bool onBuildCalled = false;

TEST_CASE("Hook/1", "[hook OnBuild]")
{
	// Custom decorators
	class CustomDecorator : public bt::DecoratorNode
	{
		bt::Status Update(const bt::Context& ctx) override { return child->Update(ctx); }
		void	   OnBuild() override { onBuildCalled = true; }
	};

	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);
	// clang-format off
    root
    .C<CustomDecorator>()
    ._().Action<A>()
    .End()
    ;
	// clang-format on

	REQUIRE(onBuildCalled);
}

TEMPLATE_TEST_CASE("Hook/2", "[hook OnEnter]", Entity, (EntityFixedBlob<16>))
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);
	// clang-format off
    root
    .Parallel()
    ._().Action<A>()
    .End()
    ;
	// clang-format on

	TestType e;

	root.BindTreeBlob(e.blob);
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->onEnterCalledA);
	root.UnbindTreeBlob();
}

TEMPLATE_TEST_CASE("Hook/3", "[hook OnTerminate]", Entity, (EntityFixedBlob<16>))
{
	bt::Tree	root;
	auto		bb = std::make_shared<Blackboard>();
	bt::Context ctx(bb);
	// clang-format off
    root
    .Parallel()
    ._().Action<A>()
    .End()
    ;
	// clang-format on

	TestType e;

	root.BindTreeBlob(e.blob);
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(!bb->onTerminatedCalledA);
	root.UnbindTreeBlob();

	root.BindTreeBlob(e.blob);
	bb->shouldA = bt::Status::FAILURE;
	++ctx.seq;
	root.Tick(ctx);
	REQUIRE(bb->onTerminatedCalledA);
	root.UnbindTreeBlob();
}
