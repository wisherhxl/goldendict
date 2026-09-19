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
