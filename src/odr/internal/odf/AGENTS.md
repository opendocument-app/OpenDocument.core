# OpenDocument (`odf/`) — design & open work

The **why** and the roadmap; what is concretely done lives in the code and the
per-feature checklist in [`README.md`](README.md). Shared architecture (the
element-adapter pattern, build/test loop, conventions) is in the top-level
[`AGENTS.md`](../../../../AGENTS.md); read it first.

**Scope.** One engine (`DecoderEngine::odr`) reading **all four ODF document
types** — text (`.odt`), presentation (`.odp`), spreadsheet (`.ods`), graphics
(`.odg`) — plus their template and legacy StarOffice variants, through the
abstract model. Reader + style resolver + a **partial editor** (text-content
edits, save). Relies on [ZIP](../zip/) (container) and [SVM](../svm/) (embedded
StarView metafile images).

## The load-bearing decision: a pugixml-node-backed registry

Unlike the binary engines (`oldms/`, `pdf/`), which parse bytes into their own
structures, ODF keeps the parsed **`content.xml` / `styles.xml` DOMs resident**
(`Document` owns `m_content_xml`, `m_styles_xml`). The `ElementRegistry` is a
thin *index over* that DOM: every `ElementRegistry::Element` stores a live
`pugi::xml_node` alongside its tree ids. Style/content/attribute access always
goes back to the node. **This is why ODF alone can edit and save**: a text edit
is a local DOM splice, and `save` re-serialises the mutated tree. The other
engines throw away the source, so their models are read-only.

Everything else follows the shared registry/adapter pattern: flat
`std::vector<Element>`, id = index + 1, `null_element_id == 0`, parent/child/
sibling ids, per-subtype side maps (`m_texts`, `m_tables`, `m_sheets`,
`m_sheet_cells`). One mega `ElementAdapter` multiply-inherits every abstract
per-type adapter and dispatches by returning `this`/`nullptr` on `element_type`.

## Design decisions

**Parsing is a name-keyed dispatch table.** `odf_parser.cpp` holds a static
`unordered_map<std::string, TreeParser>` mapping XML tag → builder; unknown tags
are **skipped** (recursion continues into children). Many tags collapse onto a
generic type — `text:h` → paragraph, `text:section`/`toc`/date fields → `group`,
`draw:g` → frame. Container types get bespoke children-parsers
(`parse_presentation_children` walks only `draw:page`, etc.).

**Text runs are coalesced.** A maximal run of consecutive text nodes
(`node_pcdata`, `text:s`, `text:tab`) becomes **one** `text` Element spanning
`[first, last]` (the end stored in `Text.last`); `text:line-break` is its own
element and breaks the run. Reading expands `text:s`→N spaces (via `text:c`),
`text:tab`→`\t`.

**Sheets are modelled sparsely, off-tree.** A `Sheet` side-struct holds
position-keyed `columns`/`rows`/`cells` maps rather than a child chain. Repeated
columns/rows/cells are stored **once** at a range key and resolved with
`lookup_greater_than`, so a 5000-row `number-columns-repeated` doesn't inflate.
Only non-empty cells get a real `sheet_cell` Element; empty ones are recorded as
repeated ranges. Cells carry a `TablePosition` + `is_repeated` flag.

**Styles resolve to a flattened `ResolvedStyle`, eagerly.** `StyleRegistry`
first builds name→node indices from *both* files (automatic and named styles land
in the same `m_index_style` map — distinguished only by name, not origin), then
constructs a `Style` for every entry. Each `Style` **recursively resolves its
`style:parent-style-name` (then `style:family` default) first, copies that
resolved base, and overlays its own node's properties** — inheritance is a
flatten-at-build, not a runtime walk. Parent beats family as the copy source. A
*second*, separate cascade runs at query time: `get_intermediate_style` walks the
**element** parent chain and `.override()`s partial styles down to the target.
Font names are indirected through `style:font-face` → `svg:font-family`.
Master pages are parsed into the element tree *and* the style index.

