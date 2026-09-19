# W3.3 View menu test responsibility migration

Base: 75de4fa427a05b9a666c5b4c05b4fbeb6fe4117f. Approved 2026-09-19.
The existing inventory's W3.3 section locks exactly ViewMenuSmoke; no additional
family may be substituted. This is conformance to the approved W3 extraction
boundary, not a new public behavior or architecture decision. Global/project
policies and activated candidate workflow apply; local final checkpoint, fresh
independent exact-candidate review, no merge/push or W4.

## Execution and acceptance

- [x] Build/run original product View menu scene with fresh owned paths; record
      the required persisted configuration fixture and retain initial failures.
- [x] Move full scenario, substitutions and assertions into view_menu_test;
      retain real composition/resources/MOC and all action/event paths.
- [x] Remove the corresponding product declaration, method and main dispatch.
- [x] Verify equivalent behavior and preserve failed attempts if any.
- [x] Extend the existing cumulative target-closure guard and its negative fixtures.
- [x] Run migrated, W1/W2/W3/P1 and affected regressions; independent ON/OFF builds
      and actual source/link closure checks.
- [x] Run actual OFF ordinary portable startup/render/normal exit via P1 layout.
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

## Baseline and scope correction (preserved initial state)

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

## Initial disposition and next boundary (before extraction authorization)

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

## Checkpoint B coverage map and implementation

The following mapping preserves the original sequence inside one scene. This does
not combine separate previously isolated scenarios. Original references are at
base 75de4fa4, MainWindow::RunViewMenuSmokeCheck lines 2708-3052. New target is
view_menu_test, slot ViewMenuTest::viewMenuThroughRealApplication.

| Original stage / assertion | New test observation and real boundary |
| --- | --- |
| Required menu/pane/toolbar objects | Existing QObject-name lookups, failure is a real QtTest failure |
| 15 View actions, 7 top menus, separators, roles, labels, accessibility | Same expected action identities and order; structure stage |
| Ctrl M/O/S/R/I/H uniqueness, empty toolbar shortcuts | Same action/shortcut enumeration |
| Seven zoom menu entries | Same zoom/word-zoom actions and separator |
| Two article views zoom/reset, callback counts 1/2 | Same real ArticleView creation and actions, same success substitute |
| Word/query/group fonts, unaffected result/history/favorite fonts, counts 3/4 | Same widget fonts/actions and value-only Preferences snapshot |
| Menubar hide/show | Restore shared real callable; same visibility/button assertions |
| Dictionary names on/off and toolbar icon sizes | Same real callable/actions/style assertions |
| Six real Preferences transactions | Additional checks: current facade changes and equals owner snapshot; disk-loaded preferences equal same configuration identity and window snapshot; persisted article session exists |
| Saved View settings survive SetPreferences reconstruction, three callbacks | Same success substitute and original two SetPreferences calls |
| Rejected save preserves actions/widgets/preferences | Same failure substitute/message and original three triggers; no direct final-state repair |
| Always-on-top true/false | Restore real callable at original point; same window flags |
| Search placement dock/toolbar, two signals and two callbacks | Same placement substitute, real toggle actions, parent checks and counts |
| Five dock/toolbar visibility cycles | Same four transitions and exact signal counts per widget |
| Layout restored, active tabs visible with positive geometry, state version 7 | Same state comparison; use existing presentation host ActivePage after real replacement instead of a stale initial tabs pointer; compare actual saveState(7) with public capture |

Private test access remains one copy-only Preferences observer. Callback install/
substitute/restore uses existing setter and shared production installer. No generic
mutable access, new production test scheduler or test-owned transaction algorithm.

Runtime setup uses the same production facade preparation and Preferences installer,
existing window setters/restorers, and P1 WebEngine path initialization. Scope cleanup
matches main: coordinator shutdown, detach facade, release snapshots, owner shutdown,
Network shutdown, then normal stack destruction. The test owns all configuration,
index/cache/profile/temp paths. A single short temporary root avoids redundant path
nesting; no production path policy changed.

CTest name remains goldendict_view_menu_smoke; executable changes from goldendict
--view-menu-smoke to view_menu_test. App-build working directory, offscreen platform,
x11 session marker, Chromium flags and 20-second timeout remain. The runner owns
its environment directories, replacing the old fixed test-home values. Original
main watchdog/exit dispatch is replaced by QtTest failure accounting and the CTest
process timeout; no successful empty run counts as validation.

