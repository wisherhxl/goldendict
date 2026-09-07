# Article Inspector Geometry: R8.2 Unit 2

## Authority and readiness

Classification: approved CRD correction IG-01 in the decision log of
[the product CRD](qt6-product-baseline-crd.md), under CRD-LOOKUP-003 and
CRD-SHELL-003. Base: `9cd55c6321e7f9a5a78e4a55cf1155aac3f556aa`.
Readiness: Ready, 2026-09-07, before production edits.

Frozen Qt 5 evidence at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`:
`articleinspector.cc:23-58` shares the first retained inspector's geometry;
`articlewebview.cc:49` calls `beforeClosed` only on article-view destruction,
not on inspector close. Hidden inspectors remain in that list. Configuration
saving in `mainwindow.cc:1106-1126,1280-1283` precedes ordinary quit and tab
destruction. `config.cc:1127-1130,2131-2132` stores root `inspectorGeometry`
as Base64. Ordinary close/quit can therefore miss the latest geometry.

## Observable contract

- Retain one lazily owned inspector per article. On show, copy the first
  retained inspector's geometry (including hidden inspectors, as in Qt 5),
  or restore the shared saved geometry when none remains. Remove destroyed
  inspectors safely. Reopening is not a user geometry adjustment.
- Close checkpoints that inspector's geometry immediately. Normal exit
  checkpoints the latest adjusted geometry, including an inspector already
  closed or destroyed; construction, restoration, sharing and destruction
  must not supersede that adjustment. If none was adjusted, retain the last
  close checkpoint or initial configuration. No writes during movement.
- Use QWidget geometry serialization, including normal/maximized state and
  Qt's screen recovery. Invalid geometry falls back to the ordinary initial
  window geometry. Missing geometry preserves the default. No OS-specific
  geometry format, global window registry or disk access in Widgets.
- Restore the frozen QWebInspector initial logical size of 450 by 300,
  confirmed by the paired native capture, rather than the unloaded
  QWebEngineView's zero size hint and Windows' resulting 160 by 160 minimum.
- Current configuration stores bounded binary `inspector_geometry` (64 KiB)
  with the existing escaping; legacy import strictly decodes root Base64
  `inspectorGeometry`. Reject duplicates, malformed encodings and oversized
  values. Preserve unrelated fields. Existing atomic Core saves protect the
  previous file on failure; retain in-memory geometry for a later save retry.
- Linux/macOS use the same implementation. Windows evidence is required here;
  Linux execution remains independently owned, not presumed passed.

## Design and delivery boundary

One cohesive delivery: Core configuration/import, private shared presentation
state, close/exit composition callbacks, tests and directly related records.
The private concrete state object coordinates retained windows and the latest
adjustment without a new service layer. Qt signals notify the composition
root; Core alone serializes files. This follows existing full-text/main-window
geometry patterns and SOLID ownership. The transport-neutral CoreConfiguration
field changes the installed DTO layout (ABI); rebuild consumers with the exact
SCM/Conan package revision. No new exported module or GUI dependency in Core.
Facade candidate views must remain inert until activated and must share the
same application-lifetime state after activation.

Tools, isolated worktree, VS 2026/MSVC 14.44 and cached Conan Qt 6.11.1 are
available. No architecture, dependency, platform-scope or visual-frontend
decision is needed for this unit. Engine-owned visual acceptance remains
separate R9.8 work; this record does not declare complete inspector parity.

## Acceptance evidence plan

Core tests cover binary/current/legacy round trips, duplicate/corrupt/bounded
input, unknown-field preservation and failed-save preservation. Widgets tests
cover valid/invalid/offscreen restoration, close/reopen, retained-window
sharing, multiple windows, latest-adjustment exit precedence and teardown.
Composition tests cover the actual MainWindow wiring, configuration reload
and close/quit persistence. Native Windows checks exercise real geometry and
restart with an isolated profile and the Qt 5 bytes. Run Release build,
focused and cumulative CTest through `run_with_conan.ps1`, script regression
tests and independent staged delivery/integration audits before pushing.

## Windows verification (2026-09-07)

VS 2026/MSVC 14.44, Conan Qt 6.11.1 Release dependency resolution, complete
build and developer-profile install pass. This is not standalone-package
acceptance. The serial cumulative suite passes 138/138 in 50.33 seconds;
Python script tests pass 171 cases with two platform skips. The native Windows
geometry/import/Elements capture run passes seven cases including setup and
cleanup, with no skipped case. Native application close/exit/restart smoke
also passes. Every Qt 6 invocation used the checkout's Conan launcher.

The initial cold cumulative run passed 137/138: the unchanged external-program
test raised an uncaught exception. Its isolated six-case rerun and the entire
subsequent cumulative run pass without code changes. Cause is not established;
retain this non-reproducing harness observation rather than claiming a fix.
The geometry fallback probe initially compared a pre-show zero size hint with
Windows' 160 by 160 minimum. Paired Qt 5 measurement established 450 by 300;
implementation and default/fallback assertions now use that actual baseline.

Workspace-relative evidence (outside Git; no dictionary payload):

- `evidence/article-inspector-geometry-tools/`: frozen-product-object Qt 5
  capture entry-point generator, launch script and provenance;
- `evidence/article-inspector-geometry-v2/qt5/`: native screenshot, metadata
  including initial size, and actual `geometry.bin`;
- `evidence/article-inspector-geometry-final/qt6/`: native QTest log, restored
  geometry, Elements and Console screenshots and metadata;
- delivery `build/Release/Testing/Temporary/LastTest.log` and
  `build/Release/native/inspector-geometry-test-home/*/`: full-suite and
  application restart evidence.

The Qt 5 bytes import unchanged through Core; Qt 6 restores logical client
geometry `(100, 110, 610, 410)`. Native tests also verify latest-adjusted exit
precedence after both windows are destroyed, empty/invalid/offscreen fallback,
maximized state and no-adjustment exit. Engine toolbar/panels/language banner
still differ; geometry evidence does not approve those R9.8 differences or
declare complete R8.2 or baseline cutover. Linux execution remains pending.

## Integration verification correction: IG-01-V1

Classification: minor test correction under the same frontend-readiness
acceptance requirement; impact check Ready. The first integration native
capture passed every geometry case but observed an empty frontend body after
the URL/readyState checks. Those checks can observe the DevTools navigation
before its actual content is ready. Wait for the existing inspected-fixture
text readiness condition before asserting child content. No assertion is
removed, and no product code, requirement, geometry or architecture changes.
Rebuild and rerun focused/native capture and cumulative tests, then obtain a
fresh delivery audit before committing this independently verifiable test fix.

Correction verification: Release rebuild and 138/138 serial CTest pass (51.86
seconds); two consecutive native seven-case geometry/import/frontend capture
runs pass, with no skips. Evidence is retained under
`evidence/article-inspector-geometry-ready-fix/qt6/` and
`evidence/article-inspector-geometry-ready-fix-repeat/qt6/`.
