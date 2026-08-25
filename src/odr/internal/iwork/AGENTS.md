# AGENTS.md — `internal/iwork`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first, then
[`PLAN.md`](PLAN.md), which is where this module is going and in what order.
This file is what the landed stages decided, and why.

Landed: **stage 1** (detection and the container), **stage 2** (Pages body
text) and **stage 5** (Keynote slides). A `.pages` opens as a text document and
renders its paragraphs; a `.key` opens as a presentation and renders each
slide's text boxes as frames where the geometry puts them. Everything else in
`PLAN.md` is still ahead.

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
sees them. `parse_pages_tree` and `parse_keynote_tree` therefore spend every
element and every byte of text against a `Budget` set far above what an
authored document reaches, which keeps a package built to expand a thrown
`std::runtime_error` rather than an allocation the process dies on.

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

`iwork_numbers` still has a `file_type_table.cpp` row with no capabilities so a
caller can name it and hand a file picker its MIME types; nothing decodes one
yet.

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
style is the default), images, `Index/Tables/`, Keynote masters
(`Index/TemplateSlide-*.iwa`, so `slide_master_page` is null) and presenter
notes, drawables anchored in a Pages text flow, and everything `PLAN.md` lists
as deferred. `password_encrypted()` is not answered either: an encrypted
package is one whose `Index/Document.iwa` does not decompress, which falls back
to reporting the file as a zip.
