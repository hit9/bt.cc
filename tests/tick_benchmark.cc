#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

// build a large tree.
void build(bt::Tree& root) {
  root.Parallel();
  for (int i = 0; i < 1000; i++) {
    // clang-format off
    root
    ._().Parallel()
    ._()._().Action<A>()
    ._()._().Action<B>()
    ._()._().Action<G>()
    ._()._().Action<H>()
    ._()._().Action<I>();
    // clang-format on
  }
}

TEST_CASE("Tick/1", "[simple traversal benchmark - without pool]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  build(root);
  BENCHMARK("benchmark simple parallel tree tick - without pool") {
      for (int i = 0; i < 100; i++)
    root.Tick(ctx);
  };
}

TEST_CASE("Tick/2", "[simple traversal benchmark - with pool]") {
  auto pool = std::make_shared<bt::NodePool>(1024 * 1000);
  bt::Tree root;
  root.BindPool(pool);
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  build(root);
  BENCHMARK("benchmark simple parallel tree tick - with pool") {
      for (int i = 0; i < 100; i++)
    root.Tick(ctx);
  };
}
