# Plain-text editing design

The editor of the **plain-text view** — `frontend/text.js` — built on the mode
frame in [`editing.md`](editing.md), and the decisions that are its own. Its
siblings are [`document-editing.md`](document-editing.md) for a document view
and [`spreadsheet-editing.md`](spreadsheet-editing.md) for a sheet.

Status: **landed.** It answers the question `editing.md` carried from the day
the mode was built — whether this view attaches to `odr.editing` or stays the
one editor that answers to nobody. It attaches.

## What makes it different

A `.txt` is a `TextFile`, **not a `DocumentFile`**. There is no element tree
behind it: no runs, no paragraphs, no ids, no registry, and no adapters. So
none of the document operations reach it, `Document::edit` and `Document::save`
do not apply to it, and the whole write side of the engines is somewhere it
cannot go.

What it has instead is a flat string of lines and, in the browser, an editor
that predates all of this — `text.js` had its own `beforeinput` gate and its
own inverse-recording undo before the mode existed.

## Decisions

### 1. It attaches to the mode rather than keeping its own API

`text.js` is an editor on `odr.editing` like every format's. The view writes
`editing.js` and the page-level `data-odr-editable` / `data-odr-keyboard`.

**Why:** decision 9 of [`editing.md`](editing.md) is that a host wires the mode
once for every file it opens and must not learn a second API because of what
the file turned out to be. Decision 11 states that `odr.editing` is on every
view either way and `odr.generateDiff()` never goes missing. Both were true of
every view **except this one**: `text_file.cpp` wrote `text.js` and not
`editing.js`, so a `.txt` page had no mode at all and `odr.generateDiff` came
back `undefined`.

**What it cost:** nothing in the editor. `text.js` already recorded an inverse
per change, so `undo`, `redo`, `canUndo` and `canRedo` answered honestly the
moment they were wired up. It is the only editor of the three that did not need
an undo built for it.

### 2. One operation, `setContent {text}`, carrying the whole file

```json
{"version": 2, "ops": [{"op": "setContent", "text": "…"}]}
```

Coalescing makes the log exactly one operation however long the session runs,
and an envelope stating none writes the file back as it was.

**Why not a line at a time**, which is the obvious alternative: **a line number
is a path**, and decision 1 of [`document-editing.md`](document-editing.md) is
that an operation must not address by one — inserting a line shifts every line
after it, so a log of more than one structural operation cannot be replayed. A
document escapes that with ids from its registry. A plain file has no registry
to hang an id on, because lines are not elements; they are where the newlines
happen to be. Per-line operations would mean inventing an identity the format
does not have.

**Why not finer:** addressing inside a line wants offsets, and decision 2 of
[`document-editing.md`](document-editing.md) refused those — JavaScript counts
UTF-16 code units and `std::string` counts bytes.

**What it costs:** the whole file crosses the bridge on every save. Less than
it looks, because `write_edited` produces the complete bytes either way, so a
finer log would only be reassembled before writing; the saving would be one
hop. Where it does bite is a large file, and the answer there is **one**
`replaceLines {from, to, text}` computed as a single diff hunk at emit time —
still one operation, still applied to the file as it was, so still nothing
positional to go stale. That needs no schema change to reach.

### 3. The write path is `PdfFile::annotate`'s shape, not `Document::save`'s

```cpp
[[nodiscard]] bool TextFile::is_savable() const noexcept;
void TextFile::write_edited(std::string_view operations, std::ostream &out,
                            const Logger & = Logger::null()) const;
```

One call taking the envelope and a stream, leaving the handle unchanged.

**Why:** a `TextFile` is an immutable handle over bytes, and there is no
document to mutate and later serialise. `PdfFile::annotate` is the precedent —
the other non-document file with a write path of its own — and the shape suits
for the same reason: nothing is held between the edit and the write.

### 4. What it writes is UTF-8, whatever the source was

`is_savable()` refuses only an encoding we cannot **decode**: the view hands
those bytes to the browser as they are, so what comes back could not be put
back. Everything else saves — and saves as UTF-8.

**Why:** `encoding/transcode.hpp` has `to_utf8` and nothing in the other
direction. A Shift-JIS file therefore opens, edits, saves, and is UTF-8
afterwards.

**Why it is stated rather than hidden:** the API doc says so, so a host can
warn the reader. Silently changing a property of someone's file that nothing
told them about is the failure mode worth avoiding here, more than the change
itself.

## Open questions

- **`from_utf8`** would let a file round-trip in its own encoding and close
  decision 4. The tables in `encoding/encoding_data` are there to reverse, so
  the work is real but bounded — except for the question it brings with it:
  what to do with a character the target encoding has no room for.
- **Undo granularity.** Typing over a selection is two steps, because
  `insertTextAction` calls `removeTextAction` and each pushes its own change.
  The document editor is one `beforeinput`, one step. It predates this work and
  is only visible now that a host can drive undo.
- **The pdf annotator** is now the one editor that answers to nobody:
  `odr.annotation` is its own API and `PdfFile::annotate` its own write path.
  That is a different gesture from editing text, so whether it should share the
  mode is a real question rather than an oversight.
