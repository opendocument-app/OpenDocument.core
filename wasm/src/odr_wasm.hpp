#pragma once

#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>

#include <emscripten/val.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

/// Shared plumbing for the WebAssembly bindings. Nothing throws across the
/// boundary and nothing escapes as an embind handle; `wasm/AGENTS.md` says why.
namespace odr::wasm {

using Handle = std::uint32_t;

/// One open document. Owns everything reachable from it, because the pieces do
/// not own each other: `HtmlView` holds a bare pointer into its service, so the
/// service has to outlive the views.
struct Session final {
  DecodedFile file;
  Logger logger;
  HtmlConfig config;
  /// The one tree render, edit and save share; `DocumentFile::document()`
  /// decodes a fresh one per call.
  std::optional<Document> document;
  std::optional<HtmlService> service;
  HtmlViews views;
};

Logger &default_logger();

/// @throws std::out_of_range if @p handle is unknown.
Session &session(Handle handle);
/// @throws NoDocumentFile if the session's file is not a document.
Document &document_of(Session &session);
Handle add_session(Session session);
bool remove_session(Handle handle) noexcept;
void clear_sessions() noexcept;

emscripten::val ok(emscripten::val value);
emscripten::val ok();
/// `{ok: false, error: {type, message, ...}}`, with `type` naming the C++
/// exception. Kept in step with `jni/src/odr_jni.cpp`'s `throw_java` and
/// `apple/src/ODRInternal.mm`.
emscripten::val error(const std::string &type, const std::string &message);

/// The envelope for the exception being handled. Call from a `catch` block.
emscripten::val current_exception_error();

template <typename F> emscripten::val guarded(F &&f) {
  try {
    return std::forward<F>(f)();
  } catch (...) {
    return current_exception_error();
  }
}

/// A `Uint8Array` copy of @p bytes. A copy because `typed_memory_view` aliases
/// the wasm heap, which `ALLOW_MEMORY_GROWTH` detaches on the next allocation.
emscripten::val to_uint8_array(const std::string &bytes);

emscripten::val to_capabilities(const FileTypeCapabilities &capabilities);

/// Reads a `HtmlConfig` off a plain JS object, leaving unset keys defaulted.
HtmlConfig to_html_config(const emscripten::val &value);

} // namespace odr::wasm
