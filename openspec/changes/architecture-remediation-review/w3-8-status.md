# W3.8 execution-result protection and HistorySmoke

Base: 0638ee817ac5d7e3dc7e08dbf248da07c7876fb6, initially clean dedicated Qt6
worktree. Release ON and W3-off OFF caches point to this source; Qt 6.11.1 package
qt7b4c1616170c6 and P1 hashes unchanged. Candidate-v1 policy hashes verified.
No reset, Qt5 changes, rewrite, merge or push. Existing approval covers this
bounded conformance change; no public contract, ownership or design change.

## Sequence and readiness

1. Verify the W3.7 actual runner failure boundary before History migration.
2. Preflight HistorySmoke from its original CTest/main/method, run original entry,
   then lock only HistorySmoke if existing assembly suffices.
3. Migrate, verify, run cumulative checks/ON-OFF/native portable startup, freeze
   candidate for fresh independent read-only review. W3/A4 remains in progress.

Evidence root: D:/workspace/goldendict/evidence/a4-test-extraction-w3-8-20260919.
W3.7 Fail/correction/Pass remain unchanged in their original root. First baseline
build exited 0; four original/current exact CTests (HistorySmoke and the three
custom event-loop runners) passed on fresh owned serialized fixtures.

## Execution-chain preflight

W3.7's observation failure used a pre-publication article-tabs pointer. Real
Preferences publication replaces the alias; the new tabs lack the old object
name. The diagnostic name lookup returned null. The existing const visibility
observer fixes that test observation without changing product lifecycle.

The result loss is in the custom QtTest main: only returning application.exec()
does not preserve qExec's assertion-failure result when normal event-loop shutdown
returns success. W3.7's stored result protects HistoryImport and OptionalParts.
The only other matching custom entry found is HistoryPreferences; direct-return
qExec runners are outside this mechanism. Conan launcher returns its subprocess
result; CTest saw the process success, and the early external summary only checked
CTest. W3.7 later added raw-report checks, which are complementary, not a substitute
for correct process failure. No evidence attributes this to PowerShell.

Add one test-only failure assertion at the END of each real scenario, activated
only by an explicit child environment value. All normal assertions/real work occur
first; RAII cleanup and QtTest logging then run normally. Instrument each actual
entry's qExec/event-loop/process results. A CMake/CTest checker runs the actual
registered entries in success and failure modes on independent owned data,
checks exact execution/raw results/cleanup and reports expected-child-failure
separately from checker success. No product fault option, business state change,
standalone replacement algorithm or permanently failing normal registration.
A failing checker before repair is infrastructure evidence, not product RED.

## History preflight (not yet migrated)

Original goldendict_history_smoke is one product process, --history-smoke,
app-build cwd, 20-second CTest limit, 10-second watchdog and zero-timer entry.
No wrapper/restart or imported-history stage. Original method sets group 7,
selects it, submits history-smoke-entry through StartLookup. The production
InstallHistoryRecording listener runs before the single-shot observer. Observer
checks word/group and live row/UserRole group, selects group 0, installs another
single-shot observer and activates the history row. Real itemActivated dispatch
calls StartLookupInTab with stored group 7; replay emits LookupSubmitted again.
Main completion reloads history and requires the same word/group at the front.

There are no callback success substitutes or restore stages. Existing
history_application recording/projection and real MainWindow/Core initialization
supply all business code. Named query/list/group widgets and normal SubmitInitialLookup
can reproduce original operations; no new mutable access or assembly is required.
Configuration/history/runtime/facade remain caller-owned; GUI-context callbacks
borrow live state and connections use the window receiver. Preferences is not
required by the scenario. Original baseline passed on fresh valid config, empty
initial history and owned index/cache/WebEngine paths. No daily data is consumed.
HistoryManagement/Export/Menu/Favorites remain excluded.

## Execution-chain result and checkpoint A

The actual same-chain negative is retained at w38-exit-before-repair/t1/tmp/ep-*.
Each child ran its real full scenario before the controlled final assertion and
resource cleanup. Both W3.7 runners reported qtest=1, event_loop=0, process=1,
child CTest=8. The matching HistoryPreferences entry reported qtest=1,
event_loop=0, process=0, child CTest=0. The checking CMake script then failed
and its outer CTest exited 8; verify-targeted exited 1. This establishes the loss
at the runner's choice of return value, before Conan/CTest, without guessing
PowerShell behavior or inferring it from PASS text. It does not assert which
individual Qt shutdown event supplied the zero.

HistoryPreferences now retains qExec's result exactly as W3.7 does. The unchanged
checker passed all six child controls: success qtest/process/CTest 0; expected
failure qtest/process 1 and CTest 8 after resource cleanup and cleanupTestCase.
The checker itself and outer CTest exited 0. exit-after-repair-results.json also
has the three normal entries passing, four exact registrations total. Missing
reports, wrong owned data path, absent cleanup, skipped/absent scenarios or wrong
numeric results fail the checker. Raw child logs remain separate from product
regression. No product or general launcher code changed.

## Locked HistorySmoke migration

With the first gate complete, lock HistorySmoke only. Baseline full original
entry passed in w38-baseline/t1. Map RunHistorySmokeCheck and --history-smoke to
history_smoke_test under BUILD_TESTS, retaining goldendict_history_smoke's name,
app-build cwd, one process, 20 seconds and 10-second watchdog. SetDictionaryGroups
and the named group combo perform the same normal group-selection operation;
SubmitInitialLookup sets the query then calls the original StartLookup. Real
recording is installed before the observer, and the real history itemActivated
binding performs replay. Expected word/row/UserRole/group assertions and main's
Core disk reload move intact. No Preferences callback is needed by this scene,
no new TestAccess or production assembly is required. Readiness: Ready under the
existing approved boundaries. Other History/Favorites entries remain untouched.

