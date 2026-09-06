# The v7 public API

Status: **proposed.** What the next major changes in `src/odr/*.hpp`, why, and
in which pull requests. Nothing here is a new feature — every item removes a
second way to do something we already do, or lifts a constraint that only a
major can lift.

## Why a major, and why this one

Three things are only possible here:

1. **The removals we deliberately deferred.** `README.md`'s *Open tasks* names
   them and says outright they are "the breaking change this deliberately is
   not": the inert `GlobalParams` pair and every mirror of it, and the inert
   libmagic build options.
2. **The enum ordinals.** `FileType`, `FileCategory`, `ElementType`,
   `HtmlResourceType` and `HtmlViewportMode` are append-only — not by taste but
   because JNI resolves them by `ordinal()` (`jni/src/jni_style.cpp:91`) and the
   wasm and apple mirrors do the same by position. Their headers say so in
   comments. A major is when that can be made a rule the build enforces instead
   of a comment.
3. **One road per operation.** Below.

Already queued as breaking for the same cut: #826 (C++23), #821 (the drawing
elements collapsed into `frame`), and the `TextAlign` `start`/`end` entry
already under `## Unreleased`.

## Finding 1 — twenty-two ways to decode a file

Counted from the headers:

| Entry point | Count |
|---|---|
| `odr::open(File\|path, [FileType\|DecodePreference], Logger)` | 6 |
| `DecodedFile(File\|path, [FileType\|DecodePreference], Logger)` | 6 |
| `DocumentFile::from_disk` / `::from_memory` | 2 |
| `DocumentFile(File)` / `DocumentFile(path)` | 2 |
| `DocumentFile::type(File\|path)` / `::meta(File\|path)` | 4 |
| `CsvFile::from_file` / `CsvFile::with_options` | 2 |

Twenty of those reach `open_strategy`; the two `CsvFile` ones do not.

And twelve answer *what is this file*: `odr::list_file_types` ×2 and
`odr::mimetype` ×2, duplicated exactly by `DecodedFile::list_file_types` ×2 and
`DecodedFile::mimetype` ×2, plus `DocumentFile::type` ×2 and `::meta` ×2.

Three of these are not merely redundant but *literally* the same code:

- **`odr::open` is a pure forward.** `odr.cpp:203-230` is six one-line
  `return DecodedFile(...)`. Two public spellings, one implementation — and the
  bindings inherited the split: JNI exposes both (`jni_core.cpp:227` calls
  `odr::open`, `jni_file.cpp:93` calls the constructor), and Python binds all
  six `open` overloads *and* the `DocumentFile` factories.
- **`DocumentFile::type`/`::meta` decode the whole file** to read one field
  (`file.cpp:384-399`), and throw for anything that is not a document.
- **`open_strategy::open_document_file` duplicates `open_file`'s cascade.**
  Same engines in the same order for zip and cfb, differing only in the
  fallback, and both ends throw `NoDocumentFile` — `as_document_file()` throws
  it too (`file.cpp:278`). `open(f).as_document_file()` is a drop-in for
  `DocumentFile(f)`, and ~90 lines of the 737 in `open_strategy.cpp` exist only
  to say the same thing twice.

The one **substantive** asymmetry underneath the noise: `CsvFile::from_file`
does not go through `open_strategy` at all (`file.cpp:331` constructs
`internal::csv::CsvFile` directly). `DecodePreference` is the generic "how to
decode" knob and `CsvOptions` is a second, format-specific one on a different
road — so a caller who wants to name a separator cannot get there through
`open`. That is the part worth fixing; the overload count is a symptom.

Worth noting the wasm binding already converged on the shape we want —
`odr.open(bytes, { name })`, `odr.openAs`, `odr.detect`, three functions and no
constructors. C++ is the layer that never got the cleanup.

### Target

```cpp
/// How to decode a file. An unset field is detected.
struct DecodeOptions final {
  std::optional<FileType> as_file_type;      ///< decode as this, skip detection
  std::vector<FileType> file_type_priority;  ///< most preferred first
  CsvOptions csv;                            ///< format-specific overrides
};

[[nodiscard]] DecodedFile open(const File &, const Logger & = Logger::null());
[[nodiscard]] DecodedFile open(const File &, const DecodeOptions &,
                               const Logger & = Logger::null());

[[nodiscard]] std::vector<FileType> list_file_types(const File &, const Logger & = …);
[[nodiscard]] std::string_view mimetype(const File &, const Logger & = …);
```

`File::from_disk` / `File::from_memory` stay the only ways to make a `File`, so
they are the path road and the `path` overloads all go: `open(File::from_disk(p))`.
Nothing is lost — a `File` carries its name, which is what gives a `.md` its
type candidate.

`DecodedFile` keeps only its `impl` constructor. `DocumentFile` keeps only its
`impl` constructor and stays what it should be, a typed view reached through
`as_document_file()`. `DecodePreference` becomes `DecodeOptions`; the two
`CsvFile` factories go and `csv` in the options replaces them.

Two functions where there were twenty-two.

## Finding 2 — twenty `html::translate` overloads

Ten take a `cache_path` the header itself documents as ignored. Of the other
ten, seven are already reachable through `translate(DecodedFile)`, which
dispatches to every one of them (`html.cpp:217-250`).

