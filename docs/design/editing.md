# Editing design

Status: **accepted direction; implementation deferred.** This records the
architecture we chose for in-browser editing of ODF and OOXML documents, the
alternatives we weighed, and *why* we took each decision. The implementation plan
and sketch at the end are **not scheduled yet** — they are captured here so the
decisions and the grounding survive until we pick the work up later.

This builds on the existing principle in [`README.md`](README.md):

> saving should not depend on our internal representation of the document but
> only the changes

## Problem

We render documents to HTML and display them in a WebView (droid / ios). We want
to let the user edit content — remove any element, add paragraphs, and toggle
simple inline formatting (bold, italic, underline, highlight) — and persist those
edits back into the original ODF/OOXML file.

Today this exists only in skeleton form:

- `html::translate(..., config.editable)` stamps `contenteditable="true"` on
  elements where `Element::is_editable()` (`internal/html/document_element.cpp`).
- `html::edit(document, diff)` (`src/odr/html.cpp`) parses a JSON blob and
  understands a single key, `modifiedText`: a map of `DocumentPath → new string`.
  It navigates to each text element and calls `set_content`.
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

1. Model keyed by `data-odr-id`; `beforeinput`-intercepting op recorder;
   composition-aware reconciliation path.
2. Browser-side undo/redo over the in-memory log; coalescing before emit.
3. Emit coalesced JSON to the WebView bridge; wire the native side to
   `html::edit` + `save`.

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
| `insertText(range)` | reuse the `text_set_content` tokenizer (`util::xml::tokenize_text`) on a sub-range | same, `w:t` tokenizer |

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
