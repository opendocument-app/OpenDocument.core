#include <odr/quantity.hpp>

#include <gtest/gtest.h>

using namespace odr;

TEST(Quantity, construct) {
  Quantity<double>{"10ms"};
  Quantity<double>{"10 ms"};
}
