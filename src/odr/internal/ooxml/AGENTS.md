# Office Open XML (`ooxml/`) — shared design & conventions

The **why** for the OOXML engine; per-feature checklists live in the
[`README.md`](README.md) files. Shared architecture (element-adapter pattern,
build/test, conventions) is in the top-level [`AGENTS.md`](../../../../AGENTS.md);
read it first.

**Scope.** One engine (`DecoderEngine::odr`) reading the three OOXML document
types — word (`.docx`), presentation (`.pptx`), spreadsheet (`.xlsx`). The three
formats **share almost nothing beyond packaging, encryption, and type
detection**; each is a self-contained module with its own `AGENTS.md`:

| Module | Format | Editable | Agent doc |
|---|---|:--:|---|
| [`text/`](text/) | `.docx` (Word) | text + save | [text/AGENTS.md](text/AGENTS.md) |
| [`presentation/`](presentation/) | `.pptx` (PowerPoint) | read-only | [presentation/AGENTS.md](presentation/AGENTS.md) |
| [`spreadsheet/`](spreadsheet/) | `.xlsx` (Excel) | read-only | [spreadsheet/AGENTS.md](spreadsheet/AGENTS.md) |

## Shared element model (same as ODF)

Like [`odf/`](../odf/), every format keeps its parsed XML parts **resident** and
the `ElementRegistry` is a thin index over them: each `Element` holds a live
`pugi::xml_node` plus its tree ids (flat vector, id = index + 1,
`null_element_id == 0`, parent/child/sibling ids, per-subtype side maps). One
mega `ElementAdapter` per format multiply-inherits the abstract per-type adapters
and dispatches by returning `this`/`nullptr` on `element_type`. Parsing is a
static `unordered_map<tag, TreeParser>` dispatch table; unknown tags are skipped
(children still visited). Text runs are coalesced into one `text` Element over a
`[first, last]` node span. **Editing, where present, splices these live nodes;**
`save` re-serialises only the mutated part and byte-copies the rest.

## OPC relationships (the cross-part mechanic)

OOXML packages are split across parts (`document.xml`, `slideN.xml`,
`sheetN.xml`, `drawingN.xml`, `sharedStrings.xml`, …) wired by
`_rels/*.rels`. `parse_relationships` (`ooxml_util`) reads a part's `.rels`
sibling into an `rId → target-path` map. Cross-part references — a slide's
`r:id`, a sheet, a drawing, an image `r:embed` — resolve through it, relative to
the *referencing part's* path. `spreadsheet` threads a per-part `ParseContext`
(path + its relations + the global part cache) so paths resolve correctly;
`presentation` uses a thinner `rId → xml` map.

## Shared files

| File (`ooxml/`) | Role |
|---|---|
| `ooxml_file.{hpp,cpp}` | `OfficeOpenXmlFile` entry point: meta, encryption state, `decrypt()`, dispatch to the per-format `Document` on `file_type()` |
| `ooxml_meta.cpp` | `parse_file_meta`: **sentinel-path** type detection (`/word/document.xml`→docx, `/ppt/presentation.xml`→pptx, `/xl/workbook.xml`→xlsx); encrypted-package detection (`/EncryptionInfo`+`/EncryptedPackage`) |
| `ooxml_util.{hpp,cpp}` | Stateless pugixml attribute readers: OOXML unit systems (half-points, hundredth-points, EMUs `/914400in`, twips `/1440in`, percents), colours, borders, font weight/style, relationship parsing |
| `ooxml_crypto.{hpp,cpp}` | Decryption (see below) |

## Encryption

Only **ECMA-376 _standard_ encryption** (AES-ECB + SHA1) is implemented
(`ECMA376Standard`, selected by `VersionInfo` major ∈ {2,3,4} & minor == 2). Key
derivation: password→UTF16LE, salt-prefixed SHA1, **50000** hash iterations, then
the block-key derivation with `0x36`/`0x5c` pads. The encryption container is a
[CFB](../cfb/) compound file (`open_strategy.cpp` routes encrypted packages
through the cfb filesystem, plain packages through [zip](../zip/)); `decrypt()`
verifies the password, AES-decrypts `/EncryptedPackage`, and re-opens the plain
ZIP in memory.

**Rejected / unsupported (throw `MsUnsupportedCryptoAlgorithm`)**: *agile*
(4.4) and *extensible* encryption. **Big-endian hosts throw `UnsupportedEndian`**
— the crypto structs are `#pragma pack(1)` + `memcpy`/`reinterpret_cast`
(`// TODO support big endian`).

## Shared open work

- **Agile encryption** — the common modern scheme; currently rejected. Highest-
  value shared gap.
- **Big-endian hosts** — crypto (and the `reinterpret_cast` reads) assume
  little-endian; guarded by a runtime throw.
- **Document statistics** (page / table counts) are not produced for any format.
- `Crypto::Util` is rebuilt per `decrypt()` call (`// TODO cache`).

Per-format element/style coverage and format-specific open work live in each
module's own `AGENTS.md`.
