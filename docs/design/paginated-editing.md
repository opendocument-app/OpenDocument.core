# Editing through projected fragments

`document-view.js` separates the editor's authoritative source DOM from a
renderer that displays fragments of that source. The adapter is inactive until
a renderer connects; continuous editing retains its existing behavior.

## Source and view

The source remains connected in `.odr-pagination-original`. Editor commands
target its original paragraphs and runs, identified by core's existing IDs.
Fragments carry private source mappings rather than duplicate editing IDs.
The renderer may hide the source, but must never put browser selection there
while its projection is active.

A logical point names a source run or paragraph, a UTF-16 offset and an
affinity (`upstream` or `downstream`). Affinity distinguishes the end of one
fragment from the start of its continuation. Ranges retain selection direction.
Repeated headers and other noneditable decorations have no document addresses.

The private `odr.documentView` interface provides mapping through `bind`,
`sourcePoint`, `inputRange`, `capture`, `restore` and `select`. Its renderer
connection supplies `active`, `point`, `update` and `pause`. Source mutations
invalidate the projection; the renderer decides how much layout to rebuild.

## Editing transaction

1. Map the browser's input range or selection into source runs.
2. Apply the existing editor command and record one undo gesture.
3. Refresh the projection and restore the resulting logical selection.

Undo snapshots name source nodes, not temporary fragments. Commands, style
reporting and operation coalescing remain shared with continuous editing.
Version 2 operations still carry whole replacement strings against original IDs:
editing one fragment of run 41 produces `setText` for the complete run 41.
Only actual paragraph edits emit `splitParagraph`; visual splits emit no edits.

## Composition

IME and some mobile input cannot reliably be canceled. The adapter pins the
active fragment and mirrors each native input into the complete source run,
preserving the undisplayed prefix and suffix. Layout resumes after composition.

Scope and protected-content restrictions are checked before normalization.
A selected range is replaced in source and projected before capturing the
composition fragment's prefix/suffix, including noncancelable input without a
preceding `compositionstart`. Rejected composition blurs the editable host.

Host formatting/history commands, disabling editing and renderer transitions
finish composition before changing its fragment. If a keyboard omits
`compositionend` on blur, the editor explicitly syncs and finishes it.

## Verification

`test/browser/document-view` uses a fixed-offset projection, independent of page
layout, to exercise mapping, native editing, composition and undo. The existing
text/slide checks continue to cover the editor without a connected projection.
Real Safari/WebKit composition and physical mobile keyboards still need validation.
