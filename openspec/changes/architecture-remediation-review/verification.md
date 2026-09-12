# W1 / A1 verification record

Scope: user-approved W1 only, 2026-09-12. A2–A6 remain unselected Draft work.
This is implementation evidence, not an independent review receipt.

## Identity and authority

- Worktree/source: `D:/workspace/goldendict/worktrees/feature-tiger-qt6-migration`.
- Branch: `feature/tiger-qt6-migration`.
- Starting HEAD and audit baseline:
  `3f3f2bf4c54eaf2c5d77c1159947212b1b43169c`; no intervening source changes.
- The pre-existing `draft-plan.md` is preserved byte-for-byte. No Qt5 edits,
  reset, cleanup, tool reconfiguration, merge or push is authorized by this unit.
- User-approved correction, proposal/design and the existing architecture's
  hidden preparation, visible commit and production caller contracts govern W1.
  Accepted ADRs and architecture rules have not been rewritten.
- Loaded routing: user/workspace/project AGENTS, global engineering-delivery
  and software-design policies, project agent-workflow, design rules,
  applicable architecture, migration/parity/porting, build/testing guidance,
  complete external audit and the existing Draft. `goldendict-candidate-v1`
  activation hashes were checked. The user's designated-worktree and no-push
  restrictions specialize normal delivery routing.
- OpenSpec is the sole change record. Native context/status/instructions were
  used; skip_specs records conformance, not approval of a new public contract.
  Serena was activated for this exact worktree, but symbol lookup returned no
  result. Textual impact analysis and compiled real paths are used; no complete
  semantic-analysis claim is made.

## Build and isolated runtime

Build: `build/Release`, Ninja, Release, MSVC, Qt 6.11.1 on Windows 11.
`CMAKE_HOME_DIRECTORY` points to the worktree above. Binaries are
`build/Release/bin/goldendict.exe`, `article_page_lifecycle_test.exe` and
`article_scheme_handler_test.exe`.

All executable commands use this checkout's `run_with_conan.ps1` and generated
`build/Release/generators` environment. The new QTest creates its own temporary
HOME, XDG config/cache, APPDATA, LOCALAPPDATA, configuration, TEMP and TMP before
QApplication; its fixture and index directory are temporary. It loads a local
generated HTML document through actual WebEngine navigation and uses real empty
Core facades. Existing smokes use their own isolated paths and checked-in Dictd
fixtures. The external regression runner additionally isolates Windows paths.
No daily dictionary/index/cache/configuration is an input or output.

External evidence root:
`D:/workspace/goldendict/evidence/a1-page-lifecycle-20260912`.
Raw logs are retained, including failed probes. They are local evidence, not a
claim of independent backup or cross-platform acceptance.

## Baseline and valid red/green

From the worktree, the baseline command was:

```powershell
.\run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release --target goldendict article_scheme_handler_test --parallel 4
```

`baseline-build.log`: exit 0. The real program and scheme-handler test built.
The exact baseline CTest selection is the eight-case prefix of the regression
selection below: scheme handler, search menu, WebEngine interaction, article
tabs, Widgets preparation, Preferences predecision, source directories, groups.
`baseline-tests-runtime.log`: exit 8, seven passed; source-directories crashed
with a WebEngine shared-context error. This is a baseline runtime failure, not
an A1 red assertion. A later isolated regression run passed that same smoke;
the original failure remains recorded and its precise cause is not established.

The first isolated launcher probe (`baseline-tests.log`, exit 1) selected the
WindowsApps Python alias after LOCALAPPDATA isolation. It reported a Python
install-manager update to 26.3 and then NoInstallsError, before any test ran.
No installer/update command was requested. Subsequent outer-isolated commands
prepend the already installed Python 3.14 runtime to process PATH to avoid the
alias; no tool files or configuration were changed deliberately.

Test instrumentation before repair comprised one friend declaration, a separate
real-production-source test target and its test file. No behavior-preserving
production extraction was needed. This is a test-instrumented baseline, not an
unchanged checkout. `red-test.cpp`, `red-tracked.diff` and `red-hashes.txt` freeze
the red inputs. The initial zoom assertion was separated into its own case to
allow normal-refresh controls to execute; it was retained as a regression.

