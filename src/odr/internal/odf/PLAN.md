# ODF drawings plan

Closing #771: everything an ODF shape carries beyond its bounding box —
`draw:transform`, the shape elements with no parser, `draw:enhanced-geometry`,
and the `draw:object` chart. The module as it stands is in
[`AGENTS.md`](AGENTS.md); the feature checklist is [`README.md`](README.md).
Keep this file honest as stages land, and delete it when nothing is left
under *Today*.

## Today

All five stages have landed. What is left is listed under each of them and in
[`README.md`](README.md): `draw:text-areas` and `draw:handle` on an enhanced
geometry, the arrowheads `draw:marker` names, `dr3d:scene`, and the chart
features below stage 5.

## The corpus

Every number below is `<tag` / `attr=` occurrences over the 124 ODF files in
`test/data/input` (both repositories), counted across every `.xml` part. It is
the evidence behind the ordering, and behind what is *not* built.

| markup | occurrences | files |
|---|---|---|
| `draw:enhanced-geometry` | 350 | 10 |
| `draw:object` | 1122 | 21 |
| `draw:transform=` | 25 | 4 |
| `draw:mirror-vertical=` / `-horizontal=` | 52 / 37 | 4 |
| `draw:measure` | 27 | 1 |
| `draw:path` | 6 | 4 |
| `draw:ellipse` | 3 | 2 |
| `draw:connector` | 2 | 1 |
| `draw:object-ole` | 2 | 2 |
| `draw:polygon`, `draw:polyline`, `draw:caption`, `draw:regular-polygon`, `dr3d:scene` | 0 | 0 |

`draw:transform` carries `rotate`, `translate` and `skewX`, and appears only on
`draw:custom-shape` (17), `draw:frame` (4) and `draw:path` (4).

Of the 350 `draw:enhanced-path` values, the commands used are `M` (275),
`L` (328), `C` (41), `U` (116), `X` (38), `Y` (38), `Z` (390) and `N` (389).
The functions used in `draw:formula` are `if` (512), `pi` (203), `sin` (103),
`cos` (100), `abs` (32), `logwidth`/`logheight` (18 each) and
`left`/`top`/`right`/`bottom` (6 each).

## Decisions taken up front

**No new `ElementType`.** Collapsing tags onto a general type is the house
style — `text:h` → paragraph, `text:section` → group, `draw:g` → frame — and
the alternative is expensive: `custom_shape` alone is named in 14 files across
four binding layers (`document_element.{hpp,cpp}`, `abstract/document.hpp`,
`odf_document.cpp`, `html/document_{element,style}.{hpp,cpp}`, plus JNI, Apple
and Python), and five of the nine unparsed shape tags occur nowhere in the
corpus. So `custom_shape` becomes *the* type for "a shape whose geometry is
given rather than named", which is what ODF's own term means, and the rest map
onto the existing types:

| tag | type | why |
|---|---|---|
| `draw:path`, `draw:polygon`, `draw:polyline`, `draw:regular-polygon`, `draw:connector` | `custom_shape` | geometry given as a path |
| `draw:ellipse`, and `draw:circle` `draw:kind` cuts | `circle` / `custom_shape` | box, or the arc it traces |
| `draw:measure` | `line` | `svg:x1/y1/x2/y2`, same as `draw:line` |
| `draw:caption` | `rect` | the box; the callout tail is dropped |
| `dr3d:scene` | — | not modelled; a 3-D scene is not a 2-D path |

**Geometry reaches the renderer as an SVG path.** `CustomShape` grows
`path()` (an SVG `d`) and `view_box()` (the user-space box `d` is written in).
`draw:path` already stores exactly that pair; `draw:polygon`, `draw:polyline`,
`draw:regular-polygon`, `draw:connector` and a resolved `draw:enhanced-geometry`
are all converted into it. One renderer branch then serves every shape, and the
`<div>` fallback stays for a shape with no geometry.

Emitting a path is not "passing the file's markup through": `svg:d` is parsed
into commands and re-serialised by us, so nothing the file wrote reaches the
output as live markup.

**A transform is not a style.** `get_intermediate_style` walks the *element*
parent chain and overrides down it, so a `GraphicStyle` field would leak a
group's transform onto every child — and CSS already composes a transform
through the nesting. `draw:transform` is therefore an accessor on the shape
handles, next to `x()`/`y()`, not a style property.

**A transform is composed, not passed through.** ODF angles are radians and CSS
wants degrees, so the list has to be read anyway. It composes to
`internal::util::math::Transform2D`'s shape — four unitless numbers and a
translation that carries a length — which is also what `matrix(a b c d e f)`
means in 19.228.

