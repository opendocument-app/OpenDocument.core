# Editing design

Status: landed. Decisions 9 to 12 are the frame every editor shares. Each
editor has its own document: [`spreadsheet-editing.md`](spreadsheet-editing.md)
for the sheet view, [`document-editing.md`](document-editing.md) for the
document view, and [`txt-editing.md`](txt-editing.md) for the plain-text view.

The principle comes from [`README.md`](README.md): a save depends on the
changes, not on the internal representation of the document.

## Problem

The apps render a document to HTML and show it in a WebView. The user edits
what they see, and the edit has to persist into the original file. There is
no live connection between the browser and C++.

## The pieces

- `html::translate` with `HtmlConfig::editable` writes the scaffolding: the
  page-level state on `<body>`, `data-odr-id` on every editable run and
  paragraph, and the scripts that carry the mode.
- `frontend/editing.js` owns `odr.editing`: the mode, the refusals, the log a
  save reads, and the callbacks a host wires. Every document view has it. An
  editor attaches to it: `frontend/sheet-editing.js`, `frontend/document.js`
  or `frontend/text.js`.
- `Document::edit` replays the envelope (`"version": 2`). The ops are
  `setCell` for a sheet, and `setText`, `setTextStyle`, `insertText`,
  `removeElement`, `splitParagraph`, `mergeParagraph` and `insertParagraph`
  for a document. `TextFile::edit` is the plain-text counterpart, because a
  `.txt` is not a document.
- The write side lives in `odf_document.cpp`, `ooxml_text_document.cpp`,
  `ooxml_presentation_document.cpp` and `ooxml_spreadsheet_document.cpp`.
- The `back_translate` CLI replays an envelope onto a source document and
  saves it.
