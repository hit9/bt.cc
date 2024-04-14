#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

// build a large tree.
void build(bt::Tree& root) {
  root.RandomSelector();
  for (int i = 0; i < 1000; i++) {
    // clang-format off
    root
    ._().Action<A>()
    ._().Action<B>()
    ._().Action<G>()
    ._().Action<H>()
    ._().Action<I>();
    // clang-format on
  }
}

TEST_CASE("Tick/1", "[simple traversal benchmark - without pool]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  build(root);
  BENCHMARK("benchmark simple parallel tree tick - without pool") {
    root.Tick(ctx);
  };
}
