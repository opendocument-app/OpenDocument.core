#include <odr/logger.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <stdexcept>

namespace odr {

namespace {

class NullLogger : public Logger {
public:
  [[nodiscard]] bool will_log(LogLevel) const override { return false; }

  void log_impl(LogLevel, const std::string &,
                const std::source_location &) override {
    // Do nothing
  }
};

class StdioLogger : public Logger {
public:
  StdioLogger(std::string name, LogLevel level, LogFormat format,
              std::unique_ptr<std::ostream> output)
      : m_name(std::move(name)), m_level(level), m_format(std::move(format)),
        m_output(std::move(output)) {
    if (m_output == nullptr) {
      m_output = std::make_unique<std::ostream>(std::cout.rdbuf());
    }
  }

  [[nodiscard]] bool will_log(LogLevel level) const override {
    return level >= m_level;
  }

  void log_impl(LogLevel level, const std::string &message,
                const std::source_location &location) override {
    *m_output << format(level, message, location, m_format) << std::endl;
  }

private:
  std::string m_name;
  LogLevel m_level;
  LogFormat m_format;
  std::unique_ptr<std::ostream> m_output;
};

class TeeLogger : public Logger {
public:
  explicit TeeLogger(const std::vector<std::shared_ptr<Logger>> &loggers)
      : m_loggers(loggers) {
    if (m_loggers.empty()) {
      throw std::invalid_argument("TeeLogger requires at least one logger");
    }
  }

  [[nodiscard]] bool will_log(LogLevel level) const override {
    return std::ranges::any_of(m_loggers, [level](const auto &logger) {
      return logger->will_log(level);
    });
  }

  void log_impl(LogLevel level, const std::string &message,
                const std::source_location &location) override {
    for (const auto &logger : m_loggers) {
      logger->log(level, message, location);
    }
  }

private:
  std::vector<std::shared_ptr<Logger>> m_loggers;
};

std::string_view level_to_string(LogLevel level) {
  switch (level) {
  case LogLevel::verbose:
    return "VERBOSE";
  case LogLevel::debug:
    return "DEBUG";
  case LogLevel::info:
    return "INFO";
  case LogLevel::warning:
    return "WARNING";
  case LogLevel::error:
    return "ERROR";
  case LogLevel::fatal:
    return "FATAL";
  default:
    throw std::invalid_argument("Unknown log level");
  }
}

} // namespace

Logger &Logger::null() {
  static NullLogger null_logger;
  return null_logger;
}

std::unique_ptr<Logger>
Logger::create_stdio(const std::string &name, LogLevel level,
                     const LogFormat &format,
                     std::unique_ptr<std::ostream> output) {
  return std::make_unique<StdioLogger>(name, level, format, std::move(output));
}

std::unique_ptr<Logger>
Logger::create_tee(const std::vector<std::shared_ptr<Logger>> &loggers) {
  return std::make_unique<TeeLogger>(loggers);
}

std::string Logger::format(LogLevel level, const std::string &message,
                           const std::source_location &location,
                           const LogFormat &format) {
  std::stringstream ss;

  if (!format.time_format.empty()) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    ss << std::put_time(std::localtime(&time), format.time_format.c_str())
       << " ";
  }

  if (format.level_width > 0) {
    std::string_view level_str = level_to_string(level);
    std::stringstream level_ss;
    level_ss << level_str.substr(0, format.level_width);
    level_ss << std::string(
        std::max<std::size_t>(0, format.level_width - level_ss.str().size()),
        ' ');
    ss << level_ss.str();
  }

  if (format.name_width > 0) {
    std::string name = location.function_name();
    std::stringstream name_ss;
    name_ss << name.substr(0, format.name_width);
    name_ss << std::string(
        std::max<std::size_t>(0, format.name_width - name_ss.str().size()),
        ' ');
    ss << name_ss.str();
  }

  if (format.location_width > 0) {
    std::string file_name = location.file_name();
    std::string line_number = std::to_string(location.line());
    std::stringstream location_ss;
    if (file_name.size() + 1 + line_number.size() > format.location_width) {
      if (1 + line_number.size() < format.location_width) {
        location_ss << file_name.substr(0, format.location_width - 1 -
                                               line_number.size())
                    << ":" << line_number;
      } else {
        location_ss << file_name.substr(0, format.location_width);
      }
    } else {
      location_ss << file_name << ":" << line_number;
    }
    location_ss << std::string(
        std::max<std::size_t>(0,
                              format.location_width - location_ss.str().size()),
        ' ');
    ss << location_ss.str();
  }

  ss << message;

  return ss.str();
}

} // namespace odr
