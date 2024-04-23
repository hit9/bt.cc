#include <any>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string_view>
#include <unordered_set>

#include "bt.h"
#include "types.h"

TEST_CASE("Builder/1", "[extend a custom decorator to builder]") {
  // CounterDecorator counts how many times the decorated nodes' ticking was executed.
  class CounterDecorator : public bt::DecoratorNode {
   public:
    CounterDecorator(std::string_view name = "CounterDecorator") : bt::DecoratorNode(name) {}
    bt::Status Update(const bt::Context& ctx) override {
      auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
      bb->customDecoratorCounter++;
      return child->Update(ctx);
    }
  };

  // Extend the builder.
  class MyTree : public bt::RootNode, public bt::Builder<MyTree> {
   public:
    MyTree(std::string_view name = "Root") : bt::RootNode(name), Builder() { bindRoot(*this); }
    auto& Counter() { return C<CounterDecorator>(); }
  };

  MyTree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
  root
  .Sequence()
  ._().Counter()
  ._()._().Action<A>()
  ._().Counter()
  ._()._().Action<B>()
  .End()
  ;
  // clang-format on

  Entity e;

  // Tick#1

  root.BindTreeBlob(e.blob);
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->customDecoratorCounter == 1);
  root.UnbindTreeBlob();

  // Tick#2
  root.BindTreeBlob(e.blob);
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->customDecoratorCounter == 3);
  root.UnbindTreeBlob();
}

TEST_CASE("Builder/2", "[node id increment]") {
  auto st = [&]() {
    bt::Tree subtree("Subtree");
    // clang-format off
      subtree
        .Sequence()
        ._().Action<A>()
        ._().Action<B>()
        .End()
      ;
    // clang-format on
    return subtree;
  };

  bt::Tree root;
  // clang-format off
  root
    .Sequence()
    ._().Switch()
    ._()._().Case<C>()
    ._()._()._().Action<A>()
    ._()._().Case<C>()
    ._()._()._().Sequence()
    ._()._()._()._().Action<A>()
    ._()._()._()._().Action<B>()
    ._().Parallel()
    ._()._().Action<A>()
    ._()._().Action<B>()
    ._().Subtree(st())
    .End()
    ;
  // clang-format on

  std::unordered_set<bt::NodeId> ids;
  bt::Node::TraversalCallback f = [&](bt::Node& node) {
    REQUIRE(ids.find(node.Id()) == ids.end());
    ids.insert(node.Id());
  };
  root.Traverse(f);
}

TEST_CASE("Builder/3", "[node count]") {
  auto st = [&]() {  // n: 4
    bt::Tree subtree("Subtree");
    // clang-format off
      subtree
        .Sequence()
        ._().Action<A>()
        ._().Action<B>()
        .End()
      ;
    // clang-format on
    return subtree;
  };

  bt::Tree root;

  // clang-format off
  root // 1, => total 18
    .Sequence() // 1
    ._().Switch() // 1
    ._()._().Case<C>() // 2
    ._()._()._().Action<A>() // 1
    ._()._().Case<C>() // 2
    ._()._()._().Sequence() // 1
    ._()._()._()._().Action<A>() // 1
    ._()._()._()._().Action<B>() // 1
    ._().Parallel() // 1
    ._()._().Action<A>() // 1
    ._()._().Action<B>() // 1
    ._().Subtree(st()) // 4
    .End()
    ;
  // clang-format on
  REQUIRE(st().NumNodes() == 4);
  REQUIRE(root.NumNodes() == 18);

  // All id should <= n;
  bt::Node::TraversalCallback cb1 = [&](bt::Node& node) { REQUIRE(node.Id() <= root.NumNodes()); };
  root.Traverse(cb1);
}
