# W3.3 View menu test responsibility migration — blocked

Base: 75de4fa427a05b9a666c5b4c05b4fbeb6fe4117f. Approved 2026-09-19.
The existing inventory's W3.3 section locks exactly ViewMenuSmoke; no additional
family may be substituted. This is conformance to the approved W3 extraction
boundary, not a new public behavior or architecture decision. Global/project
policies and activated candidate workflow apply; local final checkpoint, fresh
independent exact-candidate review, no merge/push or W4.

## Execution and acceptance

- [x] Build/run original product View menu scene with fresh owned paths; record
      the required persisted configuration fixture and retain initial failures.
- [ ] Move full scenario, substitutions and assertions into view_menu_test;
      retain real composition/resources/MOC and all action/event paths.
- [ ] Remove the corresponding product declaration, method and main dispatch.
- [ ] Verify equivalent behavior and preserve failed attempts if any.
- [ ] Extend the existing cumulative target-closure guard and its negative fixtures.
- [ ] Run migrated, W1/W2/W3/P1 and affected regressions; independent ON/OFF builds
      and actual source/link closure checks.
- [ ] Run actual OFF ordinary portable startup/render/normal exit via P1 layout.
- [ ] Record mapping, candidate identity and independent review externally.

The initial proposed private boundary was a copy-only Preferences snapshot; all actions and
widgets use existing QObject names and all callback changes use an existing public
operation. The runner owns test scheduling and substitutions. It reuses the W3
presentation closure, with a new BUILD_TESTS-only executable and preserved CTest
name, app-build working directory and 20-second timeout. No GUI thread/lifecycle,
lookup ownership, transaction or P1 setting change. Qt package 6.11.1 WebEngineCore
SHA256 remains 518710148d58470a2fd6a85d1d6ffde66bb122b7682b547d4454d494dad06822;
reuse P1 capability evidence without new investigation. Historical Fail/Pass and
platform/Qt5 geometry/whole-filesystem limits are unchanged.

External evidence root:
D:/workspace/goldendict/evidence/a4-test-extraction-w3-3-20260919.

## Baseline and scope correction

The initial snapshot-only proposal is insufficient and has not been implemented.
The original method saves and restores the main-installed Preferences callback
around its substitute callback segments. The restored callback executes real
configuration transactions for menu/toolbar actions. Existing W3 runners do not
install that callback; main.cpp is excluded from their presentation source closure.
The callback captures configuration, history, facade ownership, runtime, credentials,
paths and diagnostics, and applies coordinator outcomes to application-owned state.
Neither a value-only preference snapshot nor the existing callback setter supplies
that production behavior. Copying the orchestration into the runner would violate
the approved requirement to reuse the actual production implementation.

| Checkpoint | Command/result | Evidence |
| --- | --- | --- |
| Original build | Conan launcher, Release goldendict target; exit 0 | baseline-build.log |
| Fresh isolated configuration | CTest goldendict_view_menu_smoke; exit 8, actual case failed | baseline-test.log |
| Diagnostic isolated configuration | Same case, supported Qt stderr logging; exit 8, missing required transaction source at boundary 0 | baseline-test-diagnostics.log |
| Seeded isolated configuration | Same unchanged binary/assertions; exit 0, 1/1 passed | baseline-test-seeded.log |
| Independent feasibility review | Existing seams cannot preserve the real callback; not a completion Pass | boundary-review.md |

Exact commands, owned paths and fixture bytes are recorded in baseline-evidence.md
under the external evidence root. The successful fixture uses the supported core
configuration format with an explicit owned index directory. No private product
state or production source was altered. Earlier failures are not erased and are
not an artificial red phase. The seeded success resolves the fixture prerequisite,
not the missing reusable production callback boundary.

## Current disposition and next boundary

The selected family is retained but classified as mode C. No substitute family
was added. No extraction, production/test/build modification, new TestAccess,
entry deletion or migration checkpoint commit was made. HEAD remains the base.
Only this status record and the existing inventory are changed and left uncommitted.

The smallest identified continuation requires a separately approved, source-private
shared production Preferences application boundary with explicit captured-state
lifetimes, preserving the current coordinator and outcomes. Its design/readiness
must be reviewed before implementation; it is not authorized by the failed
snapshot-only scope assumption. The currently authorized alternative is to retain
the original scene and stop this family. A different family requires a new explicit
batch selection, not a silent replacement. No W4 ownership work is authorized.

Final implementation validation is **not performed**: migrated/cumulative regression,
new guard negative fixtures, ON/OFF final builds, OFF ordinary portable render/quit
and fresh completion review remain unfulfilled. No changed implementation candidate
exists to validate. Historical P1/W1/W2/W3 evidence retains its original scope;
it is not presented as a W3.3 rerun. The independent read-only scope review is
recorded separately and must not be described as completion review or acceptance.

W3.3 implementation: blocked/not started. Baseline behavior: passed with documented
fixture. Independent completion review: pending. W3/A4: in progress, not closed.

## Authorized continuation: two checkpoints

The subsequent explicit approval permits the minimal private production Preferences
assembly extraction identified above, followed by this same ViewMenuSmoke family.
The blocked snapshot and its review remain preserved externally in pre-extraction-*
files and boundary-review.md. No scenario is replaced. This is a behavior-preserving
organization change within the desktop composition root, not W4 or a public API change.

