# `.ppt` (PowerPoint) support: design & open work

Shared `oldms/` conventions are in [`../AGENTS.md`](../AGENTS.md).

**Scope.** The visible text of each slide, positioned in its text boxes, plus
direct character formatting (font, size, bold, italic, underline, color) as
styled spans, plus JPEG and PNG pictures referenced by slide shapes. The
generic HTML renderer lays each slide out as positioned frames. No paragraph
styles, no master-inherited formatting or pictures, no master or notes pages,
charts, tables or animations.

**Specs.** `[MS-PPT]` (the PowerPoint stream), `[MS-ODRAW]` (the Office Art
drawing records) and the `[MS-CFB]` container.

## Module layout

| File (`oldms/presentation/`) | Role |
|---|---|
| `ppt_structs.hpp` | `#pragma pack(1)` PODs (`RecordHeader`, atom bodies, `Anchor`), the `RecordType`, `BlipInstance` and `SlideListInstance` enums |
| `ppt_io.{hpp,cpp}` | `read_*` helpers over `std::istream`: record headers, atoms, anchors, text |
| `ppt_style.{hpp,cpp}` | `parse_style_text_prop_atom` (StyleTextPropAtom to `TextCFRun`s), `resolve_style` (`TextCFRun` plus `StyleContext` to a style index), the 18pt default, the `StyleRegistry` that owns the resolved `TextStyle`s and font names |
| `ppt_parser.{hpp,cpp}` | `parse_tree(registry, style_registry, files)`: walks the stream, builds the tree, fills the `StyleRegistry` |
| `ppt_element_registry.{hpp,cpp}` | The element store plus text and frame payloads and a per-element style index |
| `ppt_document.{hpp,cpp}` | `internal::Document` subclass and the `ElementAdapter` |

The tree is `root → slide → frame (one per text box) → paragraph (split on
0x0D) → span (one per formatting run) → text`, with `line_break` (0x0B)
elements between spans.

## Design decisions

**Slides resolve through the persist directory, the one spec path.** The
persist directory gives the slide order for incrementally saved files (stream
order is not presentation order) and the live `DocumentContainer`. The read:
`CurrentUserAtom` (`/Current User`), then the `UserEditAtom` chain from newest
to oldest (the newest offset per id wins), then the live `DocumentContainer`
via `docPersistIdRef`, then the slide list's `SlidePersistAtom`s in
presentation order. There is no scan fallback: both streams are required
(§2.1.1, §2.1.2), every conformant file has a `UserEditAtom` and a
`PersistDirectoryAtom`, and a scan can serve a stale container or the wrong
order. `collect_slides` returns empty only for the one optional structure, a
missing slide list (§2.4.1).

**Two places hold slide text.** The outline (`SlideListWithTextContainer`,
§2.4.14.3) is optional and carries placeholder text only. The
`SlideContainer` (§2.5.1) is authoritative: on-slide text lives in the
drawing's `ClientTextbox` records. LibreOffice leaves the outline empty.
PowerPoint placeholders often carry an `OutlineTextRefAtom` (§2.9.78)
instead of inline text, which indexes the i-th `TextHeaderAtom` block of the
outline. The parser reads both and resolves the reference. Inline
`ClientTextbox` text wins.

**`RT_SlideListWithText` recInstance selects the list.** `0x000` is slides,
`0x001` masters, `0x002` notes (`SlideListInstance`).

**Sequential reading, no `tellg`.** The CFB-backed stream's `tellg()` is not
reliable. The caller `seekg`s to known offsets, and child records are walked
forward with a `ChildCursor` that tracks the bytes left in the container. A
record that overruns its container throws.

**Fail early.** Throw on: a missing required stream; a wrong or truncated
record type (`read_header`); a record that overruns its container; a missing
mandatory child (`DrawingContainer`, `OfficeArtDgContainer`,
`OfficeArtSpgrContainer`, via `require_child`); an `OfficeArtClientAnchor`
whose `recLen` is neither 8 nor 16; a looping or empty `UserEditAtom` chain;
an unresolved `docPersistIdRef`; a slide `persistIdRef` not in the directory.
Pass through: an absent slide list, a shape with no anchor (unpositioned
frame), nested groups and non-`Sp` records in a group, any unrecognised
child.

**Top-level shapes only.** The parser reads the direct children of the root
`OfficeArtSpgrContainer` plus the drawing's optional non-grouped shape
([MS-ODRAW] 2.2.13). Their anchors are in master units (1/576 inch). Shapes
with neither text nor a picture are dropped, so the group shape itself
disappears.

**Direct character formatting, resolved to styled spans.** Each text atom
stays raw until the `StyleTextPropAtom` (0x0FA1, §2.9.44) that follows it.
Its character runs (`TextCFRun`, counted in UTF-16 units and covering one
implicit final paragraph mark) split the text into spans. Each span and
paragraph stores an index into the `StyleRegistry`. Index 0 is the default
style, 18pt, which stands in for the unread master text styles.

- The paragraph-level runs precede the character runs in the atom. The
  parser skips them field by field (`TextPFException`, including the
  variable `tabStops`).
