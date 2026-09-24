# Office Open XML (`ooxml/`) — shared design & conventions

The shared mechanics of the OOXML engine. The per-feature checklists are in
the [`README.md`](README.md) files. The element-adapter pattern, the build loop
and the conventions are in the top-level [`AGENTS.md`](../../../../AGENTS.md).

The three formats share only packaging, encryption and type detection. Each is
a self-contained module with its own `AGENTS.md`:

| Module | Format | Editable | Agent doc |
|---|---|---|---|
| [`text/`](text/) | `.docx` | runs, paragraphs, text style, save | [text/AGENTS.md](text/AGENTS.md) |
| [`presentation/`](presentation/) | `.pptx` | runs, paragraphs, text style, save | [presentation/AGENTS.md](presentation/AGENTS.md) |
| [`spreadsheet/`](spreadsheet/) | `.xlsx` | cell values, save | [spreadsheet/AGENTS.md](spreadsheet/AGENTS.md) |

## Shared element model

As in [`odf/`](../odf/), each format keeps its parsed XML parts resident. The
`ElementRegistry` is an index over them: its `RegistryElement` adds a live
`pugi::xml_node` to the shared `ElementNode`. One `ElementAdapter` per format
derives from `internal::RegistryElementAdapter`. Parsing is a static
`unordered_map<tag, TreeParser>` dispatch table. An unknown tag is skipped, and
its children are still visited. Text runs coalesce into one `text` element over
a `[first, last]` node span. Editing splices these live nodes. `save`
re-serialises only the mutated parts and byte-copies the rest.

## OPC relationships

Parts (`document.xml`, `slideN.xml`, `sheetN.xml`, `drawingN.xml`,
`sharedStrings.xml`, ...) are wired by `_rels/*.rels`. `parse_relationships`
(`ooxml_util`) reads a part's `.rels` sibling into an `rId → target` map. A
target resolves relative to the referencing part's path. `spreadsheet` threads
a per-part `ParseContext` (path, its relations, the part cache).
`presentation` uses an `rId → xml` map.

## Shared files

| File (`ooxml/`) | Role |
|---|---|
| `ooxml_file.{hpp,cpp}` | `OfficeOpenXmlFile`: meta, encryption state, `decrypt()`, dispatch to the per-format `Document` on `file_type()` |
| `ooxml_meta.cpp` | `parse_file_meta`: type detection by sentinel path (`/word/document.xml`, `/ppt/presentation.xml`, `/xl/workbook.xml`); an encrypted package has `/EncryptionInfo` and `/EncryptedPackage` |
| `ooxml_util.{hpp,cpp}` | Stateless attribute readers: half-points, hundredth-points, EMUs, twips, percents, colours, borders, font weight and style; relationship parsing; `write_text_nodes`, `insert_in_sequence` for the writers |
| `ooxml_crypto.{hpp,cpp}` | Decryption |

## Encryption

Only ECMA-376 standard encryption (AES-ECB plus SHA1) is implemented:
`ECMA376Standard`, selected when `VersionInfo` major is 2, 3 or 4 and minor is
2. Key derivation: password as UTF-16LE, salt-prefixed SHA1, 50000 iterations
(`ITER_COUNT`), then the block key with the `0x36` and `0x5c` pads. The
container is a [CFB](../cfb/) compound file. `open_strategy.cpp` routes an
encrypted package through the cfb filesystem and a plain one through
[zip](../zip/). `decrypt()` verifies the password, decrypts
`/EncryptedPackage` and reopens the plain zip in memory.

Agile (4.4) and extensible encryption throw `MsUnsupportedCryptoAlgorithm`. A
big-endian host throws `UnsupportedEndian`, because the crypto structs are
`#pragma pack(1)` and read by `memcpy`.

## Shared open work

- Agile encryption, the common modern scheme.
- Big-endian hosts.
- Document statistics (page and table counts) for any format.
- `Crypto::Util` is rebuilt per `decrypt()` call.
