# AGENTS.md — `internal/iwork`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first, then
[`PLAN.md`](PLAN.md), which is where this module is going and in what order.
This file is what the landed stages decided, and why.

Landed: **stage 1** (detection and the container), **stage 2** (Pages body
text), **stage 5** (Keynote slides), **stage 6** (the tile reader) and
**stage 7** (Numbers). A `.pages` opens as a text document and renders its
paragraphs and the tables its text anchors; a `.key` opens as a presentation
and renders each slide's text boxes as frames where the geometry puts them; a
`.numbers` opens as a spreadsheet, one odr sheet per Numbers table.
Everything else in `PLAN.md` is still ahead.

## The files split by framework, not by app

`iwork/` is flat, and one `ElementRegistry`, one `Document` and one `IworkFile`
serve all three apps. That is `odf/`'s shape rather than `ooxml/`'s, for a
reason the two analogies do not quite carry: Apple factored the format by
**framework**, so `TSWP` text, `TSD` drawables, `TST` tables and `TSS` styles
mean the same thing in a `.pages`, a `.key` and a `.numbers`, and only the
spine above them is per-app. Parsing splits along those seams — `iwork_table.cpp`
reads `TST` tiles into a `TableModel`, `iwork_text.cpp` turns a `TSWP` storage
into elements, and `iwork_parser.cpp` holds the three spines that call them —
never along app lines. `PLAN.md` carries the argument and the files each stage
adds.

The line between the two readers is the registry: `iwork_table.cpp` touches
none, so a tile is decoded as data and tested as data, and everything that
builds elements — a table's rows, a cell's own storage, a drawable a paragraph
anchors — lives in `iwork_text.cpp`, which is where the recursion between them
closes.

`Budget` is a header of its own because it is a parse-wide meter, not a text
one: the spines spend for slides, sheets and frames as the text layer spends
for paragraphs, runs and cells. **A table's extent is the file's word**, so
`parse_table` spends per row and per cell — a `TST.TableModelArchive` declaring
a grid of millions is a few bytes on the wire.

`reference_identifier` and `reference_identifiers` live in `iwork_archive.cpp`
with the rest of `TSP`: all three readers resolve references, and a copy per
reader is how that drifts.

## There is no spec, so a fixture is the citation

Apple has never published the `.proto` schemas and nothing is vendored under
`offline/documentation/`. Where `oldms/` writes `[MS-XLS] §2.4.1`, this module
writes `empty.pages Index/Document.iwa +0` — the byte layout verified against a
file in the repo is the only claim treated as fact.

Everything the engine reads by number lives in `iwork_types.hpp`, each constant
cited to the fixture it was read off. Read `numbers-parser`, `keynote-parser`,
`obriensp/iWorkFileFormat` and `libetonyek` for facts; **copy code from none of
them**.

A fixture is the citation for what the format *is*, not the only way to state
a test input. Shapes no fixture holds are built inline by
`test/src/internal/iwork/iwork_test_util.hpp`, which assembles the protobuf,
archive and Snappy layers. It is test-only and must never grow into a writer.

**Fail soft on a type id we have not mapped, fail fast on broken framing.** The
root `AGENTS.md` says to throw where the spec dictates what to expect. Here
there is no spec, and an unknown type id means Apple shipped a version we have
not seen — a reader that throws on one cannot open next year's files. What does
throw: framing that overruns the file, a Snappy block that does not fill its
declared length, a varint that does not terminate, an identifier the package
does not hold, and text that is not UTF-8.

A declared length is the file's word, so nothing is allocated or written
against one before it is known to fit: `snappy_decompress_block` caps its
reservation at what the compressed bytes could expand to and checks every tag
against what the block has left.

**A reference list is the file's word too.** Because the graph is walked by
identifier and `Package::object` memoises, a `TSP.Reference` list that names
one object a million times costs four bytes a repeat on the wire and a fresh
subtree — elements plus a copy of the storage's text — every time it is
resolved. The repeats are siblings rather than ancestors, so no cycle check
sees them. Every tree the module parses therefore spends each element and each
byte of text against a `Budget` set far above what an authored document
reaches, which keeps a package built to expand a thrown `std::runtime_error`
rather than an allocation the process dies on. A nested storage — a cell
holding a table holding a cell — is bounded a second way, by `Context::deeper`,
because depth costs stack rather than budget.

## No new dependencies

Two pieces would normally be a conan line each, and both would be wrong.

- **Snappy** — the `.iwa` framing is Apple's own (`0x00`, a little-endian
  24-bit compressed length, repeated to EOF), not Snappy's stream framing, so
  only the *block* decoder applies. `iwork_snappy.cpp` is that, in about a
  hundred lines.
- **Protobuf** — only the wire format is needed, and with no schemas a code
  generator has nothing to generate. Linking conan `protobuf` would drag it
  into the wasm, android and apple builds to replace `iwork_protobuf.cpp`.

Both stay inside `iwork/` until something else wants them; a wire reader with
one user has not earned a package.

## `Message` views the buffer it was read from

