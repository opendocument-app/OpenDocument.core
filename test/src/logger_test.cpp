#include <odr/logger.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using namespace odr;

namespace {

/// A format that prints the bare message, so tests can compare exactly.
LogFormat bare_format() {
  return LogFormat{.time_format = "",
                   .level_width = 0,
                   .name_width = 0,
                   .location_width = 0};
}

/// Returns a logger writing into a stream owned by it, plus a borrowed view.
std::pair<Logger, std::ostringstream *> capturing_logger() {
  auto output = std::make_unique<std::ostringstream>();
  auto *const captured = output.get();
  return {Logger::create_stdio("test", LogLevel::verbose, bare_format(),
                               std::move(output)),
          captured};
}

/// A sink defined outside the library, exercising `ILogger` as the public
/// extension point.
class CollectingLogger final : public ILogger {
public:
  [[nodiscard]] bool will_log(const LogLevel level) const override {
    return level >= LogLevel::warning;
  }

  void log(Time, LogLevel, const std::string &message,
           const std::source_location &location) override {
    messages.push_back(message);
    lines.push_back(location.line());
  }

  void flush() override { ++flushes; }

  std::vector<std::string> messages;
  std::vector<std::uint_least32_t> lines;
  std::size_t flushes{0};
};

} // namespace

TEST(Logger, stdio) {
  const auto logger = Logger::create_stdio("test", LogLevel::verbose);

  logger.log(LogLevel::verbose, "Test message with log function");

  ODR_VERBOSE(logger, "Test message with verbose log macro");
}

TEST(Logger, default_constructed_is_null) {
  EXPECT_FALSE(Logger().will_log(LogLevel::fatal));
  EXPECT_FALSE(Logger::null().will_log(LogLevel::fatal));
}

TEST(Logger, copies_share_the_sink) {
  auto [logger, captured] = capturing_logger();
  const Logger copy = logger;

  copy.log(LogLevel::info, "from the copy");
  logger.log(LogLevel::info, "from the original");
  copy.flush();

  EXPECT_EQ(captured->str(), "from the copy\nfrom the original\n");
}

TEST(Logger, respects_the_level) {
  auto [logger, captured] = capturing_logger();
  const Logger warning =
      Logger::create_stdio("test", LogLevel::warning, bare_format(), nullptr);

  logger.log(LogLevel::verbose, "kept");
  logger.flush();

  EXPECT_EQ(captured->str(), "kept\n");
  EXPECT_FALSE(warning.will_log(LogLevel::debug));
  EXPECT_TRUE(warning.will_log(LogLevel::error));
}

TEST(Logger, tee_fans_out) {
  auto [first, first_captured] = capturing_logger();
  auto [second, second_captured] = capturing_logger();

  const Logger tee = Logger::create_tee({first, second});
  tee.log(LogLevel::info, "fanned out");
  tee.flush();

  EXPECT_EQ(first_captured->str(), "fanned out\n");
  EXPECT_EQ(second_captured->str(), "fanned out\n");
}

TEST(Logger, tee_rejects_empty) {
  EXPECT_THROW(std::ignore = Logger::create_tee({}), std::invalid_argument);
}

TEST(Logger, custom_sink_drives_the_handle) {
  const auto sink = std::make_shared<CollectingLogger>();
  const Logger logger(sink);

  logger.log(LogLevel::debug, "dropped by will_log");
  logger.log(LogLevel::error, "kept");
  ODR_WARNING(logger, "kept too");
  logger.flush();

  EXPECT_EQ(sink->messages, (std::vector<std::string>{"kept", "kept too"}));
  EXPECT_EQ(sink->flushes, 1);
  // The macro reports the call site, not a spot inside the logger.
  ASSERT_EQ(sink->lines.size(), 2);
  EXPECT_GT(sink->lines.at(1), 0);
}

TEST(Logger, custom_sink_composes_with_tee) {
  const auto sink = std::make_shared<CollectingLogger>();
  auto [stdio, captured] = capturing_logger();

  const Logger tee = Logger::create_tee({Logger(sink), stdio});
  tee.log(LogLevel::error, "both");
  tee.log(LogLevel::verbose, "only the stdio one");
  tee.flush();

  EXPECT_EQ(sink->messages, (std::vector<std::string>{"both"}));
  EXPECT_EQ(captured->str(), "both\nonly the stdio one\n");
}

TEST(Logger, rejects_a_null_sink) {
  // Braces, not parens: `Logger(std::shared_ptr<ILogger>())` is a function
  // declaration, not an expression (MSVC diagnoses it, clang accepts it).
  const std::shared_ptr<ILogger> sink;
  EXPECT_THROW(Logger{sink}, std::invalid_argument);
}
