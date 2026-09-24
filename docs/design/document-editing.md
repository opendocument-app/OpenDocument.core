# Document editing design

The editor of the document view, `frontend/document.js`, and the decisions
that are its own. The shared mode frame is in [`editing.md`](editing.md);
[`spreadsheet-editing.md`](spreadsheet-editing.md) covers the sheet view and
[`txt-editing.md`](txt-editing.md) the plain-text view. Text documents,
presentations and drawings share this editor: it edits runs and paragraphs
wherever the format puts them.

Status: landed, inline formatting included. The open items are at the end.

## What the editor has to express

1. Type over a selection that spans two runs.
2. Press Enter: the paragraph splits at the caret.
3. Press Backspace at the start of a paragraph: it merges into the one before.
4. Delete a selection that spans paragraphs.

Each one creates or destroys elements, so the schema and the adapters both
have structural operations.

## Decisions

### 1. An op names an element by its id, not by its path

`data-odr-id` carries the `ElementIdentifier` the registry assigns, and
`Document::element_by_id` turns it back into an `Element` at replay. A path is
positional: after one inserted paragraph every later path in the log names a
different element. Ids do not move, because a registry id is the index of a
`std::deque` that only grows and no op renumbers. An id holds for one
`translate`, `edit`, `save` cycle only. `back_translate` replays against a
fresh decode of the same bytes, which assigns the same ids because parsing is
deterministic.

### 2. The wire carries no character offsets

An op names whole elements and whole strings. JavaScript counts UTF-16 code
units and `std::string` counts bytes, so an offset needs a conversion and a
rule about which side does it. The browser owns the model, so it knows the
text each run ends up with and sends that. The log is coalesced before it is
emitted, so the cost is one whole run per changed run per save.

### 3. A split point is a run boundary, so `splitParagraph` needs no offset

Enter in the middle of a run is three ops: `setText` keeps the head in the
run, `insertText` puts the tail in a new run after it, `splitParagraph` splits
after the first run. ODF and OOXML both store a styled stretch as its own run,
so the split of the paragraph is then a move of whole children.

### 4. A created element is addressed by a negative id

An op that creates an element carries `"id": -1`, `-2`, and so on. A later op
in the same log names it by that number. Replay stays a pure function of the
log, with no minted ids sent back to the browser. A sign needs no per-engine
argument about a reserved range: odf already spends the top bit of the 64-bit
id on a positional cell id. Replay keeps a per-log map from the negative
number to the minted id. A number used before it is created, or created twice,
throws.

### 5. Formatting fits this schema unchanged

