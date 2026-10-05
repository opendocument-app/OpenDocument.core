#include <odr_wasm.hpp>

#include <odr/document.hpp>
#include <odr/error_code.hpp>
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

/// Lists file types, aliases and capabilities for file pickers.
emscripten::val file_types() {
  emscripten::val result = emscripten::val::array();
  for (const FileType type : odr::all_file_types()) {
    emscripten::val entry = emscripten::val::object();
    entry.set("fileType", static_cast<std::int32_t>(type));
    entry.set("name", odr::file_type_to_string(type));
    entry.set("category",
              static_cast<std::int32_t>(odr::file_category_by_file_type(type)));
    entry.set("documentType",
              static_cast<std::int32_t>(odr::document_type_by_file_type(type)));
    entry.set("extensions",
              string_array(odr::file_extensions_by_file_type(type)));
    entry.set("mimeTypes", string_array(odr::mimetypes_by_file_type(type)));
    entry.set("capabilities",
              to_capabilities(odr::capabilities_by_file_type(type)));

    result.call<void>("push", entry);
  }
  return result;
}

/// Builds enum tables from the core; tests pin the manually listed enums.
emscripten::val enum_tables() {
  const auto table = [](const auto &...entries) {
    emscripten::val result = emscripten::val::object();
    (result.set(entries.first, entries.second), ...);
    return result;
  };
  const auto entry = [](const char *name, auto value) {
    return std::pair<const char *, std::int32_t>{
        name, static_cast<std::int32_t>(value)};
  };

  emscripten::val file_type = emscripten::val::object();
  for (const FileType type : odr::all_file_types()) {
    file_type.set(odr::file_type_to_string(type),
                  static_cast<std::int32_t>(type));
  }

  emscripten::val file_category = emscripten::val::object();
  for (const FileCategory category :
       {FileCategory::unknown, FileCategory::text, FileCategory::image,
        FileCategory::archive, FileCategory::document, FileCategory::audio,
        FileCategory::video, FileCategory::font}) {
    file_category.set(odr::file_category_to_string(category),
                      static_cast<std::int32_t>(category));
  }

  emscripten::val document_type = emscripten::val::object();
  for (const DocumentType type :
       {DocumentType::unknown, DocumentType::text, DocumentType::presentation,
        DocumentType::spreadsheet, DocumentType::drawing}) {
    document_type.set(odr::document_type_to_string(type),
                      static_cast<std::int32_t>(type));
  }

  // `all_text_encodings` leaves `unknown` out, and it is the one with no name
  emscripten::val text_encoding = emscripten::val::object();
  text_encoding.set("unknown",
                    static_cast<std::int32_t>(TextEncoding::unknown));
  for (const TextEncoding encoding : odr::all_text_encodings()) {
    text_encoding.set(std::string(odr::text_encoding_to_string(encoding)),
                      static_cast<std::int32_t>(encoding));
  }

  emscripten::val error_code = emscripten::val::object();
  for (const ErrorCode code : odr::all_error_codes()) {
    error_code.set(std::string(odr::error_code_name(code)),
                   static_cast<std::int32_t>(code));
  }

  emscripten::val result = emscripten::val::object();
  result.set("ErrorCode", error_code);
  result.set("FileType", file_type);
  result.set("TextEncoding", text_encoding);
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
  result.set("HtmlEditingScope",
             table(entry("paragraph", HtmlEditingScope::paragraph),
                   entry("document", HtmlEditingScope::document)));
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
