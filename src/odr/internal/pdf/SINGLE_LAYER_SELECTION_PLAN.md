# Single-layer selection — design discussion & conclusions

Status: **design only, not implemented.** This records a discussion about
evolving the PDF→HTML text model from the current dual-layer scheme toward a
mostly single-layer one (à la pdf2htmlEX) for native browser
find/select/copy. No code in this direction has been written yet.

## Background: where we are today

- **Dual layer.** A visual glyph layer (`PageOut::items`, paint order,
  PUA-re-encoded glyphs in the embedded font, `user-select:none`,
  `aria-hidden`) plus a separate transparent selection layer (real Unicode,
  `.i` = `color:transparent`).
- **Uniform PUA re-encode (decision 2026-06-19).** Every glyph is re-encoded
  to `U+E000 + glyph_id` for display; the extracted Unicode is carried
  separately for the selection layer. Chosen to dodge cmap collision /
  ligature / no_unicode problems and to decouple display fidelity from
  text-extraction gaps.
- **Per-line flow blocks (current branch `pdf-text-selection-layer`).** The
  selection layer was changed from per-run absolutely-positioned spans to one
  absolutely-positioned `<div>` per PDF line containing inline `<span>` runs
  that flow naturally; inter-run gaps are carried as separator spans with
  `data-w` (gap width), and an on-load JS pass fits each run by setting
  `letter-spacing = (data-w − measured) / textContent.length`.

The remaining weakness: the selection layer renders in an **unknown system
font**, so its advances don't match the PDF, hence the runtime JS fit.

## The question explored

Should we drop the separate transparent selection layer and use a single
layer that is simultaneously visualized and selected — like pdf2htmlEX, which
uses one `<div>` per line with relatively-shifted spans (negative margins)?
And how to handle ligatures / non-invertible Unicode mappings?

## Conclusions

### 1. Correct the font-vs-PDF advance with margins, not by rewriting `hmtx`

- **Don't touch `hmtx`.** Stay with the pass-through-font philosophy (cmap
  rewrite only, no outline surgery). Correct the difference between the
  embedded font's advances and the PDF's desired x-positions with
  pdf2htmlEX-style offset / negative-margin spans at run/word boundaries.
- **The key enabler of a single layer:** the browser renders *our embedded
  font*, whose advances we know exactly. So the correction is computable at
  **generation time** (`desired_x − Σ embedded advances`) — no JS runtime
  measurement. This is precisely what the transparent layer could never do,
  because it rendered in an unknown system font.
- Intra-word drift is normally zero because PDF `/Widths` are usually derived
  from the font's own `hmtx`; corrections are only needed where they diverge
  (subsetting, `Tz`, synthetic widths).
- The **Q3 conflict** (`hmtx` holds one advance per glyph id, but a PDF can
  require different advances for the same glyph) stops being a font problem:
  it's just another margin correction at the occurrence.
- Run-level `Tc`/`Tw`/`Tz` remain run-level CSS constants
  (`letter-spacing`/`word-spacing`/scale), as today — not per-glyph.

### 2. A single layer forces clean glyphs to carry *real* Unicode

This is the non-obvious consequence and a **partial reversal of the uniform
PUA decision**:

- Browser find (Ctrl+F) indexes *rendered text runs*. `user-select:none` does
  **not** exclude an element from find, and neither does `color:transparent`.
- The only reason today's PUA glyphs are invisible to find is that their
  codepoint is `U+E000+gid` — no real-word query matches them.
- Therefore, for find/copy to work in a single layer, the **visible** glyph's
  text content must *be* the real Unicode character, which means the embedded
  font needs a **real-Unicode cmap entry** for that glyph (not a PUA one).

So the model splits by glyph:

- **Clean, unambiguous glyphs (the majority):** real-Unicode cmap entry → the
  visible character is correctly shaped *and* real text → natively findable,
  selectable, copyable, positioned by gen-time margins. Single layer, no
  overlay.
- **Ambiguous / ligature / no_unicode glyphs (the minority):** the visible
  glyph must be kept **out of the DOM text stream entirely** (see §3) +
  an overlay carrying the real Unicode.

This is essentially pdf2htmlEX's strategy adapted to odr's font philosophy:
still cmap-rewrite only, just building a real-Unicode cmap for the invertible
subset instead of a blanket PUA cmap.

### 3. Overlay for the unclean minority — and why `user-select:none` is not enough

> **Correction to an earlier draft of this plan.** The first version said
> the unclean glyph could stay PUA *DOM text* with `user-select:none`, and
> that find would ignore it "for free" because no real query matches a
> private-use codepoint. That is true only when the glyph sits at the *end* of
> meaningful text. **Mid-word it breaks**, because the PUA codepoint is still
> in the DOM text stream, wedged between the real characters.

Take "final" where "fi" is a single ligature glyph. If the visible glyph is
PUA *DOM text*, the browser's text stream is:

```
"fi" (overlay) + "\uE0xx" (PUA glyph) + "n" + "a" + "l"  →  "fi\uE0xxnal"
```

- **Search "final"** fails — `user-select:none` blocks *selection*, not
  *find*; find still reads the PUA codepoint, so the word is no longer
  contiguous.
- **Double-click** — the word segmenter treats the PUA char as a boundary, so
  you don't cleanly get "final".
- **Triple-click** copies `"fi\uE0xxnal"` — the garbage codepoint lands on the
  clipboard.

So a mid-word PUA glyph rendered as DOM text is broken for exactly
find / double-click / triple-click.