Toggling bold on part of a run is, in both formats, "split the run, restyle
the middle one". The split is decision 3's first two ops. What is left is one
op that names whole runs, `setTextStyle`. See [Inline formatting](#inline-formatting).

### 6. The browser applies the edit itself, so undo is ours

The editor cancels `beforeinput` and mutates the DOM itself. Contenteditable's
result for a selection that spans two paragraphs is browser specific and does
not map onto the element tree, so reading the DOM back after the browser edits
it does not work. The cost is that the browser's undo stack goes empty, so the
editor carries undo and redo. One `beforeinput` is one undo step.

### 6b. The page is the model

The runs and paragraphs addressed by `data-odr-id` in the page are the model.
There is no parallel structure, because nothing but this editor writes the
page. A composition cannot be cancelled, so there the browser does write inside
a run. The editor notes the run's text before the browser writes and reads it
back after every `input`, and the difference is the operation. It reads after
every change, not once at `compositionend`, because an Android keyboard holds a
composition open on the word under the caret.

### 7. Read-only engines say nothing

The structural adapter hooks default to throwing `UnsupportedOperation`. Ten
engines have a `TextAdapter` and two can write, so a pure virtual would cost
eight identical throwing bodies.

## The op envelope

Version 2. Version 1 addressed by path, and replay refuses it.

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
| `setTextStyle` | `id`, `style` | states the listed properties on one run; see [Inline formatting](#inline-formatting) |
| `setCell` | `sheet`, `column`, `row`, `value` | see [`spreadsheet-editing.md`](spreadsheet-editing.md) |

Every `id` on an op that creates an element is negative (decision 4). Every
other id is one the page wrote.

### The four reader gestures, as ops

Type over a selection spanning two runs, `a[bc` to `de]f` becoming `aXf`:

```json
[{"op": "setText", "id": 10, "text": "aX"},
 {"op": "setText", "id": 11, "text": "f"}]
```

Enter in the middle of a run (decision 3):

```json
[{"op": "setText", "id": 10, "text": "head"},
 {"op": "insertText", "after": 10, "text": "tail", "id": -1},
 {"op": "splitParagraph", "paragraph": 9, "after": 10, "id": -2}]
```

Backspace at the start of a paragraph:

```json
[{"op": "mergeParagraph", "paragraph": 9}]
```

Delete a selection spanning three paragraphs:

```json
[{"op": "setText", "id": 10, "text": "head"},
 {"op": "removeElement", "id": 11},
 {"op": "removeElement", "id": 20},
 {"op": "setText", "id": 31, "text": "tail"},
 {"op": "mergeParagraph", "paragraph": 9},
 {"op": "mergeParagraph", "paragraph": 9}]
```

## The C++ side

A handle says what an element holds: `Text::set_content`, `Text::set_style`,
`Sheet::set_cell`. The document says what the tree holds: `Document::remove`,
`insert_text_before`, `insert_text_after`, `split_paragraph`,
`merge_paragraph_with_next` and `insert_paragraph_after`. An `Element` is an
immutable handle, so the tree changes through the document. Each structural
call refuses an element of another document.

The adapter hooks, all defaulting to `UnsupportedOperation` (decision 7):
`element_remove`, `text_insert`, `text_set_style`, `paragraph_split`,
`paragraph_merge_next` and `paragraph_insert_after`. Each engine resolves the
id to its registry entry, splices the pugixml subtree and fixes the registry
links. Only the tag names differ: `text:p` and `text:span` against `w:p`,
`w:r`, `a:p` and `a:r`. `internal::ElementRegistry` has `unlink_child`,
`insert_sibling_after` and `insert_sibling_before` for this.

### What a split does to what is around it

`splitParagraph` names a descendant, because the caret sits in a run and the
run may sit in a span. The split walks from the run up to the paragraph and
splits every element on the way. Only a span and a link are split through;
anything else, a frame for example, refuses with `UnsupportedOperation`. A
copy carries the original's attributes and the property children the format
writes ahead of the content, `w:pPr` and `w:rPr`. A split exactly at the end
of a span leaves an empty copy of that span, which is valid in both formats.

### Which formats

| Format | Engine | State |
|---|---|---|
| `.odt`, `.odp`, `.ods`, `.odg` | `odf` | edits and saves |
| `.docx` | `ooxml/text` | edits and saves |
| `.pptx` | `ooxml/presentation` | edits and saves, over `a:p` and `a:r` |
| `.xlsx` | `ooxml/spreadsheet` | cells only; see [`spreadsheet-editing.md`](spreadsheet-editing.md) |
| `.txt` | `text` | not a document; see [`txt-editing.md`](txt-editing.md) |
| everything else | | read-only, by decision 7 |

## What the editor does with a keystroke

Every edit is one of two shapes:

- A range replaced by some text: typing, replacing a selection, every delete,
  each line of a paste. Inside one run it is a `setText`. Across runs it is a
  `setText` on each end and a `removeElement` between. Across paragraphs it is
  that plus a `mergeParagraph`.
- A split where the caret sits: Enter, and every line break in a paste. The
  run is cut in two first (decision 3) unless the caret is at a run boundary.

Two rules the checks pin down:

- A delete whose range the browser did not state is one character in the
  direction the key names, or the paragraph boundary at the start of a
  paragraph. An Android WebView is reported to leave the range out. The word
  and line deletes are not extended this way and do nothing.
- The line box stays as a fresh render writes it: `<br>` where a paragraph
  holds nothing, `<wbr>` where it holds something. An edited page then looks
  like a re-rendered one.

`replaceRange` removes the runs between its ends. It also removes a frame that
holds no run, a picture or a plain shape. A frame that holds runs is a text
box, and the edit refuses. A picture alone in its paragraph is out of reach,
because there is no run to anchor the edit to; the edit changes nothing.

## Inline formatting

Landed: bold, italic, underline, strikethrough, highlight, colour and size.
Font name, superscript and subscript are not written, and a delta that sets
one refuses.

| On the wire | `TextStyle` | ODF `style:text-properties` | docx `w:rPr` | pptx `a:rPr` |
|---|---|---|---|---|
| `bold` | `font_weight` | `fo:font-weight="bold"` / `"normal"` | `<w:b/>` / `<w:b w:val="0"/>` | `b="1"` / `b="0"` |
| `italic` | `font_style` | `fo:font-style="italic"` / `"normal"` | `<w:i/>` / `<w:i w:val="0"/>` | `i="1"` / `i="0"` |
| `underline` | `font_underline` | `style:text-underline-style="solid"` / `"none"` | `<w:u w:val="single"/>` / `"none"` | `u="sng"` / `u="none"` |
| `strikethrough` | `font_line_through` | `style:text-line-through-style="solid"` / `"none"` | `<w:strike/>` / `<w:strike w:val="0"/>` | `strike="sngStrike"` / `"noStrike"` |
| `highlight` | `background_color` | `fo:background-color="#rrggbb"` / `"transparent"` | `<w:highlight w:val="yellow"/>` or `<w:shd w:val="clear" w:fill="RRGGBB"/>` | `<a:highlight><a:srgbClr val="RRGGBB"/></a:highlight>` |
| `color` | `font_color` | `fo:color="#rrggbb"` | `<w:color w:val="RRGGBB"/>` | `<a:solidFill><a:srgbClr val="RRGGBB"/></a:solidFill>` |
| `size` | `font_size` | `fo:font-size="12pt"` | `<w:sz w:val="24"/>`, in half-points | `sz="1200"`, in hundredths of a point |

Every cell is what `odf_style.cpp`, `ooxml_text_style.cpp` and
`ooxml_presentation_style.cpp` read, so a saved file renders what the editor
showed.

```json
{"op": "setTextStyle", "id": 41,
 "style": {"bold": true, "highlight": "#ffff00", "size": "14pt"}}
```

`style` holds any of the seven keys. A key not listed is untouched. The four
toggles carry `true` or `false`, `highlight` carries `#rrggbb` or `null`,
`color` carries `#rrggbb`, and `size` carries a length as `Measure` spells it.

### 8. The wire carries a value, not a toggle

"Toggle bold" needs the run's current state, and the browser has it. Colour
and size are not on or off. So the browser resolves the gesture and the wire
carries the result, and replay stays a pure function of the log.

### 9. Off is written, never removed

`bold: false` writes `fo:font-weight="normal"`, `<w:b w:val="0"/>` or
`b="0"`. The cascade underneath may be bold, and the reader asked for not
bold. `highlight: null` writes `transparent` in ODF and `w:val="none"` in
docx. `color` has no null; see the open items.

### 10. One run per op

A gesture over twenty runs is twenty ops. Coalescing decides it: two
`setTextStyle` on one run merge into one op where the later keys win, and one
on a run a later `removeElement` removes is dropped. A list of ids would turn
both into set arithmetic.

### 11. A mark gives the run a container of its own

`data-odr-id` sits on the text element: a text node in ODF, `w:t` in docx,
`a:t` in pptx. All three take their style from the container around them and
share it with every sibling. So a mark on one run first cuts the container so
that only this run is in it. `TreeEditor::isolate` in `xml/xml_tree_edit.hpp`
is that cut, shared by the three engines: a split before the run and one after
it, each copying the container's shell. docx and pptx apply the delta to the
copy of `w:rPr` or `a:rPr`. ODF wraps a bare run in a new `text:span` and
gives the span a fresh automatic style (decision 12). Marking part of a run is
decision 5: `setText` and `insertText` split it, `setTextStyle` names the
middle.

### 12. ODF reaches a mark through a fresh automatic style

An automatic style may be shared by any number of spans, so writing into one
restyles text the reader never selected. The writer adds a
`<style:style style:family="text">` to `office:automatic-styles` under an
unused name, copies the span's old properties with the delta applied, turns a
named style on the span into `style:parent-style-name`, and points the span at
it. One style per distinct result is kept for the length of a replay.

### 13. `w:rPr` is a sequence, and the writer keeps its order

Word refuses a file that breaks the schema order of `w:rPr`. The writer's
order is `w:b`, `w:i`, `w:strike`, `w:color`, `w:sz`, `w:highlight`, `w:u`,
`w:shd`. It inserts each at its position and replaces one already present.
`w:bCs`, `w:iCs` and `w:szCs` follow their sibling. In pptx `a:rPr` is
attributes for five of the seven and ordered children for `a:solidFill` and
`a:highlight`, fill first. `a:rPr` must be the first child of `a:r`, so a run
without one gets it made.

### 14. Highlight is the character background, and docx spells it two ways

The wire carries a colour, because ODF and pptx take any colour. docx
`w:highlight` takes one of sixteen names and `w:shd` takes any colour. The
writer spells one of the sixteen as `w:highlight` and anything else as
`w:shd w:val="clear" w:fill`, and the reader reads `w:shd` on a run, so a
saved highlight renders on reopen.

### 15. The editor writes the css the renderer writes

`translate_text_style` puts the style inline on the `x-s`, and the editor
sets the same declarations: `font-weight:bold`, `font-style:italic`,
`text-decoration:underline`, `text-decoration:line-through`, `color`,
`background-color`, `font-size`. Underline and strikethrough on one run are
one declaration, `text-decoration:underline line-through`. The selection state
a host shows (decision 16) reads the same declarations back.

### 16. The gesture reaches the editor two ways

- The host asks: `odr.editing.format(style)` applies a partial style to the
  selection and answers false where it refused. `odr.editing.toggle(property)`
  is a chord's rule for a button: a mixed selection turns on, as Word does.
  The editor reports the computed style of the selection through
  `odr.onSelectionChange(style)`, one key per property and none where the
  covered runs differ.
- The browser asks: `formatBold`, `formatItalic`, `formatUnderline` and
  `formatStrikeThrough` are on the whitelist. They are chords, so where a
  host keeps that class the editor cancels the browser's mark and does
  nothing else.

Formatting sits behind the scope gate: under `paragraph` every formatting
gesture refuses with `outOfScope`. A collapsed caret inside a word marks the
word. At a word boundary, or in a paragraph with no run, the mark is pending:
the next typed text is cut into a run of its own and marked. A caret that
moves away drops the pending mark.

Undo needs nothing new: a step holds the runs' old and new `style`
attributes. Two marks on one run fold into one op; any other op naming the
run stops the fold.

`Text::set_style(delta)` reaches `TextAdapter::text_set_style`. The delta is a
`TextStyle` whose set fields are the change. `highlight: null` is a
`background_color` with alpha 0. The bindings expose it in python, Java,
Objective-C and the npm package.

## Open items

- A list item is a paragraph in a list. Enter at the end of one makes a bare
  paragraph, not a new list item.
- Colour back to automatic: docx has `w:color w:val="auto"`, ODF has only
  removal, which decision 9 forbids. A colour once set can only become another
  colour.
- A mark over a text box refuses like a replace does, although a mark loses
  nothing.
- ODF percentage sizes are read as resolved lengths; whether they are ever
  written back as absolute is a question the fixtures answer.
- `text-decoration` is drawn through every descendant, so an underline taken
  off a run inside an underlined span still shows in the page and in a fresh
  render. The saved file is right.
- `read_color` in `odf_style.cpp` reads `fo:background-color="transparent"`
  as unstated, so a highlight taken away on a run inside a highlighted
  paragraph still shows in our render, not in LibreOffice's. Reading it as
  alpha 0 fixes it and moves every page whose styles write `transparent`.
