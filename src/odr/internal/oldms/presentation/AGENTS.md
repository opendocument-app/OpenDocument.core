# `.ppt` (PowerPoint) support — status, design & open work

What the `oldms/presentation/` module does **today**, the **design decisions**
behind it, and the **open work**. Shared `oldms/` conventions are in
[`../AGENTS.md`](../AGENTS.md).

**Scope.** Extract the **visible text of each slide, positioned in its text
boxes**, and expose it through the abstract document model so the generic HTML
renderer lays each slide out as positioned frames. No character/paragraph
styles, master/notes pages, images, charts, tables, or animations.

**Specs.** `offline/documentation/MS-PPT/` (the PowerPoint stream) and
`MS-ODRAW/` (the Office Art / Escher drawing records). CFB container handling
already existed in the engine.

---

## What works

- `.ppt` is detected and decoded to a `Document` (presentation), one `slide`
  per presentation slide **in presentation order**.
- Each slide's on-slide **text boxes** become positioned `frame`s; their text is
  split into paragraphs / line breaks.
- A text box that stores no inline text but an `OutlineTextRefAtom` (the common
  PowerPoint placeholder representation) is resolved against the slide's text in
  the `SlideListWithTextContainer`, so placeholder/body text is not lost.
- The generic HTML renderer produces one page per slide with each text box
  absolutely positioned (verified: `position:absolute;left:…;top:…` with the
  decoded coordinates).

## Module layout (mirrors `../text`)

| File (`oldms/presentation/`)     | Role                                                |
|----------------------------------|-----------------------------------------------------|
| `ppt_structs.hpp`                | `#pragma pack(1)` PODs (`RecordHeader`, atom bodies, `Anchor`) + `static_assert` sizes + the `RecordType` / `SlideListInstance` enums |
| `ppt_io.{hpp,cpp}`               | `read(...)` helpers over `std::istream` (text atoms, the anchor rect, fixed structs) |
| `ppt_parser.{hpp,cpp}`           | `parse_tree(registry, files)` → walks the stream and builds the element tree |
| `ppt_element_registry.{hpp,cpp}` | Flat element store (copy of `doc_element_registry`) + text & frame side-payloads |
| `ppt_document.{hpp,cpp}`         | `internal::Document` subclass + the `ElementAdapter` |

`ElementRegistry` is a `vector<Element>` (id = index) with parent/child/sibling
ids and side maps for the text and frame payloads; `create_element` /
`create_text_element` / `create_frame_element` / `append_child` are the only
builders.

## Pipeline: how a `.ppt` becomes the element tree

1. **Wiring.** `LegacyMicrosoftFile` already detected `.ppt` (the `/PowerPoint
   Document` stream → `FileType::legacy_powerpoint_presentation`,
   `DocumentType::presentation`) and `open_strategy` routed it here; the
   `legacy_powerpoint_presentation` case in `LegacyMicrosoftFile::document()`
   returns `presentation::Document`.
2. **Resolve slides (persist directory).** `parse_tree` opens both required
   streams and hands them to `collect_slides(current_user, document)`, following
   the `[MS-PPT]` reading algorithm: read `CurrentUserAtom` from `/Current User`
   → walk the `UserEditAtom` chain newest→oldest, building the persist object
   directory (newest offset per id wins) → resolve the **live**
   `DocumentContainer` via `docPersistIdRef` → walk the slide list's
   `SlidePersistAtom`s **in presentation order**, resolving each `persistIdRef`
   to its `SlideContainer`. See *Design decisions* for why this is the only read
   path.
