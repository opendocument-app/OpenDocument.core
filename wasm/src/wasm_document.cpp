#include <odr_wasm.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <emscripten/bind.h>

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

namespace odr::wasm {

namespace {

/// `capabilities()` narrowed to this document, or `TextFile::is_savable`.
emscripten::val is_editable(const Handle handle) {
  return guarded([&] {
    Session &s = session(handle);
    if (s.file.is_text_file()) {
      return ok(emscripten::val(s.file.as_text_file().is_savable()));
    }
    return ok(emscripten::val(document_of(s).is_editable()));
  });
}

/// Null for a plain text file and for a document stating none.
emscripten::val locale(const Handle handle) {
  return guarded([&] {
    Session &s = session(handle);
    if (s.file.is_text_file()) {
      return ok(emscripten::val::null());
    }
    const std::optional<std::string> result = document_of(s).locale();
    return ok(result ? emscripten::val(*result) : emscripten::val::null());
  });
}

emscripten::val is_savable(const Handle handle, const bool encrypted) {
  return guarded([&] {
    Session &s = session(handle);
    if (s.file.is_text_file()) {
      return ok(
          emscripten::val(!encrypted && s.file.as_text_file().is_savable()));
    }
    return ok(emscripten::val(document_of(s).is_savable(encrypted)));
  });
}

/// `{changed, circular, unevaluated}`, each a list of `{sheet, column, row}`.
emscripten::val recalculate(const Handle handle) {
  return guarded([&] {
    const Recalculation result = document_of(session(handle)).recalculate();
    const auto list = [](const std::vector<SheetPosition> &positions) {
      emscripten::val array = emscripten::val::array();
      for (const SheetPosition &position : positions) {
        emscripten::val entry = emscripten::val::object();
        entry.set("sheet", position.sheet);
        entry.set("column", position.cell.column);
        entry.set("row", position.cell.row);
        array.call<void>("push", entry);
      }
      return array;
    };
    emscripten::val object = emscripten::val::object();
    object.set("changed", list(result.changed()));
    object.set("circular", list(result.circular()));
    object.set("unevaluated", list(result.unevaluated()));
    return ok(object);
  });
}

/// Resolves a rendered `data-odr-id`; rejects invalid or absent identifiers.
/// Ids cross the boundary instead of elements because a number needs no handle.
Element element_of(Session &session, const double identifier) {
  const Element element = document_of(session).element_by_id(
      checked_integer<ElementIdentifier>(identifier));
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

/// Replays a style operation through the shared editor envelope parser.
emscripten::val edit_style(const Handle handle, const std::string &op,
                           const std::string &place,
                           const emscripten::val style) {
  Session &s = session(handle);
  if (style.isUndefined() || style.isNull() ||
      style.typeOf().as<std::string>() != "object") {
    throw std::invalid_argument(op + " takes a style object");
  }
  const std::string json =
      emscripten::val::global("JSON").call<std::string>("stringify", style);
  document_of(s).edit(R"({"version":2,"ops":[{"op":")" + op + R"(",)" + place +
                      R"(,"style":)" + json + "}]}");
  return ok();
}

emscripten::val set_text_style(const Handle handle, const double id,
                               const emscripten::val style) {
  return guarded([&] {
    return edit_style(
        handle, "setTextStyle",
        "\"id\":" +
            std::to_string(element_of(session(handle), id).identifier()),
        style);
  });
}

emscripten::val set_paragraph_style(const Handle handle, const double id,
                                    const emscripten::val style) {
  return guarded([&] {
    return edit_style(
        handle, "setParagraphStyle",
        "\"id\":" +
            std::to_string(element_of(session(handle), id).identifier()),
        style);
  });
}

std::string index_field(const std::string &name, const double index) {
  return '"' + name + R"(":)" +
         std::to_string(checked_integer<std::uint32_t>(index));
}

emscripten::val set_cell_style(const Handle handle, const double sheet,
                               const double column, const double row,
                               const emscripten::val style) {
  return guarded([&] {
    return edit_style(handle, "setCellStyle",
                      index_field("sheet", sheet) + "," +
                          index_field("column", column) + "," +
                          index_field("row", row),
                      style);
  });
}

emscripten::val set_row_style(const Handle handle, const double sheet,
                              const double row, const emscripten::val style) {
  return guarded([&] {
    return edit_style(
        handle, "setRowStyle",
        index_field("sheet", sheet) + "," + index_field("row", row), style);
  });
}

emscripten::val set_column_style(const Handle handle, const double sheet,
                                 const double column,
                                 const emscripten::val style) {
  return guarded([&] {
    return edit_style(handle, "setColumnStyle",
                      index_field("sheet", sheet) + "," +
                          index_field("column", column),
                      style);
  });
}

/// Replays a row or column operation; @p axis names its index.
emscripten::val edit_structure(const Handle handle, const std::string &op,
                               const std::string &axis, const double sheet,
                               const double index, const double count) {
  return guarded([&] {
    document_of(session(handle))
        .edit(R"({"version":2,"ops":[{"op":")" + op + R"(",)" +
              index_field("sheet", sheet) + "," + index_field(axis, index) +
              "," + index_field("count", count) + "}]}");
    return ok();
  });
}

emscripten::val insert_rows(const Handle handle, const double sheet,
                            const double row, const double count) {
  return edit_structure(handle, "insertRows", "row", sheet, row, count);
}

emscripten::val delete_rows(const Handle handle, const double sheet,
                            const double row, const double count) {
  return edit_structure(handle, "deleteRows", "row", sheet, row, count);
}

emscripten::val insert_columns(const Handle handle, const double sheet,
                               const double column, const double count) {
  return edit_structure(handle, "insertColumns", "column", sheet, column,
                        count);
}

emscripten::val delete_columns(const Handle handle, const double sheet,
                               const double column, const double count) {
  return edit_structure(handle, "deleteColumns", "column", sheet, column,
                        count);
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
    Session &s = session(handle);
    std::ostringstream out;
    if (s.file.is_text_file()) {
      s.file.as_text_file().save(out);
    } else {
      document_of(s).save(out);
    }
    return ok(to_uint8_array(out.str()));
  });
}

