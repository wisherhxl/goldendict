# W3.7 bounded migration

Base: 8784eeaae8a618cae0719bfd4515bcae58361545, clean dedicated Qt6 task checkout,
branch feature/tiger-qt6-migration. Both existing Release/Ninja caches bind this
source (Release ON / W3-off OFF); actual Qt 6.11.1 package qt7b4c1616170c6 and
WebEngineCore SHA-256 remain unchanged from P1/W3.6. W3.6 external closeout and
independent Pass identify this base. Fetch completed; local accepted checkpoints
are retained, without reset/rewrite, Qt5 edits, merge or push.

## Locked scope and impact/readiness

Exactly two complete independent scenarios, after both old exact CTest entries
passed 1/1 on distinct fresh Core-serialized/loaded fixtures. Order:
1. HistoryImportSmoke: RunHistoryImportSmokeCheck, --history-import-smoke,
   goldendict_history_import_smoke -> history_import_test.
2. OptionalPartsPreferencesSmoke: RunOptionalPartsPreferencesSmokeCheck,
   --optional-parts-preferences-smoke, goldendict_optional_parts_preferences_smoke
   -> optional_parts_preferences_test.

The earlier inventory grouped four History presentation entries for navigation;
it did not establish a shared multi-process contract. HistoryImport has its own
complete CTest process, fixture, handler and persistence assertions, no dependency
on the other three entries. It is independently migratable via W3.6's installer.
No other family is selected, substituted or partly migrated.

Both use the existing real MainWindow/Core/Network setup and private production
source closure, with runner-owned configuration/index/cache/profile/temp data.
History import installs the existing real history_application importer; Optional
Parts uses unchanged InstallPreferencesApplication. No new production assembly,
public contract, business ownership, path rule or Accepted-design adjustment.
This conforms to the approved existing migration contract; impact/readiness is
Ready from actual source and runnable old baseline. User-approved pure migration
uses baseline/equivalence, not artificial product RED. The final independent
review remains mandatory.

## Full old-to-new execution contract

Both old registrations are one product process, app-build working directory,
20-second CTest timeout and 10-second watchdog armed after initialization, with
zero-timer scene entry and exits 0/1/2 (success/assertions/watchdog). Neither has
a wrapper or restart stage. Existing x11/offscreen/Chromium flags are preserved;
fixed HOME/XDG directories become fresh runner-owned equivalents, with P1's
explicit Network/WebEngine paths. New QtTest -o is reporting only; failures stay
nonzero. The runner enters through the Qt event loop and arms its watchdog after
real initialization. Each run has independent temporary ownership and cleanup;
no daily or previous-test data is consumed. DictionaryContext remains its own
two-process/shared-profile/40-second regression, unchanged.

| Original stage and assertion | Destination and real path |
| --- | --- |
| History main: write BOM + ` Alpha \\r\\n第二个\\n`, require successful preparation | Runner writes same bytes in owned directory; no hand-made final history |
| History method: install SingleShot observer after production importer, emit selected group | Same connection order and signal; selected group observed via existing named group selector, expected 0 |
| Observer: requested path/group, two live projected words Alpha/第二个 | Same synchronous handler completion, named history list and caller-owned vector observations; no private mutable access |
| History main completion: LoadHistory has exactly Alpha/第二个 | Same Core reload, plus equality with live vector; no post-import fixture rewrite |
| Optional cancel: exact label/tooltip/initial checkbox, toggle then reject, prefs unchanged | Original inspect/executor sequence using existing finite access |
| Optional error: forced callback error, dialog not accepted, visible validation, prefs unchanged | Same test-only substitute; then restore actual callback returned by InstallPreferencesApplication |
| Optional accepted true, reopen/cancel true | Real QAction/dialog/coordinator/persistence/publication; same stage order |
| Optional final: true, unchanged session/layout, nonnull central widget, visible article tabs; main reload true | Same copy-only Preferences access and narrow current-tab visibility observer, real disk reload; publication additionally observed |

No original import error/dedup/restart or optional-render content assertion is
invented. ImportHistoryText normalization is exercised by BOM/whitespace/Unicode
fixture; algorithms are neither copied nor changed. The original Optional scene
checks preference application, not a new content-rendering matrix.

