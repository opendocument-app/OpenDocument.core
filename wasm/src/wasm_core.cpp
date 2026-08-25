#include <odr_wasm.hpp>

#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>

#include <emscripten/bind.h>

#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace odr::wasm {

namespace {

emscripten::val version() { return emscripten::val(odr::version()); }
emscripten::val identify() { return emscripten::val(odr::identify()); }

emscripten::val string_array(const std::span<const std::string_view> values) {
  emscripten::val result = emscripten::val::array();
  for (const std::string_view value : values) {
    result.call<void>("push", std::string(value));
  }
  return result;
}

/// Every file type, with what a viewer needs before it holds a file: an
/// `<input accept>` list, and what the PWA manifest declares it opens.
emscripten::val file_types() {
  emscripten::val result = emscripten::val::array();
  for (const FileType type : odr::all_file_types()) {
    emscripten::val entry = emscripten::val::object();
    entry.set("fileType", static_cast<int>(type));
    entry.set("name", odr::file_type_to_string(type));
    entry.set("category",
              static_cast<int>(odr::file_category_by_file_type(type)));
    entry.set("documentType",
              static_cast<int>(odr::document_type_by_file_type(type)));
    entry.set("extensions",
              string_array(odr::file_extensions_by_file_type(type)));
    entry.set("mimeTypes", string_array(odr::mimetypes_by_file_type(type)));
    entry.set("capabilities",
              to_capabilities(odr::capabilities_by_file_type(type)));

    result.call<void>("push", entry);
  }
  return result;
}

/// Enum name to ordinal, so the JS side never restates an ordinal by hand.
/// `FileType`, `FileCategory` and `DocumentType` are derived from the library's
/// tables and cannot drift; the rest have no runtime table and are listed here,
/// pinned by `tests/enums.test.mjs`.
emscripten::val enum_tables() {
  const auto table = [](const auto &...entries) {
    emscripten::val result = emscripten::val::object();
    (result.set(entries.first, entries.second), ...);
    return result;
  };
  const auto entry = [](const char *name, auto value) {
    return std::pair<const char *, int>{name, static_cast<int>(value)};
  };

  emscripten::val file_type = emscripten::val::object();
  for (const FileType type : odr::all_file_types()) {
    file_type.set(odr::file_type_to_string(type), static_cast<int>(type));
  }

  emscripten::val file_category = emscripten::val::object();
  for (const FileCategory category :
       {FileCategory::unknown, FileCategory::text, FileCategory::image,
        FileCategory::archive, FileCategory::document, FileCategory::audio,
        FileCategory::video, FileCategory::font}) {
    file_category.set(odr::file_category_to_string(category),
                      static_cast<int>(category));
  }

  emscripten::val document_type = emscripten::val::object();
  for (const DocumentType type :
       {DocumentType::unknown, DocumentType::text, DocumentType::presentation,
        DocumentType::spreadsheet, DocumentType::drawing}) {
    document_type.set(odr::document_type_to_string(type),
                      static_cast<int>(type));
  }

  emscripten::val result = emscripten::val::object();
  result.set("FileType", file_type);
  result.set("FileCategory", file_category);
  result.set("DocumentType", document_type);
  result.set("HtmlResourceType",
             table(entry("html_fragment", HtmlResourceType::html_fragment),
                   entry("css", HtmlResourceType::css),
                   entry("js", HtmlResourceType::js),
                   entry("image", HtmlResourceType::image),
                   entry("font", HtmlResourceType::font),
                   entry("media", HtmlResourceType::media),
                   entry("file", HtmlResourceType::file)));
  result.set("HtmlTableGridlines",
             table(entry("none", HtmlTableGridlines::none),
                   entry("soft", HtmlTableGridlines::soft),
                   entry("hard", HtmlTableGridlines::hard)));
  result.set("HtmlColorScheme",
             table(entry("light", HtmlColorScheme::light),
                   entry("dark", HtmlColorScheme::dark),
                   entry("system", HtmlColorScheme::system)));
  result.set(
      "HtmlViewportMode",
      table(entry("automatic", HtmlViewportMode::automatic),
            entry("fit_width", HtmlViewportMode::fit_width),
            entry("actual_size", HtmlViewportMode::actual_size),
            entry("none", HtmlViewportMode::none),
            entry("fit_width_by_view", HtmlViewportMode::fit_width_by_view)));
  result.set("PdfTextMode",
             table(entry("dual_layer", PdfTextMode::dual_layer),
                   entry("single_layer", PdfTextMode::single_layer)));
  result.set("EncryptionState",
             table(entry("unknown", EncryptionState::unknown),
                   entry("not_encrypted", EncryptionState::not_encrypted),
                   entry("encrypted", EncryptionState::encrypted),
                   entry("decrypted", EncryptionState::decrypted)));
  result.set("LogLevel", table(entry("verbose", LogLevel::verbose),
                               entry("debug", LogLevel::debug),
                               entry("info", LogLevel::info),
                               entry("warning", LogLevel::warning),
                               entry("error", LogLevel::error),
                               entry("fatal", LogLevel::fatal)));
  return result;
}

} // namespace

} // namespace odr::wasm

EMSCRIPTEN_BINDINGS(odr_core) {
  emscripten::function("version", &odr::wasm::version);
  emscripten::function("identify", &odr::wasm::identify);
  emscripten::function("fileTypes", &odr::wasm::file_types);
  emscripten::function("enumTables", &odr::wasm::enum_tables);
}
