#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <functional>

#include "bt.h"
#include "types.h"

TEST_CASE("Blob/1", "[simple tree blob test]") {
  bt::DynamicTreeBlob blob;
  std::function<void(bt::NodeBlob*)> cb = nullptr;

  // Allocate one
  auto p0 = blob.Make<bt::NodeBlob>(1, nullptr);
  REQUIRE(p0 != nullptr);
  auto p1 = blob.Make<bt::NodeBlob>(1, nullptr);
  REQUIRE(p1 == p0);
  REQUIRE(p1->lastStatus == bt::Status::UNDEFINED);
  p1->lastStatus = bt::Status::RUNNING;
  auto p2 = blob.Make<bt::NodeBlob>(1, nullptr);
  REQUIRE(p2->lastStatus == bt::Status::RUNNING);

  // Allocate another.
  struct CustomNodeBlob : bt::NodeBlob {
    int x;
  };

  auto q = blob.Make<CustomNodeBlob>(2, nullptr);
  REQUIRE(q != nullptr);
  REQUIRE(!q->running);
  auto p3 = blob.Make<CustomNodeBlob>(2, nullptr);
  REQUIRE(p3 == q);
  REQUIRE(p3->x == 0);
  REQUIRE(p3->lastStatus == bt::Status::UNDEFINED);
  p3->x = 1;
  p3->lastStatus = bt::Status::RUNNING;
  auto p4 = blob.Make<CustomNodeBlob>(2, nullptr);
  REQUIRE(p4->x == 1);
  REQUIRE(p4->lastStatus == bt::Status::RUNNING);
  auto p5 = static_cast<bt::NodeBlob*>(blob.Make<CustomNodeBlob>(2, nullptr));
  REQUIRE(p5->lastStatus == bt::Status::RUNNING);
}

TEMPLATE_TEST_CASE("Blob/2", "[multiple entites]", Entity,
                   (EntityFixedBlob<16, sizeof(bt::StatefulSelectorNode::Blob)>)) {
  bt::Tree root;

  // clang-format off
  root
  .StatefulSelector()
  ._().Action<A>()
  ._().Action<B>()
  ._().Action<E>()
  .End();
  // clang-format on

  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);

  TestType e1, e2, e3;

  // e1: Tick#1
  root.BindTreeBlob(e1.blob);
  bb->shouldA = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  REQUIRE(bb->counterA == 1);
  REQUIRE(bb->statusB == bt::Status::RUNNING);
  REQUIRE(bb->counterB == 1);
  root.UnbindTreeBlob();

  // e2: Tick#1
  root.BindTreeBlob(e2.blob);
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
  REQUIRE(bb->counterA == 2);  // +1
  REQUIRE(bb->counterB == 1);  // + 0
  root.UnbindTreeBlob();

  // e3: Tick#1
  root.BindTreeBlob(e3.blob);
  bb->shouldA = bt::Status::FAILURE;
  bb->shouldB = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  REQUIRE(bb->counterA == 3);  // +1
  REQUIRE(bb->counterB == 2);  // +1
  REQUIRE(bb->counterE == 1);
  REQUIRE(bb->statusE == bt::Status::RUNNING);
  root.UnbindTreeBlob();

  // e1: Tick#2
  root.BindTreeBlob(e1.blob);
  bb->shouldB = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::RUNNING);
  REQUIRE(bb->counterA == 3);  // +0
  REQUIRE(bb->counterB == 3);  // +1
  REQUIRE(bb->counterE == 2);  // +1
  root.UnbindTreeBlob();

  // e2: Tick#2
  root.BindTreeBlob(e2.blob);
  bb->shouldA = bt::Status::SUCCESS;
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::SUCCESS);
  REQUIRE(bb->counterA == 4);  // +1
  REQUIRE(bb->counterB == 3);  // +0
  REQUIRE(bb->counterE == 2);  // +0
  root.UnbindTreeBlob();

  // e3: Tick#2
  root.BindTreeBlob(e3.blob);
  bb->shouldE = bt::Status::FAILURE;
  root.Tick(ctx);
  REQUIRE(root.LastStatus() == bt::Status::FAILURE);
  REQUIRE(bb->counterA == 4);  // +0
  REQUIRE(bb->counterB == 3);  // +0
  REQUIRE(bb->counterE == 3);  // +1
  root.UnbindTreeBlob();
}
