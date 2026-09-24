# AGENTS.md — `internal/iwork`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file holds the
rules of the module. [`PLAN.md`](PLAN.md) lists the open work.

A `.pages` opens as a text document and renders its paragraphs and the tables
its text anchors. A `.key` opens as a presentation and renders the text boxes
of each slide as frames. A `.numbers` opens as a spreadsheet, one odr sheet per
Numbers table, with the values the app last computed. The module reads iWork
'13 and later only. It has no writer, and the document stays read-only.

## The files split by framework, not by app

`iwork/` is flat: one `ElementRegistry`, one `Document` and one `IworkFile`
serve all three apps. Apple factored the format by framework, so `TSWP` text,
`TSD` drawables and `TST` tables mean the same thing in a `.pages`, a `.key`
and a `.numbers`. Only the spine above them is per app. The parsers split
along those seams:

| File | Reads |
|---|---|
| `iwork_table.cpp` | `TST` tiles into a `TableModel`. Touches no registry, so a tile is decoded and tested as data. |
| `iwork_text.cpp` | A `TSWP` storage into elements. Everything that builds elements (rows, a cell's own storage, an anchored drawable) lives here, where the recursion closes. |
| `iwork_parser.cpp` | The three spines (body, show, sheets), calling the two above. |
| `iwork_archive.cpp` | `TSP`: the package, components, objects, and `reference_identifier(s)`. All three readers resolve references through it. |
| `iwork_budget.hpp` | A parse-wide meter. The spines spend for slides, sheets and frames. The text layer spends for paragraphs, runs and cells. |

The module is named `iwork`, not `apple`, because `apple/` at the repo root is
the bindings. The namespace is `odr::internal::iwork`. Three `FileType`s,
`iwork_pages`, `iwork_numbers` and `iwork_keynote`, share the one engine.

## There is no spec, so a fixture is the citation

Apple has never published the `.proto` schemas, and nothing is vendored under
`offline/documentation/`. Where `oldms/` cites `[MS-XLS] §2.4.1`, this module
cites a fixture and an offset: `empty.pages Index/Document.iwa +0`. A byte
layout verified against a file in the repo is the only claim treated as fact.

Every number the engine reads lives in `iwork_types.hpp`, each cited to the
fixture it was read off. Read `numbers-parser`, `keynote-parser`,
`obriensp/iWorkFileFormat` and `libetonyek` for facts. Copy code from none of
them.

The fixtures are `test/data/input/odr-public/pages/{empty,style-various-1}.pages`
(iWork 13.2) and `key/` and `numbers/` with the same names (iWork 14.4, written
to order on macOS). Shapes no fixture holds are built inline by
`test/src/internal/iwork/iwork_test_util.hpp`, which assembles the protobuf,
archive and Snappy layers. It is test-only and must never grow into a writer.

## Fail soft on an unmapped type id, fail fast on broken framing

The root rule says to throw where the spec dictates what to expect. Here there
is no spec. An unknown type id means Apple shipped a version we have not
mapped, and a reader that throws on one cannot open next year's files. So an
unmapped type id is skipped.

What does throw: framing that overruns the file, a Snappy block that does not
fill its declared length, a varint that does not terminate, an identifier the
package does not hold, and text that is not UTF-8. The spine entry points
throw on a wrong root type, because a mismatch there leaves nothing to render.
`read_table` is per drawable, so a wrong archive there loses one table and not
the document.

## Every declared size is the file's word

- A `TST.TableModelArchive` declaring millions of cells is a few bytes on the
  wire. `parse_table` spends per row and per cell against the `Budget`.
- A tile list may name one tile any number of times, and `Package::object`
  hands every repeat back from its cache. So `read_table` spends per cell and
  per byte of cell text as `read_tile` produces them, not once a model exists.
  `TableCache` reads each table once per parse, so a repeated table costs
  elements, not a second decode.
- A `TSP.Reference` list that names one object a million times costs four
  bytes per repeat and a fresh subtree per resolution. The repeats are siblings,
  so no cycle check sees them. Every tree spends each element and each byte of
  text against the `Budget`, whose limits sit far above an authored document,
  so a package built to expand throws `std::runtime_error` instead of an
  allocation the process dies on.
- A nested storage (a cell holding a table holding a cell) is bounded by
  `Context::deeper`, because depth costs stack rather than budget.
- `snappy_decompress_block` caps its reservation at what the compressed bytes
  can expand to and checks every tag against what the block has left.

## No new dependencies

- Snappy: the `.iwa` framing is Apple's own (`0x00`, a little-endian 24-bit
  compressed length, repeated to EOF), not Snappy's stream framing. Only the
  block decoder applies, and `iwork_snappy.cpp` is that.
