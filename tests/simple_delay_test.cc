#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "bt.h"
#include "types.h"

using namespace std::chrono_literals;

TEST_CASE("Simple/Delay/1", "[simple delay]") {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  // clang-format off
    root
    .Delay(100ms)
    ._().Action<A>()
    ;
  // clang-format on

  // Tick#1: A is not started.
  root.Tick(ctx);
  REQUIRE(bb->counterA == 0);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // 100ms later
  std::this_thread::sleep_for(100ms);

  // Tick#2: A is started.
  root.Tick(ctx);
  REQUIRE(bb->counterA == 1);
}
