# W2 / A5 verification

## Scope and candidate record

Approved minor correction from W1 commit
`693c3e8ccb65a99fea8754476341690178f93fa2`; worktree
`D:/workspace/goldendict/worktrees/feature-tiger-qt6-migration`, branch
`feature/tiger-qt6-migration`. Initial status was clean. Fetch succeeded;
the branch was one W1 commit ahead of origin, with no incoming commit.
The user's explicit worktree and no-push/no-merge instructions govern this
sequential delivery. W3 and all other architecture tickets remain unselected.

The activated and four-hash-verified `goldendict-candidate-v1` profile permits
a local recoverable candidate before independent review. The exact candidate,
tree, binary hashes and independent receipt belong in the external closeout;
this file does not manufacture an independent Pass.

Evidence root (all filenames below are relative to it):
`D:/workspace/goldendict/evidence/a5-publication-preallocation-20260912`.
W1 evidence remains in `../a1-page-lifecycle-20260912`. Its reviewed candidate
matches the starting commit and tree `1fcd6025009e2d228cfadbe40ef86742dd4a5eee`.
W1 receipt SHA-256:
`17D1456336BA8B765307E7CBE693F5CBE99A8C1F0458C0DFEF20D01482A6D24F`.
The original Draft and all W1 production/test/verification files are unchanged.

## Rules and environment

Actually read: user/global AGENTS and engineering-delivery/software-design
policies; workspace and checkout AGENTS; docs/agent-workflow.md,
docs/project-design-rules.md; applicable build/testing guidance; architecture
sections for Core activation, prepared Network dispatcher, reservations and
cross-module publication; this OpenSpec proposal/design/tasks and W1 records.
The existing architecture requires preparation before the durable decision and
Network → Core → Widgets publication followed by explicit forward maintenance.
No Accepted ADR, global rule or installed public interface was rewritten.

Native OpenSpec context resolves this worktree; strict validation succeeds with
the existing conformance-only skip_specs declaration. The exact-path Serena
activation succeeded, but PublishReservedOnly lookup returned an empty result.
No onboarding, upgrade or reconfiguration was performed. Textual definitions,
all private-header consumers and real linked builds/tests provide the documented
equivalent impact coverage; no complete semantic-reference claim is made.

Build: `build/Release`, Ninja, Release, MSVC, Qt 6.11.1 shared, Windows 11.
CMAKE_HOME_DIRECTORY is this worktree. Executables are in `build/Release/bin`;
the actual Core DLL is `goldendict_core160.dll`; Network is linked from the real
static module. Every executable/CTest command uses this checkout's Conan launcher.
No dependency, tool, profile or generated configuration was upgraded/reinitialized.

run-baseline.ps1, run-regression.ps1 and run-native.ps1 isolate HOME, XDG config
and cache, APPDATA, LOCALAPPDATA, GOLDENDICT_TEST_CONFIG_ROOT, TEMP and TMP.
Their process PATH selects the already-installed Python 3.14 executable rather
than the WindowsApps alias. The new test further owns a temporary parent profile;
children inherit it and use unique temporary transaction fixtures. Config,
history, recovery records, indexes and cache are all synthetic/local. No external
dictionary, real user profile or public HTTP service is used.

## Commands and outcomes

All build invocations have the prefix:

```powershell
.\run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release
```

| Evidence | Command suffix / execution | Result |
| --- | --- | --- |
| baseline-build.log | --target goldendict application_service_test http_client_test runtime_composition_test article_page_lifecycle_test --parallel 4 | exit 0 |
| baseline-fulltext-build.log | --target full_text_index_test --parallel 4 | exit 0 |
| baseline-tests.log | run-baseline.ps1, actual 8 CTest tests | exit 0, 8/8 |
| red-build.log | --target publication_preparation_test --parallel 4 | exit 0, real executable linked |
| red-tests.log / red-qtest.txt | Conan CTest -R '^publication_preparation_test$' -j 1 --output-on-failure | exit 8; 3 QTest passes, 6 failures, zero skips |
| tokens-build.log / tokens-tests.log / tokens-qtest.txt | same target/test after moving only token construction | build 0, CTest 8; 7 passes, 2 failures |
| green-build.log / green-tests.log / green-qtest.txt | same target/test after prepared dispatcher change | build 0, CTest 0; 9 passes, zero failures/skips |
| final-build.log | --target goldendict publication_preparation_test application_service_test http_client_test runtime_composition_test full_text_index_test article_page_lifecycle_test --parallel 4 | exit 0 |
| profile-fix-build.log | --target publication_preparation_test --parallel 4, final isolated test harness | exit 0 |
| final-regression-tests.log | run-regression.ps1 | exit 0; 9/9 CTest tests |
| final-qtest.txt | final W2 parent/isolated child reports | 11 scenario rows plus init/cleanup: 13 passes, zero failures/skips |
| w1-regression-qtest.txt | article_page_lifecycle_test | 17 passes, zero failures/skips |
| native-tests.log / native-*.txt | run-native.ps1; Conan launcher binary -o 'absolute/file.txt,txt' | all four exit 0 |

