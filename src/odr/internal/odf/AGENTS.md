# OpenDocument (`odf/`) — design & open work

Rules and design decisions for this module. The per-feature checklist is in
[`README.md`](README.md). The shared architecture is in the top-level
[`AGENTS.md`](../../../../AGENTS.md). Read it first.

**Scope.** Reads all four ODF document types (`.odt`, `.odp`, `.ods`, `.odg`),
their template, flat-xml and legacy StarOffice variants. Reader, style
resolver and a partial editor. Relies on [ZIP](../zip/) (container),
[XML](../xml/) (recognises a flat document) and [SVM](../svm/) (embedded
images).

## The registry is an index over the pugixml DOM

The `Document` keeps the parsed `content.xml` and `styles.xml` resident
(`m_content_xml`, `m_styles_xml`). Every `RegistryElement` stores a live
`pugi::xml_node` beside its tree ids, and every style, content or attribute
read goes back to the node. This is why pugixml is built in compact mode (see
the top-level `AGENTS.md`), and why a text edit is a DOM splice and `save`
re-serialises the tree.

The rest is the shared machinery: `internal::ElementRegistry<RegistryElement,
StoredId>` with the payload tables `m_texts`, `m_tables`, `m_sheets`,
`m_sheet_cells` and `m_shape_types`, and one `ElementAdapter` derived from
`internal::RegistryElementAdapter`.

## Design decisions

**Parsing is a name-keyed dispatch table.** `odf_parser.cpp` maps an XML tag to
a `TreeParser`. An unknown tag is skipped, and the recursion continues into its
children. Many tags collapse onto a generic type: `text:h` is a paragraph,
`text:section`, `text:table-of-content` and date fields are a `group`,
`draw:g` is a frame. A container gets its own children parser
(`parse_presentation_children` walks only `draw:page`).

**Every `draw:*` shape is a `frame`.** `create_shape_element` records the kind
in `m_shape_types`, and `frame_shape_type` reads it back. `draw:rect` and
`draw:caption` are `rect`, `draw:line` and `draw:measure` are `line`,
`draw:circle` and `draw:ellipse` are `ellipse` unless `draw:kind` cuts them,
and every shape whose geometry is drawn rather than named is `custom`.

**Text runs are coalesced.** A maximal run of consecutive text nodes
(`node_pcdata`, `text:s`, `text:tab`) is one `text` element spanning
`[first, last]` (`Text.last`). `text:line-break` is its own element and ends
the run. Reading expands `text:s` to N spaces (`text:c`) and `text:tab` to
`\t`.

**Sheets are modelled sparsely, off-tree.** A `Sheet` side-struct holds
`columns`, `rows` and `cells` keyed by position. A repeated column, row or cell
is stored once, at the end of the range it repeats over, and resolved with an
upper bound, whether or not the cell has content. This matters: expanding a
repeat per position lets a 400-byte document ask for a `1048576 × 1024` grid,
because both counts are legal repeats. Only a non-empty cell gets a
`sheet_cell` element. The `SheetCell` payload carries the anchor's
`TablePosition` and an `is_repeated` flag.

**A repeated cell is addressed by position, not by index.** One element stands
for many positions, so `sheet_cell` hands out
`tag | ordinal(15) | column(24) | row(24)` (`positional_id`) for a repeat and
the index otherwise. `resolve_id` decodes it against the sheet's cell index.
It is the `ElementRegistry::resolve_id` hook, identity for every other engine,
and `RegistryElementAdapter` navigates through `element_at`, so nothing else
knows. Consequences: `SheetCell::position()` and the id name the cell asked
for; a handle follows the index, so a run can be split under it; children are
shared, so a run inside a repeated cell belongs to the anchor.

**The sheet containers are sorted vectors, not maps.** Parsing appends in
document order, so the keys only grow, and a vector costs less than a tree
node per entry. The cells of every row live in one array per sheet, and each
row records where its run starts, so `register_cell` must follow its row's
`register_row`. `reserve_sheet` counts the row and cell nodes first, so the
arrays are allocated once. The elements are a `std::deque`, because
`create_element_` hands back a reference the parser holds on to.

