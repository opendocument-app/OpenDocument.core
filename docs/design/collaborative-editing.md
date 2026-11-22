# Collaborative editing proposal

Status: proposed; no collaboration protocol or implementation exists yet.
This records the intended direction and a staged implementation plan. Wire
names, algorithms and compatibility policies below still need validation.

Related: [editing](editing.md), [document editing](document-editing.md),
[structure editing](structure-editing.md), and the
[WebAssembly bindings](../../wasm/README.md).

## Scope and decisions

Offer shared editing through a hosted service or a small self-hosted server.
Browsers and native apps participate in the same session. Start with text
documents: an ODT prototype, then DOCX before claiming general text-document
support. Sheets and presentations are later work.

- Survive brief disconnections. Taking a file offline ends collaboration;
  merging days of independent edits or an externally replaced file is out
  of scope.
- Persist a source document plus accepted edits, with periodic checkpoints.
  Keep the original package and edit it through core. Do not introduce a
  second, format-independent document representation or reconstruct the file
  from rendered HTML.
- The server owns the accepted revision, resolves conflicts, validates edits
  with core, and can produce the final file without a connected client.
- Retain the browser's DOM editing model. Add semantic transactions and
  reconciliation around it. A diff describes document changes, not arbitrary
  HTML mutations.
- Formalize document edit semantics in core. Keep accounts, connections,
  access control and storage orchestration in the hosting application.
- Evaluate libraries against a small implementation of our own. No library,
  server language or transport is selected by this proposal.

## Existing foundation and gaps

Today the browser edits locally and coalesces operations for C++ to replay
on save. `Document::edit` accepts version 2 envelopes. The WASM package can
open, render, edit and save, and native hosts use the same frontend through
their WebViews. No network round trip is needed for local typing.

That envelope is a useful starting point, but is not a collaboration
protocol:

| Current behavior | Collaboration requirement |
|---|---|
| `setText` replaces a whole run | Express intent relative to a known revision; concurrent replacements must not silently overwrite one another |
| IDs belong to a decode/edit/save session | Identify targets across clients, reconnects and checkpoints |
| Negative creation IDs belong to one log | Distinguish creations from different transactions and participants |
| Replay can fail after applying earlier operations | Accept or reject a whole transaction without partially changing accepted state |
| The DOM has one local writer | Apply remote edits while preserving pending edits, selection and IME composition |
| Undo stores local inverse data; committing resets it | Undo one's own accepted changes against the current revision |
| Replay assumes the same source and semantics | Explicitly negotiate compatible core/editor behavior |

Keep the existing local editing API working. Decide how the new transaction
API relates to version 2 before changing that envelope; do not silently give
existing operations new semantics.

## Responsibilities and deployment

```text
Browser: DOM editor + core in WASM ----\
                                      > collaboration service + core
App: WebView editor + native core ----/             |
                                        checkpoint + accepted log
```

Core defines typed operations, preconditions, validation, application and
the supported reconciliation rules. It continues to decode, render and
save the actual document. Its live decoded document is a working instance
of the existing model, not an additional persistent representation.

The frontend captures gestures as transactions, updates the DOM immediately,
tracks unacknowledged work, and applies accepted remote operations. Selection
and presence are transient session information, separate from file edits.

The service authenticates participants, checks access, serializes acceptance
per document, persists transactions, and coordinates checkpoints and export.
Only one authority may advance a document revision at a time. A single
process is sufficient initially; multiple instances would need ownership
and fencing before sharing that responsibility.

Core's `HttpServer` currently serves HTML services. It need not become the
collaboration backend. A small deployment can serve the web application,
synchronization endpoint and local storage together. A hosted deployment
can separate them while retaining the same session protocol.

Native apps can keep native core. Browsers use WASM. Reconciliation helpers
could be shared through native bindings and WASM rather than implemented
independently in JavaScript. The DOM adapter still lives in JavaScript.
Separate runtime components do not require separate repositories: keep the
edit contract and conformance fixtures with core, and decide service/package
placement during the prototype.

## Edit transactions and session envelope

The core transaction describes document operations and their preconditions.
The session envelope identifies the document, session epoch, base revision,
transaction, and selected compatibility profile. The service attaches the
authenticated author; client-supplied authorship is not authoritative.