- Inline formatting is `setTextStyle` on the wire and `odr.editing.format` in
  the page. [`document-editing.md`](document-editing.md#inline-formatting)
  holds its decisions.

## Decisions

### 1. Fat browser, replay on save

The browser owns the document model for the duration of an editing session and
records an operation log. On save, C++ replays the coalesced log onto the
source document and writes the output. C++ is not consulted per keystroke.

Why: a live C++ model needs incremental HTML rendering and a round trip per
edit. Replay works offline and fits the one-shot `translate` and
`back_translate` pipeline. The cost is that op semantics exist twice.

### 2. JSON over the WebView message bridge

The payload is JSON. The channel is the platform bridge
(`WKScriptMessageHandler` on iOS, `addJavascriptInterface` on Android). In the
CLI the same JSON is a file. A localhost HTTP server would have a port and a
lifecycle, which fights the one-shot pipeline.

### 3. Record operations, do not compute a diff

The wire format is an operation log, not a state-to-state diff.

Why: a diff compares two states and is ambiguous (edited, or deleted and
reinserted?). An op is unambiguous and replays with no interpretation step.

### 4. Address by stable element id, not by path

An op names an element by its `ElementIdentifier`, written as `data-odr-id`
and resolved at replay by `Document::element_by_id`. `DocumentPath` is not on
the edit path.

Why: a path is positional, so an insert or a delete shifts every later path.
An id does not move. An id only has to hold for one session of
`translate`, `edit` and `save`, so two rules are enough: `create_element`
appends, and `unlink_child` leaves the slot taken. No live id is renumbered.

### 5. The schema addresses locations and ranges, not only elements

An insert carries an anchor, because no element exists at the place yet. A
formatting op carries a character range on a run. The schema is built on
`id + anchor + offset`, and a path never enters it.

### 6. The payload is one-way; undo lives in the browser

Undo and redo run in the browser over its in-memory log, where the inverse
data is local. The payload sent to C++ is coalesced and is not invertible.

Why: C++ never replays backward, because the user still has the original
file. An invertible removal would carry the removed payload (an image, a
table) across the bridge for nothing. Persistent change tracking is a
different feature and belongs to `<text:tracked-changes>`, `w:ins` and `w:del`.

### 7. C++ replay is the validator; saves are atomic

The browser refuses edits for UX only. Replay validates again and throws on an
op it cannot apply. It applies to an in-memory copy and writes only on full
success. The envelope states a `version`, and replay refuses any other.

Open: a conformance corpus of `(base document, op log) → expected saved
result` cases, replayed in C++ as a GoogleTest, is not built. What stands in
for it is that the browser check pages under `test/browser/` assert the log
they emit, and `document_edit_test.cpp` replays the same shapes.

### 8. The browser editor is hand-rolled and model-first

The editor intercepts `beforeinput`, cancels it, and applies the change itself.
The DOM is a projection of the model. No editor framework is used.

Why: native `contenteditable` output differs per browser and does not map onto
the element model. The feature set is small, and a JS dependency is awkward to
carry here. The hard parts are IME composition, which cannot be cancelled, and
edits that cross a paragraph boundary.

### 9. One mode for every format; a format brings only its editor

`odr.editing` is generic and is written by `editing.js` for every document
view. It owns the mode (`enable`, `disable`, `isEnabled`, `isEditable`), the
refusals (`odr.onEditRefused`, with the codes of `odr::ErrorCode` written into
the page as `odr.errorCodes`), the log (`getOperations`, `undo`, `redo`,
`committed`, and the `dirty`, `canUndo` and `canRedo` state that
`odr.onEditChange` reports), the formatting seam (`format`, `toggle`,
`odr.onSelectionChange`), and the keyboard classes of decision 12.

A format's editor calls `odr.editing.attach({name, enable, disable,
operations, undo, redo, ...})`. Only `operations` is required. `getOperations`
concatenates what the attached editors report, and `undo` asks each in turn.

Why: a host wires the mode once for every document it opens, whatever the
format. A cell is edited by an overlay and a paragraph by a caret, so the
editors share the log and nothing else.

### 10. The page states its editing frame on `<body>`

| Attribute | Meaning |
|---|---|
| `data-odr-editable="true" \| "readOnly"` | whether `enable()` can succeed |
| `data-odr-keyboard="navigation shortcuts"` | the key classes the scripts may take (decision 12) |
| `data-odr-editing-scope` | the scope of decision 14 |

Per element the page states only the exceptions: `data-odr-id` addresses an
editable run, and `odr-locked` with `data-odr-lock="<reason>"` marks what
refuses. Everything unmarked is editable.

Why: the frame is a fact about the document, not about one table. `"readOnly"`
is explicit because an absent attribute cannot be told from a page written by
an older library.

### 11. `HtmlConfig::editable` writes the scaffolding; JavaScript turns the mode on

`editable` decides whether the render offers editing. True writes the
per-element addresses, the page-level state and the editor script of the
format. False writes none of them, because `data-odr-id` on every run is the
expensive half and cannot be added later.

The mode starts off, and a host calls `odr.editing.enable()` where it wires
its callbacks. `editing.js` is written either way, so `odr.editing` and
`odr.generateDiff()` exist on a read-only page: `isEditable()` is false,
`enable()` refuses with `readOnly`, and `getOperations()` hands out an empty
envelope. A document that cannot be edited states
`data-odr-editable="readOnly"`, so a host can grey its button.
`data-odr-keyboard` is written on every document view, because a read-only
sheet still takes Escape to clear a pinned cell.

### 12. A host keeps the keys it needs

| Class | Keys | Config |
|---|---|---|
| The open editor's | Escape, Enter, Tab while an editor holds focus | always taken |
| Navigation | the arrows, Tab, Escape, and the keys that open the editor over the selection | `HtmlConfig::keyboard_navigation` |
| Shortcuts | ctrl/cmd+Z, ctrl/cmd+Y, ctrl/cmd+shift+Z | `HtmlConfig::keyboard_shortcuts` |

Both options default to on, and the page states them in `data-odr-keyboard`.

Why: the key handlers call `preventDefault` in the capture phase, so a host
with its own bindings loses them. Navigation collides with a host that moves a
selection, the chords with a host that owns undo, so the two are separate. The
editor's own keys are the only way out of it. The scripts are static files
(`cmake/frontend_assets.cmake`), so a config value reaches them only through
the markup.

### 13. One editable view, and every edit it cannot replay is refused

`enable()` puts `contenteditable` on the body. The editor intercepts
`beforeinput` and takes only the edits it can express as operations:

| The edit | What happens |
|---|---|
| text typed, replaced or deleted, inside one run or across runs and paragraphs | taken |
| Enter | taken: the paragraph splits at the caret |
| Backspace at the start of a paragraph | taken: the paragraph merges into the one before |
| a paste of plain text | taken: each line after the first opens a paragraph |
| a mark (ctrl/cmd+B, I, U, or `odr.editing.format`) under scope `document` | taken: a run covered in part is cut, and the covered runs are restyled |
| a composition (CJK, autocorrect, dictation) | let through, and each change recorded after its `input` |
| a soft line break (`insertLineBreak`) | refused, reason `newLine` |
| a range over a picture | taken: the frame carries an address |
| a range over a text box or a table | refused, reason `range` |
| an edit outside every run | refused, reason `range` |
| anything else (a list, a rule, a drop) | refused, reason `unsupportedEdit` |

Why one view: `contenteditable` per run makes every run a wall the caret
cannot cross. Why `beforeinput`: it fires before the browser changes anything,
states the `inputType` and the target ranges, and is cancelable. The whitelist
is closed, because an edit we cannot replay costs the reader the document.
The address is the whole guard: an edit is allowed because it lands inside a
`x-s[data-odr-id]` run and reaches over nothing but runs. The editor owns the
edit and its undo, and one `beforeinput` is one step.

Known holes: a scripted `document.execCommand` can bypass the gate, and a
composition cannot be cancelled, so the editor records the run's text after
each `input`. A run the browser took out of the page raises `unnameableEdit`.

### 14. The scope is host policy, and the page refuses past it

`Document::is_editable` is an engine fact. What a host offers is
`HtmlConfig::editing_scope`, written as `data-odr-editing-scope`, with
`ErrorCode::edit_out_of_scope` (1010, `outOfScope`) as the signal back.

| Scope | What the document editor takes |
|---|---|
| `document` (default) | everything in decision 13 |
| `paragraph` | an edit that starts and ends in one paragraph, and no formatting |

Why a paragraph and not a run: Word splits runs by revision session, so a wall
at a run would stand in the middle of uniform text. The editor reads the
attribute per edit, so a host widens the scope with no second render.

## Open work

- The conformance corpus of decision 7.
- `element_is_editable` answers one bool, and ooxml text answers `true` for
  everything. A refusal needs a reason for the host. Whether the adapter hook
  grows into `element_edit_lock(id) -> reason` is undecided.
- The pdf annotator is the one editor outside the mode: `odr.annotation` is
  its own API, `PdfFile::annotate` its own write path, and it reports on
  `odr.onAnnotationChange`. A pdf page does not carry `editing.js`.