`Message` parses one level eagerly and leaves nested messages, strings
and packed fields as `std::string_view`s into the bytes it was handed. So the
buffer has to outlive it — `Component` owns its decompressed data behind a
`unique_ptr` for exactly that reason, and a `Message(some_temporary())` is a
dangling read rather than a compile error.

## An `.iwa` is an object graph, not a tree

A component file is a flat sequence of `(varint length, TSP.ArchiveInfo,
payload)`, and objects reference each other by identifier — across components.
So `Package` reads the component list from `Index/Metadata.iwa` first and
decompresses a component when something in it is asked for; `object(id)` loads
further components until the identifier turns up. Walking files in directory
order and hoping a tree falls out is the mistake to avoid.

**Component names are not file names.** `Index/Metadata.iwa` maps a component's
name to its locator, and the locator carries an identifier suffix often enough
that globbing for `CalculationEngine.iwa` finds it in one fixture and not in
the other.

The one place that skips the component list is detection: `IworkFile` reads
`/Index/Document.iwa` directly, because it runs on every zip a caller opens and
`Document` is the component whose file name never carries a suffix.

## Which app wrote the package — the root archive, then the components

`TP.DocumentArchive` is type 10000, verified on both `.pages` fixtures. The
extension is not consulted — a caller may have lost it — and neither is
`Metadata/Properties.plist`, which names an app version but not the app.

**Type ids are namespaced per app, and Keynote and Numbers collide.** Pages
numbers its archives from 10000, but `KN.DocumentArchive` and
`TN.DocumentArchive` are *both* type 1: `empty.key` and `empty.numbers`
`Index/Document.iwa` object 1 (iWork 14.4). So is the archive below each —
`KN.ShowArchive` and `TN.SheetArchive` are both type 2. The root archive alone
cannot tell the two apart, and a table mapping id → archive is only meaningful
once you already know the app. Only the `TS*` frameworks — `TSWP` text storage,
`TSD` drawables, `TST` tables — mean the same thing in all three.

What does tell them apart is the **component list**: Keynote writes one `Slide`
component per slide and neither Numbers nor Pages writes any. Checked against
all six fixtures, and pinned by the negative the rule rests on —
`IworkKeynote.a_numbers_package_is_not_keynote` opens both `.numbers` files.
That costs `Index/Metadata.iwa`, which detection otherwise avoids reading, so
it is only reached once the root archive has already come back as type 1.

A package whose root archive is type 1 and that holds no `Slide` component is
therefore a `.numbers` — by elimination, which is as strong a claim as the
format allows.

## A slide is drawables, and the drawable list is the one to walk

`KN.DocumentArchive` → `KN.ShowArchive` (field 2) → the slide tree (field 3),
whose repeated field 2 names one `KN.SlideNodeArchive` per slide in
presentation order; a node's field 2 is the `KN.SlideArchive` itself.

A slide names its title and body placeholders in fields 5 and 6 *and* repeats
its drawables in field 7. Read **field 7**: a placeholder the slide leaves
empty is named in field 5/6 but is not in the drawable list, so walking the
list renders what is on the slide and nothing else — `style-various-1.key`
slide 3 has an empty body and slide 4 empty title and body. (Field 42 repeats
the same list on all four slides; nothing needs both.)

A drawable is a `TSWP.ShapeArchive` (2011) or a `KN.PlaceholderArchive` (7),
which is the same shape one level deeper — its field 1. Either way field 2 of
the shape references the `TSWP.StorageArchive` that stage 2 already reads.
Every other drawable kind is skipped: `style-various-1.key` slide 4 carries a
`TST.TableInfoArchive` (6000) that stage 6 will pick up.

Geometry is `shape → TSD.ShapeArchive → TSD.DrawableArchive → geometry`, with
position in field 1 and size in field 2, both `TSP.Point`s of `float` points.
**A size of zero is a box that grows with its text**, not a box of zero height
— the free text box on slide 4 stores `(0, 0)` — so `shape_rect` leaves that
side of the `Rect` unset and the content decides. A *position* of zero is a
box against the slide's edge and is reported as the `0pt` it says: only the
extent is optional.

Slides carry no name in the archive, so they are numbered in presentation
order the way `oldms/presentation` numbers `.ppt` slides. The slide size is the
show's (field 4), 1024×768 points on both fixtures.

## A table is tiles, and a tile is packed cell records

`TST.TableInfoArchive` (6000) is the drawable; `TST.TableModelArchive` (6001)
behind it carries the name, the extent (**field 6 is rows and field 7 is
columns** — pinned by the `Wide` table of `style-various-1.numbers`, three rows
of six columns) and a `TST.DataStore`.

The data store holds the **tiles** — each covering a range of rows, `256` on
every fixture — and the side lists cells reference by key. Two are read: the
string list and the rich text list. Formats, formulas and styles are the
others, and none is read, which is why `0.075` renders as `0.075` where Numbers
shows `7.5%`.

A tile carries a `TST.TileRowInfo` per row that holds anything. Its cells are
packed back to back in one buffer, addressed by an `std::int16_t` per column
that is `-1` where the row has no cell there — so a sparse row costs nothing
and the reader gets its extent from the offsets rather than from a count.

