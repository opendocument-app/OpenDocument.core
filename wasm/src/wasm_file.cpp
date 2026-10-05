#include <odr_wasm.hpp>

#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/util/odr_meta_util.hpp>

#include <emscripten/bind.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

namespace odr::wasm {

namespace {

/// Embind copies binary parameters verbatim; string returns decode as UTF-8.
File from_bytes(const std::string &bytes, std::string name) {
  return File::from_memory(bytes, std::move(name));
}

emscripten::val opened(DecodedFile file, const emscripten::val &config) {
  Session s{.file = std::move(file),
            .logger = default_logger(),
            .config = to_html_config(config),
            .document = {},
            .service = {},
            .views = {}};
  return ok(emscripten::val(add_session(std::move(s))));
}

emscripten::val detect(const std::string &bytes, std::string name) {
  return guarded([&] {
    const File file = from_bytes(bytes, std::move(name));
    const Logger &logger = default_logger();

    emscripten::val types = emscripten::val::array();
    for (const FileType type : odr::list_file_types(file, logger)) {
      types.call<void>("push", static_cast<std::int32_t>(type));
    }

    emscripten::val result = emscripten::val::object();
    result.set("fileTypes", types);
    result.set("mimeType", std::string(odr::mimetype(file, logger)));
    return ok(result);
  });
}

emscripten::val open(const std::string &bytes, std::string name,
                     const emscripten::val &config) {
  return guarded([&] {
    return opened(
        odr::open(from_bytes(bytes, std::move(name)), {}, default_logger()),
        config);
  });
}

emscripten::val open_as(const std::string &bytes, std::string name,
                        const std::int32_t as, const emscripten::val &config) {
  return guarded([&] {
    return opened(odr::open(from_bytes(bytes, std::move(name)),
                            DecodeOptions::as(static_cast<FileType>(as)),
                            default_logger()),
                  config);
  });
}

/// Creates a document with a backing file, as `open` does.
emscripten::val create(const std::int32_t type, const emscripten::val &config) {
  return guarded([&] {
    const auto file_type = static_cast<FileType>(type);
    return opened(odr::open(odr::create_document(file_type).save_to_memory(),
                            DecodeOptions::as(file_type), default_logger()),
                  config);
  });
}

/// Uses the same metadata serializer as the CLI.
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

/// Decrypts in place, because a new handle would leave the caller holding two,
/// one of them useless.
emscripten::val decrypt(const Handle handle, const std::string &password) {
  return guarded([&] {
    Session &s = session(handle);
    s.file = s.file.decrypt(password);
    // whatever was decoded or translated came from the encrypted file
    s.document.reset();
    s.service.reset();
    s.views.clear();
    return ok();
  });
}

emscripten::val file_name(const Handle handle) {
  return guarded(
      [&] { return ok(emscripten::val(session(handle).file.file().name())); });
}

emscripten::val file_type(const Handle handle) {
  return guarded([&] {
    return ok(emscripten::val(
        static_cast<std::int32_t>(session(handle).file.file_type())));
  });
}

/// Whether this file can take annotations, the counterpart of `isEditable`.
emscripten::val is_annotatable(const Handle handle) {
  return guarded([&] {
    const Session &s = session(handle);
    return ok(emscripten::val(s.file.file_type() ==
                                  FileType::portable_document_format &&
                              s.file.as_pdf_file().is_annotatable()));
  });
}

/// What `/P` states, or `null` without an `/Encrypt`.
emscripten::val permissions(const Handle handle) {
  return guarded([&] {
    const Session &s = session(handle);
    if (s.file.file_type() != FileType::portable_document_format ||
        !s.file.as_pdf_file().has_permissions()) {
      return ok(emscripten::val::null());
    }
    const PdfPermissions permissions = s.file.as_pdf_file().permissions();
    emscripten::val result = emscripten::val::object();
    result.set("print", permissions.print);
    result.set("modifyContents", permissions.modify_contents);
    result.set("copy", permissions.copy);
    result.set("modifyAnnotations", permissions.modify_annotations);
    result.set("fillForms", permissions.fill_forms);
    result.set("copyForAccessibility", permissions.copy_for_accessibility);
    result.set("assemble", permissions.assemble);
    result.set("printHighQuality", permissions.print_high_quality);
    return ok(result);
  });
}

/// Returns annotated PDF bytes from the browser annotation payload.
emscripten::val annotate(const Handle handle, const std::string &payload) {
  return guarded([&] {
    Session &s = session(handle);
    std::ostringstream out;
    s.file.as_pdf_file().annotate(payload, out, s.logger);
    return ok(to_uint8_array(std::move(out).str()));
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
  emscripten::function("create", &odr::wasm::create);
  emscripten::function("meta", &odr::wasm::meta);
  emscripten::function("capabilities", &odr::wasm::capabilities);
  emscripten::function("isPasswordEncrypted",
                       &odr::wasm::is_password_encrypted);
  emscripten::function("decrypt", &odr::wasm::decrypt);
  emscripten::function("fileType", &odr::wasm::file_type);
  emscripten::function("fileName", &odr::wasm::file_name);
  emscripten::function("isAnnotatable", &odr::wasm::is_annotatable);
  emscripten::function("permissions", &odr::wasm::permissions);
  emscripten::function("annotate", &odr::wasm::annotate);
  emscripten::function("close", &odr::wasm::close);
  emscripten::function("closeAll", &odr::wasm::close_all);
}
