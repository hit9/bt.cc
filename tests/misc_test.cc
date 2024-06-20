#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>

#include "types.h"

TEST_CASE("Misc/1", "[any node should be movable]") {
  J j1{"a really looooooooong name to avoid the string SSO optimization",
       "a really looooooooong s string to avoid the string SSO optimization"};
  auto addr11 = static_cast<void*>(j1.s.data());
  auto addr12 = static_cast<const void*>(j1.Name().data());
  J j2(std::move(j1));
  auto addr21 = static_cast<void*>(j2.s.data());
  auto addr22 = static_cast<const void*>(j2.Name().data());
  // The two address of underlying data should be the same if the movement successful done.
  REQUIRE(addr11 == addr21);
  // And the base class Node's underlying storage should also movable.
  REQUIRE(addr12 == addr22);
}
