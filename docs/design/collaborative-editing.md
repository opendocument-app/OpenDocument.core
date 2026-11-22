# Collaborative editing proposal

Status: proposed; no protocol or implementation selected. Related:
[editing](editing.md), [document editing](document-editing.md),
[structure editing](structure-editing.md), [WASM](../../wasm/README.md).

## Scope and invariants

- Hosted or small self-hosted service; browsers and native apps share sessions.
  Start with ODT, then DOCX; sheets and presentations follow later.
- Survive brief disconnections. Taking a file offline ends collaboration;
  long-offline merging and externally replaced files are out of scope.
- Persist the original package plus accepted operations and verified
  checkpoints. Core edits that package and exports without a connected client.
- The server owns accepted revisions, conflict resolution and validation.
  Core defines document semantics; the service owns access and persistence.
- Client representations must preserve source correspondence and emit supported
  operations. They must not become a lossy whole-file export path. A disposable
  browser editing model is allowed; a second authoritative document format is
  not required.
- Keep existing local editing working. Define a new transaction contract's
  relationship to version 2 explicitly; never silently reinterpret old ops.

## Current gaps

The browser edits addressed DOM runs/paragraphs and coalesces operations for
`Document::edit` on save. Native apps and WASM use the same frontend.

| Today | Required |
|---|---|
| Whole-run `setText` | Revision-relative text changes with explicit conflict outcomes |
| Decode-local IDs; log-local negative IDs | Session identities and creation mappings across clients/checkpoints |
| Replay may partially apply before failing | Atomic validation and application |
| JS and C++ separately implement editing semantics | Shared behavior and an authoritative return path |
| WASM edit returns success, invalidates cached views | Canonical resulting state and affected rendering regions |
| DOM undo captures old values/nodes; commit clears it | Rebased local undo and bounded retention |
| Local DOM edits, with browser-owned composition | Remote edits, pending work, selection and IME reconciliation |
| Same source/behavior assumed | Tested compatibility profiles across core/editor versions |

An accepted operation does not prove the browser displays core's result.
For example, `frontend/document.js::splitParagraph` mirrors source-tree
splitting in JS; `wasm/src/wasm_html.cpp::edit` returns no resulting state.
Collaboration increases the cost of maintaining that agreement.

## Architecture alternatives

All viable options retain source-plus-operations persistence. Client models
and execution placement are separate choices; some options can be combined.

| Approach | Benefit | Cost / assessment |
|---|---|---|
| DOM editing + operation acknowledgements | Least frontend change | Leaves display/core agreement unchecked; insufficient end state |
| DOM editing + canonical core results | Detects and repairs drift while retaining the editor | Recommended first experiment; still needs pending-edit and IME reconciliation |
| Bounded browser editing model + source operations | Explicit state for rebase, undo, recovery and testing | Credible longer-term option; adds projection schema and DOM adapter |
| Local core processes every edit and directs DOM updates | Consolidates structural/formatting decisions in native/WASM core | Serious alternative to prototype early; needs incremental results, input/selection mapping and speculative-state management |
| Server processes every edit before display | Centralizes semantics and reduces client-core requirements | Network latency affects typing; adding prediction brings reconciliation back |
| Generic browser model used to reconstruct the file | Convenient standalone editor representation | Reject: loses source details absent from that model |

### Every edit through core

```text
Browser input + logical selection
    → local core edit command
    → document transaction + view changes + selection mapping
    → JS applies view changes to DOM
    → server accepts/reconciles transaction → local core/client reconcile
```

To remove duplicated editing decisions, core must receive intent such as
“insert text at this selection” or “split here,” then decide run splitting,
formatting and structure. Passing the existing JS-decided operation sequence
through core validates it but leaves those decisions duplicated.

A core component need not manipulate browser DOM objects. It can return
keyed text/style/structure patches or rendered fragments; JS handles browser
input, native selection and patch application. Local core uses WASM in a
browser or native bindings in an app, with no per-edit network round trip.
Measure worker/WebView latency and main-thread blocking before choosing the
execution boundary.

This can eliminate independently authored document mutation rules, but cannot
guarantee zero drift: DOM patching, browser composition, renderer dependencies,
client/server versions and speculative edits remain boundaries to verify.
Uncancellable composition must be observed and converted into core commands.
Server reconciliation still needs rollback/rebase of pending local core state.

### Bounded browser projection

Represent supported text, formatting, stable source references and opaque
boundaries, possibly only for loaded regions. Render it to the DOM and emit
transactions against the original package; never reconstruct that package
from the projection. Use one transaction path, not independent DOM and model
writers.

Preserve source distinctions even when runs look identical. Normalizing them
can lose mappings to different hidden semantics. Unknown content stays in
core; opaque references alone do not make splitting/deleting across fields,
bookmarks or wrappers safe. Core must implement or refuse those edits.

## Canonical results and projection agreement

Every accepted transaction needs a revision-tagged result derived from core:
created/deleted IDs and mappings, actual text/style/structure changes, affected
rendering regions, and position mappings for selections/pending edits where
supported. Results are derived views, not another persistent document model.

