#include <odr_wasm.hpp>

#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/util/odr_meta_util.hpp>

#include <emscripten/bind.h>

#include <string>
#include <utility>

namespace odr::wasm {

namespace {

/// An embind `std::string` *parameter* takes a `Uint8Array` and copies the
/// bytes verbatim, so this is binary-safe — unlike a `std::string` *return*,
/// which goes through `UTF8ToString`.
File from_bytes(const std::string &bytes) { return File::from_memory(bytes); }

emscripten::val opened(DecodedFile file, const emscripten::val &config) {
  Session s{.file = std::move(file),
            .logger = default_logger(),
            .config = to_html_config(config),
            .service = {},
            .views = {}};
  return ok(emscripten::val(add_session(std::move(s))));
}

emscripten::val detect(const std::string &bytes) {
  return guarded([&] {
    const File file = from_bytes(bytes);
    const Logger &logger = default_logger();

    emscripten::val types = emscripten::val::array();
    for (const FileType type : DecodedFile::list_file_types(file, logger)) {
      types.call<void>("push", static_cast<int>(type));
    }

    emscripten::val result = emscripten::val::object();
    result.set("fileTypes", types);
    result.set("mimeType", std::string(DecodedFile::mimetype(file, logger)));
    return ok(result);
  });
}

emscripten::val open(const std::string &bytes, const emscripten::val &config) {
  return guarded([&] {
    return opened(DecodedFile(from_bytes(bytes), default_logger()), config);
  });
}

emscripten::val open_as(const std::string &bytes, const int as,
                        const emscripten::val &config) {
  return guarded([&] {
    return opened(DecodedFile(from_bytes(bytes), static_cast<FileType>(as),
                              default_logger()),
                  config);
  });
}

/// The meta blob as `cli/src/meta.cpp` produces it, reusing the same serialiser
/// rather than growing a second one that drifts.
emscripten::val meta(const Handle handle) {
  return guarded([&] {
    const Session &s = session(handle);
    const auto json = internal::util::meta::meta_to_json(s.file.file_meta());
    return ok(emscripten::val(json.dump()));
  });
}

emscripten::val capabilities(const Handle handle) {
  return guarded(
      [&] { return ok(to_capabilities(session(handle).file.capabilities())); });
}

emscripten::val is_password_encrypted(const Handle handle) {
  return guarded([&] {
    return ok(emscripten::val(session(handle).file.password_encrypted()));
  });
}

/// Decrypts in place: a new handle would leave the caller holding two, one of
/// them useless.
emscripten::val decrypt(const Handle handle, const std::string &password) {
  return guarded([&] {
    Session &s = session(handle);
    s.file = s.file.decrypt(password);
    // whatever was translated came from the encrypted file
    s.service.reset();
    s.views.clear();
    return ok();
  });
}

emscripten::val file_type(const Handle handle) {
  return guarded([&] {
    return ok(
        emscripten::val(static_cast<int>(session(handle).file.file_type())));
  });
}

emscripten::val close(const Handle handle) {
  return guarded([&] { return ok(emscripten::val(remove_session(handle))); });
}

emscripten::val close_all() {
  return guarded([] {
    clear_sessions();
    return ok();
  });
}

} // namespace

} // namespace odr::wasm

EMSCRIPTEN_BINDINGS(odr_file) {
  emscripten::function("detect", &odr::wasm::detect);
  emscripten::function("open", &odr::wasm::open);
  emscripten::function("openAs", &odr::wasm::open_as);
  emscripten::function("meta", &odr::wasm::meta);
  emscripten::function("capabilities", &odr::wasm::capabilities);
  emscripten::function("isPasswordEncrypted",
                       &odr::wasm::is_password_encrypted);
  emscripten::function("decrypt", &odr::wasm::decrypt);
  emscripten::function("fileType", &odr::wasm::file_type);
  emscripten::function("close", &odr::wasm::close);
  emscripten::function("closeAll", &odr::wasm::close_all);
}
