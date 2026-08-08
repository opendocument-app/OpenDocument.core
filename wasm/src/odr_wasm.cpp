#include <odr_wasm.hpp>

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

emscripten::val error_for(const std::exception &e, const std::string &type) {
  return error(type, e.what());
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

emscripten::val error(const std::string &type, const std::string &message) {
  emscripten::val detail = emscripten::val::object();
  detail.set("type", type);
  detail.set("message", message);

  emscripten::val result = emscripten::val::object();
  result.set("ok", false);
  result.set("error", std::move(detail));
  return result;
}

emscripten::val current_exception_error() {
  try {
    throw;
  } catch (const UnsupportedOperation &e) {
    return error_for(e, "UnsupportedOperation");
  } catch (const FileNotFound &e) {
    return error_for(e, "FileNotFound");
  } catch (const UnknownFileType &e) {
    return error_for(e, "UnknownFileType");
  } catch (const UnsupportedFileType &e) {
    // the only error carrying a payload the caller acts on: a viewer names the
    // format it cannot show
    emscripten::val result = error_for(e, "UnsupportedFileType");
    result["error"].set("fileType", static_cast<int>(e.file_type));
    return result;
  } catch (const FileReadError &e) {
    return error_for(e, "FileReadError");
  } catch (const FileWriteError &e) {
    return error_for(e, "FileWriteError");
  } catch (const NoDocumentFile &e) {
    return error_for(e, "NoDocumentFile");
  } catch (const UnknownDocumentType &e) {
    return error_for(e, "UnknownDocumentType");
  } catch (const UnsupportedCryptoAlgorithm &e) {
    return error_for(e, "UnsupportedCryptoAlgorithm");
  } catch (const WrongPasswordError &e) {
    return error_for(e, "WrongPassword");
  } catch (const DecryptionFailed &e) {
    return error_for(e, "DecryptionFailed");
  } catch (const NotEncryptedError &e) {
    return error_for(e, "NotEncrypted");
  } catch (const FileEncryptedError &e) {
    return error_for(e, "FileEncrypted");
  } catch (const DocumentCopyProtectedException &e) {
    return error_for(e, "DocumentCopyProtected");
  } catch (const std::exception &e) {
    return error_for(e, "OdrError");
  } catch (...) {
    return error("OdrError", "unknown native error");
  }
}

emscripten::val to_capabilities(const FileTypeCapabilities &capabilities) {
  emscripten::val result = emscripten::val::object();
  result.set("detectByContent", capabilities.detect_by_content);
  result.set("open", capabilities.open);
  result.set("decrypt", capabilities.decrypt);
  result.set("translateHtml", capabilities.translate_html);
  result.set("edit", capabilities.edit);
  result.set("save", capabilities.save);
  result.set("encrypt", capabilities.encrypt);
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
