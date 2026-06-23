# PDF stage 3.5 — Type1 (`/FontFile`)

Design for the Type1 font sub-stage. Status: **design draft** (no implementation
yet — this PR seeds the branch). Roadmap entry lives in
[`src/odr/internal/pdf/AGENTS.md`](../../../src/odr/internal/pdf/AGENTS.md).

Stacked on **3.4** — the whole point is to reuse 3.4's CFF → OTF path, so Type1
support is "translate Type1 to CFF, then everything downstream is 3.4".

## Goal

Read a PDF `/FontFile` (a Type1 / PostScript font program) and render it through
the same `@font-face` + dual-layer pipeline as TrueType (3.3) and CFF (3.4). The
hardest single font piece, but precisely specified (Adobe *Type 1 Font Format*
T1 spec; pdf.js as a reference implementation).

## What gets read (`internal/font/type1_font.{hpp,cpp}`)

`/FontFile` has three parts sized by the descriptor's `/Length1` (clear ASCII),
`/Length2` (binary eexec), `/Length3` (trailer of zeros + `cleartomark`):

1. **Clear text** — `/Encoding` (code → glyph name, or `StandardEncoding`),
   `/FontMatrix`, `/FontBBox`, `/FontName`.
2. **eexec section** — decrypt with R = 55665 (skip the 4 random bytes), then
   parse:
   - **`/Subrs`** — index → (decrypted) charstring.
   - **`/CharStrings`** — glyph name → charstring; each charstring decrypted
     with R = 4330, `lenIV` (default 4) leading bytes dropped.
3. **Trailer** — ignored.

PFB segment framing (`0x80` markers) is handled if present; PDF embeds the raw
three-segment form.

## Type1 → Type2 charstring translation (the core)

Translate each decrypted **Type1** charstring into a **Type2 (CFF)** charstring,
then build a CFF and hand it to 3.4's wrap. The non-trivial cases:

- `hsbw` → seed the left side bearing + advance width, emit as the Type2 width +
  initial `rmoveto`.
- `seac` (accented composite) → decompose into base + accent (StandardEncoding
  lookup), or emit `endchar` with the seac operands (Type2 deprecated-seac form).
- `div`, `callsubr` / `return` (Subrs), and the `callothersubr` family —
  **flex** (OtherSubrs 0–2) and **hint replacement** (OtherSubr 3) must be
  interpreted/flattened, not passed through; this is the part that needs care.
- hint operators (`hstem`/`vstem`/`dotsection`) → Type2 equivalents (or drop;
  display tolerates missing hints).

Output: a `cff::CffFont` (3.4) built from the translated charstrings, a charset
from the glyph names, and a private dict carrying the widths. Everything after
that — OTF wrap, PUA re-encode, OTS gate — is 3.4 unchanged.

## Reverse map

Charstring **glyph names** → AGL → Unicode (reuse `pdf_encoding`), same shape as
CFF. A symbolic Type1 with a built-in encoding becomes selectable via this map.

## PDF wiring (reuse 3.3 / 3.4)

- `pdf_document_parser`: `/FontFile` → `Type1Font` → (translate) → `CffFont` →
  `Font::embedded_font`.
- `Font::glyph_for_code` simple-font branch resolves code → glyph name via the
  PDF `/Encoding` (Differences over base) or the font's built-in `/Encoding`,
  then name → glyph id through the CFF charset.
- `to_unicode` reverse-map fallback and HTML dual-layer emission unchanged.

## Scope / non-goals

- CID-keyed Type1 (Type1 in a Type0, rare) — defer unless a corpus file needs it.
- Multiple Master Type1 — out of scope.
- Hinting fidelity is best-effort (display only).

## Tests

Font-only, assertion-based: a minimal hand-built (or frozen-literal) Type1 —
eexec + charstring decryption, an `hsbw` + a `flex`/hint-replacement charstring
translated and round-tripped through 3.4's CFF wrap and OTS, the glyph-name
reverse map. Plus a `pdf_document_parser` case: `/FontFile` → `embedded_font`
with Unicode recovery.