## Ownership, captures and exclusions

Configuration/history/facade/owner/runtime/coordinator remain in their original
production roles and runner-local equivalents. Existing installers borrow caller
state and window context, with state constructed before window; no threading or
connection lifecycle changes. Test callback captures live synchronous locals and
is cleared before destruction. HistoryImport needs no new access. OptionalParts adds only ArticleTabsVisible to the existing ArticlesPreferencesTestAccess (no new friend): a const boolean observation of the current published widget, no pointer or mutable state escapes.
History widget observations replace reads of its projected private vector; the
unfiltered visible list plus real owner vector/disk proves the original values.

HistoryManagement still needs main's ClearHistoryRequested save/clear/refresh
callback. HistoryExport needs main's SetHistoryExportCallback UTF-8 BOM/QSaveFile
implementation. HistoryMenu uses those export/clear paths plus menu/provider
observations. These specific missing shared installers remain blocked, not
reimplemented or replaced with success substitutes. HistorySmoke is not selected
in this two-family batch; its lookup/replay stage needs a separate complete
preflight. Favorites remains the separately recorded unapproved assembly boundary.

## Execution checkpoints

- Baseline build production + fixture target: exit 0.
- Old exact entries: baseline-results.json, 2/2 actual isolated cases, exit 0.
- Migrate and verify HistoryImport first; only after success begin Optional Parts.
- Final run includes both new entries, prior seven families, W1/W2/P1, affected
  History/configuration recovery, cumulative guard, separate ON/OFF builds and
  actual production linkage, ordinary OFF portable rendering/normal exit.
- Freeze final candidate for fresh no-history independent read-only cumulative
  review starting at old CTest/main source. Keep review reruns and GUI evidence
  reuse distinct; preserve any Fail and repair records.

Evidence: D:/workspace/goldendict/evidence/a4-test-extraction-w3-7-20260919.
P1 Windows/Qt observation limits and existing Qt5 Inspector skip remain unchanged.
W3/A4 stays in progress; stop after these two families, no Favorites/W4 or push.

### History import checkpoint

The new exact CTest entry passed (history-migrated-results.json, 1/1).
Shared History Preferences, View Menu and cumulative architecture fixture tests
then passed (history-regression-results.json, 3/3). Production and runner build
exited 0 (history-build.log). The first command's nested PowerShell argument
forwarding ran only the first requested entry; the explicit subsequent array
invocation supplied the remaining three. No absent case is counted as executed.
No production History/Preferences implementation or finite access was changed.
This is a local recoverable checkpoint; final cumulative independent review is pending.


### Optional Parts correction and equivalence checkpoint

The first migrated run returned CTest 0 but its QtTest report contained one failed
assertion; optional-first-fail.txt and optional-migrated-results.json are retained
as NOT accepted. Investigation found that Preferences publication replaces the
article_tabs_ alias (PublishMaintainedFacadeCommit), while the migrated test had
captured the old named widget. Prepared tabs have no such object name. Diagnostic
observations showed preference/session/layout/central state correct; dereferencing
a fresh named lookup then crashed because it was null (optional-diagnostic-fail.txt,
owned CTest log). These are migration-test errors, not product RED evidence.

The corrected test uses ArticleTabsVisible(const MainWindow&) in the existing
private test access header. It observes only current visibility, preserving the
original member assertion across real replacement without changing product names,
lifecycles or exposing state. Dialog executor still only controls interaction;
real Preferences persistence/publication remains installed after the error stage.
Both new runners retain qExec's return independently of QApplication::exec so a
shutdown exit cannot mask a QtTest failure. Existing W3.6 runner uses the inherited
exit pattern; its passing report is explicitly checked, and no unrelated runner is
changed. External verification now preserves and checks fresh QtTest reports,
in addition to exact CTest execution and exit status.

optional-corrected-results.json records 8/8 actual cases, including both new
families, History/Articles/DictionaryContext/Synonym/ViewMenu and architecture
fixtures. The two-process DictionaryContext entry and its 40-second timeout are
unchanged. optional-corrected-build.log exits 0. Final cumulative review remains
pending; these results supersede neither the retained failure evidence nor history.
