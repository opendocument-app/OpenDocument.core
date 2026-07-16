# `.ppt` (PowerPoint) support — design & open work

The **why** and the roadmap; what is concretely done lives in the code. Shared
`oldms/` conventions are in [`../AGENTS.md`](../AGENTS.md).

**Scope.** Extract the **visible text of each slide, positioned in its text
boxes**, plus **direct character formatting** (font, size, bold, italic,
underline, color) as styled spans, through the abstract model so the generic
HTML renderer lays each slide out as positioned frames. No paragraph styles or
master-inherited formatting, master/notes pages, images, charts, tables, or
animations.

**Specs.** `[MS-PPT]` (the PowerPoint stream) + `[MS-ODRAW]` (the Office Art /
Escher drawing records) + `[MS-CFB]` container. Section numbers cited inline in
code and in the drawing-tree map below.

## Module layout (mirrors `../text`)

| File (`oldms/presentation/`) | Role |
|---|---|
| `ppt_structs.hpp` | `#pragma pack(1)` PODs (`RecordHeader`, atom bodies, `Anchor`) + `static_assert` sizes + `RecordType` / `SlideListInstance` enums |
| `ppt_io.{hpp,cpp}` | `read(...)` helpers over `std::istream` + `parse_style_text_prop_atom` (StyleTextPropAtom → `TextCFRun`s) |
| `ppt_parser.{hpp,cpp}` | `parse_tree(registry, files)` → walks the stream, builds the element tree |
| `ppt_element_registry.{hpp,cpp}` | Flat element store + text & frame side-payloads + per-element `TextStyle` map + font-name intern store |
| `ppt_document.{hpp,cpp}` | `internal::Document` subclass + the `ElementAdapter` |

Element tree shape: `root → slide → frame (one per text box) → paragraph (split
on 0x0D) → span (one per formatting run) → text`, with `line_break` (0x0B)
elements between spans.

## Design decisions

**Slide resolution is persist-directory based — the single spec path.** The
persist directory gives correct slide **ordering** for incrementally-saved files
(stream order ≠ presentation order) and picks the **live** `DocumentContainer`
rather than the first in the stream. The read follows the `[MS-PPT]` algorithm:
`CurrentUserAtom` (`/Current User`) → walk the `UserEditAtom` chain newest→oldest
building the persist directory (newest offset per id wins) → resolve the live
`DocumentContainer` via `docPersistIdRef` → walk the slide list's
`SlidePersistAtom`s in presentation order.