Retain operations for paragraph structure and explicit formatting values.
Add text changes that express insertion and deletion relative to the base
revision, with enough context to verify their targets. For example, inserting
` Alice` after `Hello` is more useful for reconciliation than replacing the
whole run with `Hello Alice`.

Specify the coordinate system before implementing these operations. JavaScript
uses UTF-16 offsets and C++ strings use UTF-8 bytes. Define conversion and
boundary rules, including emoji, combining characters and malformed input;
do not let the native and browser implementations choose independently.

One gesture is one atomic transaction, including the multiple operations
needed to split a styled run and paragraph. Preserve boundaries and
preconditions in the accepted log. The existing save-time coalescing must
not erase information still needed for reconciliation or auditing.

Use session identities distinct from registry indices, with a mapping into
the current core document. New targets need identities unique across
transactions, including references between pending local transactions.
Wire encodings must preserve full identity and revision precision in
JavaScript. The exact identity scheme is an early design decision.

### Acceptance and reconnect

1. The browser edits its DOM and queues a transaction against its accepted
   revision. Initially allow one submitted transaction per client at a time;
   subsequent typing remains local and queued.
2. The server checks access, profile, epoch, transaction identity and base
   revision. Reusing an identity with different contents is an error.
3. It reconciles against intervening accepted transactions, then validates
   and applies the result to isolated candidate state.
4. It durably records the accepted transaction and its outcome before
   acknowledging success. Publication of the new live state and recovery
   from a crash must agree with that durable record.
5. It broadcasts the accepted operations, revision and creation mappings.
   The sender processes its acknowledgement without applying its own edit
   twice. All clients advance through accepted revisions in order.

Retries return the recorded outcome rather than applying a transaction
again. Retain deduplication information across reconnects and checkpoints
for the supported retry window. A lost acknowledgement must not duplicate
text. Rejected transactions do not advance the document revision.

Clients detect revision gaps and fetch missing accepted changes. Reconciliation
must account for both submitted and unsent local edits; clearing the local
queue or reloading HTML is not an acceptable implicit recovery mechanism.
If a pending edit cannot be reconciled, retain it for explicit recovery.

### Conflict behavior and undo

The server provides a total order, but ordering whole-run replacements alone
does not preserve concurrent intent. Specify and test rules for at least:

- Disjoint edits and concurrent insertions at the same position.
- Overlapping deletions and insertion within a concurrently deleted range.
- Text edits against paragraph split, merge or removal.
- Formatting against text insertion/deletion, including boundary behavior.
- Undo of a local accepted change after remote changes to the same content.

Start with a bounded set of supported transformations. Reject unsupported
combinations with a structured conflict result and recoverable local work.
Do not use silent last-writer-wins as the default for text. Define whether
each allowed combination transforms, becomes a reported no-op, or conflicts.

Undo is a new transaction relative to current state, not reversal of the
server's revision history. It needs operation-specific inverse information
and the same reconciliation rules. Checkpointing does not automatically
clear the user's undo history as `editing.committed()` does today. Decide
and expose a bounded undo retention policy.

### DOM reconciliation

Keep addressed runs and paragraphs as the browser's document model. Maintain
only transaction, identity, selection and recovery metadata alongside them.
Remote operations must use controlled DOM editing paths, without echoing
them as new local transactions.

Prototype how to rebase pending transactions and reconcile the DOM using
the editor's existing inverse data, or an equivalent incremental patch.
Preserve selection and define how remote changes interact with active IME
composition. Server conflict resolution does not remove this client work.
If it cannot be done reliably without replacing the editor model, revisit
the design explicitly before extending the supported editing surface.

## Validation, checkpoints and export

Initially isolate validation by reconstructing a candidate from a checkpoint
and accepted operations, then applying the new transaction. This is a
correctness baseline, not a performance commitment. Public document handles
share state: copying a handle does not make an isolated candidate. Later
optimization must preserve all-or-nothing acceptance.

Validation covers operation shape, preconditions, target liveness, supported
format capabilities and document invariants. Successful replay is not proof
of valid export or application interoperability. Exercise save and reopen,
and validate ODT/DOCX fixtures in independent readers during development.

A checkpoint contains the saved source package, accepted revision, session
identity metadata and compatibility profile needed to recover. It is not
just the file bytes. At a fixed accepted revision:

1. Save a candidate package without modifying the published recovery base.
2. Reopen it and check supported document state and identity remapping.
3. Persist the package and metadata, then atomically publish their reference.
4. Retain the previous recovery base and required log until replacement is
   verified and retention rules permit cleanup.