Removed: RunViewMenuSmokeCheck declaration/definition, internal smoke flag and
main dispatch. The callback local substitutes, assertions and scene state now occur
only in the BUILD_TESTS target. Existing predecision-smoke fault schedule and other
unmigrated families remain at their previous owners; no added historical exemption.

Development failures are retained: first missing header compile failure; first
runtime exception (exact terminating instruction not captured); long nested fixture
path causing a persistence-decision file error, with same-binary short-path control
passing; then stale initial tabs observer revealed by invalid dimensions, and an
initial-name lookup that correctly failed after replacement. The final observation
uses the existing active host, preserving the original member's current-page meaning.
No assertion or production failure semantics were weakened to resolve these issues.
The initial runtime exception cannot be assigned a definitive stack cause; the stale
observer was removed and subsequent full-scene runs verify the corrected observer.

Targeted corrected run: b-host/ctest.log, 2/2 exit 0; ViewMenu QtTest 3 passed,
0 failed/0 skipped; guard 5 clean fixture configurations and 20 deliberate violations
(4 modes for each of 5 cumulative migrated-runner/access names). Build failures and
intermediate failed runs remain beside the success logs; directory names containing
"passed" describe intended attempts, not their actual verdict (see exit/log).

## Final implementation verification (before independent completion review)

Checkpoint A: 3e897b17df1083ce3f2e15c8eece23887568cb24, extraction with old scene
intact, 11/11 regression pass. Checkpoint B contains only the full ViewMenu family
migration, guard extension and associated records. Its exact committed identity,
full cumulative diff and independent receipt are recorded externally to avoid
mutating the frozen candidate to record review results.

- ON Release and independent OFF Release production builds: exit 0. Both source
  roots are this worktree and both use the unchanged Qt 6.11.1 package.
- verify-cumulative.ps1 -RunName candidate1: 27/27 actual CTest cases passed,
  individually isolated roots with fresh serialized fixtures; no zero matches.
  candidate1-results.json maps every case to its command output and owned root.
- ViewMenu scene: 3 QtTest passes (init, actual scene, cleanup), zero skips.
- ArticleInspector retains the already approved NativeLegacyGeometryImport skip:
  14 passed / 1 skipped. No new skips or platform claims were added.
- Cumulative guard: 5 clean and 20 rejected source/interface-source/dependency/genex
  fixtures, including all W3.1/W3.2 families and the new runner/access.
- Production closure reports and ninja command graphs contain no migrated runner;
  actual ON/OFF main_window object symbols contain none of the three removed Run
  methods. OFF target inventory has no view_menu_test. Shared Preferences source is
  compiled/linked into actual goldendict and the test closure, not a different copy.
- Ordinary OFF startup: ordinary-off/result.json, exit 0, no application arguments,
  portable recognized beside the copied executable, real main window and WebEngine
  welcome document observed, one fixture dictionary reported, Ctrl+Q normal exit.
  ui-main-window.txt and loaded-qt.json bind the observed window and actual Qt DLLs.
  Watcher reported 78 events inside the owned run root and adjacent sentinel intact;
  event count is evidence, not an acceptance threshold.

Startup paths: configuration/history/favorites/recovery under ordinary-off/bin/portable;
indexes under ordinary-off/indexes; Network at portable/cache/qt-network-http;
WebEngine at portable/webengine/article; temp and cwd under ordinary-off/tmp and
ordinary-off/working. The exact unchanged P1 launcher/watch scripts are reused.
No smoke branch, Network/WebEngine bypass, daily profile writes or daily sentinels.
Bounded observation remains Windows/current Qt package and this portable scenario,
not all Chromium file access, nonportable default startup, Linux/macOS or Qt5 parity.

Source scope: no W1/W2/P1 behavior or path implementation modifications, no Core or
Network source changes, no public installed interface or Accepted ADR changes.
Remaining main test header/fault fields/shared fixtures are existing legacy scope.
This batch removes one complete family, leaving 50 MainWindow Run*Check definitions;
the number is navigation only, not architecture acceptance.

Implementation and developer behavior verification are complete. Independent
completion acceptance and closure require the fresh exact-candidate external
receipt. Earlier scope review and readiness are not completion Pass. Historical
failures, including P1's original Fail and limits, are not overwritten.
