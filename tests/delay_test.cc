#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "bt.h"
#include "types.h"

using namespace std::chrono_literals;

TEMPLATE_TEST_CASE("Delay/1", "[simple delay]", Entity,
                   (EntityFixedBlob<16, sizeof(bt::DelayNode::Blob)>)) {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Delay(100ms)
    ._().Action<A>()
    .End()
    ;
  // clang-format on

  TestType e;

  REQUIRE(bb->counterA == 0);
  // Tick#1: A is not started.
  root.BindTreeBlob(e.blob);
  ++ctx.seq;root.Tick(ctx);;
  REQUIRE(bb->counterA == 0);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  root.UnbindTreeBlob();

  // 100ms later
  std::this_thread::sleep_for(100ms);

  // Tick#2: A is started.
  root.BindTreeBlob(e.blob);
  ++ctx.seq;root.Tick(ctx);;
  REQUIRE(bb->counterA == 1);
  root.UnbindTreeBlob();
}
