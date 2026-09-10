# Editing design

Status: **the mode frame is landed for every format; the text editor behind it
is not.** This records the architecture we chose for in-browser editing of ODF
and OOXML documents, the alternatives we weighed, and *why* we took each
decision. Decisions 9 to 12 are the frame every format shares, and they are in
the code. The phases below them are the text editor, and they are **not
scheduled yet** — they are captured here so the decisions and the grounding
survive until we pick the work up later.

[`spreadsheet-editing.md`](spreadsheet-editing.md) is the first editor built on
the frame, and it is where a sheet's own decisions live.

This builds on the existing principle in [`README.md`](README.md):

> saving should not depend on our internal representation of the document but
> only the changes

## Problem

We render documents to HTML and display them in a WebView (droid / ios). We want
to let the user edit content — remove any element, add paragraphs, and toggle
simple inline formatting (bold, italic, underline, highlight) — and persist those
edits back into the original ODF/OOXML file.

Today the frame is there and the text editor is not:

- `html::translate(..., config.editable)` writes the editing scaffolding: the
  page-level state on `<body>`, `data-odr-path` on every editable run, and the
  scripts that carry the mode (`internal/html/document_element.cpp`,
  `internal/html/document.cpp`).
- `frontend/editing.js` owns `odr.editing` — the mode, the refusals, the log a
  save reads and the callbacks a host wires. Every format's editor attaches to
  it; `frontend/sheet-editing.js` is the first one (decision 9).
- `frontend/document.js` holds the text editor, and it is still the skeleton:
  `contenteditable` runs, a `MutationObserver` keyed by `data-odr-path`, and one
  `setText` op per changed run. No selection model, no marks, no undo.
- `Document::edit(diff)` (`src/odr/document.cpp`) parses the op envelope and
  dispatches `setCell` and `setText`.
- `back_translate` CLI replays a diff file onto a source document and `save`s it.

The goal is to generalise this from "replace text in a span" to full content and
formatting edits, without a live connection between the browser and C++.

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
- the **refusals** — the code table, the repeat suppression, the outline a
  refused element gets, and `odr.onEditRefused`;
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
skeleton keeps a map of changed runs and answers `false`. The moment phase 1
gives text a real op log, it moves onto the shared one and the concatenation
becomes a single array.

### 10. The page states its editing frame at page level, on `<body>`

Three attributes, on the body element of every document view that offers
editing:

| Attribute | Meaning |
|---|---|
| `data-odr-editable="true" \| "readOnly"` | whether `enable()` can succeed at all |
| `data-odr-keyboard="navigation shortcuts"` | which key classes the scripts may take (decision 12) |

Per element the page states only the exceptions: `data-odr-path` addresses an
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
`data-odr-path` attribute on every editable run, the lock class on every locked
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
nothing. `data-odr-path` on the runs of a text document is the expensive half,
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

## Preliminary implementation plan (ODF / OOXML)

Ordered to de-risk the linchpin (id stability) first and to keep every step
shippable. "Text" = `odt`/`docx` first; spreadsheet/presentation follow the same
adapters.

### Phase 0 — Stable ids across the HTML boundary (linchpin spike)

1. Emit `ElementIdentifier` into the HTML as `data-odr-id` for editable elements
   (extend `internal/html/document_element.cpp`).
2. Add a resolver `Document::element_by_id(ElementIdentifier)` (public handle →
   adapter lookup) so C++ can turn an id from the log back into an `Element`.
3. Confirm the ODF and OOXML text registries can take **append-only inserts +
   tombstone deletes** without renumbering live ids. This is the go/no-go for A.

### Phase 1 — Op log format + engine-agnostic replay

1. Define the op schema (JSON) and a C++ `EditOp` variant: `setText`,
   `insertText`, `deleteRange`, `toggleMark`, `splitParagraph`, `insertParagraph`,
   `deleteElement`. Addressing per decisions 4–5 (`id + anchor + offset`).
