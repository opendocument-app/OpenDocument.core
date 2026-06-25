# PDF→HTML output size — PR-by-PR plan

## Problem

The regenerated reference output
`test/data/reference-output/odr-private/output/pdf/Core_v5.1.pdf/document.html`
is **129 MB**, over GitHub's 100 MB hard limit, so the submodule
(`test/data/reference-output/odr-private`, a private repo) can no longer be
pushed. We want the file smaller while staying a **plain-text, line-diffable**
artifact (no gzip, no LFS).

## Where the bytes go (Core_v5.1, 129 MB total)

| Component | Size | Notes |
|---|---|---|
| `<span ...>` open tags (1.70M) | ~52 MB | `<span class="` literal alone ≈ 22 MB |
| `</span>` close tags (1.70M) | ~12 MB | |
| `d="..."` path coordinate data | 23 MB | 151k vector paths, already 2-decimal |
| class-string content | 27 MB | 8.6 MB of it on glyph spans |
| leading-whitespace indentation | 4.5 MB | ~1.2M lines, `\n` + spaces |
| base64 `@font-face` fonts | 2 MB | |

The text layer dominates because every embedded-font run is emitted **twice**
(`src/odr/internal/html/pdf_file.cpp:540-550`):

```html
<span class="t l1 t1 f1 i">RealUnicode<span class="t g gv ff4">«PUA glyphs»</span></span>
```

- **Outer** span: real Unicode, `color:transparent` (`.i`) — owns copy/search.
- **Inner** span: PUA code points (`U+E000 + glyph`, `sfnt_transform.cpp:21`)
  rendered in the re-encoded `@font-face` font (`.gv` = `color:#000`) — owns the
  visible pixels; `.g` keeps it off the clipboard.

→ 1.70M spans instead of ~0.85M. The duplicated `<span>`/`</span>` framing plus
duplicated class strings is ~34 MB.

**Why poppler's output (84 MB) is smaller despite carrying raster images:** it
bakes glyphs into the page raster and emits a **single** transparent text layer
for selection. It never pays for a second per-glyph render layer. Collapsing our
dual layer is the same move.

## Plan: three stacked PRs

Logically independent (all touch `html/pdf_file.cpp`); stacked for review in the
order below. **Each PR regenerates and commits the reference output** in the
`test/data/reference-output/odr-private` submodule, and the diff is expected to
be large (especially PR 1). Build/test in `cmake-build-relwithdebinfo`.

Projected cumulative size: 129 → ~94 (PR1) → ~90 (PR2) → ~86 (PR3) MB.

---

### PR 1 — Collapse the dual text layer (the structural win, ~−28 MB)

**Idea.** When a run's codes map 1:1 to real Unicode scalars that uniquely
identify their glyphs, emit a **single** span that is both visible and
selectable, rendering the *real Unicode* directly in the embedded font. Fall
back to today's dual layer only for the cases that genuinely need it.

**Font side (`font/sfnt_transform.*`, `font/cff_transform.*`, extraction).**
Today the re-encoded `cmap` is a uniform PUA map: `pua_code_point(glyph) → glyph`
(`cff_transform.cpp:144-170`). Extend it with **real-Unicode → glyph** entries:

- During page extraction, collect the set of `(unicode_scalar, glyph)` pairs
  actually used per font (we already know both: the glyph index drives the PUA
  code, the Unicode comes from `/ToUnicode`).
- Add those as extra `cmap` entries alongside the PUA range. **Keep the full PUA
  range** as a fallback so nothing stops rendering.
- Resolve conflicts deterministically: if two glyphs claim the same Unicode
  scalar, only the first wins a real-Unicode entry; the rest stay PUA-only and
  their runs keep the dual layer. Record which `(unicode)`→`(glyph)` mappings
  "won" so the HTML side knows which runs can collapse.
- `serialize_cmap` is BMP/format-4 only (`sfnt_transform.cpp:149`); real-Unicode
  entries above the BMP keep the dual layer (rare).

**HTML side (`html/pdf_file.cpp:525-569`).** A run **collapses to one span** iff:

- it has an embedded font (`font != 0`), and
- `text.text` is non-empty, and
- the run is 1:1 (`spacing_one_to_one`, already computed at `:509-512`: no
  ToUnicode expansion / ActualText / inferred spaces — i.e.
  `utf8_length(text.text) == advances.size()`), and
- every glyph in the run won a real-Unicode `cmap` entry (no conflict).

Then emit `base + " ff" + font` carrying `escape_text(text.text)` and **no
nested glyph span** — the placement classes already on `base` apply, color is the
normal `.gv` black (drop `.i`/`.g`; the span is the selectable text). Otherwise
keep the existing dual-layer branch verbatim.

**Risks / tests.** Pixel fidelity must not regress — the perceptual-diff oracle
(STAGE4_PLAN 4.18) guards this. Watch: glyphs whose Unicode differs from their
shape (small caps, swashes, alt forms) — these are exactly the conflict cases
that must *not* collapse. Add focused tests for a clean 1:1 font (collapses), a
ligature/ToUnicode-expansion run (stays dual), and a font with two glyphs
sharing one Unicode (first collapses, second stays dual).

---

### PR 2 — Shorten glyph-layer classes (~−4 MB on remaining fallback spans)

After PR 1 the nested glyph span survives only on fallback runs, but it still
carries the verbose `t g gv ffN` / `t g i ffN` (4 atomic classes). Fold the
constant trio into one combined per-(paint, font) class:

- Define, per used font `N`, `.gvN` (visible) and/or `.giN` (invisible):
  `position:absolute;left:0;top:0;transform-origin:0 0;white-space:pre;
  user-select:none;color:#000;font-family:'odr-fN'`. (~110 bytes × ≤108 rules ≈
  12 KB of CSS — negligible.)
- Emit the nested glyph span as `class="gv4"` instead of `class="t g gv ff4"`.
- Keep `.t`, `.g`, `.gv`, `.i`, `.ffN` for any spans that still use them.

Mechanical, fully diff-safe. Saving scales with how many fallback glyph spans
remain after PR 1 (≈−7 MB if PR 2 is measured *before* PR 1's collapse).

---

### PR 3 — Single-tab indentation (~−4 MB)

The writer indents nested elements with multiple spaces
(`html/pdf_file.cpp` write pass). Indentation carries no information for a
line-based diff. Replace per-level multi-space indentation with a **single tab**
(or drop leading indentation entirely while keeping one item per line). Keeps the
one-element-per-line layout the writer already chose for diff legibility
(`pdf_file.cpp:623`), just cheaper per line.

Verify the size win and that the per-line structure (hence diffability) is
unchanged.

---

## Validation per PR

1. Build in `cmake-build-relwithdebinfo`, regenerate the Core_v5.1 output.
2. Confirm the file shrinks as projected and is still valid, selectable HTML.
3. Run the perceptual-diff oracle — **no visual regression**.
4. Regenerate **all** PDF reference outputs and commit them in the
   `test/data/reference-output/odr-{public,private}` submodules; bump the
   superproject's submodule pointers.
