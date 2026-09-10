# Document editing design

The editor of the **document view** — `frontend/document.js` — built on the
mode frame in [`editing.md`](editing.md), and the decisions that are its own.
[`spreadsheet-editing.md`](spreadsheet-editing.md) is the sibling document for
the sheet view. Text documents, presentations and drawings share this one:
what it edits is runs and paragraphs, wherever the format puts them.

Status: **the schema and the replay are landing; the browser editor follows.**
This document is written ahead of the code, and each section says what is in
and what is not.

Scope of this work: an edit that spans several runs, a new paragraph, and a
delete or a replace that reaches across both. Inline formatting (bold, italic,
highlight) is *not* in it — decision 5 below says why the schema takes it later
without changing.

## What the editor has to express

A reader does four things the old `setText` could not say:

1. **Type over a selection that spans two runs.** The selection starts in one
   run and ends in another, so one `setText` cannot name it.
2. **Press Enter.** The paragraph splits, and everything after the caret moves
   into a new paragraph.
3. **Press Backspace at the start of a paragraph.** The paragraph merges into
   the one before it.
4. **Delete a selection that spans paragraphs.** Runs disappear, paragraphs
   disappear, and the two ends become one paragraph.

Every one of them creates or destroys elements, which is why the schema and the
adapters both change.

## Decisions

### 1. An op names an element by its id, not by its path

`data-odr-path` is replaced by `data-odr-id`, carrying the
`ElementIdentifier` the registry already assigns. `Document::element_by_id`
turns it back into an `Element` at replay. This is decision 4 of
[`editing.md`](editing.md), now that there is something that needs it.

**Why:** a `DocumentPath` is positional. The moment an op inserts a paragraph,
every path recorded after it in the same log names a different element, so a
log of more than one structural op cannot be replayed. Ids do not move.

**Why it is safe:** an id has to hold for one `translate → edit → save`, and a
registry id is the index of a `std::deque` that only grows. The ops below
append and unlink; none of them renumbers. The sheet write side already creates
elements after the parse and leaves the old ones unreachable
([`odf/AGENTS.md`](../../src/odr/internal/odf/AGENTS.md)), so the discipline is
one the engines keep already.

**The cost:** an id is meaningless outside the render that wrote it. A path was
readable and could be written by hand; an id cannot. `back_translate` replays
against a fresh decode of the same file, and a fresh decode of the same bytes
assigns the same ids, because parsing is deterministic. Nothing else read the
attribute.

### 2. The wire carries no character offsets

An op names whole elements and whole strings. There is no `(id, start,
length)`. A reader typing in the middle of a run produces `setText` with the
run's new text, not an insertion at an offset.

**Why:** JavaScript counts a string in UTF-16 code units and `std::string`
counts bytes, so an offset on the wire needs a conversion on one side and a
rule about which side that is. An emoji, a combining accent and a `text:s`
run-of-spaces each make the two disagree. Nothing in the feature needs the
offset: the browser owns the model (decision 8 of [`editing.md`](editing.md)),
so it already knows the text each run ends up with, and handing that text over
is both shorter to write and impossible to misread.

**What it costs:** the log is longer. Typing one character in the middle of a
long run sends the whole run. The log is coalesced before it is emitted
(decision 6 of [`editing.md`](editing.md)), so the length is per run and per
save, not per keystroke.

**Why it does not paint us into a corner:** see decision 5.

### 3. A split point is a run boundary, so `splitParagraph` needs no offset

Enter in the middle of a run is three ops, not one:

1. `setText` — the run keeps the text before the caret.
2. `insertText` — a new run after it holds the text after the caret.
3. `splitParagraph` — the paragraph splits after the first run.

**Why:** it keeps decision 2, and it is what the file formats do anyway. ODF
and OOXML both represent a styled stretch of text as its own run, so a split
inside one *is* a split of the run followed by a split of the paragraph. Doing
it in that order makes the second step a pure move of whole children.

### 4. A created element is addressed by a negative id

An op that creates an element carries `"id": -1`, `-2`, … . A later op in the
same log names the created element by the same negative number. A positive id
is one the render wrote into the page.

