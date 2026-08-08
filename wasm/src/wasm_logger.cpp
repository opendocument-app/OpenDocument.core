#include <odr_wasm.hpp>

#include <odr/logger.hpp>

#include <emscripten/bind.h>

#include <memory>
#include <string>
#include <utility>

namespace odr::wasm {

namespace {

/// Forwards log records to a JS callback. The sink must be worker-local and
/// synchronous: one that needed the main thread would deadlock a render behind
/// a `postMessage` round trip.
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
    m_sink(static_cast<int>(level), message);
  }

  void flush() override {}

private:
  emscripten::val m_sink;
  LogLevel m_level;
};

/// Routes logging into @p sink for every document opened after this call; null
/// restores silence.
emscripten::val set_logger(const emscripten::val &sink, const int level) {
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

Logger &default_logger() {
  static Logger instance = Logger::null();
  return instance;
}

} // namespace odr::wasm

EMSCRIPTEN_BINDINGS(odr_logger) {
  emscripten::function("setLogger", &odr::wasm::set_logger);
}
