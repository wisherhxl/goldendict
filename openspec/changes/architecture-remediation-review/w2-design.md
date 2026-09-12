# W2 / A5: prepare publication objects before the durable decision

Selected by the user's W2-only approval on 2026-09-12. This is a minor correction
to the existing transaction preparation/publication contract, not a new transaction
design. Base: 693c3e8ccb65a99fea8754476341690178f93fa2. W1 evidence and implementation
remain unchanged; A2/A3/A4/A6 remain unselected. No merge, push or W3 execution.

## Actual phases and ownership

The real ConfigurationReloadTransactionCoordinator first prepares persistence,
Network, Core and Widgets, then reserves Network/Core and performs reversible
Widgets maintenance. PersistDesiredConfiguration publishes the kDesiredCommit
pending record with PublishNoReplace. That namespace publication is irreversible;
directory-sync uncertainty and subsequent failure retain the existing forward
recovery rules. kAfterDecisionPublished observes the actual boundary. The earlier
coordinator kPersistenceDecision event is only entry to that operation.

After desired persistence and kDesiredRuntimeApplying, publication remains
Network → Core → Widgets. Forward work remains Network cleanup → Widgets finish
→ Core old-executor stop/new-executor submission → transaction finalization.

Core currently constructs PublishedCoreFacadeCandidate::Impl with make_unique
inside noexcept PublishReservedOnly. Its old composition keeps the previous
facade/activation alive until FinishPublished stops the old executor and submits
the new one. Prepared/Reserved tokens own the inactive candidate; reservation
blocks competing owner mutation. Abandon/Abort never performs published retirement.

Network currently constructs NetworkRuntimeTransaction::Published::Impl inside
noexcept Publish, after dispatcher publication. The reservation owns the prepared
candidate and its already registered owner-thread resource. Published owns that
candidate until Finish; terminal withdrawal then releases it. Cache binding and
storage lease remain runtime-owned and are not replaced by token preparation.

The transitive Network split-publish path also constructs a BlockingQueuedConnection
call on demand. Its existing timer dispatcher copies the resource vector on each
wakeup. These allocation paths must be addressed in this same bounded correction;
moving only the two token allocations would not close A5.

## Intended correction and constraints

- Fully construct each future published Impl inside its existing fallible
  PrepareCandidate, before any durable decision. Prepared ownership transfers
  through reservation to publication; abort destroys an empty unpublished token.
  No early active registration, version advance, old-resource release or executor
  activation is permitted.
- Reuse the existing registered Network candidate and timer for split publish and
  finish commands. Publish/finish select prepared command states and wait for the
  owning thread; no new queued Qt invocation is constructed at publication. Timer
  traversal must not allocate a copied vector. Preserve synchronous completion,
  reservation exclusion, owner-thread execution and explicit post-work ordering.
- Preserve noexcept, token one-shot rules, module order, persistent formats,
  failure visibility and recovery. Inspect moved-from destruction, pointer
  transfers, optional composition moves, cache setters and transitive callbacks.
- W1 page code and other architecture tickets are outside this change.

The coordinator also prepares ConfigurationRecoveryRequest before participant
preparation instead of copying its path/identity after the decision. Existing
error results transfer by move; their public meaning and forward-failure paths
are unchanged. This is necessary preparation/ownership plumbing on the real
transaction path, not a change to its commit order or recovery algorithm.

Core Prepared owns an empty, fully constructed publication Impl until Reserve
transfers Prepared ownership; Publish transfers that Impl and fills its old
composition by nothrow moves. Abort only destroys empty publication storage and
the inactive candidate. Finish alone stops the retired activation and submits
the successor. Network Prepared initially owns its empty publication Impl;
Publish first extracts that pointer, then transfers Prepared into Published.
These two ownership directions never coexist as a cycle. Finish waits for owner
thread post-work and terminal withdrawal before releasing Prepared and Published.
The runtime retains its original manager, bound cache and storage lease.

The dispatcher now distinguishes split publication from compatibility commit:
the prepared timer services kPublishRequested without post-work, acknowledges
completion after the publication observer returns, then services kFinishRequested
only when the coordinator reaches forward maintenance. Existing kCommitRequested
still performs both operations. Registry iteration holds one shared resource
while processing/erasing it, without allocating a vector snapshot. No timer,
connection, queued callable, active-state registration or callback is created
at publication.

## Test boundary and impact check

Use a separate test target compiling the real application composition sources
excluding main.cpp, as existing presentation tests do. Invoke the actual
ConfigurationReloadTransactionCoordinator, Core owner and Network runtime; do not
copy its algorithm or add MainWindow scenarios. Local temporary configuration,
cache/index/history/recovery paths are required.

A narrow extension of existing source-private Core/Network test access observes
the actual published Impl allocation/construction/destruction boundaries. A
fixed callback can fail that allocation in an isolated child. The observer has
no containers, logging or allocation in the tested interval. This instrumentation
is recorded separately from the untouched starting baseline. Additionally observe
ordinary allocation entries linked into the test executable during the bounded
Network/Core publication interval; report DLL, thread and allocator blind spots.
No factory-only count is a claim of whole-program zero allocation.

Required evidence: per-module red after the actual durable decision; unchanged
green pre-decision contract; normal ordered publication, each preparation failure,
partial preparation unwind in the actual Network-before-Core order, subsequent
preparation failure/abandon, two consecutive transactions/stale tokens, retained
old references/leases and final cleanup. Run existing transaction, Network/Core,
executor/recovery tests and W1 lifecycle regression. Potential termination is
contained in children with an explicit target-path marker, never memory exhaustion.

Impact check: Ready within the approved private ownership/command boundaries.
No public failure contract, persistence schema, transaction order or W1 design
change is selected. Any newly discovered boundary conflict must be reported.
