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
    ._().Sequence()
    ._()._().Action<A>()
    ._()._().Action<B>()
    ._()._().Action<G>()
    ._()._().Action<H>()
    ._()._().Action<I>();
    // clang-format on
  }
  root.End();
}

TEST_CASE("Tick/1", "[simple traversal benchmark ]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  build(root);
  Entity e;
  bb->shouldA = bt::Status::SUCCESS;
  bb->shouldB = bt::Status::SUCCESS;
  bb->shouldG = bt::Status::SUCCESS;
  bb->shouldH = bt::Status::SUCCESS;
  bb->shouldI = bt::Status::SUCCESS;
  BENCHMARK("benchmark simple parallel tree tick ") {
    root.BindTreeBlob(e.blob);
    root.Tick(ctx);
    root.UnbindTreeBlob();
  };
}
