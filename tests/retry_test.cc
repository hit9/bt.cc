#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "bt.h"
#include "types.h"

using namespace std::chrono_literals;

TEMPLATE_TEST_CASE("Retry/1", "[simple retry success]", Entity,
                   (EntityFixedBlob<16, sizeof(bt::RetryNode<>::Blob)>)) {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Retry(3, 30ms)
    ._().Action<A>()
    .End()
    ;
  // clang-format on
  TestType e;
  root.BindTreeBlob(e.blob);

  // Tick#1
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A success.
  bb->shouldA = bt::Status::SUCCESS;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

  root.UnbindTreeBlob();
}

TEMPLATE_TEST_CASE("Retry/2", "[simple retry final failure]", Entity,
                   (EntityFixedBlob<16, sizeof(bt::RetryNode<>::Blob)>)) {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Retry(3, 30ms)
    ._().Action<A>()
    .End()
    ;
  // clang-format on

  TestType e;
  root.BindTreeBlob(e.blob);

  // Tick#1
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A failure
  bb->shouldA = bt::Status::FAILURE;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Retry 2 more times
  for (int i = 1; i <= 2; i++) {
    std::this_thread::sleep_for(30ms);
    bb->shouldA = bt::Status::RUNNING;
    ++ctx.seq;root.Tick(ctx);
    REQUIRE(root.LastStatus() == bt::Status::RUNNING);
    bb->shouldA = bt::Status::FAILURE;
    ++ctx.seq;root.Tick(ctx);
    REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  }

  // Next the whole tree should failure.
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);

  root.UnbindTreeBlob();
}

TEMPLATE_TEST_CASE("Retry/3", "[simple retry final success ]", Entity,
                   (EntityFixedBlob<16, sizeof(bt::RetryNode<>::Blob)>)) {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Retry(3, 30ms)
    ._().Action<A>()
    .End()
    ;
  // clang-format on
  TestType e;
  root.BindTreeBlob(e.blob);

  // Tick#1
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A failure
  bb->shouldA = bt::Status::FAILURE;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Makes A ok.
  std::this_thread::sleep_for(30ms);
  bb->shouldA = bt::Status::SUCCESS;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
  root.UnbindTreeBlob();
}

TEMPLATE_TEST_CASE("Retry/4", "[simple retry forever ]", Entity,
                   (EntityFixedBlob<16, sizeof(bt::RetryNode<>::Blob)>)) {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .RetryForever(1ms)
    ._().Action<A>()
    .End()
    ;
  // clang-format on

  TestType e;
  root.BindTreeBlob(e.blob);

  // Tick#1
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Makes A failure
  bb->shouldA = bt::Status::FAILURE;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 2);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Makes A ok.
  for (int i = 0; i < 30; i++) {
    std::this_thread::sleep_for(1ms);
    bb->shouldA = bt::Status::FAILURE;
    ++ctx.seq;root.Tick(ctx);
    REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  }
  root.UnbindTreeBlob();
}