- Font names come from the `FontCollection` (0x07D5, inside `RT_Environment`
  0x03F2), indexed by each `FontEntityAtom`'s `recInstance`. The
  `StyleRegistry` owns the strings, so it reads them before any style is
  resolved.
- Colors are `ColorIndexStruct`s. Only explicit sRGB values (index `0xFE`)
  are used. Scheme indexes stay unset.

**Pictures resolve through the BLIP store.** A shape names its picture with
the `OfficeArtFOPT` property `pib` (picture shape) or `fillBlip` (picture
fill, which is how LibreOffice places pictures). `pib` wins. The value is a
one-based index into the `OfficeArtBStoreContainer` of the document's drawing
group. Each `OfficeArtFBSE` locates the BLIP in the `/Pictures` stream at
`foDelay`, or embeds it. A JPEG or PNG BLIP becomes an `image` element under
the shape's frame, served as an in-memory `odr::File`. WMF, EMF, PICT, DIB
and TIFF BLIPs are skipped. A shape flagged `fBackground` (`OfficeArtFSP`)
gets a full-slide anchor when it has none and moves before the slide's other
shapes, so it renders underneath.

**Slide size** comes from the `DocumentAtom` (master units), with 10in by
7.5in as the fallback. `slide_name` returns "Slide N" in presentation order.
`Document::is_editable()` is `false`. `save` throws.

**Text decoding.** `TextCharsAtom` is UTF-16 (`recLen/2` code units).
`TextBytesAtom` is one byte per character. `0x0D` is a paragraph break,
`0x0B` a manual line break, `0x09` is kept, other controls are dropped.

## Tests

- `OldMs.ppt_parse_style_text_prop_atom`: PF-run skipping, CFStyle bold and
  italic, fontRef and size, explicit sRGB color.
- `OldMs.ppt_empty` (`empty.ppt`): one slide.
- `OldMs.ppt_style_various` (`style-various-1.ppt`): 8 slides, positioned
  frames, per-box text, style assertions, the slide-6 background picture.
- `OldMsEncryption.*`: the header token.
- `html_output_test` compares the `.ppt` fixtures against the reference output.

No fixture exercises the `OutlineTextRefAtom` path.

## Drawing-tree reference

Every record starts with an 8-byte `RecordHeader` (`recVer:4`,
`recInstance:12`, `recType:u16`, `recLen:u32`). `recVer == 0xF` marks a
container. Otherwise it is an atom with `recLen` payload bytes.

Key records: `CurrentUserAtom` 0x0FF6, `UserEditAtom` 0x0FF5,
`PersistDirectoryAtom` 0x1772, `DocumentContainer` 0x03E8,
`SlideListWithText` 0x0FF0, `SlidePersistAtom` 0x03F3, `SlideContainer`
0x03EE, `MainMaster` 0x03F8 (skipped), `Notes` 0x03F0 (skipped),
`TextHeaderAtom` 0x0F9F, `TextCharsAtom` 0x0FA0, `TextBytesAtom` 0x0FA8,
`OutlineTextRefAtom` 0x0F9E.

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

- `OfficeArt*` records are `[MS-ODRAW]`. The client records (`0xF00D`,
  `0xF010`, `0xF011`) and `DrawingContainer` are `[MS-PPT]`. The parser
  matches by recType, so the child order above is only what to expect.
- Anchor body (`OfficeArtClientAnchor`), field order top, left, right, bottom:
  `recLen == 8` is `SmallRectStruct` (§2.12.8, four signed 2-byte values),
  `recLen == 16` is `RectStruct` (§2.12.7, four signed 4-byte values).
  `width = right − left`, `height = bottom − top`, master units to inches
  is `/576`.
- The first `OfficeArtSpContainer` of the root group is the group shape
  itself (it holds `OfficeArtFSPGR` and no `clientTextbox`).

# Open work

## 1. Frame refinements

- Nested groups. A shape in a sub-group has its anchor in that group's
  coordinate system (`OfficeArtFSPGR` 0xF009: `xLeft`, `yTop`, `xRight`,
  `yBottom`), not in slide units. Recurse into nested
  `OfficeArtSpgrContainer`s and map each anchor onto the group shape's own
  anchor rect in the parent, composing transforms down the nesting, before
  the `/576` conversion.
- Inherited anchor. A shape without an `OfficeArtClientAnchor` yields a frame
  with no position. PowerPoint placeholders often inherit geometry from the
  matching placeholder on the master slide (via
  `OfficeArtClientData.placeholderAtom`).

## 2. Smaller shortcomings

- Picture formats and locations. WMF, EMF, PICT, DIB and TIFF BLIPs are
  skipped (DIB needs a synthesized BMP header). Pictures on master slides are
  not rendered, and LibreOffice photo decks put each photo on a per-slide
  master.
- No `OutlineTextRefAtom` fixture. A PowerPoint-authored file that uses the
  outline indirection is needed.
- Master-inherited formatting. Placeholder text without direct formatting
  falls back to the 18pt default instead of the master's
  `TxMasterStyleAtom` styles, and scheme-indexed colors (index below `0xFE`)
  are dropped. Read the main master's text styles and color scheme.
- Auto-field metacharacters (`RT_*MetaCharAtom`: slide number, date, header,
  footer) are ignored.
- Endianness. See [`../AGENTS.md`](../AGENTS.md).
