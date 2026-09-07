# Article Inspector Parity

## R8.2 Unit 1: Inspect entry points and private DevTools lifecycle

Classification: conformance delivery under `CRD-LOOKUP-003`, `CRD-SHELL-003`
and R8.2 of [the gap inventory](qt6-baseline-gap-audit.md). Approved Qt 5
behavior is the authority; no new feature or supported-scope decision is made.
Base Commit ID: `ad6f0190cfc29a2e1dcca41d104c1e51d0bbfca9`.

Frozen evidence at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`:

- `articleview.cc:352-355,1984-1985,2564-2567` attaches the `Inspect` action
  and F12 shortcut to the article view and places Inspect after dictionary
  references at the end of its context menu.
- `articlewebview.cc:25-64,70-88` lazily owns one inspector per article view,
  shows/raises it for keyboard invocation, and inspects the context-menu
  target for pointer invocation. Closing/reopening reuses the inspector;
  destroying the article view destroys its inspector.
- `articleinspector.cc` additionally defines shared/persisted geometry. That
  remains the explicit Unit 2 obligation, not a completed part of Unit 1.

Unit 1 restores both real entry points, one reusable nonmodal inspector per
article view, independent inspectors for separate tabs, safe page replacement
and destruction, and usable built-in Elements/Console tools. It preserves the
existing context actions and dictionary-reference order. Native DevTools is
the already mapped WebKit-to-WebEngine replacement in `porting-map.md`, not a
copy of obsolete WebKit frontend code. Its engine-specific frontend appearance
must remain visible in comparison evidence; full visual acceptance is not
declared by this entry-point/lifecycle unit.

## Readiness and design

Result: Ready. R2.3 is integrated. Windows Qt 5/Qt 6 runtime tools and native
capture instrumentation are available; Linux environment execution is owned
separately, per the approved platform assignment. Shared implementation uses
Qt Widgets/WebEngine APIs only, with no Windows-only API or shell dependency.

`ArticleView` translates keyboard/context-menu intent. A private
`ArticleInspector` owns the top-level window, dedicated off-the-record profile
and DevTools page; it detaches the inspected page before destroying its own
page/profile. The article owner closes the inspector before page replacement
or view destruction. This follows the existing private presentation-adapter
boundary and uses RAII/Qt lifetime connections rather than a new service or
public interface. Core, persistence, dictionary parsing and network policy are
unchanged. There is no remote-debugging port, global developer switch, article
JavaScript-policy relaxation or inspector creation from untrusted markup.

The subsequent [Unit 2 record](article-inspector-geometry.md) restores geometry
sharing and current/legacy persistence through Core-owned state and
composition-root callbacks, with the explicitly approved IG-01 save-timing
correction. Full R8.2 and R9.8
remain open until their complete evidence is accepted; no geometry requirement
or remaining legacy capability is excluded by this split.

The context menu reuses the article's QAction, matching the frozen action
ownership. A probe sending F12 directly to an already-open QMenu did not
activate Qt's WindowShortcut on this host. It is not evidence that Qt 5
supports that combined state, and no custom shortcut override is introduced.
R9.8 retains matched popup-focus/shortcut behavior verification; Unit 1 proves
F12 from the article window and activation of the actual context-menu item
separately.

## Verification plan

- Focused QTest coverage of real F12/action metadata and routing, context-menu
  tail order, lazy creation/reuse, multi-view isolation, close/reopen, page
  replacement and teardown, off-the-record profile ownership, and actual
  DevTools frontend/inspected-document readiness.
- Preserve and run existing article context-menu, tab, WebEngine, facade and
  configuration transaction regression coverage.
- Run Release configure/build, serial cumulative CTest, script tests, install
  and matched native Windows Qt 5/Qt 6 inspector evidence, with recorded
  dimensions/style/DPI and explicit remaining differences.
- All Qt 6 build-tree executions use the checkout's `run_with_conan.ps1`;
  isolated fixtures/profiles contain no private dictionary payload. Linux
  build/runtime evidence remains pending, not inferred from Windows results.
- Freeze the staged functional unit for independent completion audit, commit
  and push after Pass, then independently audit its isolated integration.

## Windows implementation evidence (2026-09-07)

Release dependency resolution, configure/build and developer-profile install
pass with VS 2026/MSVC 14.44 and Conan Qt 6.11.1. This is not a claim of a
self-contained runtime package. The cumulative CTest suite passes 137/137
(49.79 seconds); the final targeted rerun passes 6/6. Script tests pass 171
cases with two platform skips. The native Windows QTest run passes all ten
cases, including initialization/cleanup. The earlier offscreen focused suite
also passed three consecutive runs. Conan launchers were used throughout;
offscreen GPU/font warnings are retained in logs, not missing-DLL failures.

The tests prove F12 dispatch to the visible tab, lazy independent window and
profile ownership, close/reuse, safe page replacement/destruction, the actual
menu's tail order, a Console `$0.id` result of `inspection-target` after right
click, and suppression after page replacement or a newly requested document.
The native capture waits for both fixture DOM and `element.style`, then proves
Console evaluation returns `console-check:GoldenDict inspector fixture`.

Workspace-relative external evidence:

- `evidence/article-inspector-v2/qt5/{article,inspector}.png` and metadata;
- `evidence/article-inspector-final-ready/qt6/{article,inspector,console}.png`,
  metadata and `test.log`;
- `evidence/article-inspector-tools/prepare_qt5_capture.py`, `run_capture.py`,
  `compare_captures.py` and `provenance.json` for reproduction;
- `evidence/article-inspector-final-ready/comparison.json` and unmasked
  difference images. Comparison SHA-256:
  `a080ee3f09145727622618ffc60a511b573e47d3d4f355236199c19d62d912ce`.

Qt 5 capture links unchanged frozen product objects with an external
capture-only replacement main entry point; the frozen checkout is untouched.
Both products use the same deterministic local HTML, native Windows QPA,
Fusion, Segoe UI 9 pt and DPR 1. Article dimensions are 800 by 600; inspector
dimensions are 1000 by 700. Both show the real document tree and remain
independent of the article window. Qt 5 is 5.15.19; Qt 6 is 6.11.1.

| Surface | Observed result | Remaining difference |
| --- | --- | --- |
| Fixture article | Same heading/body content; 4,780 of 480,000 pixels differ | Engine text rasterization remains visible; not normalized away |
| Inspector Elements | Same fixture DOM is inspectable; native Qt 6 style sidebar is usable | WebKit/WebEngine toolbar, panels, colors, title/URL presentation and language prompt differ; 700,000 of 700,000 pixels differ without masks, including near-white background differences |
| Qt 6 Console | Actual keyboard entry evaluates against the inspected document; returned marker is captured | Complete matched Console/popup-focus/media/error-state visual matrix remains R9.8 |

These are entry-point/lifecycle conformance results, not a visual-equivalence,
complete R8.2, Linux acceptance, or baseline-cutover declaration. No obsolete
frontend is copied or intentionally excluded. Unit 2 geometry and the full
R9.8 engine/frontend comparison remain required follow-up work.