**Ids are stored narrow, payloads in sorted arrays.** `StoredId` is 32 bits.
Every boundary widens it to the public 64-bit `ElementIdentifier`. The payload
tables are `SortedSideTable`s written in id order. `m_list_types` and
`m_list_markers` are plain `SideTable`s, written when a list is resolved.

**Styles resolve to a flattened `ResolvedStyle`, eagerly.** `StyleRegistry`
builds name-to-node indices from both files. Automatic and named styles land
in one `m_index_style`, told apart by name only. Then it builds a `Style` for
every entry: each one resolves its `style:parent-style-name` (else the
`style:family` default) first, copies that resolved base and overlays its own
properties. A second cascade runs at query time: `get_intermediate_style`
walks the element parent chain and overrides partial styles down to the
target. Font names go through `style:font-face` to `svg:font-family`. Master
pages are parsed into the element tree and into the style index.

**Percent units.** A percent font size, and the relative size of
`style:text-position`, multiply the inherited size. A percent line height
passes through, and the renderer writes it as a unitless CSS ratio. A percent
margin is of the same margin in the parent style ([OpenDocument] 16.2), and
is dropped when the parent states none.

**`draw:fill` decides whether `draw:fill-color` is paint.** The two cascade
independently, and LibreOffice writes a `draw:fill-color` with no `draw:fill`
beside it. So the fill state rides in the resolved colour's alpha. `draw:fill`
sets it, `draw:fill-color` sets the rgb and keeps it, and `draw:fill` defaults
to none, so a colour with no fill anywhere in the chain paints nothing.

**A flat document is the same `Document`, minus the package.** Its
`office:document` root carries what `content.xml` and `styles.xml` carry
between them, so the flat constructor passes that root as both roots. Images
are `office:binary-data`, base64 decoded lazily by the adapter and named after
their element id, because the `xlink:href` beside the bytes names no file of
ours. `save` re-serialises the one tree. Recognition is `office:document` plus
a known `office:mimetype`, read off the tree the `XmlFile` built.
`odf_flat_file.cpp` then reparses with the document parse options, because a
source view's options cannot substitute for them.

**Decryption is manifest-driven, two layouts.** `odf_crypto.cpp` supports a
single `encrypted-package` blob (decrypt, inflate, new ZIP filesystem) and
lazy per-file decryption (`DecryptedFilesystem` decrypts on `open()` and
validates the password up front against the smallest encrypted file).
Algorithms: AES256-CBC, AES256-GCM, Triple-DES-CBC, Blowfish-CFB. Key
derivation: PBKDF2 and Argon2id (the LibreOffice `loext:` extension). Start
key: SHA1 and SHA256, with the 1K variants. An unknown algorithm throws.

**Fail fast on structure, tolerate content.** A registry-id violation, an
already-parented child, unknown crypto, a wrong password, and a missing
`content.xml`, manifest or mimetype throw. An unknown tag, an unparsable
measure or colour (`nullopt`), a missing style or font reference, and an
unknown mimetype are tolerated.

## Editing

- **Text.** Runs and paragraphs: `text_set_content`, `text_insert`,
  `paragraph_split`, `paragraph_merge_next`, `paragraph_insert_after`,
  `element_remove`. `text_set_style` writes seven properties (weight, style,
  underline, line-through, size, colour, background). It cuts the `text:span`
  around the run (`TreeEditor::isolate`) or wraps a bare run in a new one,
  and points it at a fresh automatic style `T<n>`
  (`StyleRegistry::create_text_style`): a copy of a shared automatic style
  plus the delta, or a child of a named style.
- **Cells.** `sheet_set_cell` writes `office:value-type`, `office:value` and
  the `text:p` under the cell, because the file states the value and shows a
  rendering of it. It writes through the run the cell holds, so the run keeps
  its style, and replaces the runs of a paragraph that holds several.
  `text_run_of` descends through a single span before it looks for the run,
  the same walk `spreadsheet.js::runOf` makes over the page. A cell that holds
  a formula, a link, a line break or several paragraphs refuses. Refusals are
  decided before any node is cut.