### Concrete boundary and lifetime design

Move the actual main.cpp Preferences callback body into private
src/preferences_application.{h,cpp}. A narrow PreferencesApplicationBindings value
contains only the existing callback's dependencies, individually specified below.
InstallPreferencesApplication installs the same returned callable in MainWindow;
the test retains that callable for the original substitute/restore stages.
The callable captures the bindings by value: reference members continue to name
original state; callable/path value members survive the installer's return.
No reference to an installer's stack local survives. No separate transaction
algorithm or prepare-core callback is supplied by main or tests.

| Binding | Purpose / owner / lifetime |
| --- | --- |
| CoreConfiguration& | Read current preferences and update the same configuration after publication; caller-owned |
| vector<HistoryEntry>& | Bound and update the same history only when changed; caller-owned |
| shared_ptr<DesktopFacade>& | Export current tab session and replace the same current snapshot after publication; caller-owned |
| DesktopFacadeActivationOwner& | Prepare/publish ownership remains with the existing owner; caller-owned |
| NetworkRuntime shared pointer | Retains the existing runtime for real preparation/composition; same runtime as coordinator |
| ConfigurationReloadTransactionCoordinator& | Execute unchanged transaction; caller-owned, outlives callback invocations |
| ForvoCredentialMap const& | Existing in-memory credentials for composition; caller-owned |
| vector<RuntimeCompositionDiagnostic>& | Update original diagnostic state after publication; caller-owned |
| configuration/history QString paths, Network root string | Stable selected locations copied into the callable; no new path policy |
| refresh_history callable | Existing presentation notification, copied safely; does not implement the transaction |

Move existing PrepareProductionFacade and ReportRuntimeCompositionDiagnostics
verbatim into the same private source, retaining all other main callers. Their
candidate/session/diagnostic behavior remains unchanged. Existing predecision-smoke
fault scheduling remains in main (unmigrated historical family); only its existing
inject_failure callable is supplied separately to the shared implementation.
No smoke names, steps, fault schedule or assertions enter the shared source.

All calls remain synchronous on the GUI thread, at the original installation point
before window.show(). Nested prepare callbacks execute inside Execute and capture
only that active invocation's locals. Main keeps original objects and destruction
order; destroying a std::function does not invoke borrowed dependencies. No new
connections, async scheduling, profile initialization or publish-stage work.
Tests declare dependencies before the window/callback usage and finish all calls
before destruction. Replacing/restoring the single setter replaces one callable,
not additive connections. No new MainWindow/Core orchestration ownership.

### Checkpoint A

First solidify a fresh isolated fixture using Core SaveConfiguration and
LoadConfiguration round-trip, never a previous run's files. A BUILD_TESTS-only
view_menu_test fixture mode supplies this before extraction; it is not counted as
a migrated scenario. After extraction the original production smoke method and
entry remain intact. Build and run that original scenario plus affected configuration,
transaction, P1 and W1/W2 checks before making checkpoint A. No B edits before A passes.

### Checkpoint B

Extend that test runner using the existing real W3 presentation closure and the
shared installer. Move the complete original method's scenario out of MainWindow,
using existing QObject names, existing public operations and a single copy-only
Preferences snapshot friend. Restore the actual shared callable at each original
restore point. Add persisted configuration/current facade observations to prove
real transaction publication, not merely successful return or callback counting.
Remove the original method/declaration/flag/dispatch and map the same CTest name to
the runner. Extend the existing closure guard/negative fixtures; no new framework.

Final validation: original-to-new assertion map, cumulative W1/W2/W3/P1/configuration
checks, ON/OFF product build and source/link closure, guard negatives and ordinary
OFF portable render/normal quit using P1 scripts in a new owned run directory.
Fresh independent completion review covers both checkpoints cumulatively; old
boundary review is not completion evidence. No push, merge, W4 or second family.

### Checkpoint A implementation result

Readiness: independent agent /root/w33_extraction_readiness returned Ready for the
private extraction (not completion Pass). The baseline fixture prerequisite passed;
checkpoint A completion additionally required the extraction/regressions below.
NetworkRuntime is a borrowed const shared_ptr reference (the earlier design table's
retention wording is superseded); no added runtime owner is introduced.

Shared production implementation: preferences_application.cpp/h. Main installs it
once at the same point and continues to own every mutable object. Existing smoke
predecision scheduling remains in main, passed as the existing boundary predicate.
Only PrepareProductionFacade and ReportRuntimeCompositionDiagnostics moved with it;
all source/group/startup callers use the same implementation. No production paths,
transaction outcomes, thread or publication ordering were changed.

Fresh serialized baseline: view_menu_test --prepare-fixture writes/loads a new
owned root using Core APIs; original scene passed 1/1 (serialized-baseline.log).
After extraction: goldendict, article_page_lifecycle_test and
publication_preparation_test build exit 0 (checkpoint-a-build.log). The unchanged
ViewMenuSmoke and ten affected regressions pass 11/11, CTest exit 0, 21.40 seconds
(checkpoint-a/ctest.log). Included view restart, history/preferences, proxy/cache,
coordinator/predecision, groups/sources, W1 and W2. Original method/entry remain.
No B migration edits preceded that result. Local checkpoint identity is recorded
externally after commit; it is not final independent acceptance.