- Protobuf: only the wire format is needed, and with no schemas a code
  generator has nothing to generate. Linking conan `protobuf` would drag it
  into the wasm, android and apple builds. `iwork_protobuf.cpp` reads fields
  by number.

Both stay inside `iwork/` until a second user appears.

## `Message` views the buffer it was read from

`Message` parses one level eagerly and leaves nested messages, strings and
packed fields as `std::string_view`s into the bytes it was handed. The buffer
must outlive it. `Component` owns its decompressed data behind a `unique_ptr`
for that reason, and `Message(some_temporary())` is a dangling read, not a
compile error.

## An `.iwa` is an object graph, not a tree

A component is a flat sequence of `(varint length, TSP.ArchiveInfo, payload)`.
Objects reference each other by identifier across components. `Package` reads
the component list from `Index/Metadata.iwa` first and decompresses a
component when something in it is asked for. `object(id)` loads further
components until the identifier turns up.

Component names are not file names. The locator in `Index/Metadata.iwa` often
carries an identifier suffix (`CalculationEngine-1732585.iwa`), so a glob for
`CalculationEngine.iwa` finds it in one fixture and not in the other.

Detection is the one reader that skips the component list. `IworkFile` reads
`/Index/Document.iwa` directly, because it runs on every zip a caller opens
and `Document` is the component whose file name never carries a suffix.

## Which app wrote the package

`TP.DocumentArchive` is type 10000 on both `.pages` fixtures. The extension is
not consulted, because a caller may have lost it. `Metadata/Properties.plist`
names an app version but not the app.

Type ids are namespaced per app, and Keynote and Numbers collide.
`KN.DocumentArchive` and `TN.DocumentArchive` are both type 1, and
`KN.ShowArchive` and `TN.SheetArchive` are both type 2. So a table mapping id
to archive is only meaningful once the app is known, and only the `TS*`
frameworks mean the same thing in all three.

The component list tells the two apart: Keynote writes one `Slide` component
per slide, and neither Numbers nor Pages writes any (`app_by_components` in
`iwork_file.cpp`). A root archive of type 1 with no `Slide` component is a
`.numbers`, by elimination. `IworkKeynote.a_numbers_package_is_not_keynote`
pins the negative.

## A slide is drawables, and the drawable list is the one to walk

`KN.DocumentArchive` → `KN.ShowArchive` (field 2) → the slide tree (field 3),
whose repeated field 2 names one `KN.SlideNodeArchive` per slide in
presentation order. A node's field 2 is the `KN.SlideArchive`.

A slide names its title and body placeholders in fields 5 and 6 and repeats
its drawables in field 7. Read field 7. A placeholder the slide leaves empty is
named in field 5 or 6 but is not in the drawable list, so the list renders what
is on the slide and nothing else (`style-various-1.key` slides 3 and 4).

A drawable is a `TSWP.ShapeArchive` (2011) or a `KN.PlaceholderArchive` (7),
which wraps the same shape in its field 1. Field 2 of the shape references the
`TSWP.StorageArchive`. Every other drawable kind is skipped.

Geometry is `shape → TSD.ShapeArchive → TSD.DrawableArchive → geometry`, with
the position in field 1 and the size in field 2, both `TSP.Point`s of `float`
points. A size of zero is a box that grows with its text, so `shape_rect`
leaves that side of the `Rect` unset. A position of zero is a box against the
slide's edge and is reported as `0pt`.

Slides carry no name, so they are numbered in presentation order, as
`oldms/presentation` numbers `.ppt` slides. The slide size is the show's
(field 4).

## A table is tiles, and a tile is packed cell records

`TST.TableInfoArchive` (6000) is the drawable. `TST.TableModelArchive` (6001)
behind it carries the name, the extent (field 6 is rows and field 7 is
columns, pinned by the `Wide` table of `style-various-1.numbers`) and a
`TST.DataStore`.

The data store holds the tiles, each covering a range of rows, and the side
lists that cells reference by key. Two lists are read: the string list and the
rich text list. Formats, formulas and styles are not read, so `0.075` renders
as `0.075` where Numbers shows `7.5%`.

