#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bt.h"
#include "types.h"

TEMPLATE_TEST_CASE("StatefulSelector/1", "[final success]", Entity,
                   (EntityFixedBlob<16, sizeof(bt::StatefulSelectorNode::Blob)>)) {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .StatefulSelector()
    ._().Action<A>()
    ._().Action<B>()
    ._().Action<E>()
    .End()
    ;
  // clang-format on

  TestType e;
  root.BindTreeBlob(e.blob);
  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->counterB == 0);

  // Tick#1
  ++ctx.seq;root.Tick(ctx);
  // A is still running, B & E has not started running.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::UNDEFINED);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Make A failure.
  bb->shouldA = bt::Status::FAILURE;
  ++ctx.seq;root.Tick(ctx);
  // A should failure, and B should started running, E should still not started.
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3
  ++ctx.seq;root.Tick(ctx);
  // A should stay failure, but without ticked.
  // B should still running, got one more tick.
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 2);
  // E still cooling down.
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#4: Makes B failure
  bb->shouldB = bt::Status::FAILURE;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked
  REQUIRE(bb->counterB == 3);  // got one more tick.
  // E started running
  REQUIRE(bb->counterE == 1);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#5: Makes E success
  bb->shouldE = bt::Status::SUCCESS;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked.
  REQUIRE(bb->counterB == 3);  // not ticked.
  REQUIRE(bb->counterE == 2);  // got one more tick.
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  REQUIRE(bb->statusE == bt::Status::SUCCESS);
  // The whole tree should SUCCESS.
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);

  // Tick#6: One more tick should restart from the first
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 3);  // got ticked.
  root.UnbindTreeBlob();
}

TEMPLATE_TEST_CASE("StatefulSelector/2", "[final failure]", Entity,
                   (EntityFixedBlob<16, sizeof(bt::StatefulSelectorNode::Blob)>)) {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .StatefulSelector()
    ._().Action<A>()
    ._().Action<B>()
    ._().Action<E>()
    .End()
    ;
  // clang-format on
  TestType e;
  root.BindTreeBlob(e.blob);

  REQUIRE(bb->counterA == 0);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->counterB == 0);

  // Tick#1
  ++ctx.seq;root.Tick(ctx);
  // A is still running, B & E has not started running.
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->counterB == 0);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::RUNNING);
  REQUIRE(bb->statusB == bt::Status::UNDEFINED);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Make A failure.
  bb->shouldA = bt::Status::FAILURE;
  ++ctx.seq;root.Tick(ctx);
  // A should failure, and B should started running, E should still not started.
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 1);
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3
  ++ctx.seq;root.Tick(ctx);
  // A should stay failure, but without ticked.
  // B should still running, got one more tick.
  REQUIRE(bb->counterA == 2);
  REQUIRE(bb->counterB == 2);
  // E still cooling down.
  REQUIRE(bb->counterE == 0);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->statusE == bt::Status::UNDEFINED);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#4: Makes B failure
  bb->shouldB = bt::Status::FAILURE;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked
  REQUIRE(bb->counterB == 3);  // got one more tick.
  // E started running
  REQUIRE(bb->counterE == 1);
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  // The whole tree should running.
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#5: Makes E failure
  bb->shouldE = bt::Status::FAILURE;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 2);  // not ticked.
  REQUIRE(bb->counterB == 3);  // not ticked.
  REQUIRE(bb->counterE == 2);  // got one more tick.
  REQUIRE(bb->statusA == bt::Status::FAILURE);
  REQUIRE(bb->statusB == bt::Status::FAILURE);
  REQUIRE(bb->statusE == bt::Status::FAILURE);
  // The whole tree should SUCCESS.
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);

  // Tick#6: One more tick should restart from the first
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterA == 3);  // got ticked.
  root.UnbindTreeBlob();
}

TEMPLATE_TEST_CASE("StatefulSelector/3", "[priority statefule selector final success]", Entity,
                   (EntityFixedBlob<16, sizeof(bt::StatefulSelectorNode::Blob)>)) {
  bt::Tree root;
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  // clang-format off
    root
    .StatefulSelector()
    ._().Action<G>()
    ._().Action<H>()
    ._().Action<I>()
    .End()
    ;
  // clang-format on
  TestType e;
  root.BindTreeBlob(e.blob);

  REQUIRE(bb->counterG == 0);
  REQUIRE(bb->counterH == 0);
  REQUIRE(bb->counterI == 0);

  bb->shouldPriorityG = 1;
  bb->shouldPriorityH = 2;
  bb->shouldPriorityI = 3;

  // Tick#1
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterG == 0);
  REQUIRE(bb->counterH == 0);
  REQUIRE(bb->counterI == 1);
  REQUIRE(bb->statusG == bt::Status::UNDEFINED);
  REQUIRE(bb->statusH == bt::Status::UNDEFINED);
  REQUIRE(bb->statusI == bt::Status::RUNNING);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#2: Make I failure.
  bb->shouldI = bt::Status::FAILURE;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterG == 0);
  REQUIRE(bb->counterH == 1);
  REQUIRE(bb->counterI == 2);
  REQUIRE(bb->statusG == bt::Status::UNDEFINED);
  REQUIRE(bb->statusH == bt::Status::RUNNING);
  REQUIRE(bb->statusI == bt::Status::FAILURE);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#3: Make H failure.
  bb->shouldH = bt::Status::FAILURE;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterG == 1);
  REQUIRE(bb->counterH == 2);
  REQUIRE(bb->counterI == 2);  // skip ticked
  REQUIRE(bb->statusG == bt::Status::RUNNING);
  REQUIRE(bb->statusH == bt::Status::FAILURE);
  REQUIRE(bb->statusI == bt::Status::FAILURE);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);

  // Tick#4: Make G SUCCESS.
  bb->shouldG = bt::Status::SUCCESS;
  ++ctx.seq;root.Tick(ctx);
  REQUIRE(bb->counterG == 2);
  REQUIRE(bb->counterH == 2);  // skip ticked.
  REQUIRE(bb->counterI == 2);  // skip ticked
  REQUIRE(bb->statusG == bt::Status::SUCCESS);
  REQUIRE(bb->statusH == bt::Status::FAILURE);
  REQUIRE(bb->statusI == bt::Status::FAILURE);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
  root.UnbindTreeBlob();
}
