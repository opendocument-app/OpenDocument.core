#include <odr_wasm.hpp>

#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <emscripten/bind.h>

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

namespace odr::wasm {

namespace {

/// An absent or null key leaves @p target alone, so a caller sends only what it
/// means to change.
template <typename T>
void read(const emscripten::val &value, const char *key, T &target) {
  const emscripten::val field = value[key];
  if (field.isUndefined() || field.isNull()) {
    return;
  }
  target = field.as<T>();
}

/// @ref read for an enum, which JS carries as its ordinal.
template <typename T>
void read_enum(const emscripten::val &value, const char *key, T &target) {
  const emscripten::val field = value[key];
  if (field.isUndefined() || field.isNull()) {
    return;
  }
  target = static_cast<T>(field.as<int>());
}

/// A css length a caller states as a string, e.g. `"3mm"`.
void read_measure(const emscripten::val &value, const char *key,
                  std::optional<Measure> &target) {
  const emscripten::val field = value[key];
  if (field.isUndefined() || field.isNull()) {
    return;
  }
  target = Measure(field.as<std::string>());
}

/// Translates on first use, so a caller that only wants metadata does not pay
/// for a render at open.
Session &warm(const Handle handle) {
  Session &s = session(handle);
  if (!s.service.has_value()) {
    s.service = html::translate(s.file, s.config, s.logger);
    s.views = s.service->list_views();
  }
  return s;
}

emscripten::val list_views(const Handle handle) {
  return guarded([&] {
    const Session &s = warm(handle);

    emscripten::val result = emscripten::val::array();
    for (const HtmlView &view : s.views) {
      emscripten::val entry = emscripten::val::object();
      entry.set("name", view.name());
      entry.set("index", static_cast<double>(view.index()));
      entry.set("path", view.path());
      if (const std::optional<HtmlSheetCut> &cut = view.sheet_cut();
          cut.has_value()) {
        emscripten::val sheet_cut = emscripten::val::object();
        sheet_cut.set("contentRows", cut->content.rows);
        sheet_cut.set("contentColumns", cut->content.columns);
        sheet_cut.set("renderedRows", cut->rendered.rows);
        sheet_cut.set("renderedColumns", cut->rendered.columns);
        entry.set("sheetCut", sheet_cut);
      }
      result.call<void>("push", entry);
    }
    return ok(result);
  });
}

/// The rendered view as one HTML string, self-contained under the default
/// `embedImages` — which is what lets a viewer drop it into a `blob:` iframe.
emscripten::val render_view(const Handle handle, const std::size_t index) {
  return guarded([&] {
    const Session &s = warm(handle);
    if (index >= s.views.size()) {
      return error("OdrError", "no such view index: " + std::to_string(index));
    }

    std::ostringstream out;
    const HtmlResources resources = s.views[index].write_html(out);

    // A located resource is one the markup links to rather than inlines. The
    // viewer has to serve those itself, so it is told rather than discovering
    // a broken `src`.
    emscripten::val external = emscripten::val::array();
    for (const auto &[resource, location] : resources) {
      if (!location.has_value()) {
        continue;
      }
      emscripten::val entry = emscripten::val::object();
      entry.set("path", *location);
      entry.set("mimeType", resource.mime_type());
      entry.set("type", static_cast<int>(resource.type()));
      external.call<void>("push", entry);
    }

    emscripten::val result = emscripten::val::object();
    result.set("html", out.str());
    result.set("externalResources", external);
    return ok(result);
  });
}

/// The bytes behind a path the service knows — a view, or a resource
/// `renderView` reported. Same contract as `HttpServer::serve_file`.
emscripten::val read_path(const Handle handle, const std::string &path) {
  return guarded([&] {
    const Session &s = warm(handle);
    if (!s.service->exists(path)) {
      return error("FileNotFound", "no such path in the document: " + path);
    }

    std::ostringstream out;
    s.service->write(path, out);

    emscripten::val result = emscripten::val::object();
    result.set("bytes", to_uint8_array(out.str()));
    result.set("mimeType", s.service->mimetype(path));
    return ok(result);
  });
}

} // namespace

HtmlConfig to_html_config(const emscripten::val &value) {
  HtmlConfig config;
  if (value.isUndefined() || value.isNull()) {
    return config;
  }

  read(value, "embedImages", config.embed_images);
  read(value, "editable", config.editable);
  read(value, "textDocumentMargin", config.text_document_margin);
  read(value, "formatHtml", config.format_html);
  read(value, "embedOutline", config.embed_outline);
  read(value, "noDrm", config.no_drm);

  read(value, "backgroundImageFormat", config.background_image_format);
  read(value, "backgroundImageDpi", config.background_image_dpi);

  read(value, "pageRangeBegin", config.page_range_begin);
  if (const emscripten::val end = value["pageRangeEnd"];
      !end.isUndefined() && !end.isNull()) {
    config.page_range_end = end.as<std::uint32_t>();
  }

  read_enum(value, "colorScheme", config.color_scheme);
  read_enum(value, "spreadsheetGridlines", config.spreadsheet_gridlines);
  // `null` drops a limit, which is how a host renders a cut sheet in full;
  // absent leaves the default in place.
  if (const emscripten::val limit = value["spreadsheetLimit"];
      !limit.isUndefined()) {
    config.spreadsheet_limit = limit.isNull()
                                   ? std::optional<TableDimensions>()
                                   : std::optional(TableDimensions(
                                         limit["rows"].as<std::uint32_t>(),
                                         limit["columns"].as<std::uint32_t>()));
  }
  if (const emscripten::val limit = value["spreadsheetCellLimit"];
      !limit.isUndefined()) {
    // as a `number`, not a BigInt - a cell budget is nowhere near 2^53
    config.spreadsheet_cell_limit =
        limit.isNull()
            ? std::optional<std::uint64_t>()
            : std::optional(static_cast<std::uint64_t>(limit.as<double>()));
  }
  read_enum(value, "viewportMode", config.viewport_mode);
  if (const emscripten::val width = value["viewportWidth"];
      !width.isUndefined() && !width.isNull()) {
    config.viewport_width = width.as<std::uint32_t>();
  }
  if (const emscripten::val zoom = value["initialZoom"];
      !zoom.isUndefined() && !zoom.isNull()) {
    config.initial_zoom = zoom.as<double>();
  }
  read_enum(value, "pdfTextMode", config.pdf_text_mode);

  if (const emscripten::val margin = value["minContentMargin"];
      !margin.isUndefined() && !margin.isNull()) {
    read_measure(margin, "top", config.min_content_margin.top);
    read_measure(margin, "right", config.min_content_margin.right);
    read_measure(margin, "bottom", config.min_content_margin.bottom);
    read_measure(margin, "left", config.min_content_margin.left);
  }

  return config;
}

} // namespace odr::wasm

EMSCRIPTEN_BINDINGS(odr_html) {
  emscripten::function("listViews", &odr::wasm::list_views);
  emscripten::function("renderView", &odr::wasm::render_view);
  emscripten::function("readPath", &odr::wasm::read_path);
}
