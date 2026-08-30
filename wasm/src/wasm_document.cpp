#include <odr_wasm.hpp>

#include <odr/document.hpp>
#include <odr/file.hpp>

#include <emscripten/bind.h>

#include <sstream>
#include <string>

namespace odr::wasm {

namespace {

/// `capabilities()` narrowed to this document.
emscripten::val is_editable(const Handle handle) {
  return guarded([&] {
    return ok(emscripten::val(document_of(session(handle)).is_editable()));
  });
}

emscripten::val is_savable(const Handle handle, const bool encrypted) {
  return guarded([&] {
    return ok(
        emscripten::val(document_of(session(handle)).is_savable(encrypted)));
  });
}

/// The document's bytes; there is no filesystem to save to.
emscripten::val save(const Handle handle) {
  return guarded([&] {
    std::ostringstream out;
    document_of(session(handle)).save(out);
    return ok(to_uint8_array(out.str()));
  });
}

emscripten::val save_encrypted(const Handle handle,
                               const std::string &password) {
  return guarded([&] {
    std::ostringstream out;
    document_of(session(handle)).save(out, password);
    return ok(to_uint8_array(out.str()));
  });
}

} // namespace

} // namespace odr::wasm

EMSCRIPTEN_BINDINGS(odr_document) {
  emscripten::function("isEditable", &odr::wasm::is_editable);
  emscripten::function("isSavable", &odr::wasm::is_savable);
  emscripten::function("save", &odr::wasm::save);
  emscripten::function("saveEncrypted", &odr::wasm::save_encrypted);
}
