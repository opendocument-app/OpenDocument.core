# Editing design

Status: **landed.** This records the architecture we chose for in-browser
editing, the alternatives we weighed, and *why* we took each decision.
Decisions 9 to 12 are the frame every format shares.

One document per editor, each holding its own decisions:
[`spreadsheet-editing.md`](spreadsheet-editing.md) for the sheet view,
[`document-editing.md`](document-editing.md) for the document view, and
[`txt-editing.md`](txt-editing.md) for the plain-text one.

This builds on the existing principle in [`README.md`](README.md):

> saving should not depend on our internal representation of the document but
> only the changes

## Problem

We render documents to HTML and display them in a WebView (droid / ios), and we
want the user to edit what they see and have it persist back into the original
file — with no live connection between the browser and C++.

## The pieces

- `html::translate(..., config.editable)` writes the scaffolding: the page-level
  state on `<body>`, `data-odr-id` on every editable run and paragraph, and the
  scripts that carry the mode.
- `frontend/editing.js` owns `odr.editing` — the mode, the refusals, the log a
  save reads and the callbacks a host wires. **Every view has it**, and each
  format's editor attaches one editor to it (decision 9):
  `frontend/sheet-editing.js`, `frontend/document.js`, `frontend/text.js`.
- `Document::edit` replays the envelope: `setCell` for a sheet, and `setText`,
  `insertText`, `removeElement`, `splitParagraph`, `mergeParagraph` and
  `insertParagraph` for a document. `TextFile::write_edited` is the plain-text
  counterpart, since a `.txt` is not a document.
- `back_translate` CLI replays an envelope onto a source document and `save`s it.

What is **not** done is inline formatting — bold, italic, underline, highlight.
Decision 5 of [`document-editing.md`](document-editing.md) says why the schema
takes it without changing.

## Decisions

### 1. Architecture: fat browser, replay-on-save (not a live C++ model)

The browser owns the authoritative document model *for the duration of an editing
session*. It records an operation log as the user edits. On **save**, the coalesced
log is handed to C++, which replays it onto the source document and writes the
output. C++ is not consulted per keystroke.

**Alternative considered — fat C++, thin browser view (B):** C++ owns a live
mutable model; the browser sends each op over the WebView bridge and re-renders
the affected fragment from odr output; save is a flush. This has a single source
of truth (no drift) and centralised validation.

**Why A:** B requires incremental HTML re-rendering from odr and a chatty
round-trip per edit; A works fully offline and fits the existing one-shot
`translate` / `back_translate` pipeline. The write-side adapter work (see plan) is
identical either way — B's only advantage is deleting the *drift* failure mode.
We accept A's duplicated op-semantics (JS + C++) and mitigate drift with a shared
conformance corpus (decision 7). The duplication is a smaller price than
continuous re-rendering.

### 2. Transport: JSON over the WebView message bridge (not HTTP)

The payload is JSON. The channel is the platform WebView bridge
(`WKScriptMessageHandler` on iOS, `addJavascriptInterface` on Android); on
desktop/CLI the same JSON is just a file, as `back_translate` already does.

**Why not HTTP:** it forces odr to run as a live localhost server with a port and
lifecycle, which fights the one-shot pipeline and is painful on mobile. The
message bridge *is* the comm line and needs no server.

### 3. Record operations, do not compute a diff

The wire format is an **operation log** recorded as the user acts
(`insertText`, `deleteRange`, `toggleMark`, `splitParagraph`, `insertParagraph`,
…), not a state-to-state diff.

**Why:** a diff is derived by comparing two document states — ambiguous (edited vs
deleted-and-reinserted?) and it reimplements tree-diff. Ops are unambiguous and
directly replayable with no interpretation step. The current `modifiedText` map is
a degenerate diff that only survives because it does in-place text swaps.

### 4. Address by stable element id, not by path

Ops reference `ElementIdentifier`, emitted into the HTML (e.g. `data-odr-id`) and
re-resolved by C++ at replay. `DocumentPath` is dropped from the edit path.

**Why:** `DocumentPath` is *positional*; any insert/delete shifts sibling paths,
so a path recorded early in a session goes stale. Ids don't move, so we don't even
need to freeze a path→id resolution up front. A path buys nothing over an id for
elements that exist.

