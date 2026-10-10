# Editing design

Shared editing behavior for the [document](document-editing.md),
[spreadsheet](spreadsheet-editing.md) and [plain-text](txt-editing.md) views.

## The pieces

- `html::translate` and `HtmlConfig::editable` emit editing state, element
  addresses and the format's editor script.
- `frontend/editing.js` owns `odr.editing`: mode, operation log, undo/redo,
  refusal codes and host callbacks. Format editors attach to it.
- `Document::edit` replays version 2 envelopes. Document operations address
  runs and paragraphs; spreadsheet operations address sheets and coordinates.
  `TextFile::edit` handles plain text.
- ODF and OOXML adapters apply document edits. CSV supports cell values and
  row/column insertion and deletion.
- `back_translate` replays an envelope onto a source file and saves it.

## Decisions

### 1. Fat browser, replay on save

The browser applies edits locally and records operations. On save, C++
replays the coalesced log onto the source document. No per-keystroke round
trip is required; JavaScript and C++ must implement the same op semantics.

**Why:** a live C++ model needs incremental rendering and a round trip per
edit. Replay works offline and fits the one-shot `translate` and
`back_translate` pipeline.

### 2. JSON over the WebView message bridge

`HtmlConfig::host_message_handler` names a function from `window`, such as
`webkit.messageHandlers.odr.postMessage` or `odrHost.postMessage`.
`host-bridge.js` calls it on its parent object with a JSON string
`{type, detail}` for each `odr.on*` callback. `type` drops the `on` prefix;
`detail` carries the arguments, including both arguments of `onError` and
`onZoomChange`.

Do not name an Android interface `odr`; the page uses that namespace. The
bridge script runs last, and hosts can replace its callbacks afterwards.
The CLI reads the same operation envelope from a file.

**Why:** a localhost server has a port and a lifecycle. One script in the
library replaces a script per app, which would drift. A string is the one
argument both platforms take, because an `addJavascriptInterface` method
takes no object.

### 3. Record operations, do not compute a diff

The wire payload is a coalesced operation log. Undo data stays in the browser.

**Why:** a diff is ambiguous (edited, or deleted and reinserted?). An op
replays with no interpretation step.

### 4. Address by stable element id, not by path

`data-odr-id` carries an `ElementIdentifier`, resolved by
`Document::element_by_id`. IDs remain stable for one translate/edit/save
session. New elements append to the registry; removed IDs are never reused.

**Why:** a path is positional, so an insert or a delete shifts every later
path. An id does not move.

### 5. The schema addresses locations and ranges, not only elements

Inserts name an anchor. Wire operations use whole runs and strings, with no
character offsets. The browser splits a partially selected run before
formatting it. See [document editing](document-editing.md).

### 6. The payload is one-way; undo lives in the browser

The browser retains inverse data for undo/redo. The coalesced payload sent to
C++ is not invertible and does not implement persistent change tracking.

**Why:** C++ never replays backward, because the user still has the original
file. An invertible removal would carry the removed payload (an image, a
table) across the bridge for nothing. Change tracking belongs to
`<text:tracked-changes>`, `w:ins` and `w:del`.

### 7. C++ validates each operation during replay

Replay accepts version 2 only and validates every operation. A failure leaves
earlier operations applied; there is no rollback. Browser checks provide
feedback but do not replace replay validation.

Saving is separate. Path saves write a temporary file beside the destination
and replace it after a successful close. Before replacement, `release_source`
moves a ZIP source reading from the destination onto a private copy for lazy
resource reads and later saves. Only the saving document's source is moved;
other open documents are not protected. Hard links keep the old bytes.
Stream saves may leave partial output on failure.

### 8. The browser editor is hand-rolled and model-first

The editor intercepts `beforeinput` and applies supported changes itself.
For the document editor, addressed DOM runs and paragraphs are the model;
there is no parallel tree. IME composition is observed through `input`
because it cannot be cancelled.

**Why:** native `contenteditable` output differs per browser and does not
map onto the element model.

### 9. One mode for every format; a format brings only its editor

`odr.editing` exposes mode controls (`enable`, `disable`, `isEnabled`,
`isEditable`), operations (`getOperations`, `undo`, `redo`, `committed`), and
formatting (`format`, `toggle`). `odr.onEditChange` reports `dirty`, `canUndo`
and `canRedo`; `odr.onSelectionChange` reports selection state.

