# RTF plan

The open work, in order. What the module does today is in
[`AGENTS.md`](AGENTS.md).

The public enum is mirrored by every binding already, so no stage below needs
binding work. Everything arrives through the abstract document model. The
table row in `file_type_table.cpp` declares `open`, `translate_html` and
`color_scheme`. `odr_test` fails if the declared capabilities exceed what the
engine does, so the row moves with the stages. There is no writer, in any
stage.

## Stage 2: character formatting

- `{\fonttbl{\fN\fcharsetN Name;}…}` and `{\colortbl;\redN\greenN\blueN;…}`
  are destinations. Parse both into a `StyleRegistry` next to the element
  registry, as `oldms/text/doc_style.*` does. Entry 0 of the colour table is
  the empty "auto" colour.
- `\b \i \ul \ulnone \strike \scaps \caps \sub \super \nosupersub`, `\fN`,
  `\fsN` (half-points), `\cfN`, `\cbN`, `\highlightN` and `\plain` (reset)
  become a `TextStyle`. Resolve them to spans as `.doc` does: equal property
  sets share one resolved style, adjacent equal-style runs merge.
- Intern font names in the `StyleRegistry` and never mutate them, so
  `TextStyle::font_name` (`const char *`) stays valid.
- The encoding chain per run: `\ansi`, `\mac`, `\pc`, `\pca` set the document
  default, `\ansicpgN` overrides it, the font's `\fcharsetN` or `\cpgN` sets it
  per font, and `\fN` selects the font. `\fcharset128`, `129`, `134` and `136`
  (Shift-JIS, EUC-KR, GB2312, Big5) are named but not decodable in
  `internal/encoding`, so their `\'hh` runs degrade. A writer that emits CJK
  usually emits `\uN` too, and `\uN` decodes regardless.

## Stage 3: paragraph, section, page

- `\pard` (reset), `\ql \qc \qr \qj`, `\liN \riN \fiN`, `\sbN \saN \slN`
  become a `ParagraphStyle`. Twips throughout. `Measure` carries the unit.
- `\paperwN \paperhN \marglN \margrN \margtN \margbN \lndscpsxn` become the
  `PageLayout` that `text_root_page_layout` returns empty today.
- `\sect` and `\sectd`: there is one page layout, so a section break is a page
  break and the first section's geometry wins.
- `{\listtext …}` and `{\pntext …}` carry the rendered bullet or number. Emit
  them as literal text. Real `ListItem` elements need `{\*\listtable}`,
  `{\*\listoverridetable}` and `\lsN\ilvlN` resolution, which is deferred.

## Stage 4: tables

*Table Definitions*: there is no table group. A row is a run of paragraphs
that carry `\intbl`, cells end with `\cell`, the row with `\row`, and the
geometry is a `<tbldef>`: `\trowd` followed by one `\cellxN` per cell, where N
is the cell's cumulative right edge in twips.

- Buffer the row. Word 97 wrote `<tbldef>` before the cells, Word 2002 to
  2007 write it after and repeat it before, and the grammar admits all three.
  Apply the last `<tbldef>` seen when `\row` fires. A repeated identical
  `\trowd` is idempotent, not a second row.
- Column widths are differences of successive `\cellxN`. `\trleftN` is the
  row's left edge.
- `\clmgf` / `\clmrg` (horizontal) and `\clvmgf` / `\clvmrg` (vertical) map
  onto `table_cell_span` plus `table_cell_is_covered`. The `f` variant starts a
  merged region, the bare one continues it, so the span is known only when the
  row ends.
- Consecutive rows with the same `<tbldef>` are one table. `\itapN` gives the
  nesting depth, with `\nestcell`, `\nestrow` and `{\*\nesttableprops}` for the
  inner levels. Do depth 0 first and treat deeper rows as their own table.
- Borders and shading (`\clbrdr*`, `\trbrdr*`, `\clcbpat`) go into
  `TableStyle` and `TableCellStyle` last.

## Stage 5: images

- `{\pict …}` carries hex data by default or `\binN` plus raw bytes. Decode to
  a `std::string`, wrap it in `common::MemoryFile`, hand it out through
  `ImageAdapter::image_file` with `image_is_internal() == true`.
- `\pngblip` and `\jpegblip` render. `\emfblip`, `\wmetafileN`, `\macpict`,
  `\pmmetafileN`, `\dibitmapN` and `\wbitmapN` have no decoder, so drop the
  frame rather than emit a broken `<img>`.
- `\nonshppict` stays in the discard table. `{\*\shppict{\pict …}}{\nonshppict{\pict …}}`
  is the same image twice.
- `\picwgoalN` / `\pichgoalN` are the display size in twips. `\picwN` /
  `\pichN` are the intrinsic size and the fallback. `\picscalexN` /
  `\picscaleyN` are percentages on top.
- Shapes (`{\shp…}`) and embedded OLE (`{\object\objemb}`) stay out of scope.
  The destination rule skips them.
- A render test needs a real fixture in `test/data/input` plus the
  reference-output regen, because a picture cannot be written inline.

## Deferred

- Headers, footers, footnotes, endnotes and comments. Each is its own
  destination, and the element model has nowhere to put them. `.doc` drops
  them for the same reason.
- Real list items (stage 3).
- The style sheet (`{\stylesheet}`) and `\sN` / `\csN` inheritance. Direct
  formatting first. Worth more here than in `.doc`, because rtf writers use
  styles for heading fonts.
- Math (`{\mmath …}`), drawing objects, bidi (`\rtlch` / `\ltrch` and the
  `\af*` chain), revision marks, East Asian composite fonts.
- Multibyte `\'hh` runs, blocked on the multibyte tier of `internal/encoding`.
- `\cellxN` outside a table. The spec invites rejection. Ignore it.