**Fix: render the unclean glyph as CSS generated content, not DOM text.**
`user-select:none` is too weak; `color:transparent` text is still found and
selected. The only way to make a *visible* glyph invisible to find,
word-segmentation, *and* selection is to take it out of the document text:

```css
.lig::before { content: "\e0xx"; }   /* embedded font on .lig; pointer-events:none */
```

Generated content (`::before`/`content`) is **not** part of the DOM text — not
findable, not selectable, never on the clipboard. The `.lig` element itself is
an *empty* span. So the only DOM text at that position is the overlapping
transparent real-Unicode overlay, and the stream becomes contiguous:

```
"fi" (overlay, transparent) + "n" + "a" + "l"  →  "final"
```

With that:

- **Search "final"** matches — the glyph contributes nothing; find concatenates
  across sibling inline spans, so per-glyph "n"/"a"/"l" spans don't break it.
- **Double-click** word-selects "final" and copies "final" — *provided the
  click hit-tests onto the selectable overlay*. So the selectable transparent
  text must be **on top**, with the visible glyph layer behind it and
  `pointer-events:none`, so clicks fall through to the real text.
- **Triple-click** selects the line block's selectable text in DOM order →
  real text, glyph skipped.
- **DOM order = reading order**, so drag-select copies in the right sequence.
- **Truly unknown glyphs (no_unicode):** generated-content glyph, no overlay.
  That text stays uncopyable/unsearchable — honest, since we don't know what
  it says.

#### Sizing the overlay: `width` vs `margin` + zero width

Three jobs pull in different directions and **cannot all be satisfied for
free**:

1. supply real searchable/selectable text in reading position → wants **plain
   `inline`**, in DOM order (so find concatenates it with neighbors);
2. make the selection-highlight rectangle cover the glyph → wants a **sized
   box** at the glyph's x;
3. not disturb the layout advance — the glyph already supplies it via its
   `::before` box → wants **zero net advance** on the overlay.

The two shapes considered:

- **`display:inline-block; width:<glyph_px>; overflow:hidden`** — satisfies (2)
  and (3) directly and needs no runtime measurement (only the box matters; the
  transparent text inside is irrelevant). *But* an atomic inline-block can
  **break the find text run across its boundary** in some browsers, which
  reintroduces the contiguity problem from above. So it trades find-correctness
  for a perfect highlight box.
- **plain `inline` + `width:0`** — `width` has **no effect on a non-replaced
  inline** element, so this does nothing on its own. To zero a plain inline's
  advance you must cancel its own rendered width with `margin-right:
  -<own_width>`. Horizontal margins *do* apply to inline elements, so the
  shift works — but `<own_width>` is the overlay text rendered in an **unknown
  system font** (browsers fall back per-character when the embedded subset
  lacks `f`/`i`), so the cancel value is not known at generation time →
  runtime measurement, defeating the no-JS goal.

So the honest statement: you can't get *plain-inline find-contiguity* +
*zero advance* + *no runtime measurement* simultaneously. The candidates,
each giving up one thing:

- give up **no-measurement**: plain inline + a tiny JS `margin-right` cancel —
  but only for the rare unclean glyphs (a small, local fit, not a whole-line
  one);
- give up **contiguity-guarantee**: inline-block box + verify on target
  browsers that find still matches across it (it may — needs testing);
- give up **system-font fallback**: ship a controlled blank/zero glyph in the
  overlay font and a cmap entry for the overlay codepoints so the overlay
  renders in *our* font with a **known** advance, cancelable at gen time with
  no measurement — at the cost of more font work.

For find/copy correctness the overlay width is irrelevant (only DOM text +
order matter); width only affects the highlight rectangle. So if forced to
choose, prefer the contiguity-safe `inline` shape and accept a loose highlight
over a ligature. **This needs empirical testing on the target browsers before
the shape is fixed.**

Net effect: the dual-layer complexity shrinks from "everything" to "the rare
unclean glyphs," which become a small *local* overlay (generated-content glyph
+ overlapping transparent real-Unicode text). The runtime JS fit disappears
for the clean majority.

## Resulting architecture (proposed)

- Build a real-Unicode cmap for the embedded subset where the Unicode→glyph
  mapping is unambiguous and invertible.
- Those glyphs render from real codepoints → single findable/selectable layer,
  positioned by gen-time negative margins computed from embedded-font
  advances. No JS.
- Ambiguous / ligature / no_unicode glyphs render the glyph via CSS
  generated content (out of the DOM text stream, `pointer-events:none`) with
  an overlapping transparent real-Unicode overlay where the Unicode is known;
  truly unknown glyphs get no overlay. The overlay's box shape
  (`inline` vs `inline-block`) is an open trade-off — see §3.

## The real work hiding here

The cmap-builder's **collision / eligibility policy**: deciding, per glyph,
whether a clean real-Unicode entry is possible or it falls back to
PUA + overlay. Before committing, the thing worth measuring is how big the
clean majority actually is across the corpus:

- how often the embedded `hmtx` already equals the PDF `/Widths` (→ how often
  margins are even needed),
- how often the same glyph carries conflicting widths (Q3 frequency),
- how often Unicode→glyph is non-invertible (→ overlay frequency).

That ratio decides whether we're mostly in the correction-free single-layer
path or frequently emitting corrections/overlays.

## Open / not decided

- Whether to proceed at all, vs. keeping the current dual layer with the JS
  fit.
- The eligibility-pass sketch against the current font path (offered, not yet
  done).
- The overlay box shape for unclean glyphs (`inline` vs `inline-block` vs a
  controlled overlay font) — needs browser testing of find-contiguity vs
  highlight-box quality (§3).
