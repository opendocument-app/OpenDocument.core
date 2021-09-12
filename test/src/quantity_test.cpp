#include <gtest/gtest.h>
#include <odr/quantity.h>

using namespace odr;

TEST(Quantity, inch) {
  Quantity<double> inch{"10ms"};
  std::cout << inch.magnitude() << std::endl;
  std::cout << inch.unit().name() << std::endl;
}