2. Rework `html::edit` from the `modifiedText`-only shape into a dispatcher over
   the op list that resolves ids and calls a new **mutation API** on the adapters.
   Keep the version stamp + atomic apply (decision 7).
3. Land the conformance-corpus harness with the text-only ops it can already
   satisfy (the current `set_content` is the first case).

### Phase 2 — Write-side adapter API (the real cost; format-specific)

The adapters are decode-only today; add the mutation surface. Deletion is easy,
insertion is the hard part, formatting sits between.

- **Abstract layer** (`internal/abstract/document.hpp`): add mutation entry points
  (`element_set_text`, `element_delete`, `element_insert_child`,
  `element_split`, `element_apply_mark`), gated by `element_is_editable`.
- **ODF** (`internal/odf/`): text edits mutate the backing pugixml tree.
  Formatting is *automatic styles referenced by name* — toggling bold means split
  the run, look up/create an automatic style with the property, reassign. Insert
  synthesises `<text:p>` / `<text:span>` scaffolding.
- **OOXML** (`internal/ooxml/`): mirror with `rPr` on runs and `pPr` on
  paragraphs; split/merge runs on mark toggles; synthesise `w:p` / `w:r`.

Sequence within Phase 2: (a) delete element, (b) toggle mark on a range,
(c) split paragraph / insert paragraph, (d) insert/paste richer content.

### Phase 3 — Browser editor

The frame is landed (decisions 9 to 12): the mode, the refusals, the callbacks
and the keyboard classes are in `frontend/editing.js`, and `document.js`
attaches the skeleton editor to it. What is left is the editor itself.

1. Model keyed by `data-odr-id`; `beforeinput`-intercepting op recorder;
   composition-aware reconciliation path. This replaces the `contenteditable`
   plus `MutationObserver` path `document.js` still uses.
2. Browser-side undo/redo over the in-memory log; coalescing before emit. The
   text editor then answers `undo()` rather than refusing it, and its ops move
   onto the shared log (decision 9).
3. Emit coalesced JSON to the WebView bridge; wire the native side to
   `html::edit` + `save`. `odr.editing.getOperations()` is already the envelope
   a host hands over.
4. Refusal reasons for text: a field, a link target, a subtree a write would
   take away unseen. The code table is shared and appended to, never renumbered
   (decision 7 in [`spreadsheet-editing.md`](spreadsheet-editing.md)).

### Phase 4 — Formatting UI + polish

Selection toolbar for bold/italic/underline/highlight; validation feedback driven
by `is_editable`; extend from `odt`/`docx` to the remaining ODF/OOXML documents.

## Implementation sketch (grounded in the current code)

### What the code already gives us

The mutation surface exists in embryo. `abstract::Document` declares
`text_set_content(ElementIdentifier, string)`
([`abstract/document.hpp`](../../src/odr/internal/abstract/document.hpp)), and both
engines implement it identically
([`odf_document.cpp`](../../src/odr/internal/odf/odf_document.cpp),
[`ooxml_text_document.cpp`](../../src/odr/internal/ooxml/text/ooxml_text_document.cpp)):

1. Resolve `ElementIdentifier` → registry `Element` + `Text` entry.
2. The `Text` entry holds two pugixml handles, `first`/`last`, spanning the run's
   nodes.
3. Rebuild the pcdata / `w:t` / `text:s` nodes between them, then update the
   registry's `node`/`last` pointers.

Every new op follows the same shape: **resolve id → registry entry (pugixml node)
→ mutate the pugixml subtree → fix up registry pointers (append / tombstone for
structural ops).** Identical in ODF and OOXML — only the tag names and the style
mechanism differ.

### Abstract API additions (`abstract/document.hpp`)

New hooks alongside `text_set_content`, all gated by `element_is_editable(id)` and
throwing on unsupported input (fail fast):