**Why an explicit number rather than a returned one:** replay stays a pure
function of the log. Returning minted ids to the browser is the round trip
architecture A exists to avoid (decision 1 of [`editing.md`](editing.md)).

**Why negative rather than a reserved high range:** `ElementIdentifier` is 64
bits and odf already spends the top bit on a positional cell id
([`odf/AGENTS.md`](../../src/odr/internal/odf/odf_element_registry.hpp)), so a
reserved range means an engine-by-engine collision argument. A sign has no such
argument to make, and the address stays one integer.

Replay keeps a per-log map from the negative number to the id it minted. A
number used before it was created, or created twice, throws.

### 5. Formatting fits this schema unchanged, which is why it is not in it yet

Toggling bold on part of a run is, in both formats, "split the run, restyle the
middle one". The split is decision 3's first two ops, and what is left is one
op naming whole runs — `setMark {ids, mark, on}`. No offsets, no new
addressing.

**Why it is not in this work:** the split is the same machinery either way, and
ODF reaches a mark through a named automatic style it may have to create, which
is a style-registry change with nothing to do with the ops. Landing it here
would double the size of the change for a feature nobody asked for yet.

### 6. The browser applies the edit itself, so undo becomes ours

The editor cancels `beforeinput` and mutates the DOM itself, rather than
letting the browser apply the edit and reading the run back afterwards
(decision 13 of [`editing.md`](editing.md) as it was first written).

**Why it has to change:** reading the run back only works when the edit stayed
inside one run. Contenteditable's answer to a selection spanning two paragraphs
is browser-specific — a `<div>` wrapper here, a merged `<b>` there — and none
of it maps onto the element tree. What we could read back afterwards would not
be what we would have to replay.

**The consequence: the browser's undo stack goes empty**, because we cancel
every edit it was going to apply. So this work has to carry undo/redo, which
until now was honestly refused (`canUndo` answered false and a host's button
stayed grey). That is phase 3 item 2 of [`editing.md`](editing.md), and it
arrives here because it is no longer optional.

### 7. Read-only engines say nothing

The new adapter hooks default to throwing `UnsupportedOperation`, rather than
being pure virtual like `text_set_content`.

**Why:** ten engines have a `TextAdapter` and two of them can write. A pure
virtual costs eight identical throwing bodies and grows every time the surface
does. `FrameAdapter` already defaults its three shape readers for the same
reason.

## The op envelope

Version **2**. Version 1 is refused rather than read: it addressed by path, and
a path in a version-2 world names the wrong element rather than none.

```json
{"version": 2, "ops": [{"op": "setText", "id": 41, "text": "typed"}]}
```

| op | fields | what it does |
|---|---|---|
| `setText` | `id`, `text` | replaces the whole text of one run |
| `insertText` | `after` or `before`, `text`, `id` | a new run beside the named one, in the same parent, so it takes the same style |
| `removeElement` | `id` | unlinks the element and removes its nodes |
| `splitParagraph` | `paragraph`, `after` (optional), `id` | the children after `after` move into a new paragraph that copies the style; no `after` moves all of them |
| `mergeParagraph` | `paragraph` | takes the children of the next sibling paragraph and removes it |
| `insertParagraph` | `after`, `id` | a fresh empty paragraph after the named one, copying its style |
| `setCell` | `sheet`, `column`, `row`, `value` | unchanged; see [`spreadsheet-editing.md`](spreadsheet-editing.md) |

Every `id` field on an op that creates an element is negative (decision 4).
Every other id is one the page wrote.

### The four reader gestures, as ops

**Type over a selection spanning two runs** — `a[bc` … `de]f` becoming `aXf`:

```json
[{"op": "setText", "id": 10, "text": "aX"},
 {"op": "setText", "id": 11, "text": "f"}]
```

**Enter in the middle of a run** — decision 3:

```json
[{"op": "setText", "id": 10, "text": "head"},
 {"op": "insertText", "after": 10, "text": "tail", "id": -1},
 {"op": "splitParagraph", "paragraph": 9, "after": 10, "id": -2}]
```

