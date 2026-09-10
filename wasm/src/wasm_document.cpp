#include <odr_wasm.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>

#include <emscripten/bind.h>

#include <sstream>
#include <stdexcept>
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

/// The element the operation names, refusing an id this document does not
/// hold. Ids are what crosses instead of elements: the render writes them into
/// the page as `data-odr-id`, and a plain number needs no handle.
Element element_of(Session &session, const double identifier) {
  const Element element = document_of(session).element_by_id(
      static_cast<ElementIdentifier>(identifier));
  if (!element) {
    throw std::invalid_argument("element not found");
  }
  return element;
}

double id_of(const Element &element) {
  return static_cast<double>(element.identifier());
}

emscripten::val remove_element(const Handle handle, const double identifier) {
  return guarded([&] {
    Session &s = session(handle);
    document_of(s).remove(element_of(s, identifier));
    return ok();
  });
}

emscripten::val insert_text_before(const Handle handle, const double anchor,
                                   const std::string &text) {
  return guarded([&] {
    Session &s = session(handle);
    return ok(emscripten::val(id_of(document_of(s).insert_text_before(
        element_of(s, anchor).as_text(), text))));
  });
}

emscripten::val insert_text_after(const Handle handle, const double anchor,
                                  const std::string &text) {
  return guarded([&] {
    Session &s = session(handle);
    return ok(emscripten::val(id_of(document_of(s).insert_text_after(
        element_of(s, anchor).as_text(), text))));
  });
}

emscripten::val append_text(const Handle handle, const double parent,
                            const std::string &text) {
  return guarded([&] {
    Session &s = session(handle);
    return ok(emscripten::val(
        id_of(document_of(s).append_text(element_of(s, parent), text))));
  });
}

/// @p after of 0 is `null_element_id`: split before every child.
emscripten::val split_paragraph(const Handle handle, const double paragraph,
                                const double after) {
  return guarded([&] {
    Session &s = session(handle);
    return ok(emscripten::val(id_of(document_of(s).split_paragraph(
        element_of(s, paragraph).as_paragraph(),
        after == 0 ? Element() : element_of(s, after)))));
  });
}

emscripten::val merge_paragraph_with_next(const Handle handle,
                                          const double paragraph) {
  return guarded([&] {
    Session &s = session(handle);
    document_of(s).merge_paragraph_with_next(
        element_of(s, paragraph).as_paragraph());
    return ok();
  });
}

emscripten::val insert_paragraph_after(const Handle handle,
                                       const double paragraph) {
  return guarded([&] {
    Session &s = session(handle);
    return ok(emscripten::val(id_of(document_of(s).insert_paragraph_after(
        element_of(s, paragraph).as_paragraph()))));
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
  emscripten::function("removeElement", &odr::wasm::remove_element);
  emscripten::function("insertTextBefore", &odr::wasm::insert_text_before);
  emscripten::function("insertTextAfter", &odr::wasm::insert_text_after);
  emscripten::function("appendText", &odr::wasm::append_text);
  emscripten::function("splitParagraph", &odr::wasm::split_paragraph);
  emscripten::function("mergeParagraphWithNext",
                       &odr::wasm::merge_paragraph_with_next);
  emscripten::function("insertParagraphAfter",
                       &odr::wasm::insert_paragraph_after);
  emscripten::function("save", &odr::wasm::save);
  emscripten::function("saveEncrypted", &odr::wasm::save_encrypted);
}
