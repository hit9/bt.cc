#include <cstdlib>
#include <string>

#include "bt.h"
using namespace std::chrono_literals;

class A : public bt::ActionNode {
 public:
  bt::Status Update(const bt::Context& ctx) override {
    if (std::rand() % 10 > 5) return bt::Status::FAILURE;
    return bt::Status::SUCCESS;
  }
  std::string_view Name() const override { return "A"; }
};

class B : public bt::ActionNode {
 public:
  std::string_view Name() const override { return "B"; }
  bt::Status Update(const bt::Context& ctx) override { return bt::Status::SUCCESS; }
};

class C : public bt::ConditionNode {
 public:
  std::string_view Name() const override { return "C"; }
  bool Check(const bt::Context& ctx) override { return std::rand() % 10 > 5; }
};

int main(void) {
  bt::Tree root("Root");

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
  std::string s;
  bt::Context ctx;
  root.TickForever(ctx, 300ms, true);
  return 0;
}