- **A repeated cell** is written by cutting the run: `claim_cell` copies the
  `table:table-row` and the `table:table-cell` around the position and leaves
  the original node as the one written, so its element survives.
  `reindex_sheet` rebuilds the position index off the DOM.
- **An empty cell** carries no element, because `index_sheet_rows` builds one
  only for a node with content or a span. `claim_cell` appends the `text:p`
  before the reindex, so the reindex builds the element.
- **A position past the sheet** is reached by `grow_to_cell`. It appends the
  rows and the empty cells it needs and declares the columns, so
  `dimensions` covers the new cell. A repeated row is cut first. Nothing caps
  the position, because ODF states no grid limit. LibreOffice drops a cell
  past its own limit.
- **A write drops the cached result of every formula that reads the cell**
  (`drop_stale_results` over `SheetDependencies`). The value attributes go and
  the `text:p` is removed as an element. The formula, the style and an
  anchored drawing stay. A formula the graph could not resolve
  (`SheetDependencies::unresolved`) keeps its result. ODF has no
  `fullCalcOnLoad`, so a formula with no result renders empty until an
  evaluator exists.
- **Removed nodes leave tombstones.** Where a write rebuilds a paragraph, the
  old children leave the DOM, and their elements keep their ids and become
  unreachable. Their `pugi::xml_node` dangles from then on.

## Module layout

| File (`odf/`) | Role |
|---|---|
| `odf_file.{hpp,cpp}` | `OpenDocumentFile`: entry point, type and encryption, `decrypt()`, builds the `Document` |
| `odf_flat_file.{hpp,cpp}` | `FlatOpenDocumentFile`: the same for a flat document, plus the root-element recogniser |
| `odf_meta.cpp` | `parse_file_meta`: mimetype to `FileType`, document type, page and table counts, encryption flag |
| `odf_manifest.{hpp,cpp}` | `META-INF/manifest.xml` to per-file crypto entries, smallest-file tracking |
| `odf_crypto.cpp` | Decryption: hashing, key derivation, cipher dispatch, package or lazy filesystem |
| `odf_element_registry.{hpp,cpp}` | Element store, payload tables, sparse sheet index, `positional_id` |
| `odf_parser.{hpp,cpp}` | `content.xml` to registry: dispatch table, run coalescing, sheet cursor |
| `odf_document.{hpp,cpp}` | `Document` (owns the DOMs, the registry and the adapter), the `ElementAdapter`, edit and `save` |
| `odf_style.{hpp,cpp}` | `StyleRegistry` and `Style`: indices, eager flatten, master pages, `ResolvedStyle` readers, `create_text_style` |
| `odf_list.{hpp,cpp}` | List styles resolved to markers |
| `odf_table.{hpp,cpp}`, `odf_value_cursor.hpp` | Table and cell helpers |
| `odf_geometry.{hpp,cpp}`, `odf_enhanced_geometry.{hpp,cpp}` | Shape paths, transforms, `draw:enhanced-path` |
| `odf_chart.{hpp,cpp}` | Charts drawn from an embedded chart object |

## Open work

1. **Save never re-encrypts.** A document decrypted from a password-protected
   package reports `is_savable(false) == false`, and every `save` overload
   throws `UnsupportedOperation`. Only `content.xml` is re-serialised, so a
   style edit would not persist.
2. **No streaming.** Crypto and save read whole files into memory and rebuild
   the whole ZIP (`// TODO stream`).
3. **Covered and repeated cells are heuristic.** `// TODO covered cells` and
   `// TODO mark as repeated` in `odf_parser.cpp`. A rowspan out of a repeated
   row is dropped.
4. **Style gaps.** `transparent` and alpha colours give `nullopt`
   (`// TODO use alpha`). The style-versus-element cascade is provisional
   (`// TODO use override?`). `text:outline-style` is indexed but not applied
   to headings.
5. **StarOffice, template and flat mimetypes** are aliased onto the four base
   types (`odf_meta.cpp`). A flat document is parsed twice: once to recognise
   it, once to build the tree.
6. **Formulas are read, not evaluated.** A written cell empties its
   dependents until an evaluator exists.
