# Legacy MS Office (`oldms/`): shared conventions

What the three binary legacy-format modules share. Each module's own design and
open work is in its `AGENTS.md`. Spec links are in [`README.md`](README.md),
the PDFs under `offline/documentation/`.

| Module | Format | Agent doc |
|---|---|---|
| [`text/`](text/) | `.doc` (Word) | [text/AGENTS.md](text/AGENTS.md) |
| [`presentation/`](presentation/) | `.ppt` (PowerPoint) | [presentation/AGENTS.md](presentation/AGENTS.md) |
| [`spreadsheet/`](spreadsheet/) | `.xls` (Excel, BIFF8) | [spreadsheet/AGENTS.md](spreadsheet/AGENTS.md) |

## Shared conventions

- Each format is a `[MS-CFB]` compound file. Each module reads its streams
  sequentially.
- Fixed-layout spec structures are `#pragma pack(1)` structs in the
  `*_structs.hpp` headers, with the spec's field names, `[MS-*]` section
  citations and a `static_assert` on the size. The reader copies the file's
  bytes straight in.
- Sub-byte fields are bit-fields in the spec's order, LSB first: `FibBase` and
  `Sprm` (`.doc`), `RecordHeader` (`.ppt`), `RkNumber` and
  `UnicodeStringFlags` (`.xls`).
- Fail early on malformed input. Skip records and structures that are not
  modelled.
- `password_encrypted()` reads the format's own flag or record, so an encrypted
  file surfaces as encrypted instead of a parse error. Reading one still needs
  a `decrypt` ([MS-OFFCRYPTO]), which throws today. `html_output_test` skips
  the encrypted `.doc` fixture for that reason.

## Endianness: little-endian host assumed (shared open work)

All three modules read multi-byte fields and UTF-16 code units in the host's
byte order, and their bit-field structs assume LSB-first allocation. The file
side is always little-endian, so only the host varies, and
`std::endian::native` knows it at compile time. GCC and Clang switch bit-field
allocation to MSB-first on big-endian targets, so one compile-time guard
covers byte order and bit order.

The fix, if a big-endian target matters: give each struct in the
`*_structs.hpp` headers a fixup function that runs after the byte copy,
swaps the multi-byte fields and re-places the bit-field values. It has to be
per struct, because a blind byte swap cannot fix a bit-field that straddles a
byte boundary (`Sprm.ispmd`, `FcCompressed.fc`, `RkNumber.num`,
`RecordHeader.recInstance`). `#if`-mirrored bit-field declarations cannot
express that either. On little-endian hosts every fixup is a no-op.
