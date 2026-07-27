#include <odr/logger.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace odr {

namespace {

class NullLogger final : public ILogger {
public:
  [[nodiscard]] bool will_log(LogLevel) const override { return false; }

  void log(Time, LogLevel, const std::string &,
           const std::source_location &) override {
    // Do nothing
  }

  void flush() override {
    // Do nothing
  }
};

class StdioLogger final : public ILogger {
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

  void log(const Time time, const LogLevel level, const std::string &message,
           const std::source_location &location) override {
    Logger::print_head(*m_output, time, level, m_name, location, m_format);
    *m_output << message << "\n";
  }

  void flush() override { m_output->flush(); }

private:
  std::string m_name;
  LogLevel m_level;
  LogFormat m_format;
  std::unique_ptr<std::ostream> m_output;
};

class TeeLogger final : public ILogger {
public:
  explicit TeeLogger(std::vector<Logger> loggers)
      : m_loggers(std::move(loggers)) {
    if (m_loggers.empty()) {
      throw std::invalid_argument("TeeLogger requires at least one logger");
    }
  }

  [[nodiscard]] bool will_log(const LogLevel level) const override {
    return std::ranges::any_of(m_loggers, [level](const Logger &logger) {
      return logger.will_log(level);
    });
  }

  void log(const Time time, const LogLevel level, const std::string &message,
           const std::source_location &location) override {
    for (const Logger &logger : m_loggers) {
      logger.log(level, message, time, location);
    }
  }

  void flush() override {
    for (const Logger &logger : m_loggers) {
      logger.flush();
    }
  }

private:
  std::vector<Logger> m_loggers;
};

std::shared_ptr<ILogger> null_impl() {
  static const std::shared_ptr<ILogger> instance =
      std::make_shared<NullLogger>();
  return instance;
}

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

Logger Logger::null() { return Logger(); }

Logger Logger::create_stdio(const std::string &name, const LogLevel level,
                            const LogFormat &format,
                            std::unique_ptr<std::ostream> output) {
  return Logger(
      std::make_shared<StdioLogger>(name, level, format, std::move(output)));
}

Logger Logger::create_tee(const std::vector<Logger> &loggers) {
  return Logger(std::make_shared<TeeLogger>(loggers));
}

Logger::Logger() : m_impl{null_impl()} {}

Logger::Logger(std::shared_ptr<ILogger> impl) : m_impl{std::move(impl)} {
  if (m_impl == nullptr) {
    throw std::invalid_argument("Logger requires an implementation");
  }
}

bool Logger::will_log(const LogLevel level) const {
  return m_impl->will_log(level);
}

void Logger::log(const LogLevel level, const std::string &message,
                 const Time time, const std::source_location &location) const {
  if (m_impl->will_log(level)) {
    m_impl->log(time, level, message, location);
  }
}

void Logger::flush() const { m_impl->flush(); }

std::shared_ptr<ILogger> Logger::impl() const { return m_impl; }

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
    const std::string file_name =
        std::filesystem::path(location.file_name()).filename().string();
    const std::string line_number = std::to_string(location.line());
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