**Target:** four, for the four genuinely distinct inputs — `DecodedFile`,
`Document`, `Filesystem`, `Archive`.

## Finding 3 — two roads into editing

1. `HtmlConfig::editable` stamps `contenteditable`, our JS produces a diff, and
   `html::edit(document, diff)` replays it; one key is understood,
   `modifiedText`.
2. `Text::set_content()` — the only mutator in an otherwise read-only 599-line
   header, and the thing `html::edit` is implemented on top of.

(Its `const` is not the problem: `Document::save` is `const` too, and the
value-semantics rule in `README.md` is about the handle, not the document. The
problem is that there are two roads, and that one of them is a single hole in
an otherwise immutable element API.)

Three separate answers to *can I edit this*, at three altitudes:
`FileTypeCapabilities::edit`, `Document::is_editable()`,
`Element::is_editable()`. Those are defensible — they answer different
questions — but only if the header says which is which.

`html::edit` is also misfiled: it lives in `namespace html` and takes a
`Document`, and nothing about it is html except that our JS *produces* its
input. [`editing.md`](editing.md) already commits to an operation log replacing
the diff blob, which changes this signature anyway.

**Target:** `Document::apply(std::string_view operations)`, in
`document.hpp`; `html::edit` and the public `Text::set_content` go. The op-log
*semantics* stay exactly what they are today — this is the entry point moving
to where v7.x can fill it in without breaking again.

## Finding 4 — smaller things a major is the only chance to fix

- **The inert surface.** `GlobalParams` entire (both paths documented inert),
  `TextFile::charset()`, `UnknownCharset`, and `HtmlConfig::{background_image_format,
  background_image_dpi, no_drm, embed_outline}`. With mirrors: `GlobalParams.java`,
  `bind_core.cpp`, `ODRGlobalParams`, `OdrAndroid.init`, and the
  `ODR_WITH_LIBMAGIC` / conan `with_libmagic` / `bundle_assets` options.
- **Enum ordinals**, per *Why a major* above. The recommendation is **not** to
  renumber — the payoff is cosmetic and a stale binding would fail silently —
  but to pin the values explicitly (`= 0, 1, 2, …`) and add a test that asserts
  the mapping, turning the append-only convention into something the build
  catches.
- **`HtmlConfig` is ~36 flat fields** mixing output location, layout,
  spreadsheet limits and pdf. Grouping into sub-structs is breaking; now or
  never. Weighed against the cost of re-mirroring it in five bindings — see
  *Not in v7*.
- **`impl()` leaks `internal::abstract` into the public API** in ten places.
  `file.hpp:345` already says `// TODO impl() might be a bit dirty`. The
  bindings are the only callers, and they are in-tree.
- **`Html` + `HtmlPage` alongside `HtmlService` + `HtmlView`** — two
  generations of the same idea, with `bring_offline` still returning the older.
- **Three covariant `decrypt()`** on `DecodedFile`, `DocumentFile` and
  `PdfFile`.

## The pull requests

Each is a self-contained breaking change with its own `CHANGELOG.md` entry, in
an order where every step compiles on its own. The bindings move in the same PR
as the C++ they mirror — a binding that lags a header is a build break, not a
deprecation.

| # | Subject | Breaking | Touches |
|---|---|---|---|
| 1 | `docs(design): the v7 api plan` | no | this file |
| 2 | `refactor(api)!: drop the inert globals` | yes | `global_params.*`, jni, python, apple, android, cmake, conan |
| 3 | `refactor(api)!: drop the inert config and charset surface` | yes | `html.hpp`, `file.hpp`, `exceptions.hpp`, apple, python, jni |
| 4 | `refactor(html)!: cut translate to its four inputs` | yes | `html.hpp/cpp`, cli, bindings |
| 5 | `refactor(api)!: one way to ask what a file is` | yes | `file.hpp`, `odr.hpp`, bindings |
| 6 | `refactor(api)!: one way to open a file` | yes | `file.hpp`, `odr.hpp`, `open_strategy.*`, cli, bindings |
| 7 | `feat(api)!: fold the decode options into one struct` | yes | `file.hpp`, `odr.hpp`, csv, bindings |
| 8 | `refactor(document)!: one way to edit a document` | yes | `document.hpp`, `document_element.hpp`, `html.hpp`, cli, bindings |
| 9 | `test(api): pin the enum ordinals` | no | enums, `test/src` |

5 before 6 because dropping `DocumentFile::type`/`::meta` is what leaves
`DocumentFile`'s constructors with no callers. 6 before 7 because
`DecodeOptions` replaces `DecodePreference` in a signature 6 has already
reduced to two.

## Not in v7

- **Grouping `HtmlConfig`.** Breaking in five bindings for readability alone,
  and it collides with #764's lazy-loading config work. Revisit when that lands
  and the field list changes anyway.
- **Hiding `impl()`.** Every caller is in-tree, so it can be done in a minor
  behind a documented `odr::internal` escape hatch. It is not blocked by the
  major.
- **Retiring `Html`/`HtmlPage`.** `bring_offline` is what the CLI and the apps
  use; replacing its return type wants the lazy-loading design first.
- **The editing features themselves** (#766, #122, #60). PR 8 fixes the
  *surface* so those land in v7.x without another break.