Editors call `odr.editing.attach({name, enable, disable, operations, undo,
redo, ...})`; only `operations` is required. Operations are concatenated,
and undo tries each editor in turn. Refusals use `odr.onEditRefused` with
`odr::ErrorCode` values exposed as `odr.errorCodes`.

**Why:** a host wires the mode once for every format. A cell is edited by an
overlay and a paragraph by a caret, so the editors share the log and nothing
else.

### 10. The page states its editing frame on `<body>`

| Attribute | Meaning |
|---|---|
| `data-odr-editable="true" \| "readOnly"` | whether `enable()` can succeed |
| `data-odr-keyboard="navigation shortcuts"` | enabled key classes (decision 12) |
| `data-odr-editing-scope` | scope (decision 14) |
| `data-odr-sheet-edit-on-click="true" \| "false"` | initial `editOnClick`; absent uses the pointer type |

`data-odr-id` addresses editable elements. `odr-locked` and
`data-odr-lock="<reason>"` mark refused regions. Explicit `readOnly`
distinguishes a non-editable page from older output without these attributes.

### 11. `HtmlConfig::editable` writes the scaffolding; JavaScript turns the mode on

`editable` controls element addresses and format editor scripts. Editing
starts disabled; hosts call `odr.editing.enable()` after wiring callbacks.

`editing.js` is included on read-only document views too: `isEditable()` is
false, `enable()` refuses with `readOnly`, and `getOperations()` returns an
empty envelope. `odr.generateDiff()` remains available. Keyboard policy is
emitted on every document view, including read-only spreadsheets.

**Why:** `data-odr-id` on every run is the expensive half of the render, and
it cannot be added later. A read-only sheet still takes Escape to clear a
pinned cell.

### 12. A host keeps the keys it needs

| Class | Keys | Config |
|---|---|---|
| Active editor | Escape, Enter, Tab while editing | always handled |
| Navigation | arrows, Tab, Escape, editor activation keys | `HtmlConfig::keyboard_navigation` |
| Shortcuts | ctrl/cmd+Z, ctrl/cmd+Y, ctrl/cmd+shift+Z | `HtmlConfig::keyboard_shortcuts` |

Both options default to on. Capture-phase handlers use `preventDefault`, so
hosts with their own bindings should disable the corresponding class.

**Why:** navigation collides with a host that moves a selection, the chords
with a host that owns undo, so the two are separate. The scripts are static
files, so a config value reaches them only through the markup.

### 13. One editable view, and every edit it cannot replay is refused

The document editor enables `contenteditable` on the body and restricts
`beforeinput` to supported operations. One input event is one undo step.

| Edit | Behavior |
|---|---|
| typing or deletion across runs and paragraphs | apply |
| Enter | split paragraph |
| Backspace at paragraph start | merge with previous paragraph |
| plain-text paste | insert text and split paragraphs at newlines |
| inline formatting under `document` scope | split partial runs and restyle |
| paragraph alignment | align selected paragraphs |
| composition, autocorrect, dictation | record each `input` |
| soft line break | refuse with `newLine` |
| selection over a picture | apply using the frame address |
| selection over a text box or table, or outside runs | refuse with `range` |
| unsupported list, rule, drop or other edit | refuse with `unsupportedEdit` |

Scripted `document.execCommand` can bypass the gate. A run removed by browser
editing raises `unnameableEdit`.

**Why:** `contenteditable` per run makes every run a wall the caret cannot
cross. `beforeinput` fires before the browser changes anything and is
cancelable. The list is closed, because an edit we cannot replay costs the
reader the document.

### 14. The scope is host policy, and the page refuses past it

`Document::is_editable` reports engine support. `HtmlConfig::editing_scope`
sets host policy through `data-odr-editing-scope`; violations report
`ErrorCode::edit_out_of_scope` (1010, `outOfScope`).

| Scope | Accepted edits |
|---|---|
| `document` (default) | decision 13 |
| `paragraph` | edits and host formatting confined to one paragraph; no formatting chords |

The editor reads the attribute per edit, so a host can change scope without
rendering again.

**Why a paragraph and not a run:** Word splits runs by revision session, so a
limit at a run would stop in the middle of uniform text.

## Open work

- A shared replay conformance corpus. Browser checks currently assert emitted
  logs; `document_edit_test.cpp` tests replay separately.
- Per-element refusal reasons; `element_is_editable` currently returns a bool.
- PDF annotation uses `odr.annotation`, `PdfFile::annotate` and
  `odr.onAnnotationChange`; PDF pages do not include `editing.js`.