3. **Read text boxes per slide.** For each `SlideContainer` the parser descends
   the drawing and reads its text boxes (with positions) — see [Text boxes
   (frames)](#text-boxes-frames).
4. **Build the tree.** `parse_tree` makes one `slide`, one `frame` per text box
   (storing its anchor), and `build_paragraphs` hangs the box's text off the
   frame:

   ```
   root  (ElementType::root)
   └── slide              (ElementType::slide)        one per slide, in order
       └── frame          (ElementType::frame)        one per on-slide text box
           └── paragraph  (ElementType::paragraph)    split on 0x0D
               ├── text       (ElementType::text)
               └── line_break (ElementType::line_break)  for 0x0B in a paragraph
   ```
5. **Render.** HTML works through the generic renderer via the public `Slide` /
   `Frame` / `Paragraph` / `Text` API and our adapters.

## Text boxes (frames)

A `.ppt` slide is a *drawing of shapes*; each text box / placeholder is a shape
with its own position. `collect_slides` returns, per slide, the on-slide text
boxes in shape (z) order, each becoming a `frame`.

Per slide the parser descends `SlideContainer → DrawingContainer (0x040C) →
OfficeArtDgContainer (0xF002) → OfficeArtSpgrContainer (0xF003)` and walks the
`OfficeArtSpContainer` (0xF004) shapes. For each shape it reads:
- the **optional** `OfficeArtClientAnchor` (0xF010) → `read_client_anchor`
  (`SmallRectStruct`/`RectStruct`, master units = 1/576 inch), and
- the text in its `OfficeArtClientTextbox` (0xF00D).

Shapes with no text are dropped, so the group shape and pictures disappear.
`FrameAdapter` returns `anchor_type = at_page` and `x/y/width/height` as Measures
(master units / 576 → inches); a shape without an anchor yields a frame with no
position.

**First cut (current):** only **top-level** shapes — direct children of the root
`OfficeArtSpgrContainer`, whose anchors are already in the slide's master-unit
system. Nested-group coordinate transforms, non-grouped shapes, and
master-placeholder geometry inheritance are deferred — see [open
work](#1-frame-refinements). The verified record map of the drawing tree is in
[Reference](#reference-the-drawing-tree).

## Adapters

`ppt_document.cpp` implements the generic `ElementAdapter` (tree navigation,
copied from `doc_document.cpp`) plus `SlideAdapter` / `FrameAdapter` /
`ParagraphAdapter` / `TextAdapter` / `LineBreakAdapter`:
- `FrameAdapter`: `anchor_type = at_page`; `x/y/width/height` from the frame's
  anchor (or empty when absent); `z_index` / `style` empty.
- `SlideAdapter`: `slide_page_layout` → hardcoded 10"×7.5" (4:3); `slide_name` →
  empty; `slide_master_page` → `null_element_id`.
- `paragraph_text_style` / `text_style` set `font_size = 11pt` so empty
  paragraphs still have height.
- `Document::is_editable()` → `false`; `save(...)` → throws
  `UnsupportedOperation`.

## Binary format reference

Every record starts with an 8-byte `RecordHeader`:

```
RecordHeader {
  uint16 recVer : 4 ;          // 0xF marks a container
  uint16 recInstance : 12 ;
  uint16 recType ;
  uint32 recLen ;              // bytes of body that follow the header
}
```

`recVer == 0xF` marks a **container** (body is a sequence of records); otherwise
it's an **atom** with `recLen` bytes of payload.

| Record                 | Type   | Kind      | Purpose                                  |
|------------------------|--------|-----------|------------------------------------------|
| `CurrentUserAtom`      | 0x0FF6 | atom      | in `/Current User`; newest edit offset   |
| `UserEditAtom`         | 0x0FF5 | atom      | edit chain + persist directory offset    |
| `PersistDirectoryAtom` | 0x1772 | atom      | persist id → stream offset               |
| `DocumentContainer`    | 0x03E8 | container | top-level document                       |
| `SlideListWithText`    | 0x0FF0 | container | per-list slide refs (+ optional outline) |
| `SlidePersistAtom`     | 0x03F3 | atom      | one per slide; `persistIdRef` + order    |
| `SlideContainer`       | 0x03EE | container | a slide (drawing + placeholders)         |
| `MainMaster`           | 0x03F8 | container | master slide (skipped)                   |
| `Notes`                | 0x03F0 | container | notes page (skipped)                     |
| `TextHeaderAtom`       | 0x0F9F | atom      | type of the text block that follows      |
| `TextCharsAtom`        | 0x0FA0 | atom      | UTF-16 text (two bytes per code unit)    |
| `TextBytesAtom`        | 0x0FA8 | atom      | "compressed" text: one byte per char     |

The Office Art drawing records (`RT_Drawing` 0x040C and `0xF00*`/`0xF010`) used
for text boxes are listed with the full drawing-tree map in
[Reference](#reference-the-drawing-tree).

### Text decoding

- `TextCharsAtom`: `recLen / 2` UTF-16 code units → `u16string_to_string`.
- `TextBytesAtom`: each byte is one character value (0x00–0xFF).
- In-text control characters: `0x0D` = paragraph break, `0x0B` = vertical tab =
  manual line break — split on these like `doc_parser`. `0x09` (tab) kept; other
  control characters dropped (`clean_text`).

---

## Design decisions

**Slide resolution is persist-directory based (the single spec path).** The
persist directory gives correct slide **ordering** for incrementally-saved files
(where stream order ≠ presentation order) and picks the **live**
`DocumentContainer` rather than the first one in the stream. Verified on
`slides.ppt`: `/Current User` → `offsetToCurrentEdit=11646` → `UserEditAtom`
(`docPersistIdRef=1`, `offsetPersistDirectory=11606`, `offsetLastEdit=0`) → 2
slides in order with correct text.

**No scan/heuristic fallback — spec-justified.** Both `/Current User` (§2.1.1)
and `/PowerPoint Document` (§2.1.2) are *required* streams, every conformant
file has at least one `UserEditAtom` + `PersistDirectoryAtom`, and the reading
algorithm has no alternative branch. An earlier draft kept a stream-scan
fallback (first `DocumentContainer`, every `SlideContainer` in stream order,
plus an outline-vs-container "more text wins" heuristic); it was **removed** —
unreachable for conformant files and able to silently serve *wrong* results (a
stale `DocumentContainer`, wrong slide order). `collect_slides` returns an empty
presentation only for the one *optional* structure: a document with no
presentation slide list (§2.4.1). Every mandatory structure that can't be
resolved — empty edit chain, unresolved `docPersistIdRef`, a slide
`persistIdRef` not in the directory — **throws**.

**Two places hold slide text — and they are not equivalent.**
- The **outline** (`SlideListWithTextContainer`, §2.4.14.3) is **optional**
  (`DocumentContainer.slideList`, §2.4.1). When present it carries, per slide,
  the title/body **placeholder** text only — free text boxes are *never* in it.
- The **`SlideContainer`** (§2.5.1) is the authoritative source: on-slide text
  lives in the drawing's `ClientTextbox` records.

In LibreOffice-exported `.ppt` the outline is **empty** (verified on
`slides.ppt`: the `0x0FF0` lists hold zero text atoms), so there we read each
slide's text from its `SlideContainer`. But PowerPoint-authored placeholders
commonly carry **no inline text** in the `SlideContainer` and instead an
`OutlineTextRefAtom` (§2.9.78) pointing, by index, at the *i*-th `TextHeaderAtom`
block of that slide in the `SlideListWithTextContainer`. So we read the outline
too: `read_slide_list_text` collects, per slide (keyed by `persistIdRef`), the
ordered list of its `TextHeaderAtom` texts, and `gather_text` resolves an
`OutlineTextRefAtom` box against it. On-slide `ClientTextbox` text still wins
when present.

**`RT_SlideListWithText` recInstance disambiguates three lists.**
`MasterListWithTextContainer` (§2.4.14.1), `SlideListWithTextContainer`
(§2.4.14.3) and `NotesListWithTextContainer` (§2.4.14.6) share `recType =
RT_SlideListWithText` (0x0FF0); only `recInstance` tells them apart:

| recInstance | container                     | meaning             |
|-------------|-------------------------------|---------------------|
| `0x000`     | `SlideListWithTextContainer`  | presentation slides |
| `0x001`     | `MasterListWithTextContainer` | masters             |
| `0x002`     | `NotesListWithTextContainer`  | notes               |

An early draft had Slides/Master swapped, making the lookup read the *master*
list; fixed in `ppt_structs.hpp` (`SlideListInstance`).

**Sequential reading, no `tellg`.** The CFB-backed stream's `tellg()` returns
bogus values (it broke an early offset-tracking `read_children`). The parser
never depends on `tellg`: the caller `seekg`s to known offsets (from the persist
directory or a parent record), and child records are walked **forward** with a
`ChildCursor` — `read` header → `read`/recurse/`ignore` body — tracking the
bytes left in the container. A record that overruns its container throws,
keeping nested containers in sync or failing loudly.

**Fail early on malformed input.** Where the spec dictates what to expect,
unexpected input **throws** (matches the sibling `.doc` parser). We **throw** on:
a missing required stream; a wrong record type (`read_header` — so a truncated
read, whose garbage type won't match, also throws); a record that overruns its
container (`ChildCursor`); a missing **mandatory** child record — the
`DrawingContainer` / `OfficeArtDgContainer` / `OfficeArtSpgrContainer` of a
slide (`require_child`); an `OfficeArtClientAnchor` whose `recLen` is neither 8
nor 16; a non-decreasing (looping) `UserEditAtom` chain, an empty chain, an
unresolved `docPersistIdRef`, or a slide `persistIdRef` not in the persist
directory. We **pass through** (no throw) for values we don't model or that are
optional: an absent presentation slide list (0 slides), a shape with no
`OfficeArtClientAnchor` (unpositioned frame), nested groups and non-`Sp` records
in a group, and any non-text / unrecognised child record.

**Endianness.** Host byte order / LSB-first bit-fields assumed; shared `oldms/`
assumption, see [`../AGENTS.md`](../AGENTS.md). For `.ppt`: every record field is
read in host byte order (see the note in `ppt_io.hpp`), and the `RecordHeader`
recVer/recInstance bit-fields assume LSB-first allocation.

## Tests

- `ppt_empty` — `odr-public/ppt/empty.ppt`: 1 slide.
- `ppt_slides` — `odr-public/ppt/slides.ppt`: 2 slides, 2 positioned frames each
  (all `at_page` with `x/y/width/height`), distinct vertical positions, exact
  per-box text.

The non-empty fixture `slides.ppt` and reference-output HTML wiring are open
items (see below).

## Out of scope

Character/paragraph styles, fonts and colours; master and notes slides;
images/charts/tables and non-text shapes; animations/transitions; and
encrypted/obfuscated presentations.

---

# Open work

## 1. Frame refinements

The first cut reads only **top-level** shapes — direct children of the root
`OfficeArtSpgrContainer` — whose anchors are already in the slide's master-unit
coordinate system. The refinements below raise fidelity; each is optional and
independent.

- **1.1 Nested groups.** A shape nested inside a sub-group has its anchor
  expressed in **that group's** coordinate system, defined by the group's
  `OfficeArtFSPGR` (0xF009, `recVer 0x1`, `recLen 16`: `xLeft, yTop, xRight,
  yBottom`), not in slide units. To support it: recurse into nested
  `OfficeArtSpgrContainer` (0xF003), and for each descendant map its anchor from
  the group's `[xLeft..xRight] × [yTop..yBottom]` onto the group shape's own
  anchor rect in the parent, composing transforms down the nesting, before the
  `/576` conversion.
- **1.2 Non-grouped shapes.** `OfficeArtDgContainer` (0xF002) also has an
  optional direct `shape` (`OfficeArtSpContainer`, §2.2.13) for a shape not in a
  group — the current walk only iterates the `OfficeArtSpgrContainer`. Rare in
  real files, but read that child too for completeness.
- **1.3 Optional / inherited anchor.** A shape without an
  `OfficeArtClientAnchor` (0xF010) currently yields a frame with no position.
  PowerPoint placeholders often omit the anchor and inherit geometry from the
  matching placeholder shape on the **master slide** (resolve via
  `OfficeArtClientData.placeholderAtom` → the master's placeholder).
- **1.4 Origin / sign sanity check.** Field order and units are spec-confirmed
  (top/left/right/bottom; master units = 1/576 inch) and verified on
  `slides.ppt`. Still worth confirming the origin (top-left of the slide) and
  non-negative values on a second, independently produced real file.

## 2. Smaller shortcomings

- **2.1 Slide size is hardcoded.** `slide_page_layout` returns a fixed 10"×7.5"
  (`ppt_document.cpp`). The real size is `DocumentAtom.slideSize`
  (`RT_DocumentAtom` 0x03E9, the first child of the `DocumentContainer`) — a
  `PointStruct` in master units (`/576` → inches). Read it and feed the page
  layout; fall back to 10"×7.5" only if absent.
- **2.2 Reference-output HTML not wired.** `html_output_test` has no `ppt` case.
  Add reference HTML under
  `test/data/reference-output/odr-public/output/ppt/...` and wire it in (needs
  the `OpenDocument.test.output` submodule).
- **2.3 Fixture not committed.** `test/data/input/odr-public/ppt/slides.ppt`
  exists only in the local `odr-public` submodule working tree. It must be
  committed/pushed to the `OpenDocument.test` repo and the submodule pointer
  bumped, or CI can't see it (so `ppt_slides` would fail there).
- **2.4 No `OutlineTextRefAtom` fixture.** `OutlineTextRefAtom` resolution is
  implemented but **unexercised by any committed fixture** — all three current
  `.ppt` files are LibreOffice-authored with an empty outline (`grep` for the
  `00 00 9E 0F 04 00 00 00` header finds none). A PowerPoint-authored `.ppt`
  whose placeholders use the outline indirection is needed to regression-test
  the path. Pairs with §2.3.
- **2.5 Auto-field metacharacters dropped.** Slide-number / date / header /
  footer placeholders are separate records (`RT_*MetaCharAtom`) interleaved with
  the text; we ignore them, so e.g. a slide-number placeholder yields nothing.
  Low priority for "visible text only".
- **2.6 `slide_name` is empty.** Could return `"Slide N"` (index-based) so the
  HTML page/tab has a label, matching how other formats name pages.
- **2.7 Endianness** — shared `oldms/` shortcoming; see [`../AGENTS.md`](../AGENTS.md).

## Reference: the drawing tree

Inside each `SlideContainer` (0x03EE) is the Office Art (Escher) drawing that
holds the slide's text boxes:

```
SlideContainer (0x03EE)                            [MS-PPT] 2.5.1
└─ drawing = DrawingContainer (RT_Drawing, 0x040C) [MS-PPT] 2.5.13
   └─ OfficeArtDgContainer (0xF002)                [MS-ODRAW] 2.2.13
      └─ OfficeArtSpgrContainer (0xF003)           shape group       [MS-ODRAW] 2.2.16
         ├─ OfficeArtSpContainer (0xF004)          shape #1 (text box) [MS-ODRAW] 2.2.14
         │  ├─ OfficeArtFSPGR        (0xF009)      group bounds (group shape only) [MS-ODRAW] 2.2.38
         │  ├─ OfficeArtFSP          (0xF00A)      shape id/flags    [MS-ODRAW] 2.2.40
         │  ├─ OfficeArtFOPT         (0xF00B)      shape properties  [MS-ODRAW] 2.2.9
         │  ├─ OfficeArtClientAnchor (0xF010)      POSITION + SIZE   [MS-PPT] 2.7.1
         │  ├─ OfficeArtClientData   (0xF011)      placeholderAtom: title/body/… [MS-PPT] 2.7.3
         │  └─ OfficeArtClientTextbox(0xF00D)      the box's text    [MS-PPT] 2.9.76
         │     ├─ TextHeaderAtom (0xF9F)
         │     └─ TextCharsAtom/TextBytesAtom (0xFA0/0xFA8)
         └─ OfficeArtSpContainer (0xF004)          shape #2 …
```

- The `OfficeArt*` container/shape records are `[MS-ODRAW]`; the
  `DrawingContainer` and the *client* records (`0xF00D` textbox, `0xF010`
  anchor, `0xF011` data) are `[MS-PPT]`. `[MS-ODRAW]` §2.2.14 defers
  `clientAnchor`/`clientData`/`clientTextbox` to the host app.
- **`OfficeArtSpContainer` (0xF004) child order** per `[MS-ODRAW]` §2.2.14:
  `shapeGroup?` (`OfficeArtFSPGR`, group shapes only), `shapeProp`
  (`OfficeArtFSP`, 16 B), `shapePrimaryOptions?` (`OfficeArtFOPT`), …,
  **`clientAnchor?`**, `clientData?`, `clientTextbox?`. The parser matches by
  recType, so order only documents what to expect.
- **Anchor body** (`OfficeArtClientAnchor`, atom, `recLen == 8` or `16`), field
  order **top, left, right, bottom** (y, x, x, y):
  - `recLen == 8` → `SmallRectStruct` (`[MS-PPT]` 2.12.8): four **signed 2-byte**.
  - `recLen == 16` → `RectStruct` (`[MS-PPT]` 2.12.7): four **signed 4-byte**.

  `width = right - left`, `height = bottom - top`; master units → inches = `/576`.
- The first child `OfficeArtSpContainer` of the root spgr is the **group shape**
  itself (holds the `OfficeArtFSPGR`, has no `clientTextbox`); the parser drops
  it implicitly because it has no text.
