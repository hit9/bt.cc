#include <catch2/catch_test_macros.hpp>

#include "bt.h"

TEST_CASE("Blob/1", "[simple tree blob test]") {
  bt::TreeBlob blob;

  // Allocate one
  REQUIRE(!blob.Exist(1));
  blob.Allocate<bt::NodeBlob>(1);
  REQUIRE(blob.Exist(1));
  REQUIRE(blob.Get<bt::NodeBlob>(1) != nullptr);
  auto p1 = static_cast<bt::NodeBlob*>(blob.Get<bt::NodeBlob>(1));
  REQUIRE(p1->lastStatus == bt::Status::UNDEFINED);
  p1->lastStatus = bt::Status::RUNNING;
  auto p2 = static_cast<bt::NodeBlob*>(blob.Get<bt::NodeBlob>(1));
  REQUIRE(p2->lastStatus == bt::Status::RUNNING);

  // Allocate another.
  struct CustomNodeBlob : bt::NodeBlob {
    int x;
  };

  REQUIRE(!blob.Exist(2));
  blob.Allocate<CustomNodeBlob>(2);
  REQUIRE(blob.Exist(2));
  REQUIRE(blob.Get<CustomNodeBlob>(2) != nullptr);
  auto p3 = blob.Get<CustomNodeBlob>(2);
  REQUIRE(p3->x == 0);
  REQUIRE(p3->lastStatus == bt::Status::UNDEFINED);
  p3->x = 1;
  p3->lastStatus = bt::Status::RUNNING;
  auto p4 = blob.Get<CustomNodeBlob>(2);
  REQUIRE(p4->x == 1);
  REQUIRE(p4->lastStatus == bt::Status::RUNNING);
  auto p5 = static_cast<bt::NodeBlob*>(blob.Get<CustomNodeBlob>(2));
  REQUIRE(p5->lastStatus == bt::Status::RUNNING);
}