**Percent units are special-cased**: percent font-size (and the relative size
of `style:text-position`) multiplies the inherited size; percent line-height
passes through (the HTML renderer emits it as a unitless CSS ratio); percent
margins are currently **dropped** (open work).

**Decryption is manifest-driven, two layouts.** `odf_crypto.cpp` supports either
a single `encrypted-package` blob (decrypt → inflate → new ZIP filesystem) or
**lazy per-file** decryption (a `DecryptedFilesystem` wrapper decrypting on
`open()`, validating the password up-front against the smallest encrypted file).
Algorithms: AES256-CBC/GCM, Triple-DES-CBC, Blowfish-CFB; key derivation PBKDF2
and Argon2id (LibreOffice `loext:` extension); start-key SHA1/SHA256 (+1K
variants). Unknown algorithm → throw.

**Fail-fast on structure, tolerate content.** Registry-id violations, an
already-parented child, unknown crypto, wrong password, a missing
content.xml/manifest/mimetype all **throw**. Unknown tags, unparsable
measures/colours (→ `nullopt`), missing style/font references, unknown mimetypes
are **tolerated** to keep rendering best-effort.

## Module layout

| File (`odf/`) | Role |
|---|---|
| `odf_file.{hpp,cpp}` | `OpenDocumentFile`: entry point; type/encryption; `decrypt()`; builds `Document` per type |
| `odf_meta.cpp` | `parse_file_meta`: mimetype→FileType, doc type, page/table counts, encryption flag |
| `odf_manifest.{hpp,cpp}` | `META-INF/manifest.xml` → per-file crypto entries, smallest-file tracking |
| `odf_crypto.cpp` | Decryption: hashing, key derivation, cipher dispatch, package-vs-lazy filesystem |
| `odf_element_registry.{hpp,cpp}` | Flat element store + Text/Table/Sheet/SheetCell side maps; sparse repeated-range maps |
| `odf_parser.{hpp,cpp}` | `content.xml` → registry: name dispatch table, text-run coalescing, sheet cursor |
| `odf_document.{hpp,cpp}` | `Document` (owns DOMs + registries + adapter); the mega `ElementAdapter`; edit + `save` |
| `odf_style.{hpp,cpp}` | `StyleRegistry` + `Style`: indices, eager parent/family flatten, master pages, `ResolvedStyle` readers |

## Status & open work

Feature coverage (element/style checkboxes) is tracked in [`README.md`](README.md).
The structural/foundational gaps, roughly by value:

1. **Editing is text-content only.** No structural edits (insert/delete/move
   elements), no attribute or style editing. `text_set_content` splices the DOM
   for one text run; that's the whole editor.
2. **Spreadsheet editing is force-disabled** (`is_editable` hardcodes `false`
   for spreadsheets — `odf_document.cpp`, `// TODO fix spreadsheet editability`).
3. **Save never re-encrypts.** Plain `save` strips all `manifest:encryption-data`
   and emits an unencrypted package; `save(path, password)` throws
   `UnsupportedOperation`. `save` also doesn't yet guard `is_savable`. Only
   `content.xml` is re-serialised, so hypothetical style edits wouldn't persist.
4. **No streaming.** Crypto and save read whole files into memory and round-trip
   the entire filesystem through a rebuilt ZIP (`// TODO stream` throughout).
5. **Repeated / covered spreadsheet cells are heuristic.** Several
   `// TODO covered cells` / `// TODO mark as repeated` in `odf_parser.cpp`;
   empty-row/cell detection is approximate.
6. **Style gaps beyond missing properties**: percent margins dropped;
   `transparent`/alpha colours → `nullopt`; the Style-vs-element cascade layering
   is provisional (`// TODO use override?`). List/outline numbering is indexed but
   not rendered as numbers.
7. **StarOffice/template mimetypes** are aliased onto the four base types; they
   may deserve distinct `FileType`s (`odf_meta.cpp`).
