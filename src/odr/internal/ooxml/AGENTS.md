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
| `ooxml_util.{hpp,cpp}` | Stateless attribute readers: half-points, hundredth-points, EMUs, twips, percents, colours, borders, font weight and style; relationship parsing; `write_text_nodes` for the writers |
| `ooxml_crypto.{hpp,cpp}` | Decryption |

## Encryption

The container is a [CFB](../cfb/) compound file. `open_strategy.cpp` routes
an encrypted package through the cfb filesystem and a plain one through
[zip](../zip/). `decrypt()` verifies the password, decrypts
`/EncryptedPackage` and reopens the plain zip in memory. `crypto::Util` picks
the algorithm from `VersionInfo`:

- **Standard** (major 2, 3 or 4, minor 2; [MS-OFFCRYPTO] 2.3.4.5):
  `ECMA376Standard`, AES-ECB plus SHA1. Key derivation: password as UTF-16LE,
  salt-prefixed SHA1, 50000 iterations, then the block key with the `0x36` and
  `0x5c` pads.
- **Agile** (4.4; 2.3.4.10): `ECMA376Agile`, the default of Office 2010 and
  later. The descriptor is XML. Only the password key encryptor is read, with
  AES in CBC mode and SHA-1, SHA-256, SHA-384 or SHA-512. `derive_key` returns
  the iterated password hash (2.3.4.11 without the block key), so the one
  `Algorithm` interface serves both. The package is decrypted in 4096-byte
  segments, each with its own IV (2.3.4.15). The `dataIntegrity` HMAC
  (2.3.4.14) is not checked.

Extensible encryption (3.3, 4.3), another cipher or chaining mode, and an
agile file with only a certificate key encryptor throw
`MsUnsupportedCryptoAlgorithm`. A big-endian host throws `UnsupportedEndian`,
because the crypto structs are `#pragma pack(1)` and read by `memcpy`.

## Shared open work

- Big-endian hosts.
- Document statistics (page and table counts) for any format.
- `Crypto::Util` is rebuilt per `decrypt()` call.
