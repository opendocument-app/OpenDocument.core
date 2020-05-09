#include <odr/src/SignalHandler.h>
#include <gtest/gtest.h>

using namespace odr;

TEST(SignalHandler, sigsegv) {
  SignalHandler::install();
  SignalHandler::sigsegv();
}

TEST(SignalHandler, derefnullptr) {
  SignalHandler::install();
  SignalHandler::derefnullptr();
}