**Consequence — id stability requirement.** In architecture A the id only needs to
be **session-stable** (one `translate → edit → save`), *not* persistent across
reload: after save we re-`translate` and the browser rebuilds its model from fresh
ids. Each engine's `ElementRegistry` is a flat vector with `id = index + 1`, so
"session-stable" reduces to two rules that edit ops must obey:

- **Append-only** — new elements take new indices at the end; never renumber
  existing ones.
- **Tombstone deletes** — a deleted element's slot stays reserved (marked dead),
  never compacted or reused within the session.

This is much cheaper than global stable identity, and it is exactly the discipline
that keeps replay safe.

### 5. Op schema addresses locations and ranges, not just elements

- A bare id can't express an **insertion location** (a place where no element
  exists yet). Insert ops carry an anchor: `(parent-id, after-id | before-id)` or
  `(parent-id, index)`.
- Inline formatting addresses a **character range**: `(id, start-offset, length)`.

So the schema is built around `id + anchor + offset` from op #1. Paths never enter.

### 6. The persisted payload is one-way and non-invertible; symmetry lives only in the browser

Undo/redo runs entirely in the browser over its in-memory op log, where the
inverse data is already local and cheap (the image is displayed, the removed
subtree is in the DOM to snapshot). The payload sent to C++ is one-way — "apply
net changes and save" — and is **coalesced** first (add-then-delete never reaches
C++).

**Why not a symmetric, git-style diff:** we never replay backward in C++ — a user
who wants the original still has the original file. Making removal ops invertible
would force carrying the removed payload (images, whole tables) across the bridge
for no benefit. Symmetry is valuable exactly where the data is already local (the
browser) and useless where it is expensive (the wire / C++).

*Future note:* persistent, reopen-surviving change tracking is a different feature
and would use the format-native mechanisms — ODF `<text:tracked-changes>`, OOXML
`w:ins` / `w:del` — not a symmetric JSON diff. Out of scope here.

### 7. C++ replay is the authoritative validator; saves are atomic; guard against drift