The contract to test for a supported transaction `T` and core document `D`:

```text
project(applyCore(D, T)) == applyClient(project(D), T)
```

Compare supported semantic state, identities and structure, not HTML bytes or
pixels. Test save/reopen separately. Canonical results provide a production
verification/repair path when this agreement fails.

Internal `translate_element` is a starting point, not a ready fragment API:
rendering depends on inherited styles, lists, direction, resources and page
context. Invalidate conservatively, widening from paragraph/container to
list/page/full view when needed. Prototype semantic results before optimizing
incremental HTML.

A result for revision R cannot overwrite a DOM already showing R plus pending
work. Reconcile at matching revisions, then rebase/reapply pending edits;
preserve logical selection anchors and avoid echoing remote changes as local
ones. Define safe updates around active IME composition: blindly replacing
HTML loses work, while waiting indefinitely for `compositionend` can stall
keyboards that keep composition open. Preserve unreconcilable edits for recovery.

## Components

| Component | Responsibility |
|---|---|
| Core, native or WASM | Decode/edit/save, commands/transactions, validation, reconciliation rules and canonical results |
| Frontend / host bridge | Input, DOM projection, selection, pending work and transient presence |
| Collaboration service | Authentication, document access, ordered acceptance, durable log, resume, checkpoints and export |

One authority advances each document revision; multiple service instances
would need ownership and fencing. Start with one process and a local database.
Core's existing `HttpServer` serves HTML services and need not become this
backend. Hosted and self-hosted deployments use the same protocol. Keep the
contract/conformance fixtures with core; repository/package placement and
service language/transport remain open. Native apps can retain native core;
share helpers through bindings/WASM rather than duplicate algorithms in JS.

## Transaction protocol

- **Core payload:** typed operations, preconditions and atomic gesture boundary,
  including multi-op splits. Retain paragraph operations and explicit style
  values; add revision-relative text insertions/deletions with target checks.
  Save-time coalescing must retain information needed for rebase and audit.
- **Envelope:** document, epoch, base revision, unique transaction ID and selected
  profile. The service supplies authenticated authorship.
- **Coordinates:** specify UTF-16/UTF-8 conversion and boundaries for emoji,
  combining characters and malformed input before implementation.
- **Identity:** map session IDs to registry IDs; distinguish concurrent creations
  and references from later pending transactions. Preserve full ID/revision
  precision on the JS wire.

### Acceptance and recovery

1. Apply local feedback and queue a transaction. Initially submit one per client
   at a time; keep subsequent typing queued with its dependencies.
2. Server checks access, profile, epoch, base revision and transaction identity.
   Reusing an ID with different contents is an error.
3. Reconcile against intervening accepted changes; validate shape, preconditions,
   target liveness, format capabilities and invariants on isolated candidate state.
4. Durably record acceptance/outcome before acknowledgement. Live-state publication
   and crash recovery must agree with that record; rejection changes no revision.
5. Broadcast accepted operations, revision and canonical results. Process revisions
   in order, detect/fetch gaps, and acknowledge own edits without applying twice.

Retries return recorded outcomes; retain deduplication across reconnects and
checkpoints for the supported retry window. Never clear submitted or unsent
work as implicit recovery. Outside the history window, obtain a fresh snapshot
and offer explicit recovery of pending edits.

Initially reconstruct isolated candidate state from checkpoint plus log.
Copying a public handle shares state and is not isolation. Optimize only while
preserving atomic acceptance; measure the cost before committing to this strategy.

### Conflicts and undo

Define transform, reported no-op or conflict for disjoint edits, same-position
insertions, overlapping deletions, insertion into deleted ranges, text against
split/merge/removal, formatting against text changes (including boundaries),
and undo after remote edits. Ordering alone does not preserve intent. Start
with bounded transformations; reject unsupported combinations with structured
reasons and recoverable work, never silently overwrite concurrent text.

Undo submits a new transaction against current state using operation-specific
inverse data and reconciliation. It does not reverse server history. Define
bounded undo retention; checkpoints must not implicitly call today's
`editing.committed()` and clear it.

## Checkpoints, export and audit

A checkpoint is a source package, accepted revision, identity metadata and
compatibility profile. At a fixed revision: save a candidate, reopen and verify
supported state/mappings, persist package/metadata, then atomically publish the
reference. Retain the previous recovery base/log until verification and retention
rules permit cleanup. Pause acceptance briefly or retain every later transaction.

Trigger checkpoints from measured log bytes/count and replay cost. Compaction
must preserve history, mappings, retry outcomes and inverse data needed within
disconnect/undo windows. On failure retain acknowledged work; repeated export
failure must stop new acceptance before recovery/storage limits are exceeded,
not discard the log.

Export a named accepted revision without clients; distinguish synced state from
the latest exported file. Preserve unsupported package content through existing
adapters. Successful replay alone proves neither export validity nor fidelity:
exercise save/reopen and independent ODT/DOCX readers, including unknown content.