Native totals: application_service_test 122 passed, 0 failed, 3 skipped;
http_client_test 24/0/0; runtime_composition_test 18/0/0;
full_text_index_test 35/0/0. Three existing Windows symlink-capability skips
are explicitly unverified: PersistenceFailuresBeforePublicationRemainAbortable,
RejectsUnsafePreviousHistoryAbsenceTargets, PreviousPersistenceFailuresRemainForwardAndTruthful
(each executes its preceding assertions before the unavailable symlink branch).
No W2 critical case is skipped. The baseline regex included a nonexistent
startup_transaction_recovery smoke name; it did not execute and is not claimed.
Actual recovery evidence comes from application_service_test, including
StartupRecoveryReplaysDesiredPersistence, RecoveryPolicyDurablyGatesDesiredAttempts,
RecoveryPolicyReportsMarkerPublicationTruthfully and the related crash/forward cases.

The first expanded run (regression-tests.log, then regression-diagnostic-*.txt)
failed because nested parent/child TEMP roots exceeded Windows transaction path
limits. The durable-decision write reported missing path. This is environment
evidence, not A5 red evidence. The test now inherits its already-isolated parent
profile and uses short unique fixture components; assertions are unchanged.
A diagnostic native invocation had an unquoted comma split into a spurious
`txt` test argument (exit 1); it is not a passing invocation. Final scripts pass
the full output argument as one value. Qt offscreen font/GLES warnings remain;
the asserted transaction and W1 cases execute successfully despite those warnings.

## Regression contract and red evidence

The red baseline is **instrumented**, not an untouched binary. Only narrow
source-private storage observers/operator-new boundaries and the test target
were added before the behavior repair. red-instrumentation.diff and
red-publication_preparation_test.cpp preserve that snapshot. No transaction
algorithm was duplicated and no production MainWindow scenario was added.

Each module's class-specific allocation/constructor was observed after
PersistDesiredConfiguration's kAfterDecisionPublished checkpoint, which follows
successful PublishNoReplace of the pending desired-commit record. The earlier
coordinator kPersistenceDecision event is not used as proof of an actual decision.
core-phase/network-phase failed exactly on the after-decision assertion.
Targeted bad_alloc at those same real allocations terminated isolated children
with W2_CORE_ALLOCATION_AFTER_DURABLE_DECISION (exit 91) and
W2_NETWORK_ALLOCATION_AFTER_DURABLE_DECISION (exit 92). Neither failure was a
startup error, timeout or machine-wide memory exhaustion.

The same original assertions passed after full construction moved into each
PrepareCandidate. The token-only intermediate still counted one executable-linked
allocation in the publication interval; success/two-transactions failed on zero
versus one. Removing the queued callable and vector snapshot passed that same
counter assertion. Later test additions strengthen cleanup/retry/limits evidence.