**The SVM replacement image stays.** For `draw:object` it is the fallback when
the chart cannot be read, and it is what a producer that wrote no chart part
leaves behind.

## Stages

Each stage is one pull request, stacked on the one before.

### 1 — `draw:transform` — landed

`DrawingTransform` in the public header; `transform()` on `Frame`, `Rect`,
`Line`, `Circle` and `CustomShape`; `odf_geometry.cpp` reads and composes the
attribute; the renderer writes `transform` + `transform-origin:0 0`.

Two things the spec text does not settle, both fixed against libreoffice's own
svg export of `style-drawing-1.odp`: the list applies to the shape **left to
right**, so a `translate` after a `rotate` is not itself rotated, and `rotate`
is **counter-clockwise** for a positive angle — libreoffice writes svg's
`rotate(-19.4°)` for the file's `rotate (0.34 rad)`. Composing both readings
and comparing against the bounding box libreoffice reports (`x=23084`, exact)
is what decided it.

### 2 — the missing shape elements — landed

The tag → type table above, the geometry conversions, `DrawingPath` and
`CustomShape::path()`, and the renderer branch that draws it into an
`<svg viewBox>`. `translate_circle` became an `<ellipse>`; it wrote `r="50%"`,
wrong for every non-square box. `text:measure` is parsed too, or a measure's
label comes out empty.

`vector-effect="non-scaling-stroke"` on the path: the view box scales, and with
`preserveAspectRatio="none"` unevenly, which the stroke must not follow.

### 3 — `draw:enhanced-path` and `draw:equation`, parser only — landed

`odf_enhanced_geometry.cpp`: the formula language of 20.36 (`$N` modifiers,
`?name` references, the named view-box values, `abs sqrt sin cos tan atan min
max atan2 if`) and every command of 19.145, converted to an svg `d`. Pure
functions over strings, unit-tested from string literals.

Decisions worth knowing: `sin`/`cos` take radians, which the corpus confirms by
writing `sin(105*(pi/180))`; an arc is emitted in segments of at most a half
turn, so the large-arc flag is never needed and a full `U … 0 360` — which one
svg `A` cannot express — still draws; `F` and `S` are read and dropped, since
painting one subpath differently is more than one `d` can say.

### 4 — enhanced geometry, rendered — landed

Stage 3 wired into `CustomShape::path()`: `svg:viewBox`, `draw:modifiers`, the
`draw:equation` children resolved on demand and memoised, and
`draw:mirror-horizontal` / `-vertical` folded into the coordinates as they are
written. Closes #159.

Still open, and deliberately: `draw:text-areas`, so a shape's text is laid out
in the whole box rather than the region the geometry reserves for it;
`draw:handle`, which only matters to an editor; and `F`/`S`, which want one
subpath painted differently from the rest.

`hasstroke` and `hasfill` are always true — the geometry reader has no style
in hand — and no corpus formula reads them.

### 5 — `draw:object` charts — landed

`odf_chart.cpp` renders the embedded part's `<chart:chart>` to svg, and the
object reaches the renderer as an image carrying it, so the existing image path
writes it out. The `draw:image` beside an object is the replacement the producer
wrote, and is skipped where the object itself draws; an object with no chart we
can read — a formula, an ole blob — still leaves it. Closes #179.

Decisions: the plotted values come from the chart's own `local-table` rather
than the cells it names in the host document, which is the snapshot the part
carries and the only one an embedded chart is guaranteed; the layout comes from
`chart:plot-area` and `chartooo:coordinate-region`, so it matches what the
producer laid out rather than something we invent.

Open: stacked and percentage plots, secondary axes, trend lines, data labels,
and the number format an axis names — a date axis shows its serial number
today.

## Not scoped

- **`dr3d:scene`** — a 3-D scene, occurring nowhere in the corpus.
- **`draw:object-ole`** (2 occurrences) — an OLE blob, not ODF markup; what is
  readable there is the replacement image we already draw.
- **Glue points and connector routing.** `draw:connector` carries `svg:d`
  written by the producer, and drawing that is the whole win. Re-routing a
  connector between the shapes it names is layout, not decoding.
- **Editing any of this.** The editor is text-content only (`AGENTS.md`).
- **`draw:type`'s preset shapes.** A named type with no `draw:enhanced-path`
  would need libreoffice's preset table; all 350 in the corpus write the path.
- **`draw:marker`** (28 in the corpus): the arrowheads a stroke ends with, an
  svg `marker` and a separate piece of style work.
