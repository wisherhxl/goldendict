## Context

P1 is an independently approved bounded requirement change after W3.2, under
CRD-DICT-004 and the existing Network cache ownership/uncached degradation contract.
Base d196d6d7c02f17e2ba91e58d34620d4f9d0c4803, same user-selected worktree/branch.
The startup and all Preferences/source/group reload call sites capture one const
network_cache_root. Recovery reconstructs through the same startup selection.
NetworkCacheStorage::Prepare appends qt-network-http, creates/probes it when
positive; NetworkRuntime construction may remove that subtree when disabled.

## Goals / Non-Goals

Keep path policy in the existing app-private configuration-location unit. Network
owns only its injected root/owned child, policy, leases, worker and cleanup. No
Network portable awareness, public API, persistence format, transaction or W1/W2
behavior change. Index defaults and WebEngine policy are outside P1; normal-run
fixtures must supply an explicit isolated index directory and prove other writes
safe. No new CLI, startup bypass, generic path framework or production test entry.

## Decisions

Add a small pure ResolveNetworkCacheRoot operation beside existing location
resolution. It receives ConfigurationLocations, the unchanged platform default
string and an optional existing explicit root string. Return explicit input
unchanged when nonempty, otherwise portable current-config parent/cache, otherwise
unchanged default. Do not probe/create here. Use the actual resolver result, not
another portable detector. main.cpp computes once before Prepare and reuses it
for every existing caller. Windows smoke root remains the existing higher-priority
explicit injection; direct Network API callers keep their injected paths.

Do not move selection into Network (wrong owner), add a CLI (unneeded public
surface), or duplicate portable conditionals at each reload (drift risk). No old
cache is migrated, inspected for writes, removed or automatically recovered.
Path unavailability preserves the existing caller-specific channel: startup
reports a redacted diagnostic and degrades to uncached traffic; Preferences
rejects failed positive-cache preparation and retains the prior runtime/config.
There is no fallback to the system cache and no new failure semantics.

## Verification and readiness

First establish baseline production/location/Network builds and tests. Extract
only the current default-vs-explicit selection into the pure function, retaining
its existing behavior and wiring main to it. This is named behavior-preserving
test access, not the untouched baseline. Add a test-only QTest target exercising
that real function plus Network Prepare/Create/PrepareCandidate/Reserve/Abort/
Commit. All default/failure/sentinel roots are temporary. The portable root
expectation must fail safely before adding the portable branch; controls retain
baseline behavior. Do not run the old ordinary executable for red evidence.

Then verify enabled/disabled/reapplied cache, setup failure without fallback,
sibling sentinels, abandoned/reserved candidates, consumed tokens and clean
shutdown. Existing W2 and http_client_test retain request/lease/recovery detail.
Build ON and OFF separately, inspect real target source/link closure and rerun
W1/W2/W3.1/W3.2 and relevant configuration/recovery checks.

Before normal startup, record a path matrix: config/history/favorites/recovery
under the fresh application-local portable directory; index explicitly beneath
it; Network child beneath portable/cache; WebEngine data paths independently
verified (off-record alone does not establish disk isolation);
TEMP/TMP, process working directory and other application writes isolated. Verify
no reparse-point alias and no Windows instance-forwarding path. Use copied actual
production executable/resources with existing portable directory detection and
normal initialization. Seed configuration using existing serializer from a
test-only fixture tool if needed. Observe actual main window and WebEngine child,
then request normal window close externally and confirm exit/checkpoint. No smoke
or test flag on the product. If another write path cannot be isolated, stop that
launch and report the specific gap. A portable run never covers default-profile
startup or retroactively changes W3.2 evidence.

Prelaunch inspection found a remaining WebEngine gap in installed Qt 6.11.1:
ProfileAdapter::dataPath() uses AppDataLocation/QtWebEngine/OffTheRecord when no
explicit data path was supplied, despite the off-record profile. Its source
explicitly notes Chromium temporary directory creation. The application does
not override this path. Ordinary launch is withheld; changing WebEngine path
policy is outside P1. Existing GUI test isolation claims also require separate
assessment; preserve prior receipts and do not label environment variables as
verified Windows Known Folder redirection.

## Risks / Trade-offs

- Pure extraction red evidence is explicitly distinguished from original HEAD.
- Multiple ownership domains: preserve Network child-only cleanup; test sibling
  and simulated-default contents; do not claim full-machine write tracing.
- GUI observation may be unavailable: report the concrete limit without adding a
  production exit hook or pretending a smoke is ordinary startup.
- Requirement-changing work has independent read-only readiness review before
  source edits and exact-candidate completion review after verification.

## Migration Plan

No data migration. One coherent P1 candidate, independently revertible without
rewriting W1/W2/W3.1/W3.2. Preserve external red/green/startup evidence. Reverting
restores the old path-selection gap and does not remove either cache directory.
No merge/push/archive or W3.3 is authorized.
