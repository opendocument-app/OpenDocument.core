# Document types design

`Document` carries methods that only one or two document types can answer:
sheets and formulas, slides, text boxes. A caller learns that a method does not
fit only when it throws `UnsupportedOperation`. The fix: one handle per
document type, holding the methods of that type. `Document` keeps what every
type shares.

## The handles

```cpp
class Document {                                 // no longer final
public:
  [[nodiscard]] TextDocument as_text_document() const;
  [[nodiscard]] Spreadsheet as_spreadsheet() const;
  [[nodiscard]] Presentation as_presentation() const;
  [[nodiscard]] Drawing as_drawing() const;
  // the methods every type shares
protected:
  std::shared_ptr<internal::abstract::Document> m_impl;
};

class TextDocument final : public Document {};
class Spreadsheet final : public Document { /* sheets, formulas */ };
class Presentation final : public Document { /* slides, text boxes */ };
class Drawing final : public Document { /* text boxes */ };
```

- `as_*` follows `DecodedFile::as_*_file`: it checks `document_type()` and
  throws `WrongDocumentType` (a new `ErrorCode`, appended) on a mismatch. There
  is no `is_*`; `document_type()` answers that.
- A subclass adds no state. Copying one into a `Document` slices nothing that
  matters, and every handle shares the one document, as before.
- The internal interface splits the same way, into document adapters. See
  [The internal side](#the-internal-side).

## Which method goes where

| Handle | Methods |
|--------|---------|
| `Document` | `is_editable`, `is_savable`, `save*`, `file_type`, `document_type`, `locale`, `edit`, `root_element`, `element_by_id`, `remove`, `insert_text_before/after`, `append_text`, `split_paragraph`, `merge_paragraph_with_next`, `insert_paragraph_after`, `as_filesystem`, `impl` |
| `TextDocument` | none yet |
| `Spreadsheet` | `insert_sheet`, `move_sheet`, `dependents` (both), `unresolved_formulas`, `recalculate` |
| `Presentation` | `slide_layouts`, `insert_slide`, `move_slide`, `insert_text_box` |
| `Drawing` | `insert_text_box` |

The paragraph and text methods stay on `Document`: a cell, a slide and a frame
hold paragraphs too. `TextDocument` is there so that every `DocumentType`
has a handle; it gets methods when a text-only one appears (page setup,
sections).

`insert_text_box` is declared on both `Presentation` and `Drawing`, with one
shared implementation.

**Why single inheritance, one level deep:** the only method two types share is
`insert_text_box`. A `Presentation : Drawing` chain would claim that a
presentation has drawing pages; a shared mixin base makes a diamond over
`m_impl` for one method. A flat hierarchy also maps one to one onto every
binding: JNI, Swift and embind mirror a single chain easily and multiple
inheritance badly. A second shared method is declared twice as well.

**Why subclasses, not sibling handles:** a `Spreadsheet` is passed wherever a
`Document` is taken, with no unwrapping to save it or walk its elements.

## The internal side

`internal::abstract::Document` gives a nullable adapter per type, as
`ElementAdapter` gives one per element kind (`sheet_adapter(id)`):

```cpp
class Document {
  // file type, document type, save, root, element_adapter, ...
  virtual const SpreadsheetAdapter *spreadsheet_adapter() const = 0;
  virtual const PresentationAdapter *presentation_adapter() const = 0;
  virtual const DrawingAdapter *drawing_adapter() const = 0;
};
```

| Adapter | Methods |
|---------|---------|
| `SpreadsheetAdapter` | `spreadsheet_edits`, `_formula_settings`, `_formula_names`, `_external_sheets`, `spreadsheet_insert_sheet`, `_move_sheet` |
| `PresentationAdapter` | `presentation_slide_layouts`, `_insert_slide`, `_move_slide`, `_text_box_layout`, `_insert_text_box` |
| `DrawingAdapter` | `drawing_text_box_layout`, `drawing_insert_text_box` |

- An adapter is null for another type, and for an engine that models none
  of it: `.xls`, `.csv`, `.numbers`, `.ppt` and `.key` give none. The public
  handle reads null as nothing to answer, and a write as
  `UnsupportedOperation`, which is what the old defaults did.
- The engine's one adapter class implements its adapters. The prefix keeps
  their names apart, as it does for the element adapters, so odf implements
  all three and both text-box inserts forward to one function.
- Every engine's `create_adapters` returns `EngineAdapters`: the element
  adapter and whichever document adapters its class implements.
  `common::Document::set_adapters_` takes it and keeps only the document
  adapter that matches the document type, so no engine repeats that check.
  The adapters are private to `common::Document`, so nothing else installs
  one.
- No `TextDocumentAdapter` until a text-only method needs one.

An internal hook sits where its public method sits: a method of a handle
goes to that type's adapter, a method of `Document` stays with the element
adapters.

- **`Document`**: what every type has. The formula queries leave it.
- **`ElementAdapter`**: navigation, `element_remove` and
  `element_append_text`. `element_move` and the sheet, slide and text-box
  inserts leave it.
- **Element adapters** (`SheetAdapter`, `ParagraphAdapter`, `TextAdapter`):
  edits of one element, as before: rows, cells, a paragraph split, a run.
- **Removal** stays `element_remove` for every kind, as `Document::remove`
  takes any element. A removal names an element the engine holds, and the
  engine branches on its kind: a sheet's references become `#REF!`, a pptx
  slide takes its notes and its show entries. An insert names no element,
  only an index, so it is the document adapter's. `Document::remove` keeps
  the one check that is the public API's, that a workbook keeps a sheet.

What a recalculation reads off the edits, the cached dependency graph and
the cells written since the last one, is a `SheetEdits` the spreadsheet
adapter owns and `spreadsheet_edits` hands out. The adapter notes each write
and move there, so `common::Document` keeps no spreadsheet state.

## Migration

**Minor release.** Add the handles, `as_*` and `WrongDocumentType`. The
sheet, slide and text-box methods are not in a release yet (v7.8.0 has none
of them), so they move outright. The formula methods shipped: they stay on
`Document`, `[[deprecated]]`, and forward to `Spreadsheet` without the type
check, so they keep answering what they answered for every type. Every caller
in this repository moves to the handles: the renderer, the CLI, the bindings
and the tests. CI builds with `-Werror`, so a call left behind fails it.

**Bindings, before the same release.** Python, java, swift and npm get the
four handles as subclasses of their `Document`, and `as_*` on it. The sheet,
slide and text-box methods are unreleased in every binding too, so they move
outright. `recalculate` shipped on every binding's `Document` and stays there,
where a spreadsheet handle inherits it; it answers empty for another type.
npm's sheet-index methods (`insertRows`, `setCellStyle` and the others)
shipped on `Document` and stay. A handle in npm shares the session, so closing
either closes both.

**Next major.** Delete the deprecated formula methods from `Document`.

## Open work

- With the next major, `recalculate` moves off every binding's `Document`
  onto `Spreadsheet`, and npm's sheet-index methods follow it.
- Typed traversal on the handles, such as `Spreadsheet::sheets()`, in place of
  a walk of `root_element()`.
