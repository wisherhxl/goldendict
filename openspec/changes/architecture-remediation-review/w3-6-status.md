# W3.6 History Preferences extraction and migration

Base: `041f8625fc45fe06e483a80dcf9089c3c87b1152`. Worktree and branch remain the explicitly authorized Qt6 task checkout. Initial working tree is clean. Both Release/Ninja caches bind this source; BUILD_TESTS is ON in build/Release and OFF in build/W3-off. Actual Qt package is qt7b4c1616170c6, Qt 6.11.1. Windows/P1 observation limits are unchanged.

## Scope and approved design

The user approves one existing family, HistoryPreferencesSmoke, and its minimum real private desktop assembly extraction. This is behavior-preserving implementation of the existing architecture.md General/History composition contract, not a new persistence policy or public interface. No Accepted design changes. Favorites and other History families are excluded. No new overall plan, generic framework or test access is needed.

Original source anchors at base: main.cpp:941 refresh projection; 984 LookupSubmitted recording; 1051 ImportHistoryRequested; 1676 smoke dispatch; main_window.cpp:3387 RunHistoryPreferencesSmokeCheck; CMakeLists.txt:468 CTest registration. The actual method has no cancel/error-substitute stages. It always uses real Preferences; the executor only fills controls and clicks OK. Inventory's earlier generic cancel/failure/export description is inaccurate for this specific method and is superseded here.

## Minimum assembly and lifetime

Add private history_application.h/.cpp beside preferences_application, with RefreshHistoryPresentation, InstallHistoryRecording and InstallHistoryImport. Separate installation functions preserve the original relative connection order around unrelated callbacks. Each uses the same main-owned configuration, history vector and window by reference; the selected history path is copied into the installed callable. No local reference survives installer return. QObject context remains window, AutoConnection and GUI-thread direct invocation remain unchanged; destruction of window disconnects. Install each exactly once. Main continues owning/loading all state. No manager, mutable-state accessor, Core GUI code, or Preferences dependency on the new module.

Recording preserves: store/max guard, copy current history, case-insensitive removal, prepend word/group, trim, SaveHistory, assign original vector, refresh; exception warning and no successful state swap on save error. Import preserves: ImportHistoryText(current maximum/group), SaveHistory, assign original vector, refresh, exception warning. Projection preserves reserve/order/word/group mapping. Main's refresh callback becomes a call to this same projection and remains consumed by existing clear and Preferences assembly. Export, clear, Favorites and all other callbacks remain in main.

Dependencies are window (signals/presentation/warning parent/context), const configuration reference (current recording/max policy), mutable history reference (original identity), QString path value (save destination). There is no packaged application state or caller-supplied business algorithm.

## Original full execution contract and mapping

One CTest entry goldendict_history_preferences_smoke, one goldendict process with --history-preferences-smoke, app-build working directory, 20-second outer timeout and 10-second in-process watchdog. Existing x11/offscreen/Chromium test flags apply. P1 test configuration root and TEMP/TMP are supplied by an owned fresh serialized fixture. No restart or shared cross-process phase exists for this family. The old zero-timer dispatch runs after window initialization; exit 0/1/2 means success/assertion failure/watchdog. The new QtTest runner will schedule the same synchronous scene after initialization on the Qt event loop, retain the watchdog/20-second timeout, and report assertion failures as nonzero. No test depends on another test's data.

Fixture: Core SaveConfiguration/LoadConfiguration validates a new owned configuration and index root. Old dispatch seeds and saves Newest/Middle/Oldest (groups 3/2/1), projects three items, and writes Imported one/Imported two/Ignored text; migration moves these steps and the main completion assertions into the test target.

| Stage | Required observation |
| --- | --- |
| Initial | Three history rows and current-size/maximum label; capture facade article session |
| Real Preferences false/2 | Accepted dialog; recording disabled; maximum 2; Newest/Middle remain; exact count/tooltip |
| Lookup Not recorded/9 | Still two rows, Newest first |
| Import group 7 | Exactly Imported one/Imported two, count 2/2 |
| Real Preferences true/1 then Recorded/11 | One Recorded row; count 1/1 and exact tooltip; original article session preserved |
| Original main completion | Import fixture write succeeded; LoadHistory(path,1) returns Recorded; LoadConfiguration says store=true, max=1 |

Original scene does not assert duplicate handling, import error, save error or restart. Preserve those implementations by exact extraction and run existing History regression consumers; do not claim missing branches as original coverage. Test-only additional disk/vector/UI checks may strengthen evidence without replacing original assertions or production operations. Existing finite executor setter and Preferences value observation suffice; history widgets already have object names.

## Execution gates

1. Build production and fixture target; run old full CTest entry on fresh owned data. Lock this family only after baseline passes.
2. Checkpoint A: extract assembly; main uses it while old scene remains. Run same entry and affected History/Preferences/coordinator/recovery cases. No migration before success.
3. Checkpoint B: move entire scene/main fixture/assertions into history_preferences_test; retain name, process/timing/environment contract, remove old production method/flag/dispatch. Extend existing target-closure guard.
4. Final: W1/W2/P1, all six earlier families (including dictionary-context two-process/40-second wrapper), affected configuration/History regressions, ON/OFF production/link isolation, ordinary OFF portable startup with real render/normal quit. Fresh independent read-only cumulative review starts at old CTest/main dispatch and binds final candidate/outputs. Preserve any Fail and repairs.

Evidence root: D:/workspace/goldendict/evidence/a4-test-extraction-w3-6-20260919. Existing P1 runtime layout and W3.5 scripts are reused with fresh owned roots. No product change to path/profile/transaction behavior. W3/A4 remains in progress; no merge/push.

Status: design and source impact recorded; baseline/readiness pending; no production edits yet.

Baseline: production/fixture build exit 0; exact original CTest 1/1 passed on fresh w36-baseline/t1 data (0.49 seconds). Scope locked to HistoryPreferencesSmoke only. No product red stage is claimed.

Independent read-only readiness: Ready (external readiness-review.md). Production extraction starts after this gate.

## Checkpoint A result

Production extraction completed without moving the old scene. checkpoint-a-build.log: exit 0, actual production/fixture/prior Preferences runner builds. extraction-results.json: 13 exact CTest entries, all actual 1/1, exit 0: original HistoryPreferences, five other legacy History consumers, ViewMenu, Articles, DictionaryContext (two processes), Synonym, configuration reload coordinator, Preferences predecision and history_store. Each has fresh serialized owned input. Separate installers preserve relative connection ordering; reference identities and same window context are unchanged. Only capture destination by value and call shared projection differ from the original handlers. No production behavior fix or test red stage.

Source analysis used explicit definitions/callsites plus real compilation/runtime checks. Serena activated the exact task path; no onboarding/reconfiguration or claim of complete semantic indexing. Checkpoint A is a local recoverable checkpoint, pending final cumulative independent review.
