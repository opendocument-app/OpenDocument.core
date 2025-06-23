#pragma once

#include <memory>
#include <source_location>
#include <sstream>
#include <string>
#include <vector>

namespace odr {

#define ODR_LOG(level, message)                                                \
  do {                                                                         \
    if (logger().will_log(level)) {                                            \
      std::stringstream ss;                                                    \
      ss << message;                                                           \
      logger().log(level, ss.str());                                           \
    }                                                                          \
  } while (0)
#define ODR_VERBOSE(message) ODR_LOG(LogLevel::verbose, message)
#define ODR_DEBUG(message) ODR_LOG(LogLevel::debug, message)
#define ODR_INFO(message) ODR_LOG(LogLevel::info, message)
#define ODR_WARNING(message) ODR_LOG(LogLevel::warning, message)
#define ODR_ERROR(message) ODR_LOG(LogLevel::error, message)
#define ODR_FATAL(message) ODR_LOG(LogLevel::fatal, message)

#define ODR_LOCAL_LOGGER(_logger)                                              \
  const auto logger = [l = _logger]() -> odr::Logger & { return *l; }

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

class Logger {
public:
  static Logger &null();
  static std::unique_ptr<Logger>
  create_stdio(const std::string &name, LogLevel level,
               const LogFormat &format = LogFormat(),
               std::unique_ptr<std::ostream> output = nullptr);
  static std::unique_ptr<Logger>
  create_tee(const std::vector<std::shared_ptr<Logger>> &loggers);

  static std::string format(const std::string &name, LogLevel level,
                            const std::string &message,
                            const std::source_location &location,
                            const LogFormat &format);

  virtual ~Logger() = default;

  [[nodiscard]] virtual bool will_log(LogLevel level) const = 0;

  inline void
  log(LogLevel level, const std::string &message,
      const std::source_location &location = std::source_location::current()) {
    if (will_log(level)) {
      log_impl(level, message, location);
    }
  }

  virtual void flush() = 0;

protected:
  virtual void log_impl(LogLevel level, const std::string &message,
                        const std::source_location &location) = 0;
};

} // namespace odr
