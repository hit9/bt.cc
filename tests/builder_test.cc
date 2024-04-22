#include <any>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <unordered_set>

#include "bt.h"
#include "types.h"

TEST_CASE("Builder/1", "[extend a custom decorator to builder]") {
  // CounterDecorator counts how many times the decorated nodes' ticking was executed.
  class CounterDecorator : public bt::DecoratorNode {
   public:
    CounterDecorator(const std::string& name = "CounterDecorator") : bt::DecoratorNode(name) {}
    bt::Status Update(const bt::Context& ctx) override {
      auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
      bb->customDecoratorCounter++;
      return child->Update(ctx);
    }
  };

  // Extend the builder.
  class MyTree : public bt::RootNode, public bt::Builder<MyTree> {
   public:
    MyTree(const std::string& name = "Root") : bt::RootNode(name) { bindRoot(*this); }
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

  // Tick#1
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->customDecoratorCounter == 1);

  // Tick#2
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->customDecoratorCounter == 3);
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
