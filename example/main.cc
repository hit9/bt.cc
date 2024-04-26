#include <cstdlib>

#include "bt.h"
using namespace std::chrono_literals;

// Action A randomly goes failure/success.
class A : public bt::ActionNode {
 public:
  bt::Status Update(const bt::Context& ctx) override {
    if (std::rand() % 10 > 5) return bt::Status::FAILURE;
    return bt::Status::SUCCESS;
  }
  std::string_view Name() const override { return "A"; }
};

// Action B always goes success.
class B : public bt::ActionNode {
 public:
  std::string_view Name() const override { return "B"; }
  bt::Status Update(const bt::Context& ctx) override { return bt::Status::SUCCESS; }
};

// Condition C randomly returns true.
class C : public bt::ConditionNode {
 public:
  std::string_view Name() const override { return "C"; }
  bool Check(const bt::Context& ctx) override { return std::rand() % 10 > 5; }
};

// Entity
struct Entity {
  bt::FixedTreeBlob<64, 64> blob;
};

int main(void) {
  bt::Tree root("Root");

  // helps to make a subtree.
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

  std::vector<Entity> entities(10);

  bt::Context ctx;

  while (true) {
    for (auto& entity : entities) {
      root.BindTreeBlob(entity.blob);
      ++ctx.seq;
      root.Tick(ctx);
      root.Visualize(ctx.seq);  // debug visualize
      root.UnbindTreeBlob();
    }
    std::this_thread::sleep_for(30ms);
  }

  return 0;
}