## History implementation and checkpoint B

history_smoke_test owns the complete original fixture/stage/observer/assertion
sequence. No new private access is added: SetDictionaryGroups, named combo/list,
SubmitInitialLookup and actual list activation invoke existing production behavior.
PrepareProductionFacade is reused from preferences_application for initialization;
the scene does not install or substitute a Preferences callback. Recording uses
InstallHistoryRecording with the same caller-owned configuration/history and GUI
receiver. The original MainWindow method/declaration and product smoke flag/branch
are removed; history_items_ and history UI remain ordinary production state.

| Original outer/inner stage | New evidence/operation |
| --- | --- |
| Fresh valid config and main-owned history path | Core Save/Load fixture roundtrip; fresh empty owned history and explicit index/Network/WebEngine paths |
| Set group 7 and query, StartLookup | Same group DTO, combo selection (existing selected/dock synchronization), public SubmitInitialLookup -> StartLookup |
| Recording callback before single-shot observer | Same production installer before observer, GUI direct delivery |
| Word/group and first visible row/UserRole 7 | Identical live row and signal assertions |
| Select group 0 then activate stored history row | Existing combo operation and list itemActivated; real StartLookupInTab with stored group 7 |
| Second single-shot observer restores word/group | Identical replay assertion and completion requirement |
| Main reload verifies nonempty first word/group 7 | Core LoadHistory, same required values, no final-state fixture rewrite |
| One process, 20 seconds, 10-second watchdog/zero timer | Same limits and entry semantics, guarded QTest result and normal resource cleanup |

No original import, clear/export/menu, restart, or extra dictionary-result contract
is claimed. The runtime's lookup request creation/retirement remains unchanged.
The new runner joins the same execution-chain checker (four success and four
expected-failure real runner invocations). history-migrated-results.json: 7/7
actual entries including HistorySmoke, the chain checker, HistoryPreferences,
HistoryImport, OptionalParts, ViewMenu and cumulative architectural fixtures.
No product RED was manufactured for this pure migration.

Preserved preparation failures: the first external edit script expected an
unformatted line and stopped before any source edit; the following build reported
an absent target (history-migration-build.log). The next build lacked the existing
preferences_application declaration header (history-migration-build-2.log).
Restoring that required include, without adding a callback or production behavior,
produced history-migration-build-3.log exit 0. These are implementation/build errors,
not acceptance, baseline failure or product RED.

## Final candidate verification

Checkpoint A: fe40283d (execution-result protection). Checkpoint B: 59a9856b
(HistorySmoke migration). Both are recoverable local commits, not prior review
Passes. Final documentation checkpoint will bind the cumulative evidence.

Commands below run from the dedicated worktree; scripts are retained under the
W3.8 evidence root and invoke the existing Conan launcher:

- verify-builds.ps1 -Prefix candidate: Release ON and W3-off OFF builds exit 0.
- verify-cumulative.ps1 -RunName candidate: 38/38 exact registered entries exit 0,
  comprising 36 product regressions, one architecture fixture checker and one
  execution-chain checker. Raw reports verify actual execution. This includes the
  ten migrated families, W1/W2/P1, configuration recovery, DictionaryContext's
  two-process shared configuration/40-second contract and affected History tests.
- verify-isolation.ps1 -Prefix candidate plus inspect-artifacts.py: both production
  command closures, actual Ninja link inputs and MainWindow object symbols exclude
  migrated implementations; OFF has no migrated test target requirement.
- Architecture fixtures: 13 legal configurations accepted, 52 deliberate source,
  interface-source, dependency and generator-expression violations rejected.
  These expected rejections are distinct from passing product tests.
- Execution checker: four real runners each execute success and controlled failure
  on fresh data. Successful child CTest exit 0; expected assertion-failure child
  CTest exit 8 after normal cleanup, qExec/process exit 1; checker exit 0.
- run_with_conan.ps1 --build-type Release -- python <evidence>/ordinary_startup.py
  build/W3-off ordinary-off: native ordinary portable production startup, executable
  only (no smoke argument), real Welcome WebEngine page and one dictionary observed,
  Ctrl+Q normal exit 0. Loaded Qt DLL versions/hashes match Qt 6.11.1/P1. Owned-root
  watcher recorded 78 notifications, no watcher errors, adjacent sentinel intact.
  Notifications include evidence writes; their count is not an isolation criterion.

Ordinary startup evidence is new W3.8 evidence, not backdated P1/W3.2 evidence.
Config/history/favorites/recovery and Network/WebEngine paths retain the P1 portable
layout; indexes/tmp/cwd and fixture are owned by this run. No daily directory probe,
cleanup or fallback was introduced. The bounded watcher is not whole-process or
whole-filesystem tracing; no claim covers all Chromium features, non-portable
user-default startup, Linux/macOS, or the preserved Qt5 Inspector
NativeLegacyGeometryImport skip. No new key skip is accepted.

Implementation and local behavior verification are complete. Final independent
read-only review is pending at this tracked snapshot; its immutable external
receipt must bind the final commit/tree and complete base-to-candidate diff.
W3/A4 remains in progress; HistoryManagement/Export/Menu and Favorites are unchanged.
