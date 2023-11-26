#include <odr/quantity.hpp>

#include <gtest/gtest.h>

using namespace odr;

TEST(Quantity, construct) {
  Quantity<double> a{"10ms"};
  Quantity<double> b{"10 ms"};
}
