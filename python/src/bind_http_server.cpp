#include "bindings.hpp"

#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/http_server.hpp>

#include <pybind11/stl.h>

namespace py = pybind11;

void odr_python::bind_http_server(py::module_ &m) {
  m.attr("has_http_server") = true;

  py::class_<odr::HttpServer> server(m, "HttpServer",
                                     "Serves translated files over HTTP.");

  py::class_<odr::HttpServer::Config>(
      server, "Config",
      "Server-wide settings. Empty since the cache path went with "
      "`serve_file`: "
      "what a service was translated into belongs to whoever translated it.")
      .def(py::init<>());

  py::class_<odr::HttpServer::Options>(server, "Options",
                                       "Socket options for `bind`.")
      .def(py::init<>())
      .def_readwrite("reuse_address", &odr::HttpServer::Options::reuse_address)
      .def_readwrite("reuse_port", &odr::HttpServer::Options::reuse_port);

  server
      .def(py::init([](const odr::HttpServer::Config &config) {
             return odr::HttpServer(config);
           }),
           py::arg("config") = odr::HttpServer::Config{})
      .def("connect_service", &odr::HttpServer::connect_service,
           py::arg("service"), py::arg("prefix"))
      .def(
          "bind",
          [](const odr::HttpServer &self, const std::string &host,
             const std::uint32_t port,
             const odr::HttpServer::Options &options) {
            return self.bind(host, port, options);
          },
          py::arg("host"), py::arg("port"),
          py::arg("options") = odr::HttpServer::Options{},
          "Bind the socket, `port` 0 for any free one; returns the port it "
          "got. Connections are accepted from here on, before `listen` runs.")
      // Release the GIL: `listen` blocks and `stop` must remain callable from
      // other Python threads. `stop` waits for the listen thread, which needs
      // the GIL back to return out of `listen`, so it has to let go of it too.
      .def("listen", &odr::HttpServer::listen,
           py::call_guard<py::gil_scoped_release>(),
           "Serve what `bind` opened until `stop`. Returns right away if the "
           "server has already been stopped.")
      .def("is_running", &odr::HttpServer::is_running,
           py::call_guard<py::gil_scoped_release>(),
           "Whether a `listen` is in flight. False again once it has returned, "
           "which is what `stop` waits for.")
      .def("clear", &odr::HttpServer::clear)
      .def("stop", &odr::HttpServer::stop,
           py::call_guard<py::gil_scoped_release>(),
           "Stop `listen` and release the socket. Blocks until `listen` has "
           "returned, so nothing is serving any more once this returns.");
}
