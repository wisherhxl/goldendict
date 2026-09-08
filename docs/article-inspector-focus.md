# Article Inspector Focus: R8.2/R9.8 Unit 3

## Authority and impact check

Classification: conformance evidence and regression-test correction under
CRD-LOOKUP-003, CRD-SHELL-003, R8.2 and R9.8, with the approved IG-02 visual
boundary. Base: `1a85fa7d01b237432453d4639858bca55930f252`.
Readiness: Ready. Continue the approved inspector acceptance work without a
new requirement or architecture decision. Do not presume that F12 activates
an inspector while a context menu is open; establish the frozen behavior first.

Frozen Qt 5 source: `articleview.cc:352-355` attaches Inspect/F12 to the actual
article widget; `1984-1990` reuses that action in the synchronous context menu;
`2562-2565` routes it to ArticleWebView. `articlewebview.cc:40-88` distinguishes
direct invocation from context-target inspection and explicitly shows, activates
and raises the inspector for direct invocation.

## Bounded delivery and verification plan

- Run the real frozen ArticleView/ArticleWebView/ArticleInspector objects with
  a local deterministic HTML fixture and isolated native Windows profile.
  Replace only the external capture entry point, never the frozen checkout.
- Record active-popup F12, Escape cancellation followed by article F12,
  repeated invocation and close/reopen focus/identity behavior. Retain native
  screenshots and machine-readable observations, including unexpected results.
- Add matching Qt 6 tests to the existing private Widgets test target. If a
  product difference is confirmed, repair only the existing presentation
  boundary; any newly required behavioral decision is raised separately.
- Preserve existing Core/persistence and DevTools security/lifetime ownership.
  No new service, abstraction, frontend fork or platform-specific product API.
- Run focused native/offscreen checks, Release build, cumulative tests and
  script regressions through the owning checkout's Conan launcher. Native
  activation evidence is Windows-only; Linux execution remains independently
  owned and must not be inferred from offscreen tests.
- Obtain independent staged delivery and integration audits before commit
  and target advancement. This unit does not close all R8.2/R9.8 states,
  standalone packaging, Linux acceptance or baseline cutover.

## Native Windows observations

Paired runs use Qt 5.15.19 and Qt 6.11.1, native Windows QPA, Fusion,
Segoe UI 9 pt, DPR 1, an 800x600 article and a 1000x700 inspector capture.
The Qt 5 probe constructs the actual frozen ArticleView, its actual Inspect
QAction and ArticleWebView; no replacement action or mock inspector is used.
QtTest's Qt 5 widget mouse injection does not itself generate the Windows
context-menu event, so the probe also sends the corresponding QContextMenuEvent
to the real article widget. This invokes its actual menu and event handling.

| Path | Qt 5 | Qt 6 | Disposition |
| --- | --- | --- | --- |
| F12 while the actual context menu is open | No trigger or inspector; menu remains open | Same | Conforming regression assertion |
| Escape, then F12 without manual focus repair | Article retains activation; inspector opens and activates | Same | Conforming regression assertion |
| Click article, then F12 with existing inspector | Same inspector activates | Same | Conforming regression assertion |
| Close inspector, click article, then F12 | Same retained inspector reopens and activates | Same | Conforming regression assertion |
| Reactivate article without a pointer event, then F12 | QAction triggers, but existing inspector does not activate | Same inspector activates | IG-03 pending; observation only, not accepted equivalence |

IG-03 requires separate direction because IG-02 approves appearance only.
The frozen ArticleWebView clears `showInspectorDirectly` on direct invocation
and restores it on mouse release/double-click, not on keyboard/window activation.
Do not add that event dependency to Qt 6 or declare its absence approved until
the decision is resolved. The test records this branch diagnostically and
asserts dispatch only; it does not prescribe either activation outcome.

The unmasked popup captures also expose a separate existing ordinary-menu gap:
Qt 5 offers `Select Current Article` for this synthetic context, while Qt 6
does not. This remains actionable R8.1/R9.8 follow-up, not an IG-02 exception
and not part of this focused shortcut/cancellation delivery.

The first Qt 5 probe lacked the native context-menu event and did not observe
a menu; its output is diagnostic only. The corrected matched runs exercise
the real menu. The first Qt 6 native invocations used stale test-discovery
metadata from an in-flight build and reported an unknown test function;
incremental regeneration and recompilation resolved that harness state.
Neither observation is recorded as product acceptance or a product fix.

## Verification and reproduction

Final verification on 2026-09-08: complete Release build and incremental test
rebuild pass; serial cumulative CTest passes 138/138 (63.63 seconds). Script
regressions pass 171 tests with two platform skips. Two native Qt 6 focused
runs pass the focus method plus initialization/cleanup, with no skips
(4.587 and 4.239 seconds). Repeated frozen Qt 5 observations agree on every
path in the table. No product code changed.

The first cumulative run had one process-start failure for the unchanged
`article_tabs_test` (BAD_COMMAND). Direct execution through Conan and the full
138-test rerun passed without source changes. The cause is not established;
this is a retained non-reproducing harness observation, not a claimed fix.
Both cumulative runs used fresh process-local TEMP/TMP roots to isolate prior
smoke state. GPU/context warnings accompany successful native assertions and
rendered captures; they are not missing-DLL failures.

Workspace-relative external evidence:

- `evidence/article-inspector-focus-tools/`: `prepare.py`, `capture.mk`,
  generated entry point, binary, `run.py` and SHA-256 provenance;
- `evidence/article-inspector-focus-final/{qt5,qt6}/`: unmasked `popup.png`,
  `inspector.png`, `result.json`, and the Qt 6 `test.log`;
- `evidence/article-inspector-focus-v3/qt5/` and
  `evidence/article-inspector-focus-v5/qt6/`: preceding matched observations;
- `evidence/article-inspector-focus-issues.md`: the IG-03 decision queue;
- delivery `build/Release/Testing/Temporary/LastTest.log`: final cumulative run.

The external Qt 5 build uses the previously prepared frozen acceptance source
and objects in `evidence/qt5-acceptance-{source,build}-r36-unit4-v2`. Generate
the entry point with `prepare.py`, then invoke `mingw32-make -f Makefile.Release
-f <focus-tools>/capture.mk capture` in that external build with the Qt 5 UCRT
toolchain on PATH. The probe is the committed `.inc` file, never a modification
of the frozen checkout. The resulting capture executable SHA-256 is
`ee9c204d3c94b5fd905c4f57cddf9e482a0b8740524e515b80ab31214557884a`.

From the delivery checkout, run native captures one at a time:

```powershell
python D:/workspace/goldendict/evidence/article-inspector-focus-tools/run.py qt5 <checkout> <fresh-qt5-output>
.\run_with_conan.ps1 --build-type Release -- python D:/workspace/goldendict/evidence/article-inspector-focus-tools/run.py qt6 <checkout> <fresh-qt6-output>
```

The external launcher isolates profile directories, clears remote debugging,
selects Windows QPA/Fusion/DPR 1, loads the prepared Qt 5 runtime for its probe,
and suppresses OS error dialogs without suppressing process failures. For
Qt 6 it inherits the owning checkout's Conan environment. Compare JSON fields
and inspect the screenshots; a probe exit alone does not establish parity.
