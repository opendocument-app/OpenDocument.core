#include <odr/logger.hpp>

#include <gtest/gtest.h>

using namespace odr;

TEST(Logger, stdout) {
  ODR_LOCAL_LOGGER(Logger::create_stdio("test", LogLevel::verbose));

  ODR_VERBOSE("Test message with verbose level");
}
