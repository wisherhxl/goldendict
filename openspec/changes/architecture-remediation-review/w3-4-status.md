# W3.4 ArticlesPreferences migration

## Authority, baseline and locked scope

The approved W3.4 bounded migration continues W3/A4, not W4/W5/W6. Base:
828d1f3a9c07a9207a1796a42cdfb0ac26ab7713, initially clean. Worktree:
D:/workspace/goldendict/worktrees/feature-tiger-qt6-migration. Activated
goldendict-candidate-v1 policy hashes match. No merge or push is authorized.
W3.3's exact-candidate independent Pass and P1's accepted Windows / Qt 6.11.1
observation limits remain unchanged. Both Release and W3-off builds reference
this source tree, with BUILD_TESTS ON/OFF respectively.

Exactly one family is locked after dependency preflight and old-scene execution:
ArticlesPreferencesSmoke. Its old RunArticlesPreferencesSmokeCheck, internal
--articles-preferences-smoke dispatch and goldendict_articles_preferences_smoke
CTest form one complete scene. No replacement or additional family is selected.

HistoryPreferences and FavoritesPreferences were inspected but excluded: in
addition to reusable Preferences they exercise main-owned history import/lookup
recording and favorites mutation/persistence callbacks. Those assembly dependencies
remain; this batch does not extract them. Other inventory entries are unchanged.

## Preflight and minimal boundary

The Articles scene has three synchronous GUI-thread stages: dialog inspection and
cancel; forced apply-error with validation display and cancel; restoration of the
real apply callback followed by successful save. Main installs that callback with
InstallPreferencesApplication. EditPreferences constructs the real dialog, invokes
the existing dialog executor (otherwise exec), and clears its busy/action guard.
The executor controls only interaction, not persistence or transaction algorithms.

Reuse preferences_application unchanged. Caller-owned configuration/history/facade,
activation owner, runtime, credentials, diagnostics and coordinator retain their
identities and lifetimes, as in W3.3. The returned callable restores the real
callback after the forced error. Borrowed objects outlive all synchronous calls;
shutdown retains coordinator/detach/snapshot/owner/runtime ordering.

One test-only private friend provides a setter for the existing dialog executor.
This is needed to preserve the original synchronous dialog interaction boundary;
public widget discovery cannot install that executor. It exposes neither mutable
configuration nor arbitrary MainWindow state. The existing copy-only Preferences
observer is reused. Actions use QObject names; current tabs use the presentation
host after transaction replacement. Existing executor storage remains because
other unmigrated scenes still use it; no scenario state is added to production.

The runner reuses the W3 production source/resource/MOC closure, actual facade
preparation, coordinator and installer. Its owned fixture is serialized and loaded
through Core, with explicit index/Network/WebEngine/temp roots. No dictionary is
required by this scene: the original contract uses the initial empty tab session.

## Evidence and execution

External evidence: D:/workspace/goldendict/evidence/a4-test-extraction-w3-4-20260919.
Baseline build: Conan launcher Release goldendict/view_menu_test, exit 0.
run-isolated.ps1 -Phase baseline -Pattern '^goldendict_articles_preferences_smoke$':
fresh Core SaveConfiguration/LoadConfiguration fixture, actual old case 1/1 passed,
CTest exit 0. No artificial defect red phase or reused run data.

Implementation, equivalence, guard, cumulative regressions, ON/OFF builds, ordinary
OFF portable startup and fresh independent completion review are pending.

## Acceptance mapping

Preserve all original Articles group/control labels, tooltips, limits (1..100000),
step 50, initial values/enabling and absent displayStyle assertions. Cancel must
leave preferences/session unchanged. Forced save failure must display the actual
validation error and leave preferences/session unchanged. Restore the shared real
callback; save collapse=true, ignore=true, limit=3450; require accepted dialog,
unchanged session/layout, visible active tabs and non-null central widget. Preserve
the main-dispatch LoadConfiguration assertions. Add changed current facade equal
to owner snapshot and disk/config/window equality as real-publication evidence.

Keep CTest name, app-build working directory, offscreen/x11/Chromium flags and
20-second timeout. QtTest owns assertions/failure exit; it replaces the old queued
smoke dispatch/watchdog without combining scenarios. Extend the existing cumulative
guard with runner/access names and all four negative membership modes.

## Implemented mapping and checkpoint

The new slot is ArticlesPreferencesTest::articlesPreferencesThroughRealApplication.
All interaction lambdas and accumulated assertions moved out of MainWindow. Only
ArticlesPreferencesTestAccess is newly friended; its one setter replaces the
already existing synchronous dialog executor. ViewMenuTestAccess::Preferences is
reused for copy-only observations. No shared production assembly was expanded.

