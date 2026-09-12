# W3.1 verification and assertion mapping

## Identity, authority and rules

Base: `feba6359e2f630cffde9c02a1e20d0765e163485`; branch
`feature/tiger-qt6-migration`; sole writer/worktree:
`D:/workspace/goldendict/worktrees/feature-tiger-qt6-migration`.
The initial worktree was clean. Fetch succeeded; the local branch was two commits
ahead, not behind. Those accepted W1/W2 commits were preserved. No Qt5 edits,
reset, new worktree, tool installation, merge or push occurred.

Read global AGENTS and engineering-delivery/software-design policies; workspace
AGENTS; project AGENTS, agent-workflow, project-design-rules, architecture/build/
testing guidance; the existing Draft, W1/W2 closeout/verification/review evidence;
and native OpenSpec context/apply instructions plus proposal/design/tasks.
The activated goldendict-candidate-v1 selector's four hashes match
`activation-check.json`. The user's designated sequential worktree and prohibition
on merge/push control over default worktree/publication guidance. Existing
conformance requirements govern this migration; no Accepted ADR or public
requirement changes. Serena symbol lookup was unavailable for the selected method;
explicit source/caller inventory plus actual compilation/runtime evidence is used,
not a claim of complete semantic indexing.

W1/W2 historical files and original Draft remain unchanged. W2's independent
receipt remains in `evidence/a5-publication-preallocation-20260912`; its SHA-256 is
`a2caa88cdc081f888ee7d156ead76aaea903afa20bd828118bd2e5998860fb91`.
The supplementary Network test is validation only, not a W2 production repair.

## Original entry and exact behavior mapping

Original MainWindow method: RunFullTextDictionaryProjectionSmokeCheck, base lines
10615–10677. The source-derived inventory preserves its complete body and callers.
Original command: goldendict --full-text-dictionary-projection-smoke
--dictionary-root apps/goldendict/tests/fixtures/dictd. New command is the
full_text_dictionary_scope_test QTest runner; CTest name
`goldendict_full_text_dictionary_projection_smoke` is unchanged.

| Original step / assertion | New projectionThroughRealWindow evidence |
| --- | --- |
| Real initialized facade, toolbar, show and processEvents | Actual runtime composition, activation owner PrepareCandidate/Activate, MainWindow SetPreferences/SetDictionaryGroups/SetFacade/show, real toolbar/host discovery and event processing |
| Collect supported catalog IDs; fail if empty | Same catalog iteration and nonempty QVERIFY; real Dictd fixture, original enabled external and disabled network source configurations |
| SelectGroup(0), RefreshDictionaryBar, show bar | Narrow TestAccess SelectGroup(0, true) calls both real operations; real show/events |
| all.filter_active and all.ids == supported | Two explicit unchanged-value assertions |
| Find supported QAction; fail if missing; trigger | Actual host ActiveActions, data identity and QAction trigger; QVERIFY for missing action |
| unchecked.filter_active and first supported ID absent | Same active flag and std::find exclusion assertions |
| Hide bar, process events, hidden.ids == supported | Same real visibility transition and equality |
| Set group 7 with supported + unresolved ID and supported muted; SelectGroup(7) | Same group DTO, public SetDictionaryGroups and real private selection; no replacement projection algorithm |
| muted.filter_active and empty IDs | Same two assertions |
| requests.size unchanged; composer not visible | Read-only request count boundary and real composer isVisible assertion |

The original missing-facade/toolbar/catalog/action failures are test precondition
failures, retained as explicit QVERIFY checks; they were not separate injected
product-error scenarios. There was no exception injector, asynchronous fault
schedule or persistent scenario field in this family. No hard branch is skipped.
The original compound bool is split for diagnosability, not weakened. One complete
sequential scenario remains one QTest case (plus init/cleanup), not four fake tests.
All processEvents calls stay in the test; normal GUI/QObject lifetime remains real.
Window destruction precedes facade owner and Network runtime destruction.

Production deletion: one method definition (64 lines including separator), one
declaration, three main.cpp sites (classification, shared fixture predicate and
dispatch), and the old CTest executable/arguments. No lookup ownership, facade
transaction, W1 lifecycle, format or resource implementation changed. MainWindow
retains only a private friend declaration for three bounded operations implemented
in the test target. Shared fixture setup remains for other families.

## Build and run evidence

All paths below are relative to the Qt6 worktree unless absolute. Logs/scripts are
under `D:/workspace/goldendict/evidence/a4-test-extraction-w3-1-20260912`.
Commands launch through run_with_conan.ps1; the existing Release Conan generators
provide MSVC 14.44, Ninja, CMake and Qt 6.11.1 shared runtime. Both caches identify
this exact source directory and Release configuration:

