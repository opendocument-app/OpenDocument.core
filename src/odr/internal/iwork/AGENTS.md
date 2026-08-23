# AGENTS.md — `internal/iwork`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first, then
[`PLAN.md`](PLAN.md), which is where this module is going and in what order.
This file is what the landed stages decided, and why.

Landed: **stage 1** (detection and the container) and **stage 2** (Pages body
text). A `.pages` opens as a text document and renders its paragraphs.
Everything else in `PLAN.md` is still ahead.

## There is no spec, so a fixture is the citation

Apple has never published the `.proto` schemas and nothing is vendored under
`offline/documentation/`. Where `oldms/` writes `[MS-XLS] §2.4.1`, this module
writes `empty.pages Index/Document.iwa +0` — the byte layout verified against a
file in the repo is the only claim treated as fact.

Everything the engine reads by number lives in `iwork_types.hpp`, each constant
cited to the fixture it was read off. Read `numbers-parser`, `keynote-parser`,
`obriensp/iWorkFileFormat` and `libetonyek` for facts; **copy code from none of
them**.

**Fail soft on a type id we have not mapped, fail fast on broken framing.** The
root `AGENTS.md` says to throw where the spec dictates what to expect. Here
there is no spec, and an unknown type id means Apple shipped a version we have
not seen — a reader that throws on one cannot open next year's files. What does
throw: framing that overruns the file, a Snappy block that does not fill its
declared length, a varint that does not terminate, an identifier the package
does not hold, and text that is not UTF-8.

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

`iwork::Message` parses one level eagerly and leaves nested messages, strings
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

## Which app wrote the package comes off the root archive

`TP.DocumentArchive` is type 10000, verified on both `.pages` fixtures. The
extension is not consulted — a caller may have lost it — and neither is
`Metadata/Properties.plist`, which names an app version but not the app.

That is also why only `.pages` is detected. `iwork_numbers` and `iwork_keynote`
have `file_type_table.cpp` rows so a caller can name them and hand a file
picker their MIME types, but no capabilities: reading their root archive types
off a guess is exactly what this module does not do, and neither has a fixture
in the test data yet.

## Paragraphs come from the run table

A `TSWP.StorageArchive` holds its text as a few large strings plus run tables
parallel to it — index/value pairs for paragraph styles, character styles and
attachments. Paragraph boundaries are the **paragraph style table's**, not
every `\n` in the text. The two agree on both fixtures, but the table is what
says so, and `U+2028` is a line break *inside* a paragraph rather than a
paragraph boundary.

Run tables count in **UTF-16 code units** while the text is UTF-8, so
`iwork_parser.cpp` translates the indices in one pass over the text. An index
that lands mid-character is an error, not a rounding.

`U+FFFC` is where a drawable is anchored. Nothing reads drawables yet, so the
anchor is dropped rather than rendered as a glyph — see stage 4.

`empty.pages` is the regression that matters at this level: a body storage that
carries no text at all must produce an empty body, not an exception.

## Not read yet

`Index/DocumentStylesheet.iwa` (so `text_root_page_layout` is empty and every
style is the default), drawables and images, `Index/Tables/`, and everything
`PLAN.md` lists as deferred. `password_encrypted()` is not answered either: an
encrypted package is one whose `Index/Document.iwa` does not decompress, which
falls back to reporting the file as a zip.
