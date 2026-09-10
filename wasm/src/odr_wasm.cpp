#include <odr_wasm.hpp>

#include <odr/error_code.hpp>
#include <odr/exceptions.hpp>

#include <emscripten/bind.h>

#include <exception>
#include <stdexcept>
#include <unordered_map>

namespace odr::wasm {

namespace {

std::unordered_map<Handle, Session> &sessions() {
  static std::unordered_map<Handle, Session> instance;
  return instance;
}

Handle &next_handle() {
  // 0 is never handed out, so a zeroed handle in JS is always invalid
  static Handle instance = 1;
  return instance;
}

emscripten::val error_for(const std::exception &e) {
  return error(error_code(e), e.what());
}

} // namespace

Session &session(const Handle handle) {
  const auto it = sessions().find(handle);
  if (it == sessions().end()) {
    throw std::out_of_range("no such document handle: " +
                            std::to_string(handle));
  }
  return it->second;
}

Document &document_of(Session &session) {
  if (!session.document.has_value()) {
    session.document = session.file.as_document_file().document();
  }
  return *session.document;
}

Handle add_session(Session session) {
  const Handle handle = next_handle()++;
  sessions().emplace(handle, std::move(session));
  return handle;
}

bool remove_session(const Handle handle) noexcept {
  return sessions().erase(handle) != 0;
}

void clear_sessions() noexcept { sessions().clear(); }

emscripten::val ok(emscripten::val value) {
  emscripten::val result = emscripten::val::object();
  result.set("ok", true);
  result.set("value", std::move(value));
  return result;
}

emscripten::val ok() { return ok(emscripten::val::undefined()); }

emscripten::val error(const ErrorCode code, const std::string &message) {
  emscripten::val detail = emscripten::val::object();
  // The catch-all keeps the name `js/index.js` gives the thrown error.
  detail.set("type", code == ErrorCode::unknown
                         ? std::string("OdrError")
                         : std::string(error_code_name(code)));
  detail.set("code", static_cast<std::int32_t>(code));
  detail.set("message", message);

  emscripten::val result = emscripten::val::object();
  result.set("ok", false);
  result.set("error", std::move(detail));
  return result;
}

emscripten::val current_exception_error() {
  try {
    throw;
  } catch (const UnsupportedFileType &e) {
    // the only error carrying a payload the caller acts on: a viewer names the
    // format it cannot show
    emscripten::val result = error_for(e);
    result["error"].set("fileType", static_cast<int>(e.file_type));
    return result;
  } catch (const std::exception &e) {
    return error_for(e);
  } catch (...) {
    return error(ErrorCode::unknown, "unknown native error");
  }
}

emscripten::val to_capabilities(const FileTypeCapabilities &capabilities) {
  emscripten::val result = emscripten::val::object();
  result.set("detectByContent", capabilities.detect_by_content);
  result.set("open", capabilities.open);
  result.set("decrypt", capabilities.decrypt);
  result.set("translateHtml", capabilities.translate_html);
  result.set("colorScheme", capabilities.color_scheme);
  result.set("edit", capabilities.edit);
  result.set("save", capabilities.save);
  result.set("encrypt", capabilities.encrypt);
  result.set("annotate", capabilities.annotate);
  return result;
}

emscripten::val to_uint8_array(const std::string &bytes) {
  const emscripten::val view(emscripten::typed_memory_view(
      bytes.size(), reinterpret_cast<const std::uint8_t *>(bytes.data())));

  emscripten::val result =
      emscripten::val::global("Uint8Array").new_(bytes.size());
  result.call<void>("set", view);
  return result;
}

} // namespace odr::wasm
