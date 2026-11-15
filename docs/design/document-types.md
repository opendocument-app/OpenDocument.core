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
- `internal::abstract::Document` does not change. The split is in the public
  handles only.

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

## Migration

**Minor release.** Add the handles, `as_*` and `WrongDocumentType`. The
sheet, slide and text-box methods are not in a release yet (v7.8.0 has none
of them), so they move outright. The formula methods shipped: they stay on
`Document`, `[[deprecated]]`, and forward to `Spreadsheet` without the type
check, so they keep answering what they answered for every type. Every caller
in this repository moves to the handles: the renderer, the CLI, the bindings
and the tests. CI builds with `-Werror`, so a call left behind fails it.

The bindings keep their surface in this step. A binding method that called a
moved method now calls it through `as_*`, so on a mismatched document it fails
with `WrongDocumentType` where it failed with `UnsupportedOperation`. That is
a changed behaviour, not a changed API.

**Next major.** Delete the deprecated formula methods from `Document`.

## Open work

- The binding surfaces: `Spreadsheet` and the others in python, java, swift
  and npm, with the methods moved off their `Document`.
- Typed traversal on the handles, such as `Spreadsheet::sheets()`, in place of
  a walk of `root_element()`.
