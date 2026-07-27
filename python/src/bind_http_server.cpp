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

  py::class_<odr::HttpServer::Config>(server, "Config")
      .def(py::init<>())
      .def_readwrite("cache_path", &odr::HttpServer::Config::cache_path);

  py::class_<odr::HttpServer::Options>(server, "Options",
                                       "Socket options for `bind`.")
      .def(py::init<>())
      .def_readwrite("reuse_address", &odr::HttpServer::Options::reuse_address)
      .def_readwrite("reuse_port", &odr::HttpServer::Options::reuse_port);

  server
      .def(py::init([](const odr::HttpServer::Config &config) {
             return odr::HttpServer(config);
           }),
           py::arg("config"))
      .def("config", &odr::HttpServer::config)
      .def("connect_service", &odr::HttpServer::connect_service,
           py::arg("service"), py::arg("prefix"))
      .def("serve_file", &odr::HttpServer::serve_file, py::arg("file"),
           py::arg("prefix"), py::arg("config"),
           "Translate a decoded file and host it under "
           "`/file/<prefix>/<view path>`; returns the views.")
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
      // other Python threads.
      .def("listen", &odr::HttpServer::listen,
           py::call_guard<py::gil_scoped_release>())
      .def("clear", &odr::HttpServer::clear)
      .def("stop", &odr::HttpServer::stop,
           py::call_guard<py::gil_scoped_release>());
}
