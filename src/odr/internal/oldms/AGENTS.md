# Legacy MS Office (`oldms/`) — shared status & conventions

What the binary legacy-format modules share. Each format's own status, design
notes, and open work live with its module; this file holds the conventions they
build on and the one piece of open work common to all three. Spec links are in
[`README.md`](README.md), the PDFs under `offline/documentation/`.

| Module                           | Format                 | Agent doc                       |
|----------------------------------|------------------------|---------------------------------|
| [`text/`](text/)                 | `.doc` (Word)          | [text/AGENTS.md](text/AGENTS.md)        |
| [`presentation/`](presentation/) | `.ppt` (PowerPoint)    | [presentation/AGENTS.md](presentation/AGENTS.md) |
| [`spreadsheet/`](spreadsheet/)   | `.xls` (Excel / BIFF8) | [spreadsheet/AGENTS.md](spreadsheet/AGENTS.md) |

## Shared conventions

All three modules follow the same approach; the per-format docs cover only what
is specific to each format.

- **CFB container.** Each format is a `[MS-CFB]` compound file; container
  handling already existed in the engine. Each module reads its stream(s)
  sequentially.
- **Byte-copy structs.** Fixed-layout spec structures are `#pragma pack(1)`
  structs in the `*_structs.hpp` headers, with the spec's field names and
  `[MS-*]` section citations, guarded by `static_assert(sizeof ...)`, filled by
  copying the file's bytes straight in.
- **Bit-fields mirror the spec tables.** Sub-byte fields are declared as
  bit-fields in the spec's order (LSB-first): `FibBase`/`Sprm` (`.doc`),
  `RecordHeader` (`.ppt`), `RkNumber`/`UnicodeStringFlags` (`.xls`).
- **Little-endian, LSB-first hosts only.** The byte copy interprets bytes in the
  host's byte order and bit-fields in the host's allocation order. See below.
- **Fail early on malformed input**; records/structures that are merely *not
  modelled* are skipped.

## Endianness and bit order: little-endian host assumed (shared open work)

All three modules read multi-byte fields and UTF-16 code units in the host's
byte order with no swap, and their bit-field structs assume LSB-first
allocation.

The file side is fixed: `[MS-DOC]`, `[MS-PPT]`, `[MS-XLS]` and the `[MS-CFB]`
container all store little-endian unconditionally — there is no big-endian
variant — so no runtime detection is needed. Only the host varies, and that is
known at compile time (`std::endian::native`). Conveniently, GCC/Clang switch
bit-field allocation to MSB-first exactly on big-endian targets, so byte order
and bit order flip together and one compile-time guard covers both. (The flip is
each ABI keeping declaration order equal to memory order: the first declared
field lands in the first byte either way.)

**Fix if a non-little-endian target matters**: give each struct in the
`*_structs.hpp` headers a fixup function, applied right after the raw byte copy,
that byte-swaps the multi-byte fields and re-places the bit-field values. It has
to be per struct — a blind byte swap cannot fix bit-fields, the transform needs
the field widths. On little-endian hosts every fixup compiles to a no-op.

**Rejected alternative**, for the record: `#if`-mirrored bit-field declarations
(the Linux `iphdr` pattern). Reversing declaration order repositions fields
within the allocation unit but cannot change how the unit's bytes are assembled,
so any field that straddles a byte boundary — `Sprm.ispmd` (9 bits),
`FcCompressed.fc` (30 bits), `RkNumber.num` (30 bits), `RecordHeader.recInstance`
(12 bits) — ends up in non-contiguous bits on a big-endian reader; only fixing
the data can express that. The pattern pays off only for zero-copy in-place
access (mapped packets/pages), which these formats rule out anyway: CFB streams
are fragmented into sectors and `.xls` records into `CONTINUE` chunks, so structs
are always assembled by copying — the fixup point is structural.