```powershell
.\run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release --target article_page_lifecycle_test --parallel 4
.\run_with_conan.ps1 --build-type Release -- cmake -E env QT_QPA_PLATFORM=windows 'QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu' build/Release/bin/article_page_lifecycle_test.exe -o 'D:/workspace/goldendict/evidence/a1-page-lifecycle-20260912/red-control-qtest.txt,txt'
```

- Red build (`red-control-build.log`): exit 0.
- Valid red (`red-control-qtest.txt`): exit 7; normal sequential and overlapping
  refresh both PASS. Prepared sequential, prepared overlapping and twice-prepared
  overlapping all observe a real loadStarted and fail `start_marked`. Additional
  failures expose initial zoom reset, abandoned-successor suppression, background
  completion and stale-view erasure. Totals include init/cleanup: 4 pass, 7 fail.
- Same-assertion green (`green-build.log`, `green-same-qtest.txt`): build exit 0,
  native QTest exit 0, 11 pass, zero fail/skip. Same command with the green log
  name; no A1 assertion was relaxed. Missing launch/output probes in red-tests.log
  and the earlier red-native-qtest.txt are not substituted for this control.

## Acceptance mapping

| Requirement | Real path / evidence |
| --- | --- |
| Normal creation and actual reconstruction | QTest `refreshUsesRealBindings` calls SetFacade/SyncArticleTabs/CreateArticleView and actual Prepare/Begin/Publish/Finish, then the real reload QAction and WebEngine load signals. |
| Preferences entry | main.cpp SetPreferencesApplyCallback → coordinator.Execute (1643) → coordinator Widgets prepare/begin/publish/finish (132/160/250/273); articles-preferences and Preferences-predecision smokes. |
| Local/online/program source entries | main.cpp apply_source_directories (1440) → apply_sources (1320), coordinator.Execute (1396); source-directories smoke checks rejection and successful complete boundary sequences. |
| Groups entry | main.cpp group callback → coordinator.Execute (1528), same Widgets methods; dictionary-groups smoke. Names alone are not coverage: these exact production callers converge before the shared builder. |
| Sequential, overlap/cancel, cleanup and later refresh | Five refresh rows; first real start optionally requests another reload and stops the current WebEngine load. Existing merge policy starts the pending generation once; both completions drain and another reload starts. |
| Multiple rebuilds / no duplicate start wiring | Twice-prepared row observes exactly one navigation-generation increment per real start, expected start count and final reload generation. |
| Hidden / abandoned / publication-crossing | `hiddenAndAbandonedCandidateDoNotStealActiveEvents`, `candidateEventsStraddlingPublication`; signals traverse actual preinstalled connections. No private in-flight state is fabricated. |
| Old page callback | `oldViewCompletionCannotEraseSuccessorReload` keeps an old QObject alive while SyncArticleTabs recreates the authoritative view; actual old signal connections cannot erase the new real reload. This is controlled late delivery, not a claim of reproducing a naturally timed GUI race. |
| Replacement during an in-flight reload | `replacementRetiresOldInFlightReload` requests a real reload, publishes the replacement without an event-loop turn, then verifies the successor starts its own generation and drains. Synchronization retires state belonging to a displaced view. |
| Valid background tab | `backgroundTabCompletesRefresh` switches through production CreateEmptyArticleTab after requesting reload and observes the prior valid tab completing. |
| Zoom initialization | `initialPreferencesSurvivePageInstallation` checks 1.25 after ordinary real page installation and after publication. |
| Search and F3 | `searchAndF3Bindings` normal/prepared rows search the real document, retain F3 → Dictionaries with cancellation via the existing executor, observe reload search completion and one search-generation increment despite an extra terminal signal. |
| Scroll and click settings | `scrollAndClickBindings` normal/prepared rows observe actual DOM/Qt page scrolling and owner epoch advancement, then actual single-click word-query selection, disabled double-click lookup and bound SelectionToInput delivery. Existing article-click restart smoke additionally exercises settings persistence and enabled/disabled cases. |

