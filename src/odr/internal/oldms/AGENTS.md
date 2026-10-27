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
  file surfaces as encrypted instead of a parse error.

## Decryption

`LegacyMicrosoftFile::decrypt` asks the module for its decrypted streams and
lays them over the compound file (`DecryptedFilesystem` in `oldms_file.cpp`).
The parsers then read plaintext and know nothing of the encryption. A module
returns nothing for a wrong password and throws
`MsUnsupportedCryptoAlgorithm` for an encryption it cannot read. `.xls`
decrypts; `.doc` and `.ppt` still throw `UnsupportedOperation`, and
`html_output_test` skips the encrypted `.doc` fixture for that reason.

`oldms_crypto.{hpp,cpp}` holds what the formats share ([MS-OFFCRYPTO]):

- `Rc4Encryption` reads the RC4 header (1.1, 2.3.6.1) or the RC4 CryptoAPI
  header (2.2, 3.2 or 4.2, 2.3.5.1). `unlock(password)` checks the verifier
  and returns `Rc4Keys`, which derives the key of each block.
  `Rc4Keys::keystream` covers a whole stream whose key changes every
  `block_size` bytes; a format then XORs only the bytes it encrypts, so the
  keystream also runs over the plain bytes, as the formats require.
- `unlock_xor_method1` and `decrypt_xor_method1`: XOR obfuscation method 1
  (2.3.7.1 to 2.3.7.3).

A format may have a default password, which an application writes when the
user set none. The constructor tries it, and a file that opens with it reads as
not encrypted, as in LibreOffice: `VelvetSweatshop` for `.xls`.

Tests compare a decrypted stream with what msoffcrypto-tool 6.0.0 decrypts, by
its SHA-1. The fixtures come from the msoffcrypto-tool tests (MIT) and from
LibreOffice, which writes only RC4 (1.1) for `.doc` and `.xls`, standard
encryption for OOXML, and no encrypted `.ppt`.

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