A cell record is a twelve-byte header — a version byte, a type byte, six bytes
nothing reads, then a flags word — followed by the optional fields the flags
name, **in ascending bit order**. The four low bits are the value: a decimal128,
a double, a date's seconds, a string key, a rich text key. The rest name styles
and formats, so the walk stops at the value and never needs their widths.

`TST.TileRowInfo` also carries the same cells in an older encoding in two other
fields. The version byte says which to read; a record that declares a version
we have not seen reads as an **empty** cell rather than a wrong one, which is
the whole point of the pinned table.

**A number is a decimal, and stays one.** Apple stores cell values as IEEE 754
decimal128 precisely so `0.1 + 0.2` is `0.3`; converting through a `double` on
the way out would put back the rounding the type exists to avoid. The reader
divides the 113-bit coefficient down by ten and formats the digits against the
exponent, so what comes out is what the file says. `util::number::to_string_significant`
is not that tool — it is documented for CSS lengths and clamps at fifteen
decimals.

Cell types the fixtures pin: number, string, date, boolean, duration and rich
text. A date is seconds from **2001-01-01T00:00:00Z** and renders ISO 8601; a
duration renders `1d 2h 3m 4s`; a boolean renders `TRUE`/`FALSE`. Only a number
reports `ValueType::float_number`, which is what right-aligns a cell.

## A Numbers sheet is many tables, and an odr sheet is one grid

`TN.DocumentArchive` repeats its sheets in field 1; a `TN.SheetArchive` names
itself in field 1 and repeats its drawables in field 2. Each table drawable
becomes **an odr sheet of its own**, named `<sheet> – <table>` — taking only the
first table of a sheet would drop data with nothing to show for it. A drawable
that is not a table (a chart, a text box) is skipped; `sheet_first_shape` is
where those go when they are read.

A sheet's cells are looked up by coordinate rather than walked, so they are not
in its child chain — that chain is for shapes. `append_sheet_cell` sets only
the parent, as `odf` does. A position no tile carries has no element, and the
public `Sheet::cell` hands back an empty `SheetCell` for it.

The declared extent is the table's; the **content** extent is computed from the
cells, so a blank table of 22 rows renders as one empty row rather than 22.

## A Pages table hangs off the text that anchors it

The `U+FFFC` stage 2 drops is named by the storage's **attachment run table**
(field 9), whose entries pair a UTF-16 index with the object anchored there —
verified against the three anchors of `style-various-1.pages` at indices 18,
460 and 469. That object is a `TSWP.DrawableAttachmentArchive` (2003) whose
field 1 is the drawable; where that drawable is a `TST.TableInfoArchive` the
table is read with the same reader Numbers uses.

The table is emitted **after the paragraph its anchor sits in**, as a sibling
rather than a child: the anchor is an inline character but a table is not
something a paragraph can hold. Anchors that resolve to something else — a
table of contents (2241), an image (3005) — are skipped, and the anchor stays
dropped from the text.

A Pages table's cells are **rich text**: the tile holds a key into the rich
text list, whose entry references a `TST.RichTextPayloadArchive` (6218) whose
field 1 is an ordinary `TSWP.StorageArchive`. So a cell's paragraphs come from
the same walk a body does — which is why that walk takes a depth bound: a cell
holds a storage which may hold a table again.

## Paragraphs come from the run table

A `TSWP.StorageArchive` holds its text as a few large strings plus run tables
parallel to it — index/value pairs for paragraph styles, character styles and
attachments. Paragraph boundaries are the **paragraph style table's**, not
every `\n` in the text. The two agree on both fixtures, but the table is what
says so, and `U+2028` is a line break *inside* a paragraph rather than a
paragraph boundary.

**The paragraph mark differs by app**: Pages ends a paragraph with `\n` and
Keynote with `\r`. Neither is what says where a paragraph starts — the run
table is — so the mark only decides which trailing byte belongs to the
paragraph it ends.

Run tables count in **UTF-16 code units** while the text is UTF-8;
`util::string::utf16_offsets` translates the indices in one pass. An index that
lands mid-character is an error, not a rounding.

`U+FFFC` is where a drawable is anchored. Nothing reads drawables yet, so the
anchor is dropped rather than rendered as a glyph — see stage 4.

`empty.pages` is the regression that matters at this level: a body storage that
carries no text at all must produce an empty body, not an exception. `empty.key`
is its Keynote counterpart — one slide whose placeholders are empty and whose
drawable list is therefore absent, which must come back as one empty slide.

## Not read yet

`Index/DocumentStylesheet.iwa` (so `text_root_page_layout` is empty and every
style is the default), images, Keynote masters (`Index/TemplateSlide-*.iwa`, so
`slide_master_page` is null) and presenter notes, non-table drawables in a
Pages text flow or on a Numbers sheet, number formats, `CalculationEngine.iwa`
(a cell shows what the app last computed, which is the position `.xls` takes),
merged cell ranges (they live outside the tiles, so a merge renders as separate
cells), and everything `PLAN.md` lists as deferred. `password_encrypted()` is not answered either: an encrypted
package is one whose `Index/Document.iwa` does not decompress, which falls back
to reporting the file as a zip.