## Regression command and interim evidence

The durable `run-regressions.ps1` records process-only isolation and executes:

```powershell
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R '^(article_page_lifecycle_test|article_scheme_handler_test|goldendict_(search_menu|webengine_interaction|article_tabs|widgets_facade_preparation|preferences_coordinator_predecision|source_directories|dictionary_groups|articles_preferences|article_click_preferences|view_preferences_restart|configuration_reload_coordinator)_smoke)$' --output-on-failure
```

`regression-tests.log`: exit 0, 13 actual tests passed. This predates the final
supplementary interaction cases and cannot stand alone as final candidate proof.

## Final verification and test-development limitations

- Final production build: `final-build.log`, exit 0 (includes the retained
  MainWindow connection context); later test-only builds end with
  `separate-interactions-build.log`, exit 0. Production sources were unchanged
  between these and the final matrix.
- Final native command is the red command above with
  `final-matrix-qtest.txt,txt`: exit 0, 16 pass (14 scenario rows plus
  init/cleanup), no failures/skips, 5.45 seconds.
- Final isolated CTest command is the selection above:
  `final-regression-tests.log`, exit 0, 13/13 tests, 19.15 seconds.
  `final-ctest-qtest.txt` preserves the complete new target's detailed result.
- `openspec context --json` resolved the exact worktree;
  `openspec validate architecture-remediation-review --strict --json` returned
  valid, no errors. `git diff --check` returned 0. No dependencies, installation
  or package outputs changed, so install/package verification is not applicable.
- Existing Draft SHA-256 remains
  `af3b3525ecaac0df0fe7f21ba4ec071a8300fff47312b4d774818c05ddddea6e`.

Supplementary test development is not red evidence: an initial combined test
incorrectly assumed F3 was find-next and entered a modal source dialog; that
owned process was stopped (exit -1, green-matrix-qtest.txt). The corrected test
observed F3 cancellation. A native exposure precondition failed once, and an
assumed first search position was invalid: WebEngine can restore either of the
two valid match positions. Those probes are retained in final-native*.txt.

The combined search-then-scroll probe failed while DOM scrollY was itself zero,
with visible pages and a 4058/4059-pixel document (`scroll-diagnostic-qtest.txt`).
It does not demonstrate a dropped Qt binding. Search and scroll/click checks now
use independent fresh real pages, with the same positive scrolling/signal/epoch
assertions. Both rows passed separately and in the complete native and CTest
matrix. The exact ordering of arbitrary scroll requests immediately after
asynchronous find/reload is not established here and is not represented as a
passed combined-timing contract.

No new Qt5 run, Linux/macOS runtime, full repository suite, physical audio or
printer output was verified. The existing non-article relay's latest-preparation
gate is unchanged; W1 corrects only article event validity, not all relay users.
A2–A6 remain Draft, including the transaction token/allocation issue.

Self-review added the in-flight-retirement case after the initial full matrix.
`retirement-red-qtest.txt` records a real assertion failure (exit 1) on the
partially repaired version: the successor retained the old in-flight generation.
This is supplementary red evidence, not the original baseline. The correction
is confined to existing SyncArticleTabs, outside hidden preparation and the
irreversible publication function. Subsequent complete results are bound in the
external candidate run record; earlier final-* files retain their actual inputs.
The final retirement-inclusive build is `retirement-green-build.log` (exit 0).
`candidate-native-qtest.txt` is exit 0, 17 pass (15 scenario rows plus init/cleanup),
zero fail/skip; `candidate-regression-tests.log` is exit 0, 13/13 CTests.
`candidate-ctest-qtest.txt` and `candidate-hashes.json` retain detailed output and
the exact source/binary hashes for this final implementation.

The local candidate identity, final binary/source hashes and independent review
receipt are stored outside the candidate under the evidence root. A local
checkpoint is not acceptance or publication; no independent Pass is asserted
by this developer-authored record.