emscripten::val save_encrypted(const Handle handle,
                               const std::string &password) {
  return guarded([&] {
    Session &s = session(handle);
    if (s.file.is_text_file()) {
      throw UnsupportedOperation();
    }
    std::ostringstream out;
    document_of(s).save(out, password);
    return ok(to_uint8_array(out.str()));
  });
}

} // namespace

} // namespace odr::wasm

EMSCRIPTEN_BINDINGS(odr_document) {
  emscripten::function("isEditable", &odr::wasm::is_editable);
  emscripten::function("isSavable", &odr::wasm::is_savable);
  emscripten::function("locale", &odr::wasm::locale);
  emscripten::function("recalculate", &odr::wasm::recalculate);
  emscripten::function("removeElement", &odr::wasm::remove_element);
  emscripten::function("insertTextBefore", &odr::wasm::insert_text_before);
  emscripten::function("insertTextAfter", &odr::wasm::insert_text_after);
  emscripten::function("appendText", &odr::wasm::append_text);
  emscripten::function("setTextStyle", &odr::wasm::set_text_style);
  emscripten::function("setCellStyle", &odr::wasm::set_cell_style);
  emscripten::function("setRowStyle", &odr::wasm::set_row_style);
  emscripten::function("setColumnStyle", &odr::wasm::set_column_style);
  emscripten::function("insertRows", &odr::wasm::insert_rows);
  emscripten::function("deleteRows", &odr::wasm::delete_rows);
  emscripten::function("insertColumns", &odr::wasm::insert_columns);
  emscripten::function("deleteColumns", &odr::wasm::delete_columns);
  emscripten::function("setParagraphStyle", &odr::wasm::set_paragraph_style);
  emscripten::function("splitParagraph", &odr::wasm::split_paragraph);
  emscripten::function("mergeParagraphWithNext",
                       &odr::wasm::merge_paragraph_with_next);
  emscripten::function("insertParagraphAfter",
                       &odr::wasm::insert_paragraph_after);
  emscripten::function("save", &odr::wasm::save);
  emscripten::function("saveEncrypted", &odr::wasm::save_encrypted);
}