Coordinate edits arriving during checkpoint creation: either pause acceptance
briefly or retain every transaction after the chosen revision. Failure keeps
the previous checkpoint and log usable. If export repeatedly fails, retain
acknowledged work and stop accepting further edits before storage or recovery
limits are exceeded; never discard the log to recover space.

Trigger checkpoint work by measured log bytes, operation count and replay
cost. Choose limits from the prototype. Compaction must retain the history,
target mappings and outcomes needed by briefly disconnected clients and undo.
Outside the supported window, require a fresh session snapshot and explicit
recovery of pending local work. Automatic merging of long-offline copies is
out of scope.

Server export materializes a named accepted revision without a browser.
Report that revision so users can distinguish synchronized edits from the
latest exported file. Preserve package parts outside supported edits through
the existing format engine rather than rebuilding them from the DOM.

For audit, retain submitted and accepted/reconciled transactions, authenticated
author, base/result revisions, profile, and rejection/no-op reasons. Audit
retention is a separate policy from the replay log's checkpoint compaction;
an audit trail need not remain in the active reconciliation window.

## Different core versions in one session

Identical JSON does not imply identical behavior. Different core builds can
decode the same bytes into different registries, resolve styles differently,
support different operations, or save/reopen a different tree. Mobile apps,
self-hosted servers and browser tabs cannot be assumed to upgrade together.

Separate these compatibility dimensions:

| Dimension | What it governs |
|---|---|
| Transport protocol | Join, submit, acknowledge, resume, errors and presence |
| Edit schema and semantics | Operation fields, Unicode coordinates, preconditions, transformations and inverse behavior |
| Core/format behavior | Decode, edit, save and identity mapping for the supported formats |
| Frontend bridge behavior | DOM operation application, selection handling and host callbacks |
| Checkpoint/log format | Recovery and migration of persisted sessions |

Define a session compatibility profile that selects supported combinations.
Record actual core/editor build identifiers for diagnostics. Core's release
version alone is insufficient, and claiming a schema version is not evidence
that two implementations behave alike.

### Join policy

- The client advertises supported protocol/profile versions and edit
  capabilities before editing is enabled. The server selects a tested
  profile already supported by the session, or refuses collaborative editing
  with a clear reason. Unknown operations never get silently dropped.
- Pin that profile for the session epoch. A newly joined client must consume
  every operation enabled for the session; intersecting only what that
  client may submit is insufficient. Do not silently reduce the session's
  capabilities when an older client arrives.
- For the prototype, require an explicitly tested core/editor revision
  combination, including equivalent native and WASM builds. Expand to mixed
  releases only after conformance tests establish compatibility. Exact
  artifact hashes will differ by platform; compatibility is an allowlisted
  behavior contract, not equality of native and WASM binaries.
- If a compatible read-only path exists, offer it on mismatch. Preserve any
  pending local work before requiring refresh or upgrade. Do not promise
  old clients can safely render every new session.

### Identity and rendering across versions

Never assume independently decoding the same source file yields matching
registry IDs across releases. Establish session identities and an explicit
mapping before a client can submit edits. A practical prototype option is
server-rendered HTML carrying authoritative session identities, paired with
the compatible frontend. Independent local/WASM rendering must demonstrate
equivalent target mapping rather than relying on parse order.

This keeps browser WASM and native rendering available, but makes their
compatibility a tested requirement. Whether apps may load the server's
frontend or must use bundled assets remains a deployment decision. Serving
new frontend code does not upgrade an app's native core or host bridge.

### Upgrades and recovery

Do not change edit semantics underneath an active epoch. A server upgrade
either continues running its supported profile or performs an explicit
migration: finish accepted submissions, create and verify a checkpoint,
validate it with the new core, establish identity mappings and start a new
epoch. Connected clients renegotiate; reconnecting old clients cannot submit
old-epoch edits as if nothing changed. Preserve their recoverable local work.

Replay persisted operations with their recorded semantics. Keep the required
implementation available or migrate and verify state before retiring it.
An incompatible server should refuse to open the session for editing rather
than reinterpret old operations. A rollback must likewise understand the
published checkpoint/log format or retain access to a compatible worker;
an old checkpoint alone omits newer acknowledged work.

