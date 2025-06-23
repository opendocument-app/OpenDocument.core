#include <odr/logger.hpp>

#include <gtest/gtest.h>

using namespace odr;

TEST(Logger, stdout) {
  auto logger = Logger::create_stdio("test", LogLevel::verbose);

  logger->log(LogLevel::verbose, "Test message with log function");

  ODR_VERBOSE(logger, "Test message with verbose log macro");
}
