#pragma once

#include <chrono>
#include <memory>
#include <source_location>
#include <sstream>
#include <string>
#include <vector>

namespace odr {

enum class LogLevel {
  verbose = 0,
  debug,
  info,
  warning,
  error,
  fatal,
};

struct LogFormat {
  std::string time_format{"%H:%M:%S"};
  std::size_t level_width{7};
  std::size_t name_width{0};
  std::size_t location_width{20};
};

/// @brief Interface for a log sink. Implement to route logging elsewhere and
/// hand it to `Logger`.
///
/// `Logger` gates on `will_log` before calling `log`, so implementations may
/// assume the level reaching `log` is enabled.
class ILogger {
public:
  using Clock = std::chrono::system_clock;
  using Time = Clock::time_point;

  virtual ~ILogger() = default;

  [[nodiscard]] virtual bool will_log(LogLevel level) const = 0;

  virtual void log(Time time, LogLevel level, const std::string &message,
                   const std::source_location &location) = 0;

  virtual void flush() = 0;
};

/// @brief Handle to a log sink.
///
/// Copies share the sink. Implement @ref ILogger for a custom one.
class Logger final {
public:
  using Clock = ILogger::Clock;
  using Time = ILogger::Time;

  /// @brief A logger discarding everything. All instances share one sink.
  static Logger null();
  static Logger create_stdio(const std::string &name, LogLevel level,
                             const LogFormat &format = LogFormat(),
                             std::unique_ptr<std::ostream> output = nullptr);
  /// @throws std::invalid_argument if @p loggers is empty.
  static Logger create_tee(const std::vector<Logger> &loggers);

  static void print_head(std::ostream &out, Time time, LogLevel level,
                         const std::string &name,
                         const std::source_location &location,
                         const LogFormat &format);

  /// @brief Constructs the null logger.
  Logger();
  explicit Logger(std::shared_ptr<ILogger> impl);

  [[nodiscard]] bool will_log(LogLevel level) const;

  void log(LogLevel level, const std::string &message, Time time = Clock::now(),
           const std::source_location &location =
               std::source_location::current()) const;

  void flush() const;

  [[nodiscard]] std::shared_ptr<ILogger> impl() const;

private:
  std::shared_ptr<ILogger> m_impl;
};

} // namespace odr

// `message` is a streaming expression (e.g. `"x=" << x`), so it deliberately
// must not be parenthesized.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ODR_LOG(logger, level, message)                                        \
  do {                                                                         \
    if ((logger).will_log(level)) {                                            \
      std::stringstream ss;                                                    \
      ss << message;                                                           \
      (logger).log(level, ss.str());                                           \
    }                                                                          \
  } while (0)
// NOLINTEND(bugprone-macro-parentheses)
#define ODR_VERBOSE(logger, message) ODR_LOG(logger, LogLevel::verbose, message)
#define ODR_DEBUG(logger, message) ODR_LOG(logger, LogLevel::debug, message)
#define ODR_INFO(logger, message) ODR_LOG(logger, LogLevel::info, message)
#define ODR_WARNING(logger, message) ODR_LOG(logger, LogLevel::warning, message)
#define ODR_ERROR(logger, message) ODR_LOG(logger, LogLevel::error, message)
#define ODR_FATAL(logger, message) ODR_LOG(logger, LogLevel::fatal, message)
