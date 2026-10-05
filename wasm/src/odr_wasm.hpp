#pragma once

#include <odr/document.hpp>
#include <odr/error_code.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>

#include <emscripten/val.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

/// Shared WebAssembly handles and error envelopes; see `wasm/AGENTS.md`.
namespace odr::wasm {

using Handle = std::uint32_t;

/// Rejects fractional, out-of-range and imprecise JavaScript integers.
template <std::integral T> T checked_integer(const double value) {
  constexpr double exact = 9007199254740991.0; // 2^53 - 1
  const double minimum =
      std::max(static_cast<double>(std::numeric_limits<T>::lowest()), -exact);
  const double maximum =
      std::min(static_cast<double>(std::numeric_limits<T>::max()), exact);
  if (!std::isfinite(value) || std::trunc(value) != value || value < minimum ||
      value > maximum) {
    throw std::invalid_argument("number is not a representable integer");
  }
  return static_cast<T>(value);
}

/// Owns one file, its editable tree and the service backing its views.
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
/// Whether the session's file opens as a document, CSV and Markdown included.
bool has_document(const Session &session);
/// @throws NoDocumentFile if the session's file is not a document.
Document &document_of(Session &session);
Handle add_session(Session session);
bool remove_session(Handle handle) noexcept;
void clear_sessions() noexcept;

emscripten::val ok(emscripten::val value);
emscripten::val ok();
/// `{ok: false, error: {type, code, message, ...}}`. Both name the same
/// @ref odr::ErrorCode, which is where every binding gets them.
emscripten::val error(ErrorCode code, const std::string &message);

/// The envelope for the exception being handled. Call from a `catch` block.
emscripten::val current_exception_error();

/// Prevents callbacks from invalidating a session used by an active call.
class CallScope final {
public:
  CallScope();
  ~CallScope();
  CallScope(const CallScope &) = delete;
  CallScope &operator=(const CallScope &) = delete;
};

template <typename F> emscripten::val guarded(F &&f) {
  try {
    const CallScope scope;
    return std::forward<F>(f)();
  } catch (...) {
    return current_exception_error();
  }
}

/// Copies bytes into JavaScript; heap growth invalidates a borrowed memory
/// view.
emscripten::val to_uint8_array(const std::string &bytes);

emscripten::val to_capabilities(const FileTypeCapabilities &capabilities);

/// Reads a `HtmlConfig` off a plain JS object, leaving unset keys defaulted.
HtmlConfig to_html_config(const emscripten::val &value);

} // namespace odr::wasm
