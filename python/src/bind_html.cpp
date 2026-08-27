#include "bindings.hpp"

#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>

#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include <sstream>
#include <string>
#include <utility>

namespace py = pybind11;

void odr_python::bind_html(py::module_ &m) {
  py::enum_<odr::HtmlResourceType>(m, "HtmlResourceType")
      .value("html_fragment", odr::HtmlResourceType::html_fragment)
      .value("css", odr::HtmlResourceType::css)
      .value("js", odr::HtmlResourceType::js)
      .value("image", odr::HtmlResourceType::image)
      .value("font", odr::HtmlResourceType::font)
      .value("media", odr::HtmlResourceType::media)
      .value("file", odr::HtmlResourceType::file);

  py::enum_<odr::HtmlTableGridlines>(m, "HtmlTableGridlines")
      .value("none", odr::HtmlTableGridlines::none)
      .value("soft", odr::HtmlTableGridlines::soft)
      .value("hard", odr::HtmlTableGridlines::hard);

  py::enum_<odr::HtmlColorScheme>(m, "HtmlColorScheme")
      .value("light", odr::HtmlColorScheme::light)
      .value("dark", odr::HtmlColorScheme::dark)
      .value("system", odr::HtmlColorScheme::system);

  py::enum_<odr::HtmlViewportMode>(m, "HtmlViewportMode")
      .value("automatic", odr::HtmlViewportMode::automatic)
      .value("fit_width", odr::HtmlViewportMode::fit_width)
      .value("actual_size", odr::HtmlViewportMode::actual_size)
      .value("none", odr::HtmlViewportMode::none)
      .value("fit_width_by_view", odr::HtmlViewportMode::fit_width_by_view);

  py::enum_<odr::PdfTextMode>(m, "PdfTextMode")
      .value("dual_layer", odr::PdfTextMode::dual_layer)
      .value("single_layer", odr::PdfTextMode::single_layer);

  py::class_<odr::HtmlResource>(m, "HtmlResource")
      .def("type", &odr::HtmlResource::type)
      .def("mime_type", &odr::HtmlResource::mime_type)
      .def("name", &odr::HtmlResource::name)
      .def("path", &odr::HtmlResource::path)
      .def("file", &odr::HtmlResource::file)
      .def("is_shipped", &odr::HtmlResource::is_shipped)
      .def("is_external", &odr::HtmlResource::is_external)
      .def("is_accessible", &odr::HtmlResource::is_accessible)
      .def("read", [](const odr::HtmlResource &resource) {
        std::ostringstream out;
        resource.write_resource(out);
        return py::bytes(out.str());
      });

  py::class_<odr::HtmlConfig>(m, "HtmlConfig")
      .def(py::init<>())
      .def(py::init<std::string>(), py::arg("output_path"))
      .def_readwrite("document_output_file_name",
                     &odr::HtmlConfig::document_output_file_name)
      .def_readwrite("slide_output_file_name",
                     &odr::HtmlConfig::slide_output_file_name)
      .def_readwrite("sheet_output_file_name",
                     &odr::HtmlConfig::sheet_output_file_name)
      .def_readwrite("page_output_file_name",
                     &odr::HtmlConfig::page_output_file_name)
      .def_readwrite("embed_images", &odr::HtmlConfig::embed_images)
      .def_readwrite("embed_shipped_resources",
                     &odr::HtmlConfig::embed_shipped_resources)
      .def_readwrite("resource_path", &odr::HtmlConfig::resource_path)
      .def_readwrite("relative_resource_paths",
                     &odr::HtmlConfig::relative_resource_paths)
      .def_readwrite("editable", &odr::HtmlConfig::editable)
      .def_readwrite("text_document_margin",
                     &odr::HtmlConfig::text_document_margin)
      .def_readwrite("color_scheme", &odr::HtmlConfig::color_scheme)
      .def_readwrite("spreadsheet_limit", &odr::HtmlConfig::spreadsheet_limit)
      .def_readwrite("spreadsheet_limit_by_content",
                     &odr::HtmlConfig::spreadsheet_limit_by_content)
      .def_readwrite("spreadsheet_gridlines",
                     &odr::HtmlConfig::spreadsheet_gridlines)
      .def_readwrite("viewport_mode", &odr::HtmlConfig::viewport_mode)
      .def_readwrite("spreadsheet_viewport_mode",
                     &odr::HtmlConfig::spreadsheet_viewport_mode)
      .def_readwrite("viewport_content", &odr::HtmlConfig::viewport_content)
      .def_readwrite("viewport_width", &odr::HtmlConfig::viewport_width)
      .def_readwrite("initial_zoom", &odr::HtmlConfig::initial_zoom)
      .def_readwrite("min_content_margin", &odr::HtmlConfig::min_content_margin)
      .def_readwrite("format_html", &odr::HtmlConfig::format_html)
      .def_readwrite("html_indent", &odr::HtmlConfig::html_indent)
      .def_readwrite("html_indent_string", &odr::HtmlConfig::html_indent_string)
      .def_readwrite("background_image_format",
                     &odr::HtmlConfig::background_image_format,
                     "Deprecated and inert.")
      .def_readwrite("background_image_dpi",
                     &odr::HtmlConfig::background_image_dpi,
                     "Deprecated and inert.")
      .def_readwrite("page_range_begin", &odr::HtmlConfig::page_range_begin)
      .def_readwrite("page_range_end", &odr::HtmlConfig::page_range_end)
      .def_readwrite("pdf_text_mode", &odr::HtmlConfig::pdf_text_mode)
      .def_readwrite("pdf_dual_layer_fallback_fonts",
                     &odr::HtmlConfig::pdf_dual_layer_fallback_fonts)
      .def_readwrite("pdf_dual_layer_fallback_font_size_adjust",
                     &odr::HtmlConfig::pdf_dual_layer_fallback_font_size_adjust)
      .def_readwrite("no_drm", &odr::HtmlConfig::no_drm,
                     "Deprecated and inert.")
      .def_readwrite("embed_outline", &odr::HtmlConfig::embed_outline,
                     "Deprecated and inert.")
      .def_readwrite("output_path", &odr::HtmlConfig::output_path)
      .def_readwrite("resource_locator", &odr::HtmlConfig::resource_locator);

  py::class_<odr::HtmlPage>(m, "HtmlPage")
      .def_readonly("name", &odr::HtmlPage::name)
      .def_readonly("path", &odr::HtmlPage::path)
      .def("__repr__", [](const odr::HtmlPage &page) {
        return "HtmlPage(name='" + page.name + "', path='" + page.path + "')";
      });

  py::class_<odr::Html>(m, "Html")
      .def("config", &odr::Html::config)
      .def("pages", &odr::Html::pages);

  // `py::dynamic_attr` so `HtmlService::list_views` can pin the service onto
  // each view; the underlying view references the service without owning it.
  py::class_<odr::HtmlView>(m, "HtmlView", py::dynamic_attr())
      .def("name", &odr::HtmlView::name)
      .def("index", &odr::HtmlView::index)
      .def("path", &odr::HtmlView::path)
      .def("config", &odr::HtmlView::config)
      .def(
          "write_html",
          [](const odr::HtmlView &view) {
            std::ostringstream out;
            auto resources = view.write_html(out);
            return std::make_pair(out.str(), std::move(resources));
          },
          py::call_guard<py::gil_scoped_release>(),
          "Render this view; returns (html, resources).")
      .def("bring_offline", &odr::HtmlView::bring_offline,
           py::arg("output_path"), py::call_guard<py::gil_scoped_release>());

  py::class_<odr::HtmlService>(m, "HtmlService")
      .def("config", &odr::HtmlService::config)
      .def("list_views",
           [](const py::object &self) {
             const auto &service = self.cast<const odr::HtmlService &>();
             py::list views;
             for (const odr::HtmlView &view : service.list_views()) {
               py::object obj = py::cast(view);
               // Keep the service alive as long as the view handle exists;
               // `translate(...).list_views()[0]` must not dangle.
               obj.attr("_service") = self;
               views.append(std::move(obj));
             }
             return views;
           })
      .def("warmup", &odr::HtmlService::warmup,
           py::call_guard<py::gil_scoped_release>())
      .def("exists", &odr::HtmlService::exists, py::arg("path"))
      .def("mimetype", &odr::HtmlService::mimetype, py::arg("path"))
      .def(
          "write",
          [](const odr::HtmlService &service, const std::string &path) {
            std::ostringstream out;
            {
              // scoped rather than a `call_guard`: the `py::bytes` below needs
              // the GIL back
              const py::gil_scoped_release release;
              service.write(path, out);
            }
            return py::bytes(out.str());
          },
          py::arg("path"))
      .def(
          "write_html",
          [](const odr::HtmlService &service, const std::string &path) {
            std::ostringstream out;
            auto resources = service.write_html(path, out);
            return std::make_pair(out.str(), std::move(resources));
          },
          py::arg("path"), py::call_guard<py::gil_scoped_release>(),
          "Render one view path; returns (html, resources).")
      .def("bring_offline",
           py::overload_cast<const std::string &>(
               &odr::HtmlService::bring_offline, py::const_),
           py::arg("output_path"), py::call_guard<py::gil_scoped_release>())
      .def("bring_offline",
           py::overload_cast<const std::string &,
                             const std::vector<odr::HtmlView> &>(
               &odr::HtmlService::bring_offline, py::const_),
           py::arg("output_path"), py::arg("views"),
           py::call_guard<py::gil_scoped_release>());

  auto html = m.def_submodule("html", "Translate decoded files to HTML.");

  html.def("standard_resource_locator", &odr::html::standard_resource_locator);

  // Rendering is long-running, so it must not hold the GIL. Re-entry is safe:
  // both ways back into Python - a `Logger` sink through the trampoline and a
  // `resource_locator` through pybind11's `std::function` wrapper - acquire the
  // GIL themselves.
  html.def(
      "translate",
      [](const odr::DecodedFile &file, const std::string &cache_path,
         const odr::HtmlConfig &config, const odr::Logger &logger) {
        return odr::html::translate(file, cache_path, config, logger);
      },
      py::arg("file"), py::arg("cache_path"), py::arg("config"),
      py::arg("logger") = odr::Logger::null(),
      py::call_guard<py::gil_scoped_release>(),
      "Translate a decoded file to HTML.");
  html.def(
      "translate",
      [](const odr::Document &document, const std::string &cache_path,
         const odr::HtmlConfig &config, const odr::Logger &logger) {
        return odr::html::translate(document, cache_path, config, logger);
      },
      py::arg("document"), py::arg("cache_path"), py::arg("config"),
      py::arg("logger") = odr::Logger::null(),
      py::call_guard<py::gil_scoped_release>(),
      "Translate a document to HTML.");
  html.def(
      "translate",
      [](const odr::Filesystem &filesystem, const std::string &cache_path,
         const odr::HtmlConfig &config, const odr::Logger &logger) {
        return odr::html::translate(filesystem, cache_path, config, logger);
      },
      py::arg("filesystem"), py::arg("cache_path"), py::arg("config"),
      py::arg("logger") = odr::Logger::null(),
      py::call_guard<py::gil_scoped_release>(),
      "Translate a filesystem to HTML.");

  html.def(
      "edit",
      [](const odr::Document &document, const std::string &diff) {
        odr::html::edit(document, diff);
      },
      py::arg("document"), py::arg("diff"),
      "Apply a diff (produced by the browser-side editor) to a document.");
}
