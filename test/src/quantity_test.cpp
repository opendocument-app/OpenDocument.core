#include <gtest/gtest.h>

#include <odr/quantity.h>

using namespace odr;

TEST(Quantity, construct) {
  Quantity<double> a{"10ms"};
  Quantity<double> b{"10 ms"};
}