Audit submitted and accepted/reconciled transactions, authenticated authors,
base/result revisions, profiles and rejection/no-op reasons. Audit retention is
separate from active replay/reconciliation history.

## Mixed versions

The same JSON can decode, edit or save differently across builds. Pin a tested
session profile covering transport, edit schema/semantics (including Unicode,
transforms and inverses), core/format behavior, frontend bridge/patch behavior,
and checkpoint/log format. Record actual build IDs for diagnostics; release or
schema version claims alone do not prove compatibility.

- Negotiate profiles/capabilities before enabling edits. Every participant must
  consume all session operations/results, not merely submit a supported subset.
  Do not drop unknown ops or silently downgrade a session for an older client.
- Initially allow only tested core/editor combinations, including equivalent
  native/WASM builds; their artifact hashes need not match. Expand supported
  releases through conformance tests. On mismatch preserve pending work and
  refuse editing; offer read-only only through a compatible rendering path.
- Never infer cross-version identity from parse order. Verify explicit mappings
  for independent native/WASM rendering. Server-rendered HTML with authoritative
  IDs and a compatible frontend is an alternative. Server-supplied JS does not
  upgrade an app's native core or bridge; bundled versus fetched assets is open.
- Keep semantics fixed within an epoch. Upgrade by retaining its implementation
  or stopping new submissions, settling accepted work, verifying a checkpoint
  with the new core, remapping identities and starting a renegotiated epoch.
  Old-epoch pending edits require explicit recovery, not silent reinterpretation.
- Replay recorded semantics using a retained implementation or verified migration.
  Refuse incompatible recovery. Rollback needs compatible checkpoint/log support
  or a compatible worker; an older checkpoint omits newer acknowledged edits.
- Test older/newer clients and servers both ways, native/WASM, and reconnect across
  upgrades. Compare addressed state and saved/reopened results. Canonical feedback
  can detect disagreement but does not make incompatible clients safe.

## Libraries to evaluate

| Candidate | Investigate |
|---|---|
| [Yjs](https://docs.yjs.dev/api/document-updates) | Synchronization, deduplication, editor integration and selective undo; fit with server validation and source operations |
| [Automerge](https://automerge.org/docs/reference/concepts/) / [rich text](https://automerge.org/docs/reference/documents/rich-text/) | Persistence/sync primitives without replacing source-backed authority |
| [ShareDB](https://share.github.io/sharedb/types/) | Server-oriented transformation with shared operation types; avoid duplicate semantic rules |
| [ProseMirror](https://prosemirror.net/docs/guide/) | Model, transactions, position mapping and DOM adapter for a bounded projection; retain source distinctions and opaque boundaries |
| Core-owned implementation | Bounded rules and command/results API shared through native/WASM |

Compare identical adversarial fixtures, integration/maintenance cost, correctness,
failure recovery, audit, upgrades, native/WASM reuse, performance and licensing.
Convergence must also preserve intended edits and valid files. No dependency is
selected; a generic library model is not automatically a suitable exporter.

## Implementation gates

1. **Agreement and alternatives, before a full service.** Drive browser gestures,
   replay into core and compare canonical editable state. Cover split/merge through
   styled spans, partial formatting, empty paragraphs, undo, IME, Unicode,
   save/reopen and unknown content; add remote edits and pending work. Prototype
   canonical DOM feedback, one bounded model-driven region, and local core
   commands directing view patches. Measure latency, rebase/selection complexity
   and recovery. Choose the client architecture from these results.
2. **Contract and core.** Inventory version 2/undo; specify command/transaction/result
   schemas, IDs, coordinates, profiles, errors and supported conflict outcomes.
   Test malformed/stale targets and incompatible profiles. Implement isolated
   validation, transformations, inverses and bindings. Shared
   native/WASM fixtures must prove atomic rejection, deterministic results and
   projection agreement for ODT; add DOCX before broadening format support.
3. **Minimal service.** Implement access, negotiation, ordered durable acceptance,
   deduplication, resume, checkpoint remapping and revision-specific export.
   Inject crashes around acceptance/publication and checkpoint replacement.
   Completion: every acknowledged edit survives restart and exports without clients.
4. **Two clients.** Integrate the selected editor path, remote/pending reconciliation,
   canonical repair, selection/IME, local collaborative undo and transient presence.
   Test brief disconnects, lost acknowledgements, conflicts, checkpoint transitions
   and incompatible clients. Completion: responsive clients agree with accepted
   state and server export without losing pending work.
5. **Apps and deployment.** Connect native WebView and browser, including deliberate
   mismatches. Package self-hosting; document upgrades, recovery, export and retention.
   Measure realistic documents to set replay/checkpoint/disconnect/undo limits.
   Completion: supported combinations pass; unsupported ones fail explicitly.

Open choices: client model/execution boundary, command/result API, IDs/coordinates,
fragment invalidation, IME/recovery UX, conflict/retention policies, isolation cost,
rendering/mapping strategy, compatibility/migration support, service placement and
transport, and library reuse. Decide these within the invariants above.