Maintain fixtures across supported client/server combinations, including
older clients with newer servers, newer clients with older servers, native
versus WASM, and reconnect across upgrade. Compare addressed document state
and saved/reopened behavior, not just JSON parsing or rendered screenshots.

## Library evaluation

Use our operation and document constraints to evaluate reusable components:

| Candidate | What to investigate | Fit question |
|---|---|---|
| [Yjs](https://docs.yjs.dev/api/document-updates) | Update synchronization, deduplication, editor integration and selective undo | Would its shared types become a second document model or obscure server validation? |
| [Automerge](https://automerge.org/docs/reference/concepts/) | Persistence and synchronization; [rich-text operations](https://automerge.org/docs/reference/documents/rich-text/) | Can useful components fit the source-plus-operations design without adopting another document representation? |
| [ShareDB](https://share.github.io/sharedb/types/) | Server-oriented operational transformation and shared operation types | Can our semantics be expressed without maintaining another set of transformation rules? |
| Core-owned transactions | Explicit transformations for a limited supported surface | Can we keep the rule set understandable, testable and bounded as editing grows? |

These are investigation candidates, not dependencies. Compare integration
cost, correctness, failure recovery, auditing, upgrade behavior, native/WASM
reuse, performance, licensing and maintenance. Use the same adversarial edit
sequences for each. Convergence alone is insufficient: the result must
preserve the chosen editing semantics and produce a valid document.

## Implementation plan and completion criteria

### 1. Specify transactions and compatibility

- Inventory version 2 operations and DOM undo behavior. Define atomicity,
  preconditions, text coordinates, identities, revisions and error outcomes.
- Define the initial compatibility profile and handshake, plus the relation
  to existing local editing and save APIs.
- Write shared fixtures for concurrent edit pairs, malformed/stale targets,
  Unicode boundaries and incompatible clients. Evaluate libraries against
  these fixtures before choosing an implementation.

Complete when the supported conflict outcomes and compatibility decisions
are executable examples, with an explicit list of unsupported combinations.

### 2. Implement core transactions

- Add validation and isolated application through existing document adapters.
- Implement the initial reconciliation and inverse rules, and expose needed
  helpers to native and WASM callers.
- Test atomic rejection, deterministic accepted results and save/reopen
  preservation for ODT. Extend the corpus to DOCX before broadening support.

Complete when no rejected transaction changes accepted state, and supported
native/WASM combinations pass the same transaction corpus.

### 3. Build a minimal authoritative service

- Add session negotiation, ordered acceptance, durable logs, deduplication,
  resume, document access checks and revision-specific export.
- Add checkpoint publication, identity remapping and recovery. Exercise
  crashes before/after durable acceptance and during checkpoint replacement.
- Keep server placement and transport replaceable; one local process and
  a local database are sufficient for this milestone.

Complete when a server restart preserves every acknowledged edit and can
export a valid document with no connected clients.

### 4. Connect two DOM editors

- Capture transactions, apply remote edits, reconcile pending local work,
  and handle acknowledgements without duplicate application.
- Preserve caret/selection and IME composition; add local collaborative undo
  and transient remote presence.
- Exercise brief disconnects, lost acknowledgements, rejected transactions,
  checkpoint transitions and incompatible clients with recoverable work.

Complete when two browsers converge on accepted state through these cases,
remain usable while typing, and display the state the server exports.

### 5. Validate app integration and deployment

- Connect a native WebView host alongside a browser. Verify the core/editor/
  bridge compatibility policy, including a deliberate version mismatch.
- Package a simple self-hosted deployment and document upgrade, recovery,
  export and retention behavior. Use the same protocol for hosted service.
- Measure replay, checkpoint and reconciliation costs on realistic text
  documents, and choose the supported disconnect and history limits.

Complete when mixed supported clients pass the compatibility matrix and an
unsupported combination fails explicitly without losing pending local work.

## Decisions still to make

- Exact text coordinate and session identity schemes, and mappings through
  save/reopen and independent rendering.
- Initial conflict rules, undo retention, disconnect window and recovery UI.
- Candidate-state isolation strategy and acceptable validation latency.
- Core API/binding shape, helper placement, and service language/transport.
- Whether the prototype uses server-rendered HTML or verified local rendering.
- Supported compatibility profiles, upgrade adapters and retention periods.
- Library reuse after the bounded prototype comparison.

These are implementation investigations within the agreed direction. They
do not reopen long-offline merging, external file replacement, or a second
persistent document model as initial requirements.