| Acceptance | Actual scenario / additional evidence |
| --- | --- |
| Both participants prepared before decision; original order and versions | core-phase, network-phase, success; exact 16 coordinator boundary sequence; real configuration/cache and facade state |
| Core allocation failure; partial prepare cleanup | core-failure: Network constructed first, Core construction absent, no decision/record/publication; old executor/config/cache intact; allocation/destruction balanced; retry succeeds |
| Network allocation failure | network-failure: no Core allocation attempted; original bad_alloc propagation preserved; no decision/publication; retry succeeds |
| Later preparation failure / abandonment | later-failure at WidgetsPrepare; reserved-abort after both reservations and reversible Widgets maintenance; decision-write-failure at actual persistence publication operation; balanced destruction, old state and retry |
| Bounded publication without target allocations | success, reduce-cache, disable-cache, two-transactions; same private allocation/constructor phase checks plus executable allocator counter |
| Token one-shot/generation and old references | two-transactions prepares stale Core/Network candidates before the successor, rejects them afterward, abandons them and balances cleanup; all rows hold old facade through retirement then observe weak expiration on last release |
| Cache/storage authority and request lifetime | unchanged bound-cache identity through transactions; http_client_test EnforcesMoveOnlyStorageLease, RejectsDuplicateRuntimeStorageAuthority, ReservesPreparedPublicationBeforeDecision, CancelsAndJoinsBeforeShutdownCleanup, existing candidate/owner-thread/stale/shutdown cases |
| Recovery / transaction integration | real coordinator smoke, Preferences predecision smoke; application_service persistence/desired replay/marker/crash/forward cases; no persistence schema or recovery algorithm change |
| W1 preservation | article_page_lifecycle_test 17 passes; W1 files have zero diff |

## Publication call-chain and cleanup review

The observed interval is coordinator kNetworkPublish entry through kWidgetsPublish
entry (successful Network/Core publication), not all process execution or forward
maintenance. The test executable replaces scalar/array and aligned C++ allocation
entry points; its atomic counter observes calls from any thread linked to those
entries. Core/Qt DLL-internal allocation and malloc-only/custom allocator calls
are blind spots. Core's specific private allocation is independently observed
inside its DLL through a thread-local callback; both specific token callbacks run
on the transaction thread. The callback uses fixed counters/arrays and no logging,
allocation or scheduling. No claim of whole-program zero allocation is made.

- Core: preconstructed publication Impl; weak/shared ownership operations do not
  allocate a control block. Old/current compositions move into existing optional
  storage (compile-time nothrow move assertion). Moved-from activation destruction
  sees an empty impl; no old executor is stopped at publication. Resetting the
  consumed Prepared destroys only moved-from/empty members. Old live composition
  remains owned until Finish and existing external shared owners release it.
- Network: reservation/fetch exclusion, mutex/CV primitives, generation fields,
  existing std::function invocation and pointer moves. Split publication uses the
  already-running timer and registered resource, no QMetaObject queued callable.
  Registry traversal copies a shared pointer, not a vector; erasure does not grow
  storage. Publication completion is acknowledged after its observer returns.
  The default preparation move assignment is compile-time nothrow. Reservation
  deletion has a moved-out candidate. Finish waits for terminal post-work before
  releasing Prepared; its withdrawal sees terminal, and there is no ownership cycle.
- Cache setter: Qt 6.11.1 qnetworkdiskcache.cpp setMaximumCacheSize only updates
  scalars and conditionally invokes virtual expire. The existing override suppresses
  expiry during publication and returns maximumCacheSize without filesystem work.
  Positive reduction's real expiry and zero-policy deletion remain forward work.
  Local reference source inspected at
  C:/Users/dev/.conan2/p/b/qte8223176676a8/b/src/qtbase/src/network/access/qnetworkdiskcache.cpp:440,
  with .cmake.conf reporting 6.11.1. qobjectdefs.h confirms the removed functor
  invocation allocates QCallableObject. This is version-matched source inspection,
  not interception of every instruction in the installed Qt DLL.
- Coordinator: recovery path/identity construction is now before preparation and
  the decision; error-result ownership moves avoid post-decision string copies.
  PersistDesiredConfiguration/BeginDesiredRuntimePublication still perform existing
  fallible I/O, serialization and recovery diagnostics under their original outcome
  contracts. Explicit forward cleanup, error logging and runtime failure persistence
  are not reclassified as an allocation-free interval or ordinary rollback. The
  shutdown-only dispatcher snapshot is outside reserved publication (shutdown is
  excluded by reservation) and is not swept into this correction.

No noexcept was removed, no new catch-and-ignore path was added, and no null token
is substituted for successful publication. This verification does not prove all
exception paths in the whole recovery subsystem safe under arbitrary OOM. Linux,
macOS, Debug, sanitizers and unavailable Windows symlink branches were not run.
The normal CTest entry and nothrow type assertions protect this bounded contract;
transitive Qt behavior, ownership retirement and callback changes still require
human review. Do not replace these checks with file-size limits or SOLID scores.