**No scan/heuristic fallback — spec-justified.** Both `/Current User` (§2.1.1) and
`/PowerPoint Document` (§2.1.2) are *required* streams, every conformant file has
a `UserEditAtom` + `PersistDirectoryAtom`, and the reading algorithm has no
alternative branch. An earlier stream-scan fallback (first `DocumentContainer`,
every `SlideContainer` in stream order, plus an outline-vs-container "more text
wins" heuristic) was **removed** — unreachable for conformant files and able to
silently serve *wrong* results (stale `DocumentContainer`, wrong order).
`collect_slides` returns empty only for the one *optional* structure: no
presentation slide list (§2.4.1). Every mandatory structure that can't be resolved
**throws**.

**Two places hold slide text — and they are not equivalent.**
- The **outline** (`SlideListWithTextContainer`, §2.4.14.3) is **optional**
  (§2.4.1). When present it carries, per slide, the **placeholder** text only —
  free text boxes are *never* in it.
- The **`SlideContainer`** (§2.5.1) is authoritative: on-slide text lives in the
  drawing's `ClientTextbox` records.

LibreOffice-exported `.ppt` leaves the outline **empty**, so there we read from the
`SlideContainer`. But PowerPoint-authored placeholders commonly carry *no* inline
text and instead an `OutlineTextRefAtom` (§2.9.78) pointing by index at the *i*-th
`TextHeaderAtom` block in the `SlideListWithTextContainer`. So we read the outline
too and resolve `OutlineTextRefAtom` boxes against it; on-slide `ClientTextbox`
text still wins when present.

**`RT_SlideListWithText` recInstance disambiguates three lists** that share
`recType` 0x0FF0: `0x000` = slides (`SlideListWithTextContainer`), `0x001` =
masters, `0x002` = notes. An early draft had slides/master swapped (read the
*master* list); fixed in `ppt_structs.hpp` (`SlideListInstance`).

**Sequential reading, no `tellg`.** The CFB-backed stream's `tellg()` returns
bogus values. The parser never depends on it: the caller `seekg`s to known offsets
(persist directory / parent record), and child records are walked **forward** with
a `ChildCursor` (`read` header → body → recurse/ignore) tracking the bytes left in
the container. A record that overruns its container throws, keeping nested
containers in sync or failing loudly.

**Fail early on malformed input** (matches the sibling `.doc` parser). **Throw**
on: a missing required stream; a wrong/truncated record type (`read_header`); a
record overrunning its container (`ChildCursor`); a missing **mandatory** child
(the `DrawingContainer`/`OfficeArtDgContainer`/`OfficeArtSpgrContainer`,
`require_child`); an `OfficeArtClientAnchor` whose `recLen` is neither 8 nor 16;
a looping/empty `UserEditAtom` chain, an unresolved `docPersistIdRef`, or a slide
`persistIdRef` not in the directory. **Pass through** (no throw) for the
unmodelled/optional: an absent slide list (0 slides), a shape with no anchor
(unpositioned frame), nested groups and non-`Sp` records in a group, any
unrecognised child.

**First cut: top-level shapes only.** Only direct children of the root
`OfficeArtSpgrContainer`, whose anchors are already in the slide's master-unit
system (master units = 1/576 inch → inches via `/576`). Nested-group transforms,
non-grouped shapes, and master-placeholder geometry inheritance are deferred
(open work §1). Shapes with no text are dropped, so the group shape and pictures
disappear.

**Direct character formatting only, resolved to styled spans.** Each text atom
is kept raw (undecoded) until the **`StyleTextPropAtom`** (0x0FA1, §2.9.44)
that most closely follows it; its character runs (`TextCFRun`, counts in
UTF-16 units covering the text plus one implicit final paragraph mark) split
the text into spans. Each span and paragraph stores a resolved `TextStyle` in
a registry side-map (paragraphs keep their first run's style for
empty-paragraph height). The non-obvious bits:
- The **paragraph-level runs precede the character runs** in the atom and are
  mask-skipped field by field (`TextPFException`, incl. the variable
  `tabStops`).
- **Font names** come from the `FontCollection` (0x07D5, inside
  `RT_Environment` 0x03F2), indexed by each `FontEntityAtom`'s `recInstance`,
  interned in the registry (`TextStyle::font_name` is a `const char *`).
- **Colors** are `ColorIndexStruct`s: only explicit sRGB values (index `0xFE`)
  are used; **scheme indexes are left unset** (they need the slide's color
  scheme — open work).
- Text without a `StyleTextPropAtom`, or characters past the last run, get a
  **default style of 18pt** — an approximation of the unread master text
  styles (`TxMasterStyleAtom`, open work). This replaces the old flat `11pt`
  placeholder.

**Slide size hardcoded 10"×7.5"** (`slide_page_layout`) — open work.
`Document::is_editable()` → `false`; `save` throws.

**Endianness.** Host byte order / LSB-first bit-fields assumed; shared `oldms/`
assumption, see [`../AGENTS.md`](../AGENTS.md).

### Text decoding

`TextCharsAtom` = UTF-16 (`recLen/2` code units); `TextBytesAtom` = 1 byte/char
(0x00–0xFF). In-text controls: `0x0D` = paragraph break, `0x0B` = manual line
break (split like `doc_parser`); `0x09` tab kept; other controls dropped
(`clean_text`).

## Tests

- `ppt_parse_style_text_prop_atom` — inline bytes: PF-run skipping (incl.
  mask-dependent fields), CFStyle bold/italic, fontRef/size, explicit sRGB
  color.
- `ppt_empty` (`empty.ppt`): 1 slide.
- `ppt_style_various` (`style-various-1.ppt`): 8 slides, positioned frames,
  per-box text, plus style assertions (44pt Arial black title; underlined blue
  32pt link text).

Fixture-commit / reference-HTML wiring / `OutlineTextRefAtom` fixture are open
(§2 below).

## Drawing-tree reference

Every record starts with an 8-byte `RecordHeader` (`recVer:4`, `recInstance:12`,
`recType:u16`, `recLen:u32`). `recVer == 0xF` marks a **container** (body is a
sequence of records); otherwise an **atom** with `recLen` payload bytes.

Key records: `CurrentUserAtom` 0x0FF6, `UserEditAtom` 0x0FF5,
`PersistDirectoryAtom` 0x1772, `DocumentContainer` 0x03E8, `SlideListWithText`
0x0FF0, `SlidePersistAtom` 0x03F3, `SlideContainer` 0x03EE, `MainMaster` 0x03F8
(skipped), `Notes` 0x03F0 (skipped), `TextHeaderAtom` 0x0F9F, `TextCharsAtom`
0x0FA0, `TextBytesAtom` 0x0FA8.

Inside each `SlideContainer` is the Office Art (Escher) drawing that holds the
text boxes:

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

- `OfficeArt*` records are `[MS-ODRAW]`; the *client* records (`0xF00D` textbox,
  `0xF010` anchor, `0xF011` data) and `DrawingContainer` are `[MS-PPT]`
  (`[MS-ODRAW]` §2.2.14 defers `clientAnchor`/`clientData`/`clientTextbox` to the
  host app). The parser matches by recType, so child order only documents what to
  expect.
- **Anchor body** (`OfficeArtClientAnchor`, atom), field order **top, left, right,
  bottom**: `recLen == 8` → `SmallRectStruct` (§2.12.8, four signed 2-byte);
  `recLen == 16` → `RectStruct` (§2.12.7, four signed 4-byte). `width = right −
  left`, `height = bottom − top`; master units → inches = `/576`.
- The first child `OfficeArtSpContainer` of the root spgr is the **group shape**
  itself (holds `OfficeArtFSPGR`, no `clientTextbox`) — dropped implicitly because
  it has no text.

---

# Open work

## 1. Frame refinements

The first cut reads only top-level shapes. Each refinement below raises fidelity
and is independent:

- **1.1 Nested groups.** A shape in a sub-group has its anchor in *that group's*
  coordinate system (the group's `OfficeArtFSPGR` 0xF009: `xLeft, yTop, xRight,
  yBottom`), not slide units. Recurse into nested `OfficeArtSpgrContainer` (0xF003)
  and map each descendant's anchor from `[xLeft..xRight]×[yTop..yBottom]` onto the
  group shape's own anchor rect in the parent, composing transforms down the
  nesting, before the `/576` conversion.
- **1.2 Non-grouped shapes.** `OfficeArtDgContainer` (0xF002) also has an optional
  direct `shape` (§2.2.13) for a shape not in a group — the current walk only
  iterates the `OfficeArtSpgrContainer`. Rare; read that child too.
- **1.3 Optional / inherited anchor.** A shape without an `OfficeArtClientAnchor`
  currently yields a frame with no position. PowerPoint placeholders often omit
  it and inherit geometry from the matching placeholder on the **master slide**
  (resolve via `OfficeArtClientData.placeholderAtom`).
- **1.4 Origin / sign sanity check.** Field order and units are spec-confirmed and
  verified on `slides.ppt`; still worth confirming origin (top-left) and
  non-negative values on a second, independently produced real file.

## 2. Smaller shortcomings

- **2.1 Slide size hardcoded.** Real size is `DocumentAtom.slideSize`
  (`RT_DocumentAtom` 0x03E9, first child of `DocumentContainer`) — a `PointStruct`
  in master units. Read it; fall back to 10"×7.5" only if absent.
- **2.2 Reference-output HTML not wired.** `html_output_test` has no `ppt` case;
  add reference HTML under `test/data/reference-output/…/output/ppt/` (needs the
  `OpenDocument.test.output` submodule).
- **2.3 Fixture not committed.** `slides.ppt` exists only in the local
  `odr-public` working tree; must be committed/pushed and the submodule pointer
  bumped, or CI can't see it.
- **2.4 No `OutlineTextRefAtom` fixture.** The resolution path is implemented but
  unexercised — all current `.ppt` files are LibreOffice-authored (empty outline).
  A PowerPoint-authored file using the outline indirection is needed. Pairs with
  §2.3.
- **2.5a Master-inherited formatting.** Placeholder text without direct
  formatting falls back to the 18pt default instead of the master's
  `TxMasterStyleAtom` styles; scheme-indexed colors (`ColorIndexStruct`
  index < 0xFE) are dropped for the same reason. Read the main master's text
  styles + color scheme to resolve both.
- **2.5 Auto-field metacharacters dropped.** Slide-number/date/header/footer
  placeholders (`RT_*MetaCharAtom`) are ignored. Low priority.
- **2.6 `slide_name` is empty.** Could return `"Slide N"` for the HTML page label.
- **2.7 Endianness** — shared `oldms/` shortcoming; see [`../AGENTS.md`](../AGENTS.md).