| Original stage at base main_window.cpp:3605-3723 | New runner evidence |
| --- | --- |
| Preconditions: Preferences action and live facade | Named real QAction plus activated production facade |
| Articles group/title, collapse/ignore labels and tooltips, initial values | Same dialog child lookups and comparisons |
| Limit 1..100000, step 50, initial enablement; symbols label; no displayStyle | Same control assertions |
| Set collapse/ignore/3450 then cancel | Same real dialog rejection; preferences/session unchanged; added same-facade check |
| Forced apply failure | Existing callback setter installs exact error substitute; real dialog OK shows validation, stays unaccepted, then rejects |
| Failed-save state preservation | Same preferences/session assertions; added same-facade check |
| Restore real callback, accept collapse/ignore/3450 | Callable returned by unchanged InstallPreferencesApplication; real coordinator persists and publishes |
| Session/layout/central widget/visible article tabs | Same observations; active host follows actual replaced tabs rather than stale pointer |
| main.cpp persisted preference checks | Actual LoadConfiguration requires all three original values; additionally disk/config/window match and published facade equals owner current snapshot |

No old RunArticlesPreferencesSmokeCheck call remains. The declaration, definition,
smoke recognition and scheduling/exit/persistence-assertion dispatch are removed.
No dedicated field existed for this family. The dialog executor field remains for
other inventoried scenes; removing it would exceed scope. No production runtime
branch or alternative implementation is added. Configuration/session/facade owners,
GUI thread, callback replacement, preparation/publication and shutdown stay intact.

First migration build and three targeted cases passed. Removing unused includes
then exposed a required ConfigurationLocations definition: ON build exit 2 in
on-build.log. Restoring legacy_configuration_location.h fixed the test compilation;
on-build-corrected.log exit 0. This is a test build failure, not a product red phase.
Original successful target evidence and the failure are retained separately.

## Final developer verification

- verify-cumulative.ps1 -RunName candidate1: 28/28 actual CTest cases, exit 0,
  each with a fresh owned serialized profile. Includes the new scene, W3.1/W3.2/
  W3.3, W1, W2, P1, coordinator/predecision/restart/configuration regressions.
- Articles QtTest: init/actual scene/cleanup 3 passed, zero failures or skips.
  ArticleInspector retains only its approved Qt5 NativeLegacyGeometryImport skip;
  no new key skip or zero-match result is accepted.
- ON and OFF production builds exit 0 in independent existing directories. New
  runner compiles the actual presentation closure/resources/MOC and shared private
  Preferences source. verify-isolation.ps1 checks actual Ninja command graphs and
  compiled main_window object symbols for both binaries; all four migrated
  families are excluded. OFF target inventory omits migrated targets.
- Cumulative guard: seven clean fixtures accepted, 28 deliberately invalid
  source/interface-source/dependency/genex memberships rejected. These are expected
  negative outcomes, not 28 product-test passes. No legacy exemption was added.
- Ordinary OFF executable-only portable startup: real main window and rendered
  WebEngine Welcome document, fixture dictionary status, Ctrl+Q normal exit 0,
  owned adjacent sentinel intact. ordinary-off/result.json records 78 owned-root
  notifications (not a pass threshold); ui-main-window.txt records actual UI.
  loaded-qt.json confirms unchanged Qt 6.11.1 Core/WebEngine DLL paths and hashes.
- Runtime roots: ordinary-off/bin/portable for configuration/history/favorites/
  recovery, portable/cache/qt-network-http and portable/webengine/article, owned
  indexes/tmp/working. No smoke flag or Network/WebEngine bypass. Reused P1 scripts
  unchanged, preserving its bounded observation rather than asserting all-process
  filesystem isolation or nonportable/other-platform validation.
- OpenSpec strict validation and diff whitespace check pass. No transaction, Core,
  Network, W1/P1 production behavior, installed public interface or Accepted ADR
  change. The only retained shared test boundary is existing executor storage.

This candidate implements one complete migrated family; 49 MainWindow Run*Check
methods remain as a navigation count. Final checkpoint identity, cumulative diff,
artifact hashes and independent verdict are recorded outside the frozen candidate.
Independent completion acceptance is pending that fresh exact-candidate receipt;
self-check and earlier W3.3 review do not replace it. No merge/push, second family,
W4 or A4-wide closure. Existing Windows/Qt, Qt5 geometry and Chromium observation
limits remain unchanged.