```cpp
virtual void text_replace_range(ElementIdentifier id, uint32_t start,
                                uint32_t length, const std::string &text) = 0;
virtual ElementIdentifier
    element_insert(ElementIdentifier parent, ElementIdentifier anchor,
                   Anchor where /*before|after|end*/, ElementType type) = 0;
virtual void element_delete(ElementIdentifier id) = 0;          // tombstone
virtual ElementIdentifier element_split(ElementIdentifier id, uint32_t off) = 0;
virtual void mark_apply(ElementIdentifier id, uint32_t start, uint32_t length,
                        Mark mark, bool on) = 0;                // bold/italic/…
```

`html::edit` stops being `modifiedText`-only and becomes a dispatcher: parse the
op list → resolve ids via a new `Document::element_by_id` → call the matching hook,
all against an in-memory copy, writing only on full success (decision 7).

### Op → replay, per engine

| Op | ODF replay | OOXML replay |
|----|-----------|-------------|
| `deleteElement` | `remove_child` across `first..last`; **tombstone** the registry slot | same, over `w:r`/`w:p` |
| `toggleMark(range, bold)` | split run; assign an **automatic style** (`text:style-name`) — find or create a `<style:style>` with `fo:font-weight="bold"` | split `w:r`; set `<w:rPr><w:b/>` on the middle run |
| `splitParagraph` | clone `<text:p>` (copy style-name), move trailing nodes into the clone | clone `<w:p>` incl. `w:pPr`, move trailing `w:r` |
| `insertParagraph` | `insert_child_after` a fresh `<text:p>` at anchor; **append** registry entry | fresh `<w:p>`; append |
| `insertText(range)` | reuse the `text_set_content` tokenizer (`xml::tokenize_text`) on a sub-range | same, `w:t` tokenizer |

Two genuinely format-specific complications, both already visible in the code:

- **Marks are styles, not attributes.** ODF references *named automatic styles*
  ([`odf_style.cpp`](../../src/odr/internal/odf/odf_style.cpp)); a bold toggle is
  "split run + find-or-create style + reassign `text:style-name`," not "set an
  attribute." OOXML is friendlier — inline `w:rPr`. The op is trivial; the replay
  is not (decision 5).
- **Registry fix-up is mandatory.** Because ids *are* registry indices and
  `first`/`last` are cached pugixml handles, every structural op must append new
  entries (never renumber) and tombstone deletes — decision 4's append-only rule
  made concrete.

### Ids for created elements (`element_split` / `insert`)

Split and insert **create** elements, so C++ mints ids the browser doesn't know.
Decide before Phase 1:

1. **Browser pre-allocates ids** from a reserved high range and passes the id to
   use — replay is then a pure function, no id echo. Cleaner for A.
2. **C++ returns minted ids**, browser reconciles on save-response —
   reintroduces the round-trip A exists to avoid.

Recommended: (1). The browser allocates provisional ids for created elements; on
re-`translate` after save they are renumbered naturally anyway (ids are
session-scoped, decision 4).

## Open questions

- Can `ElementIdentifier` inserts stay append-only in *every* engine's registry,
  or does any engine rebuild indices in a way that breaks session stability?
- Do we need a `data-odr-id` on non-editable structural elements too (as insertion
  anchors), or only on editable leaves?
- Highlight in ODF/OOXML: character background vs. a highlight-specific property —
  which maps cleanly to a single toggle?
- `element_is_editable` answers one bool, and ooxml text answers `true` for
  everything. A refusal needs a reason, because the reason is what a host puts
  on a snackbar (decision 7 in
  [`spreadsheet-editing.md`](spreadsheet-editing.md)). Does the adapter hook
  grow into `element_edit_lock(id) -> reason`, or does the renderer keep
  deciding the reason from the element it is over?
- The plain-text view (`html/text_file.cpp`) is outside the mode: `text.js` is
  its own editor, with its own `beforeinput` interception and its own undo, and
  `config.editable` writes the `contenteditable` it needs. Nothing replays those
  edits into a file, because `txt` declares no `edit` capability. Does that view
  attach to `odr.editing` — which would need an editable-but-not-savable state —
  or stay the one editor that answers to nobody?