A tile carries a `TST.TileRowInfo` per row that holds anything. Its cells are
packed back to back in one buffer, addressed by an `std::int16_t` per column
that is `-1` where the row has no cell. A sparse row costs nothing, and the
reader gets its extent from the offsets.

A cell record is a twelve-byte header (a version byte, a type byte, six bytes
nothing reads, a flags word) followed by the optional fields the flags name, in
ascending bit order. The five low bits are the value: a decimal128, a double, a
date's seconds, a string key, a rich text key. The rest name styles and
formats, so the walk stops at the value and never needs their widths.

`TST.TileRowInfo` also carries the same cells in an older encoding in two other
fields. The version byte says which to read. A record with a version we have
not seen reads as an empty cell, not a wrong one
(`IworkNumbers.a_record_we_have_not_mapped_is_an_empty_cell`). The framing
below it still fails fast (`a_row_that_contradicts_its_own_offsets_throws`).

A number is a decimal, and stays one. Apple stores cell values as IEEE 754
decimal128 so that `0.1 + 0.2` is `0.3`. The reader divides the 113-bit
coefficient down by ten and formats the digits against the exponent.
`util::number::to_string_significant` is not that tool: it is for CSS lengths
and clamps at fifteen decimals.

Cell types the fixtures pin: number, string, date, boolean, duration and rich
text. A date is seconds from 2001-01-01T00:00:00Z and renders ISO 8601. A
duration renders `1d 2h 3m 4s`. A boolean renders `TRUE` or `FALSE`. Only a
number reports `ValueType::float_number`, which right-aligns the cell.

## A Numbers sheet is many tables, and an odr sheet is one grid

`TN.DocumentArchive` repeats its sheets in field 1. A `TN.SheetArchive` names
itself in field 1 and repeats its drawables in field 2. Each table drawable
becomes an odr sheet of its own, named `<sheet> – <table>`, because taking
only the first table would drop data. A drawable that is not a table is
skipped. `sheet_first_shape` is where those go once they are read.

A sheet's cells are looked up by coordinate, so they are not in its child
chain. `append_sheet_cell` sets only the parent, as `odf` does. A position no
tile carries has no element, and the public `Sheet::cell` returns an empty
`SheetCell` for it. The content extent is computed from the cells, so a blank
table of 22 rows renders as one empty row.

## A Pages table hangs off the text that anchors it

A `U+FFFC` in the text is named by the storage's attachment run table (field
9), whose entries pair a UTF-16 index with the object anchored there. That
object is a `TSWP.DrawableAttachmentArchive` (2003) whose field 1 is the
drawable. Where the drawable is a `TST.TableInfoArchive`, the table is read
with the same reader Numbers uses. Any other anchored object is skipped, and
the anchor stays dropped from the text.

The table is emitted after the paragraph its anchor sits in, as a sibling,
because a paragraph cannot hold a table.

A Pages table cell is rich text: the tile holds a key into the rich text list,
whose entry references a `TST.RichTextPayloadArchive` (6218) whose field 1 is
an ordinary `TSWP.StorageArchive`. So a cell's paragraphs come from the same
walk a body does, and that walk takes a depth bound.

## Paragraphs come from the run table

A `TSWP.StorageArchive` holds its text as a few large strings plus run tables
parallel to it. Paragraph boundaries are the paragraph style table's, not
every `\n` in the text. `U+2028` is a line break inside a paragraph.

The paragraph mark differs by app: Pages ends a paragraph with `\n` and Keynote
with `\r`. The run table says where a paragraph starts. The mark only decides
which trailing byte belongs to the paragraph it ends.

Run tables count in UTF-16 code units while the text is UTF-8.
`util::string::utf16_offsets` translates the indices in one pass. An index
that lands mid-character is an error.

`empty.pages` pins that a body storage with no text produces an empty body,
not an exception. `empty.key` pins that one slide with empty placeholders and
no drawable list comes back as one empty slide.

## Not read yet

`Index/DocumentStylesheet.iwa` (so `text_root_page_layout` is empty and every
style is the default), images, Keynote masters (`Index/TemplateSlide-*.iwa`,
so `slide_master_page` is null) and presenter notes, non-table drawables in a
Pages text flow or on a Numbers sheet, number formats, `CalculationEngine.iwa`
(a cell shows what the app last computed, as `.xls` does), and merged cell
ranges (they live outside the tiles, so a merge renders as separate cells).
`password_encrypted()` is not answered: an encrypted package is one whose
`Index/Document.iwa` does not decompress, and it is reported as a zip.
[`PLAN.md`](PLAN.md) has the rest.