**Backspace at the start of a paragraph:**

```json
[{"op": "mergeParagraph", "paragraph": 9}]
```

**Delete a selection spanning three paragraphs:**

```json
[{"op": "setText", "id": 10, "text": "head"},
 {"op": "removeElement", "id": 11},
 {"op": "removeElement", "id": 20},
 {"op": "setText", "id": 31, "text": "tail"},
 {"op": "mergeParagraph", "paragraph": 9},
 {"op": "mergeParagraph", "paragraph": 9}]
```

## Where the C++ API puts an edit

A **handle** says what an element holds — `Text::set_content`,
`Sheet::set_cell`. The **document** says what the tree holds —
`Document::remove`, `Document::insert_text_before` / `insert_text_after`, and
the paragraph operations below. An `Element` is an immutable handle, so
restructuring the tree through one would leave a handle naming something
unreachable; and the document is what owns the tree either way. Each structural
call refuses an element of another document.

## The adapter surface

Alongside `TextAdapter::text_set_content`, all defaulting to
`UnsupportedOperation` (decision 7):

```cpp
// ElementAdapter
virtual void element_remove(ElementIdentifier id) const;

// TextAdapter
virtual ElementIdentifier text_insert(ElementIdentifier anchor_id,
                                      Placement where,
                                      const std::string &text) const;

// ParagraphAdapter
virtual ElementIdentifier paragraph_split(ElementIdentifier id,
                                          ElementIdentifier after_id) const;
virtual void paragraph_merge_next(ElementIdentifier id) const;
virtual ElementIdentifier paragraph_insert_after(ElementIdentifier id) const;
```

Each engine does the same three things it already does for a text edit:
**resolve the id to its registry entry, splice the pugixml subtree, fix up the
registry links.** Only the tag names differ — `text:p` / `text:span` against
`w:p` / `w:r` / `a:p` / `a:r`.

The shared `internal::ElementRegistry` grows the links the structural ops need:
`unlink_child`, `insert_child_after` and `insert_child_before`. It has only
`append_child` today, because until now nothing built a tree except a parser
reading forward.

## Which formats

| Format | Engine | State |
|---|---|---|
| `.odt`, `.odp`, `.ods`, `.odg` | `odf` | edits and saves today; the new ops land here |
| `.docx` | `ooxml/text` | edits and saves today; the new ops land here |
| `.pptx` | `ooxml/presentation` | **read-only today.** It already has `text_set_content` and keeps its slide DOMs resident; what it lacks is `save`, the two flags and the capability row |
| everything else | — | read-only, and says so by decision 7 |

`.odp` needs nothing of its own: a presentation is the same odf `Document` as a
text document, and a run inside a slide's frame is the same `text` element.

## Order of work

Each step is a pull request that builds and tests on its own.

1. **Address by id.** `data-odr-id` on runs and paragraphs,
   `Document::element_by_id`, `setText` by id, envelope version 2.
   **Landed.**
2. **Runs come and go.** `insertText` and `removeElement`, the registry links
   they need, odf and ooxml text. A selection spanning runs is replayable.
   **Landed.**
3. **Paragraphs split and merge.** `splitParagraph`, `mergeParagraph`,
   `insertParagraph`.
4. **The browser editor.** Model-first, owns the DOM mutation, records the ops,
   and carries undo/redo (decision 6).
5. **pptx writes.** `save`, `is_editable`, `is_savable`, the capability row and
   the new hooks over `a:p` / `a:r`.

## Open questions

- A run inside a **link** or a **bookmark** splits differently: splitting the
  paragraph has to decide whether the link follows the tail. Today it would,
  because the link is a child that moves whole. Whether that is right is a
  question for step 3.
- **A list item** is a paragraph in a list. Enter at the end of one should make
  a new list item, not a bare paragraph. Step 3 splits what the element tree
  says is a paragraph; the list case is not covered.
- The **plain-text view** (`html/text_file.cpp`) is still its own editor and
  still answers to nobody. Unchanged by this work, and still the open question
  at the end of [`editing.md`](editing.md).
