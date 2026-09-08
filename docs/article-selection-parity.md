# Select Current Article: R8.1/R9.8

## Authority and impact check

Classification: minor conformance correction under CRD-SHELL-003 and
CRD-LOOKUP-003. Base: `c0f9e17a4b042bc3ff1202f97b2d92c827f04ca8`.
The approved default-to-Qt-5 rule authorizes this missing ordinary article
action; no IG-02/IG-03 exception applies. Impact check: Ready.

Frozen evidence at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`:
`articleview.cc:340-344,732-786,1923-1932` supplies the action, Ctrl+Shift+A,
current-dictionary lookup and unselected-context placement before Select All.
`article_maker.cc:183-194,637-638,683-689` initializes the first dictionary,
updates it on click/context menu, and selects the complete dictionary container
with a DOM Range, including its heading and all entries. Dictionary navigation
also makes its target current. A page without an article has no valid target.

## Bounded implementation plan

- Reuse the clean, sequential delivery worktree
  `worktrees/test-qt6-inspector-focus-parity` on the new dedicated branch
  `fix/qt6-select-current-article`. Prior deliveries are integrated; there is
  one writer and no concurrent development in this worktree.
- Core continues to assemble inert HTML. Preserve escaped dictionary identity
  as section metadata; no GUI dependency, active dictionary script or public
  ABI is added to Core. Existing lookup responses group entries by dictionary.
- The private ArticleView adapter owns QAction/shortcut, isolated-world DOM
  current-section state, pointer tracking and selection. Reuse its existing
  command and WebEngine adapter patterns; no new service or factory is needed.
- Select the current dictionary's contiguous result sections (including all
  entries), not the whole page. First result is the default; click, right-click
  and result navigation change the current target. Blank/error pages are no-op.
- Replace an existing selection on shortcut; hide the action from selected-text
  context menus as Qt 5 does. Keep tab isolation, page replacement, popup
  cancellation and stale-context safety. Do not change settings or clipboard
  formats, loosen CSP, or execute dictionary-provided code.
- Verify Core escaping and composed markup, actual Widgets shortcut/menu and
  selected text across multiple results, native paired menu captures, page/tab
  lifecycle, cumulative CTest and script regressions. Run all Qt 6 binaries
  through this checkout's Conan launcher. Linux execution is separately owned;
  shared implementation must remain platform-neutral.
- Stage the complete functional unit for a fresh independent completion audit,
  commit/push it, then verify and independently audit the fast-forward candidate
  in the dedicated integration worktree before advancing the authorized target.

This unit does not declare R8.1/R9.8, real-corpus visual acceptance, packaging,
Linux acceptance or the overall baseline cutover complete.

## Acceptance evidence and remaining gaps

The Qt 5 probe uses the frozen ArticleView and the actual JavaScript emitted
by `ArticleMaker::makeEmptyPage()`. A deterministic two-dictionary body follows
the frozen container/click conventions. Its active-change callback is an inert
fixture stub; selection, keyboard dispatch and navigation run the real product
implementations. It confirms first-dictionary selection (all entries), click
and right-click selection of the second dictionary, and navigation back to the
first. It records text, platform, shortcut, assertions and unmasked screenshots.

Qt 6 tests compose their fixture through the public DesktopFacade and Core
assembler. They assert selected text and real DOM range boundaries, not only
action availability. The private application world bypasses no content CSP:
the assembled `script-src 'none'` stays intact and page-world code cannot read
the selection controller. The controller does not execute dictionary markup.

Final native evidence is workspace-relative
`evidence/article-selection-final/{qt5,qt6}/`; the external build and isolated
runtime drivers are in `evidence/article-selection-tools/`. The committed
`apps/goldendict/tests/qt5_article_selection_probe.inc` is the only replacement
entry-point input; frozen product sources and objects remain unchanged. Build
the probe with `prepare.py` and the generated `capture.mk` against the prepared
`qt5-acceptance-build-r36-unit4-v2` objects. Run `run.py qt5 <checkout> <output>`
and, through the owning Conan launcher, `run.py qt6 <checkout> <output>`.

Final verification on 2026-09-08: Release build passes; serial cumulative CTest
passes 139/139 in 62.36 seconds; Python script regressions pass 171 tests with
two platform skips. Native Windows Qt 6 passes 12 checks with no skips in
26.262 seconds, including actual `apple` and `book` lookups in the user-owned
`D:/workspace/goldendict/content/CALD3` corpus, using fresh temporary indexes.
Set `GOLDENDICT_SELECTION_CORPUS` to that directory for reproduction. The
corpus rows are only registered when that opt-in variable is present; ordinary
CTest contains the deterministic synthetic row instead. No private dictionary
text or real-content screenshots are committed. The frozen Qt 5 probe's
assertions pass; its final executable SHA-256 is
`2befbe0ce0abbc11f6a609283902319cbdaafb65fcabaf374fb35ee79478a638`.

The initial parallel focused CTest invocation reported ProcessNotStarted for
the new executable. Direct Conan-launched execution and the serial cumulative
run passed without production changes. The cause is not established; this is
a retained harness observation, not a claimed product fix. Use fresh short
system-TEMP roots for cumulative smokes, as documented by preceding deliveries.

Remaining separately tracked R9.8 differences visible in these captures:

- Qt 6 repeats the dictionary heading for multiple entries from one dictionary;
  Qt 5 has one enclosing heading. Selection includes the actual displayed
  heading(s) and all entries; this unit does not redesign article composition.
- Select All already lacks the Qt 5 Ctrl+A menu label in Qt 6. Menu width and
  shortcut-column spacing also differ. This is an ordinary-menu visual gap,
  not covered by the inspector-only IG-02 exception.
- Synthetic captures do not prove real-corpus appearance or full browser parity.
  Real-corpus selection is checked separately without committing private text.
