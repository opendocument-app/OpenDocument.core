#include <odr/logger.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <stdexcept>

namespace odr {

namespace {

class NullLogger final : public Logger {
public:
  [[nodiscard]] bool will_log(LogLevel) const override { return false; }

  void log_impl(Time, LogLevel, const std::string &,
                const std::source_location &) override {
    // Do nothing
  }

  void flush() override {
    // Do nothing
  }
};

class StdioLogger final : public Logger {
public:
  StdioLogger(std::string name, const LogLevel level, LogFormat format,
              std::unique_ptr<std::ostream> output)
      : m_name(std::move(name)), m_level(level), m_format(std::move(format)),
        m_output(std::move(output)) {
    if (m_output == nullptr) {
      m_output = std::make_unique<std::ostream>(std::cout.rdbuf());
    }
  }

  ~StdioLogger() override { flush(); }

  [[nodiscard]] bool will_log(const LogLevel level) const override {
    return level >= m_level;
  }

  void log_impl(const Time time, const LogLevel level,
                const std::string &message,
                const std::source_location &location) override {
    print_head(*m_output, time, level, m_name, location, m_format);
    *m_output << message << "\n";
  }

  void flush() override { m_output->flush(); }

private:
  std::string m_name;
  LogLevel m_level;
  LogFormat m_format;
  std::unique_ptr<std::ostream> m_output;
};

class TeeLogger final : public Logger {
public:
  explicit TeeLogger(const std::vector<std::shared_ptr<Logger>> &loggers)
      : m_loggers(loggers) {
    if (m_loggers.empty()) {
      throw std::invalid_argument("TeeLogger requires at least one logger");
    }
  }

  ~TeeLogger() override { flush(); }

  [[nodiscard]] bool will_log(const LogLevel level) const override {
    return std::ranges::any_of(m_loggers, [level](const auto &logger) {
      return logger->will_log(level);
    });
  }

  void log_impl(const Time time, const LogLevel level,
                const std::string &message,
                const std::source_location &location) override {
    for (const auto &logger : m_loggers) {
      if (!logger->will_log(level)) {
        continue;
      }
      logger->log(level, message, time, location);
    }
  }

  void flush() override {
    for (const auto &logger : m_loggers) {
      logger->flush();
    }
  }

private:
  std::vector<std::shared_ptr<Logger>> m_loggers;
};

std::string_view level_to_string(const LogLevel level) {
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

std::unique_ptr<Logger> Logger::create_null() {
  return std::make_unique<NullLogger>();
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

void Logger::print_head(std::ostream &out, Time time, LogLevel level,
                        const std::string &name,
                        const std::source_location &location,
                        const LogFormat &format) {
  if (!format.time_format.empty()) {
    auto t = Clock::to_time_t(time);
    out << std::put_time(std::localtime(&t), format.time_format.c_str()) << " ";
  }

  if (format.level_width > 0) {
    std::string_view level_str = level_to_string(level);
    std::stringstream level_ss;
    level_ss << level_str.substr(0, format.level_width);
    level_ss << std::string(
        std::max<std::size_t>(0, format.level_width - level_ss.str().size()),
        ' ');
    out << level_ss.str() << " ";
  }

  if (format.name_width > 0) {
    std::stringstream name_ss;
    name_ss << name.substr(0, format.name_width);
    name_ss << std::string(
        std::max<std::size_t>(0, format.name_width - name_ss.str().size()),
        ' ');
    out << name_ss.str() << " ";
  }

  if (format.location_width > 0) {
    std::string file_name =
        std::filesystem::path(location.file_name()).filename().string();
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
    out << location_ss.str() << " ";
  }
}

} // namespace odr
