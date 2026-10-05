#include <odr_wasm.hpp>

#include <odr/logger.hpp>

#include <emscripten/bind.h>

#include <memory>
#include <string>
#include <utility>

namespace odr::wasm {

namespace {

/// Forwards logs to a synchronous, worker-local callback.
class JsLogger final : public ILogger {
public:
  JsLogger(emscripten::val sink, const LogLevel level)
      : m_sink{std::move(sink)}, m_level{level} {}

  [[nodiscard]] bool will_log(const LogLevel level) const override {
    return level >= m_level;
  }

  void log(Time /*time*/, const LogLevel level, const std::string &message,
           const std::source_location & /*location*/) override {
    if (!will_log(level)) {
      return;
    }
    m_sink(static_cast<std::int32_t>(level), message);
  }

  void flush() override {}

private:
  emscripten::val m_sink;
  LogLevel m_level;
};

/// Sets the sink for subsequently opened documents; null restores silence.
emscripten::val set_logger(const emscripten::val &sink,
                           const std::int32_t level) {
  return guarded([&] {
    if (sink.isUndefined() || sink.isNull()) {
      default_logger() = Logger::null();
      return ok();
    }
    default_logger() =
        Logger(std::make_shared<JsLogger>(sink, static_cast<LogLevel>(level)));
    return ok();
  });
}

} // namespace

} // namespace odr::wasm

namespace odr {

Logger &wasm::default_logger() {
  static Logger instance = Logger::null();
  return instance;
}

} // namespace odr

EMSCRIPTEN_BINDINGS(odr_logger) {
  emscripten::function("setLogger", &odr::wasm::set_logger);
}
