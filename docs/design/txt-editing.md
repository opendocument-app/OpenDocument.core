# Plain-text editing design

The editor of the plain-text view, `frontend/text.js`, on the mode frame of
[`editing.md`](editing.md). Its siblings are
[`document-editing.md`](document-editing.md) for the document view and
[`spreadsheet-editing.md`](spreadsheet-editing.md) for the sheet view.

Status: landed.

## What makes it different

A `.txt` is a `TextFile`, not a `DocumentFile`. It has no element tree, so no
runs, paragraphs, ids, registry or adapters. `Document::edit` and
`Document::save` do not apply to it. It has a flat string of lines, and
`text.js` gates its own `beforeinput` and records an inverse per change for
undo.

## Decisions

### 1. It attaches to the mode rather than keeping its own API

`text.js` is an editor on `odr.editing` like every format's. The view
(`html/text_file.cpp`) writes `editing.js` and the page-level
`data-odr-editable` and `data-odr-keyboard`.

**Why:** decision 9 of [`editing.md`](editing.md). A host wires the mode once
for every file, so `odr.generateDiff()`, `undo`, `redo`, `canUndo` and
`canRedo` answer on a `.txt` page too.

### 2. One operation, `setContent {text}`, carrying the whole file

```json
{"version": 2, "ops": [{"op": "setContent", "text": "…"}]}
```

Coalescing makes the log one operation however long the session runs. An
envelope with no operation writes the file back as it was.

**Why not a line at a time:** a line number is a path, and decision 1 of
[`document-editing.md`](document-editing.md) refuses positional addressing,
because an inserted line shifts every line after it. A plain file has no
registry to hang an id on. Decision 2 of the same document refuses offsets
inside a line, because JavaScript counts UTF-16 code units and `std::string`
counts bytes.

**Cost:** the whole file crosses the bridge on every save. `TextFile::save`
writes the complete bytes anyway. If a large file makes this bite, one
`replaceLines {from, to, text}` computed as a single diff hunk at emit time
stays one operation on the file as it was, with no schema change.

### 3. The write path has `Document`'s names

```cpp
[[nodiscard]] bool TextFile::is_savable() const noexcept;
void TextFile::edit(std::string_view operations,
                    const Logger & = Logger::null()) const;
void TextFile::save(std::ostream &out) const;
```

`edit` keeps the edit in the file, so every handle over it and the next render
see it. `save` writes the text.

**Why:** decision 9 of [`editing.md`](editing.md) again. A host saves a `.txt`
with the calls it makes for a document. `TextFile::write_edited` stays,
deprecated. A `TextFile` handle is therefore no longer immutable, like a
document whose edit lives in its shared tree.

### 4. What it writes is UTF-8, whatever the source was

`is_savable()` is false for a text file that is not `FileType::text_file`
(json), and for an encoding we cannot decode, because the view hands those
bytes to the browser as they are. Everything else saves as UTF-8.

**Why:** `encoding/transcode.hpp` has `to_utf8` and nothing in the other
direction. A Shift-JIS file opens, edits, saves, and is UTF-8 afterwards. The
API doc on `TextFile::edit` says so, so a host can warn the reader.

## Open questions

- **`from_utf8`** would let a file round-trip in its own encoding. The tables
  in `encoding/encoding_data` are there to reverse. Open: what to do with a
  character the target encoding cannot represent.
- **Undo granularity.** Typing over a selection is two undo steps, because
  `insertTextAction` calls `removeTextAction` and each pushes its own change.
  The document editor makes it one step.
- **The pdf annotator** is the one editor outside the mode: `odr.annotation`
  is its own API and `PdfFile::annotate` its own write path. It is a different
  gesture from editing text, so sharing the mode is an open question.