- ON: build/Release, BUILD_TESTS=ON; binaries build/Release/bin/*.exe.
- OFF: build/W3-off, BUILD_TESTS=OFF; binary build/W3-off/bin/goldendict.exe.
  Configured with existing conan-release preset plus `-B build/W3-off`, quoted
  `-DCMAKE_TOOLCHAIN_FILE=D:/workspace/goldendict/worktrees/feature-tiger-qt6-migration/build/Release/generators/conan_toolchain.cmake`
  and `-DBUILD_TESTS=OFF`. No dependency resolution or tool reconfiguration.

| Evidence / command following launcher | Result |
| --- | --- |
| baseline-build-2.log: --with-build-environment -- cmake --build --preset conan-release --target goldendict full_text_query_composer_test article_page_lifecycle_test publication_preparation_test http_client_test --parallel 4 | Exit 0; real old program/tests built |
| run-baseline.ps1 / baseline-tests.log: five exact CTest names (old projection, composer, W1, W2, HTTP) | Exit 0, 5/5; original behavior baseline before removal |
| w2-supplement-build.log and w2-supplement.txt: http_client_test.exe SplitPublicationPreservesThreadCompletionAndNonReentry | Exit 0; one actual case plus init/cleanup, no skips |
| migration-build-3.log: real goldendict and full_text_dictionary_scope_test | Exit 0; actual presentation/resource/MOC sources reused |
| candidate-build.log: goldendict full_text_dictionary_scope_test http_client_test article_page_lifecycle_test publication_preparation_test | Exit 0 after final code formatting and guard location correction |
| off-core-build.log, off-build.log, off-final-build.log: cmake --build build/W3-off --target goldendict_core / goldendict --parallel 4 | Exit 0; complete separate ordinary production build, then final guard configure/build |
| run-regression-isolated.ps1 / final-regression.log | Ten exact CTest invocations, each one matching test, all exit 0 |
| run-off-startup.ps1 / off-startup.log | Exit 0; OFF goldendict --webengine-smoke --dictionary-root fixture, real window/page load and toPlainText marker assertion |
| full_text_scope_isolation_test | Clean configure exit 0; four actual violating target configurations exit 1 with W3.1 isolation violation; probe test exits 0 |

The ten final cases are projection, composer, article_page_lifecycle_test,
publication_preparation_test, http_client_test, full_text_scope_isolation_test,
full-text-dialog, dictionary-bar, configuration-reload-coordinator and Preferences
predecision. QTest records contain 17 W1 and 13 W2 passes, zero failures/skips;
projection has 3 passes including init/cleanup. The W2 supplement proves observer
thread identity against the existing owner-thread entry, notification completion
before Publish returns, no caller event-loop reentry, unchanged single notification
through Finish, and cache clearing deferred until Finish. No production Network
or transaction file changed.

OFF startup uses the retained real WebEngine smoke dispatch, not the pre-GUI
--smoke loader probe and not the migrated runner. Uninstrumented no-argument
Windows startup is not claimed: QStandardPaths can resolve daily profile locations.
The retained Windows smoke isolation explicitly redirects configuration/index/
cache/recovery data. Each final legacy case gets its own fresh directory, recorded
by the script. New QTest fixtures own their own temporary profiles. No daily data
or dictionary/index is used; source fixture dictionaries remain read-only.

## Target isolation and effective guard

The configure guard is apps/goldendict/full_text_scope_isolation.cmake, deferred
until the root directory finishes defining targets. It walks actual SOURCES,
INTERFACE_SOURCES, LINK_LIBRARIES, INTERFACE_LINK_LIBRARIES and explicit target
dependencies, including target references in conditional expressions. It rejects
this family's source/access names or runner from the product dependency closure.
No historical exemptions are introduced. Negative probes mutate real CMake target
membership: transitive source, interface source, custom dependency and conditional
link. Their logs preserve the specific rejection, not merely any nonzero exit.

Release/W3-off-production-closure.txt, on/off-production-commands.log,
off-targets.log, off-imports.log and isolation-observations.json jointly show actual
source/link metadata, real main_window.cpp compilation and product linkage, absence
of the migrated family and Qt Test from product commands, no OFF runner target,
and no Qt6Test import in the OFF executable. Reuse of actual presentation sources
is the established W1/W2 pattern; no product .cpp include, macro access override or
different production implementation exists. Configure-time guard code is build
validation, not runtime test orchestration. Arbitrarily renamed/copied scenario
logic and general semantic responsibility still require independent review.

## Failed attempts and limits

Preserved non-passing attempts: initial nonexistent build target (exit 1); first
OFF configure with a PowerShell-split toolchain argument (exit 1); first migration
build with incorrect private header name and second with QObject discovery of a
class without Q_OBJECT (both exit 2); first guard integration used a nested
PROJECT_SOURCE_DIR (exit 1); first probe defaulted to unavailable NMake (CTest exit
8). These were test/build wiring errors, never product red or passing evidence.
A transient write was rejected while the compiler held the new test file; it was
retried after the build ended. The guard's initially new untracked file was moved
out of the cmake submodule; that submodule remains clean and unchanged.

The first expanded regression script reused one explicit smoke configuration root
across cases: dictionary-bar and Preferences predecision failed (overall exit 8).
No source/assertion change was used to resolve this. Fresh per-case roots produced
10/10 passes, preserved in regression-isolated.log and final-regression.log. The
initial shared-root failure remains evidence and is not classified as a product
fix. Original W1/W2 records are not overwritten.

Not verified: Linux/macOS execution, full historical suite, external CI configuration,
packaging/install, uninstrumented no-argument Windows startup, semantic detection
of arbitrary renamed test logic. No new platform/whole-A4/SOLID conformance claim.
The complete current inventory and remaining work are in w3-inventory.md.

Final candidate commit/tree, complete base diff, evidence hashes and independent
review receipt are recorded externally after the candidate is frozen. Independent
review is pending until an actual fresh read-only reviewer supplies its own verdict.
Rollback is this coherent W3.1 unit only; it changes no persistent data format.
