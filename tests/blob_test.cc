#include <catch2/catch_test_macros.hpp>

#include "bt.h"

TEST_CASE("Blob/1", "[simple tree blob test]") {
  bt::TreeBlob blob;

  // Allocate one
  auto p0 = blob.Make<bt::NodeBlob>(1);
  REQUIRE(p0 != nullptr);
  auto p1 = blob.Make<bt::NodeBlob>(1);
  REQUIRE(p1 == p0);
  REQUIRE(p1->lastStatus == bt::Status::UNDEFINED);
  p1->lastStatus = bt::Status::RUNNING;
  auto p2 = blob.Make<bt::NodeBlob>(1);
  REQUIRE(p2->lastStatus == bt::Status::RUNNING);

  // Allocate another.
  struct CustomNodeBlob : bt::NodeBlob {
    int x;
  };

  auto q = blob.Make<CustomNodeBlob>(2);
  REQUIRE(q != nullptr);
  REQUIRE(!q->running);
  auto p3 = blob.Make<CustomNodeBlob>(2);
  REQUIRE(p3 == q);
  REQUIRE(p3->x == 0);
  REQUIRE(p3->lastStatus == bt::Status::UNDEFINED);
  p3->x = 1;
  p3->lastStatus = bt::Status::RUNNING;
  auto p4 = blob.Make<CustomNodeBlob>(2);
  REQUIRE(p4->x == 1);
  REQUIRE(p4->lastStatus == bt::Status::RUNNING);
  auto p5 = static_cast<bt::NodeBlob*>(blob.Make<CustomNodeBlob>(2));
  REQUIRE(p5->lastStatus == bt::Status::RUNNING);
}
