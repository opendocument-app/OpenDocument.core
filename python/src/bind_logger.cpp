#include "bindings.hpp"

#include <odr/logger.hpp>

#include <pybind11/chrono.h>
#include <pybind11/stl.h>

#include <memory>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace {

/// Trampoline letting a Python class implement `odr::ILogger`.
class PyLogger final : public odr::ILogger {
public:
  [[nodiscard]] bool will_log(const odr::LogLevel level) const override {
    PYBIND11_OVERRIDE_PURE(bool, odr::ILogger, will_log, level);
  }

  void log(const Time time, const odr::LogLevel level,
           const std::string &message,
           const std::source_location &location) override {
    PYBIND11_OVERRIDE_PURE(void, odr::ILogger, log, time, level, message,
                           location);
  }

  void flush() override {
    // Deliberately not PURE: sinks get flushed from destructors, which may run
    // while the interpreter is tearing down and the Python object is already
    // gone. Throwing there would abort the process.
    const py::gil_scoped_acquire gil;
    if (const py::function override = py::get_override(
            static_cast<const odr::ILogger *>(this), "flush")) {
      override();
    }
  }
};

/// Wraps a Python sink in a `shared_ptr` that owns a reference to the Python
/// object, so the sink survives for as long as any C++ `Logger` holds it — the
/// Python handle is often a temporary (`Logger(MySink())`).
std::shared_ptr<odr::ILogger> adopt_sink(py::object sink) {
  auto *const raw = sink.cast<odr::ILogger *>();
  return {raw, [captured = std::move(sink)](odr::ILogger *) mutable {
            // Python owns the object itself; we only release our reference.
            if (Py_IsInitialized() == 0) {
              // The interpreter is gone — dropping the reference now would
              // crash, so let it leak with the rest of the heap.
              static_cast<void>(captured.release());
              return;
            }
            const py::gil_scoped_acquire gil;
            captured = py::object();
          }};
}

} // namespace

void odr_python::bind_logger(py::module_ &m) {
  // `arithmetic()` so `level >= LogLevel.warning` works in a `will_log`
  // implementation, which is the whole point of the level.
  py::enum_<odr::LogLevel>(m, "LogLevel", py::arithmetic())
      .value("verbose", odr::LogLevel::verbose)
      .value("debug", odr::LogLevel::debug)
      .value("info", odr::LogLevel::info)
      .value("warning", odr::LogLevel::warning)
      .value("error", odr::LogLevel::error)
      .value("fatal", odr::LogLevel::fatal);

  py::class_<odr::LogFormat>(m, "LogFormat")
      .def(py::init<>())
      .def_readwrite("time_format", &odr::LogFormat::time_format)
      .def_readwrite("level_width", &odr::LogFormat::level_width)
      .def_readwrite("name_width", &odr::LogFormat::name_width)
      .def_readwrite("location_width", &odr::LogFormat::location_width);

  py::class_<std::source_location>(m, "SourceLocation",
                                   "Call site a log message originated from.")
      .def_property_readonly("file_name",
                             [](const std::source_location &l) {
                               return std::string(l.file_name());
                             })
      .def_property_readonly("function_name",
                             [](const std::source_location &l) {
                               return std::string(l.function_name());
                             })
      .def_property_readonly(
          "line", [](const std::source_location &l) { return l.line(); })
      .def_property_readonly(
          "column", [](const std::source_location &l) { return l.column(); });

  py::class_<odr::ILogger, PyLogger, std::shared_ptr<odr::ILogger>>(
      m, "ILogger",
      "Interface for a log sink. Subclass and implement `will_log`, `log` and\n"
      "`flush`, then wrap it in a `Logger` to hand it to the library.")
      .def(py::init<>())
      .def("will_log", &odr::ILogger::will_log, py::arg("level"))
      .def("log", &odr::ILogger::log, py::arg("time"), py::arg("level"),
           py::arg("message"), py::arg("location"))
      .def("flush", &odr::ILogger::flush);

  py::class_<odr::Logger>(m, "Logger",
                          "Handle to a log sink. Copies share the sink.")
      .def(py::init<>(), "Constructs the null logger.")
      .def(py::init([](py::object sink) {
             return odr::Logger(adopt_sink(std::move(sink)));
           }),
           py::arg("sink"), "Wraps a custom `ILogger`.")
      .def_static("null", &odr::Logger::null)
      .def_static(
          "create_stdio",
          [](const std::string &name, const odr::LogLevel level,
             const odr::LogFormat &format) {
            return odr::Logger::create_stdio(name, level, format);
          },
          py::arg("name"), py::arg("level"),
          py::arg("format") = odr::LogFormat())
      .def_static("create_tee", &odr::Logger::create_tee, py::arg("loggers"))
      .def("will_log", &odr::Logger::will_log, py::arg("level"))
      .def(
          "log",
          [](const odr::Logger &logger, const odr::LogLevel level,
             const std::string &message) { logger.log(level, message); },
          py::arg("level"), py::arg("message"))
      .def("flush", &odr::Logger::flush);
}
