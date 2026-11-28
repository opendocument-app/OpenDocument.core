# Progressive text pagination

Status: initial implementation. `pagination.js` projects the source through
`document-view.js`; the editor and viewport remain separate scripts. It paginates
text documents; slides and PDF pages already have their own physical boundaries.

## Configuration and API

| HtmlConfig | JavaScript option | Default |
|---|---|---|
| `text_document_pagination` | `enabled` | false |
| `text_document_page_breaks` | `explicitBreaks` | true |
| `text_document_page_borders` | `showPageBorders` | true |
| `text_document_page_animation` | `animate` | true |

Python uses these C++ names. Java, Swift/ObjC and wasm use camelCase.

```js
odr.pagination.setOptions({ enabled: true, animate: true });
odr.pagination.getOptions();
odr.pagination.getState();
odr.pagination.getPages();
odr.pagination.refresh();
```

`getState()` returns `enabled`, `active`, `complete`, `pageCount`,
`processedBlocks`, `totalBlocks` and `oversizedPages`. Enabled is the requested
mode; active is false during search and print. Editing keeps pagination active.
Counts cover the pages formatted so far, and are final only when complete is
true. `getPages()` lists those pages' layout keys, actual dimensions in CSS
pixels and oversized flags.
`refresh()` restarts layout after a host changes content or styles directly.

`odr.onPaginationChange(state)` and the `odr-pagination-change` DOM event report
progress. The host bridge forwards `paginationChange`. The viewport's existing
page count/navigation API sees pages as they become available. It cannot jump
to an unformatted page; text documents still use continuous page layout.

While a frame, a restart or a deferred restart is queued, the pagination root
carries `aria-busy="true"`. It is cleared once layout is idle, which includes
stopping short of the end of a long document far below the viewport. The
reference tests' screenshots (`compare-html` 2.5.0 and later) wait for it.

Disabling reveals the source DOM. `text_document_margin` selects the initial
fallback: flowing text, or boxes at manual breaks. On first activation, legacy
page-box bodies are normalized into one source flow so paragraph navigation
can cross manual page boundaries. Later disabling/printing uses that flow,
retaining authored breaks. The margin option does not control automatic
pagination. Templates and the script are supplied even when pagination starts
disabled, so a host can enable it without rendering again.

Automatic fitting in an iframe is recalculated as pages appear or change width,
including runtime activation. Explicit zoom pins and actual-size settings remain
in control. Top-level automatic fitting retains native viewport-meta behavior;
runtime activation from flow still needs mobile-browser validation. Use
`fit_width_by_view` when the script must manage top-level fitting.

## Core's layout contract

The renderer consumes the [page-layout markup](text-page-layout.md) exported
from core's existing model. When dimensions are absent, it displays A4 as a
fallback; the model remains unchanged. Disabling explicit breaks does not
suppress authored layout changes.

## Work near the viewport

Each animation frame processes blocks/fragments for six milliseconds; one
browser layout operation can exceed that budget. Pagination stops when the
current page ends more than two viewport heights below the visible viewport.
Scrolling advances the frontier. Unvisited source blocks remain in an estimated
flow, so the scrollbar and final page count can change as the document is
formatted. That flow is split into chunks of 64
blocks with `content-visibility: auto`, estimated at 3em a block until shown:
an offscreen chunk skips layout, so measuring a page does not lay out every
block after it. The chunks carry the property rather than each block because
Chrome visits every such element on each style update.

Text paragraphs are measured with a growing prefix and lookahead, then split
at a rendered line boundary using DOM ranges. A very long paragraph therefore
does not have to be laid out in full to create its first page. Paragraph
continuations omit the first-line indent and the top margin. Headers and footers
occupy space inside the margins; the body gets the remaining height. A short
opacity/translation animation introduces new pages unless reduced motion is
requested. Between edits a batch only adds pages at the frontier, so the text
above it does not move and the scroll offset is kept. Edit reflow captures a
reading-position anchor before removing page boxes, since that removal can clamp
the browser's scroll position. If the viewport starts in a margin or header,
reflow restores the saved scroll offset instead. The column temporarily retains
its height until the visible pages are rebuilt, so an intermediate frame cannot
clamp that offset. Unformatted remainders are not used as reading anchors.

Font completion, image loading and a width change invalidate pagination,
coalesced into one restart. A height-only resize, such as a soft keyboard
opening, keeps the pages. They currently restart from the source, rather than invalidating
only an affected suffix. Previously measured pages remain in the DOM; this is
progressive layout, not a sliding DOM window.

## Editing, selection and search

The original paragraph is never split. Temporary fragments clone ranges from
it, strip DOM/editing IDs, and carry a private source-offset mapping. A pending
remainder supplies the unformatted tail and is shortened in place as pages
advance, rather than cloned again for each page. Toggling pagination restores the
original paragraph and text nodes, rather than attempting to merge clones.

Selection endpoints map between original nodes and fragments, including a
selection crossing a physical page boundary. The copy handler reconstructs
selected logical paragraphs so page splits do not introduce extra paragraph
breaks. Layout operations never append editing operations.

The [source/view adapter](paginated-editing.md) maps commands and logical
selection through the fragments, preserving original IDs and version 2 editing
operations. Headers and footers remain outside the editable projection.

Edits invalidate pages from the affected paragraph/page onward. Text-only edits
inside a long paragraph preserve earlier fragments, with one preceding page of
lookbehind. Visible reflow happens immediately; subsequent work uses the same
frame budget and viewport frontier. Replacement pages do not animate on every
keystroke. Layout convergence/reuse of an unchanged downstream suffix remains
future work.

Composition pins its active run fragment, mirrors each native input into the
complete source run, and defers layout until composition ends. A selected range
is replaced in the source before composition begins so the browser never deletes
across page wrappers. See [`paginated-editing.md`](paginated-editing.md).

Search suspends pagination before creating marks, and resumes after clearing
them. Printing reveals the source synchronously and uses native browser
fragmentation, then resumes screen pagination afterward.

## Remaining fidelity work

- Split tables at appropriate row boundaries, with repeated heading rows.
- Handle inline drawings and protected fields while splitting paragraphs.
  Those paragraphs, paragraphs containing bookmark targets, tables and other
  compound blocks currently remain whole.
- Honor widow/orphan counts, keep-with-next, keep-together, multi-column layout,
  and left/right break parity. Forced break values currently start the next page.
- Preserve deliberate blank pages from leading/consecutive break nodes; current
  behavior follows the existing renderer's coalescing of those breaks.
- Match page-relative fields, anchored drawings, format-specific section rules
  and print output to the computed screen pages. Page-number fields are not
  recomputed by this script.
- Reuse unchanged downstream pages once reflow converges; narrow font/image
  invalidation instead of restarting layout.
- Validate composition with Safari/WebKit and physical mobile keyboards.

An indivisible block taller than the available body expands its page rather
than clipping or overlapping the footer. `oversizedPages` and each page's
`oversized` flag expose this departure from the physical paper height.

## Verification

`test/browser/pagination` covers lazy work, mixed sizes, marginals, manual
breaks, reversible toggles, long paragraphs, selection/copy across splits,
live editing, local reflow, composition, search suspension and native-print
restoration. Native keyboard checks export a diff that is replayed into core,
saved and reopened to compare text, paragraphs and formatting. C++ HTML tests
check the templates, master graph and defaults; binding tests check that the
configuration crosses each interface.