- The browser policing valid edits is UX only. C++ **re-validates on replay and
  fails fast** (repo convention: throw, don't degrade) — it never trusts the log
  to be applyable.
- Replay applies to an in-memory copy and writes only on full success;
  `back_translate` already saves to a separate output path.
- The log is stamped with a document + odr-model-version identifier so a stale log
  replayed against a changed model is rejected, not misapplied.
- **Conformance corpus** to catch JS/C++ op-semantics drift (A's main risk): a set
  of `(base document, op log) → expected saved result` cases replayed in C++ as a
  GoogleTest, ideally cross-checked against the JS model producing HTML that
  matches a fresh `translate` of the saved document. Stand this up alongside the
  first non-text op.

### 8. Browser editor: hand-rolled, model-first (not contenteditable-diffing, not a framework)

The browser keeps a structured model mirroring odr's element tree (keyed by id).
Edits intercept `beforeinput`, `preventDefault`, mutate the model, and re-render;
the DOM is a *projection* of the model, never the source of truth. We build this
ourselves rather than adopting ProseMirror/Lexical.

**Why not diff contenteditable:** native contenteditable emits wildly inconsistent
DOM across browsers (`<b>` vs `<strong>` vs inline style, wrapper divs, `<br>` vs
`<p>`), which won't map onto odr's element model.

**Why build not buy:** the constrained feature set is a few hundred lines for text
and a bit more for documents; JS dependencies are awkward to carry inside this
project, and hand-rolling gives full control over the model↔op mapping.

**Known hard parts (budget here, not on marks):**

- **IME / composition** in mobile WebViews. You cannot `preventDefault` during an
  active composition (CJK, autocorrect, swipe-type, dictation); Android WebView
  also reports incomplete `beforeinput`. The loop must let composition complete
  (`compositionstart`/`compositionend`) and *reconcile* into the model. Test
  against a real Android IME early.
- **Cross-block selection and block splitting** — split-paragraph,
  merge-on-backspace-at-boundary, delete across paragraphs. This is where the real
  complexity lives, not inline marks.

### 9. Editing is one browser mode for every format; a format brings only its editor

`odr.editing` is the mode, and it is generic. It is written by
`frontend/editing.js` for every document view and it knows nothing about cells,
runs or paragraphs. It owns:

- the **mode** — `enable()`, `disable()`, `isEnabled()`, `isEditable()`;
- the **refusals** — the repeat suppression, the outline a refused element
  gets, and `odr.onEditRefused`. The codes are `odr::ErrorCode` and the
  renderer writes them into the page as `odr.errorCodes`, so the script holds
  the wording and not the numbers;
- the **log** — `getOperations()`, `undo()`, `redo()`, `committed()`, and the
  `dirty` / `canUndo` / `canRedo` state `odr.onEditChange` reports;
- the **keyboard classes** the page may take (decision 12).

A format's editor is a second script that **attaches** one editor to the mode:

```js
odr.editing.attach({
  name: "sheet",        // what the log's ops belong to
  enable: function () {},   // the mode turned on
  disable: function () {},  // the mode turned off
  operations: function () {}, // the ops this editor would hand a save
  undo: function () {}, // false where this editor has nothing to take back
  redo: function () {},
});
```

`frontend/sheet-editing.js` attaches the cell overlay; `frontend/document.js`
attaches the text runs. Neither states a mode, a code table or a callback of its
own.

**Why:** the mode, the refusal channel and the dirty flag are what a *host*
wires, and a host wires them once for every document it opens — it must not
learn a second API because the file turned out to be a sheet. Before this, all
of it lived in `sheet-editing.js`, so a `.docx` had no `odr.editing` at all and
an app could not grey its edit button without knowing the format first.

**Why an editor per format, rather than one editor over the element tree:** a
cell is edited by an overlay and a paragraph by a caret in the flow. The two
share the log and share nothing else. Decision 8 already said the DOM is a
projection of a model; the projection is what differs per format.

**The log is the editor's until an editor can invert its ops.** `getOperations()`
concatenates what the attached editors report, and `undo()` asks each in turn.
The sheet keeps an op with its inverse beside it, so it answers; the text
skeleton kept a map of changed runs and answered `false`. Every editor answers
now, so the concatenation is what a save reads and nothing else.

### 10. The page states its editing frame at page level, on `<body>`

Three attributes, on the body element of every document view that offers
editing:

| Attribute | Meaning |
|---|---|
| `data-odr-editable="true" \| "readOnly"` | whether `enable()` can succeed at all |
| `data-odr-keyboard="navigation shortcuts"` | which key classes the scripts may take (decision 12) |

Per element the page states only the exceptions: `data-odr-id` addresses an
editable run, and `odr-locked` plus `data-odr-lock="<reason>"` marks what
refuses. Everything unmarked is editable.

**Why page level:** the frame is a fact about the document, not about a table.
`data-odr-editable` sat on the `.odr-sheet` element first, which answered the
question for the one view that had an editor and for no other. A `.docx` view
has no sheet to hang it on.

**Why on `<body>` and not in `<head>`:** `document.body` is one lookup, the body
writer already exists in `html/document.cpp`, and a `<meta>` block would need a
name space of its own for two attributes. The cost is two attributes on one
element per view.

**Why `"readOnly"` rather than an absent attribute:** an absent attribute cannot
be told from a page written by an older library. The page says which of the two
it means.

### 11. `HtmlConfig::editable` writes the scaffolding; only JavaScript turns the mode on

`editable` steers one thing: whether the render **offers** editing. True writes
the per-element addressing, the page-level editable state and the editor script
of the format at hand. False writes none of those.

The mode itself always starts **off**. A host that opens a document to edit it
calls `odr.editing.enable()` at the point it wires its callbacks, which is after
the page has loaded either way.

**`odr.editing` is on every document view either way**, because `editing.js` is
written unconditionally. On a page with no scaffolding it answers
`isEditable() === false`, `enable()` refuses with `readOnly`, and
`getOperations()` hands out an empty envelope. So a host asks the page rather
than tracking what it rendered with, and `odr.generateDiff()` — the name the
apps and the wasm package already call — never goes missing.

What the flag keeps out of a read-only render is what actually costs: the
`data-odr-id` attribute on every editable run, the lock class on every locked
cell, and the editor script (`document.js`, `sheet-editing.js`). The mode script
itself is small and buys the host one API for every format.

**Why not let it steer the default state of the mode:** it would be a second
meaning on one flag, and it buys a host nothing. A host assigns
`odr.onEditRefused` and friends on the load event (decision 7 in
[`spreadsheet-editing.md`](spreadsheet-editing.md)), so `enable()` costs it one
more line at a point it already has. An attribute that opens the view in edit
mode would be the only way to skip that line, and nothing needs it.

**Why not drop the flag and always write the scaffolding:** a read-only host
would carry the editor's script bytes and an attribute on every editable run for
nothing. `data-odr-id` on the runs of a text document is the expensive half,
and it cannot be added later — the mode can only turn on if the addresses are
already in the page.

**Why the keyboard classes are stated either way:** they are not an editing
fact. A read-only sheet has a pinned cell, and Escape clears it
(`spreadsheet.js`), so `data-odr-keyboard` is written on every document view.

**Why it may still change the markup, when decision 3 of
[`spreadsheet-editing.md`](spreadsheet-editing.md) said it must not:** that
decision is about *switching modes*, and it stands — a user toggling the edit
button must not cost a second `translate`. Whether editing is offered at all is
a host's decision, made once before it renders. The two look alike and are not:
one is a gesture, the other is a build-time choice.

**A document that cannot be edited still gets the scaffolding** where the config
asks for it, and states `data-odr-editable="readOnly"`. That is what lets a host
grey its button rather than discover the refusal after a tap.

### 12. A host keeps the keys it needs

The scripts take three classes of key event, and two of them are configurable:

| Class | Keys | Config |
|---|---|---|
| The open editor's | Escape, Enter, Tab while an editor holds focus | always taken |
| Navigation | the arrows, Tab, Escape, and the keys that open the editor over the selection (Enter, F2, Delete, a character) | `HtmlConfig::keyboard_navigation` |
| Shortcuts | the chords: ctrl/cmd+Z, ctrl/cmd+Y, ctrl/cmd+shift+Z | `HtmlConfig::keyboard_shortcuts` |

Both default to **on**, and the page states what it may take in
`data-odr-keyboard`.

**Why configurable at all:** the sheet's key handler is registered in the
capture phase and calls `preventDefault`, so an arrow key never reaches the
embedder. A host with its own bindings — an app whose arrow keys page through
the document, a site whose iframe sits inside a keyboard-driven shell — loses
them with no way to ask for them back.

**Why the open editor's keys are not configurable:** the editor holds focus, and
Escape and Enter are the only way out of it. A host that took them would leave
the user in an overlay nothing closes.

**Why two classes and not one:** they fail differently. Navigation collides with
a host that moves a selection of its own; the chords collide with a host that
owns undo for the whole app, which is the common case on desktop and the rarer
one on mobile.

**Why the page states it rather than the script asking the config:** the scripts
are static files embedded at build time (`cmake/frontend_assets.cmake`), so a
config value reaches them only through the markup. One attribute carries both
classes as a token list, and a third class appends to it without a fourth
attribute.

### 13. One editable view, and every edit it cannot replay is refused

`enable()` puts `contenteditable` on the **body**, not on each run. The editor
then intercepts `beforeinput` and takes the edits it can express as operations:

| The edit | What happens |
|---|---|
| text typed, replaced, deleted — inside one run, across runs, across paragraphs | taken |
| Enter | taken: the paragraph splits where the caret sits |
| Backspace at the start of a paragraph | taken: the paragraph merges into the one before it |
| a paste of plain text, over as many lines as it holds | taken: each line after the first opens a paragraph |
| a composition (CJK, autocorrect, dictation) | let through and reconciled on `compositionend` |
| a soft line break (`insertLineBreak`) | refused, reason `newLine` - no operation carries one |
| a range reaching over a picture | taken: the frame carries an address, so the picture goes with the text |
| a range reaching over a text box or a table | refused, reason `range` - it holds text of its own, which the reader did not mean to lose |
| anything else the browser offers (a mark, a list, a drop) | refused, reason `unsupportedEdit` |
| an edit landing outside every run | refused, reason `range` |

**Why the whole view rather than a run at a time:** `contenteditable` per run
makes every run its own editing host, and a host is a wall. The caret cannot
cross it, a selection cannot span two of them, and a reader who selects a
sentence gets nothing — silently, with no way to say why. One host gives the
document the caret, selection and word-double-click a reader expects, and the
refusal channel (decision 9) is what says no where we cannot follow. That is
also far less markup to write and one attribute to toggle rather than a walk
over every run.

**Why `beforeinput` is the gate:** it fires before the browser changes anything,
it says *what* the edit is (`inputType`), it says *where* (`getTargetRanges()`),
and it is cancelable. `text.js` already edits the plain-text view this way.

**The whitelist is closed, not open.** Only the input types the editor can
express are taken; anything unrecognised is refused. Refusing something we could
have allowed costs a reader one gesture; allowing something we cannot replay
costs them their document.

**The address is the whole guard.** No element is marked non-editable: an edit is
allowed because it lands inside a `x-s[data-odr-id]` run and reaches over
nothing but runs, so a picture, a table's furniture and the page box are all
refused without a single attribute of their own. That is decision 10's rule —
mark the exceptions, not the rest — applied to the caret instead of to a cell.

**The editor owns the edit.** It cancels the `beforeinput` and splices the page
itself, rather than letting the browser apply the change and reading the run
back. See decision 6 of [`document-editing.md`](document-editing.md) for why that had
to change.

**Undo is the editor's**, because cancelling every edit leaves the browser's own
stack empty. Each step holds the operations it puts on the wire and the two
halves of taking it back, so `canUndo` and the chord now agree and a host's undo
button is live. One `beforeinput` is one step.

**Known holes, both narrow.** A scripted `document.execCommand` can bypass the
gate, because Chrome does not fire a cancelable `beforeinput` for every command;
trusted input, which is all a reader has, goes through it. And a composition
cannot be cancelled at all, so the editor lets it finish and reads the run back
on `compositionend`; a composition that landed where no run can name it raises
code 9 rather than being dropped. Android WebView's incomplete `beforeinput`
(decision 8) is the reason that report exists, and the reason a delete whose
range the browser did not state is extended by one character rather than
refused; verify both on a device.

## What landed, and what did not

The plan this document carried ran in five steps, and the first four are in.
Each per-editor document holds what its own step decided.

| Step | State |
|---|---|
| Stable ids across the html boundary — `data-odr-id`, `Document::element_by_id` | landed |
| The op envelope and a replay that dispatches over it | landed |
| The write side of the engines — odf, ooxml text, ooxml presentation | landed |
| The browser editor, owning the edit and its own undo | landed |
| **Inline formatting** — bold, italic, underline, highlight | **not started** |

Formatting is the one left, and decision 5 of
[`document-editing.md`](document-editing.md) is why the schema takes it without
changing: toggling a mark on part of a run is, in both formats, "split the run,
restyle the middle one", and the split is already two operations we have. What
it needs is a `setMark {ids, mark, on}` and, for ODF, the automatic style a mark
is reached through — a style-registry change with nothing to do with the ops.

The **conformance corpus** decision 7 asks for is still not built. What stands
in for it is that both sides pin the same operation shapes: the browser check
pages assert the log they emit, and `document_edit_test.cpp` replays those same
shapes in C++. That is weaker than a shared corpus, and it missed the envelope
version drifting until a new check page caught it.

## Open questions

- ~~Can inserts stay append-only in every registry?~~ **Answered: yes**, for the
  three that write. `create_element` appends and `unlink_child` leaves the slot
  taken, so no live id is renumbered.
- ~~Is a `data-odr-id` needed on structural elements as insertion anchors?~~
  **Answered: on paragraphs**, which is what a split or an insert anchors on.
  Nothing above them needed one.
- Highlight in ODF/OOXML: character background vs. a highlight-specific property —
  which maps cleanly to a single toggle?
- `element_is_editable` answers one bool, and ooxml text answers `true` for
  everything. A refusal needs a reason, because the reason is what a host puts
  on a snackbar (decision 7 in
  [`spreadsheet-editing.md`](spreadsheet-editing.md)). Does the adapter hook
  grow into `element_edit_lock(id) -> reason`, or does the renderer keep
  deciding the reason from the element it is over?
- ~~The plain-text view is outside the mode.~~ **Answered: it attaches.**
  See [`txt-editing.md`](txt-editing.md).
- The **pdf annotator** is now the one editor that answers to nobody:
  `odr.annotation` is its own API and `PdfFile::annotate` its own write path.
  It is a different gesture from editing text, so whether it should share the
  mode is a real question rather than an oversight
  ([`txt-editing.md`](txt-editing.md) carries it too).
