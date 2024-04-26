#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

// build a large tree.
void build(bt::Tree& root) {
  root.Sequence();
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
  root.End();
}

// build a large tree.
void buildStateful(bt::Tree& root) {
  root.StatefulSequence();
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
  BENCHMARK("bench tick without priorities ") {
    root.BindTreeBlob(e.blob);
    root.Tick(ctx);
    root.UnbindTreeBlob();
  };
}

TEST_CASE("Tick/2", "[simple traversal benchmark - priority ]") {
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
  bb->shouldPriorityI = 4;
  bb->shouldPriorityH = 3;
  bb->shouldPriorityG = 2;
  BENCHMARK("bench tick with priorities ") {
    root.BindTreeBlob(e.blob);
    root.Tick(ctx);
    root.UnbindTreeBlob();
  };
}

TEST_CASE("Tick/3", "[simple traversal benchmark - stateful ]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  buildStateful(root);
  Entity e;
  bb->shouldA = bt::Status::SUCCESS;
  bb->shouldB = bt::Status::SUCCESS;
  bb->shouldG = bt::Status::SUCCESS;
  bb->shouldH = bt::Status::SUCCESS;
  bb->shouldI = bt::Status::SUCCESS;
  BENCHMARK("bench tick without priorities - stateful ") {
    root.BindTreeBlob(e.blob);
    root.Tick(ctx);
    root.UnbindTreeBlob();
  };
}

TEST_CASE("Tick/4", "[simple traversal benchmark - fixed blob ]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  build(root);
  EntityFixedBlob<6002, sizeof(bt::NodeBlob)> e;
  bb->shouldA = bt::Status::SUCCESS;
  bb->shouldB = bt::Status::SUCCESS;
  bb->shouldG = bt::Status::SUCCESS;
  bb->shouldH = bt::Status::SUCCESS;
  bb->shouldI = bt::Status::SUCCESS;
  BENCHMARK("bench tick without priorities - fixed blob ") {
    root.BindTreeBlob(e.blob);
    root.Tick(ctx);
    root.UnbindTreeBlob();
  };
}
