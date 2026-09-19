# P1 WebEngine supplement verification — 2026-09-19

This record supplements `verification.md`; it does not rewrite that record, W3.2,
or either historical independent P1 Fail. Governing approved choice and observation
limits are in `webengine-supplement.md`. Supplement base is
`e35e21fdd54503eb7e0e09e01283af09bf45dcec`; cumulative P1 base is
`d196d6d7c02f17e2ba91e58d34620d4f9d0c4803`. Exact final commit/tree and fresh review
receipts are external, so recording the verdict does not mutate its candidate.

Evidence root: `D:/workspace/goldendict/evidence/p1-webengine-supplement-20260919`.
Only Windows 11 / Release / the characterized Conan Qt 6.11.1 package is covered.
The existing independent capability report remains
`../webengine-path-capability-20260919/blocker-supplement.md` relative to that root.

## Implementation and ownership

`main.cpp` calls `InitializeWebEngineStorage` after application identity and
ResolveConfigurationLocations/legacy-file validation, before recovery, Network,
Core and MainWindow. RegisterArticleScheme does not obtain a profile; Linux's early
help uses QTextBrowser. MainWindow's view constructor and ArticleView::setPage can
implicitly obtain default pages, but both are later than this call. The private
helper validates/writes only the selected owned subtree before obtaining Qt's
default singleton, immediately applies its data path, and never owns/deletes it.
Application metadata prevents repeated calls from reinitializing a live profile;
profile metadata carries the selected root to the existing Inspector creation site.
Non-portable without override performs no profile/path operation. Empty/relative,
blocked or conflicting selections throw rather than restore Qt defaults.

ArticleInspector retains its independent QObject-owned profile, with an individual
RAII directory below inspectors. Both normal and partial-construction cleanup
release page/view, then profile, then directory. ArticleView reports setup failure
and does not open a fallback Inspector. No W1 binding algorithm, Core/Network token,
publication phase, recovery semantics, or public interface was changed. Existing
handler/interceptor/script ownership and default-profile sharing remain intact.

## Red and green

A minimal no-op initializer was first added solely to compile the same product
entry test. This is explicitly a test seam, not an untouched original program.
`red-noop.cpp`, `red-tracked.diff` and `red-test.txt` retain that phase. The new test
used a unique GDStorageTest identity, Qt test paths and an owned temporary root.
No old ordinary GoldenDict program was run for red evidence.

Commands run from the authorized worktree, with the existing Python directory on
PATH and through `run_with_conan.ps1 --build-type Release --`:

- Build with `--with-build-environment -- cmake --build --preset conan-release
  --target webengine_storage_paths_test --parallel 4`: exit 0.
- `build/Release/bin/webengine_storage_paths_test.exe configuredBeforeRealPage
  -o 'D:/workspace/goldendict/evidence/p1-webengine-supplement-20260919/red-test.txt,txt'`
  with offscreen / `--disable-gpu`: exit 1 at the expected storage-path comparison.
  Actual was the unique qttest identity's fallback; expected was owned webengine/article.
  One preceding unquoted PowerShell comma argument produced exit 2 (unknown `txt`
  function); that invocation is not the red evidence.
- After implementation, normal `ctest --preset conan-release -R
  '^webengine_storage_paths_test$' --output-on-failure`: exit 0. Same path assertion
  and real page load pass. The expanded target has 11 passes including init/cleanup,
  five fresh-process selection rows and four integration/failure cases, no skips.

## Acceptance mapping

| Contract | Executed evidence |
| --- | --- |
| Portable and explicit selection; unchanged non-portable | freshSelection portable/explicit/nonportable child processes, each actual Qt singleton; explicit precedence does not create portable default subtree |
| Empty/unavailable selection, no fallback | freshSelection empty/blocked in fresh processes, rejectsInvalidAndConflictingPathsWithoutFallback; fallback controls use unique test identity only |
| Setup precedes real page; exact singleton and memory policies | configuredBeforeRealPage, successful setHtml/loadFinished/title; off-record, MemoryHttpCache, NoPersistentCookies, StoreInMemory |
| Normal and unpublished candidate pages share configured profile | W1 candidateEventsStraddlingPublication assertions before/after actual PrepareFacadeCandidate/maintenance/publication, followed by real page load/refresh |
| Reapply/rebuild and page lifecycle | W1 17 passes; Preferences/source/group production smoke paths; helper same-root idempotence and conflicting-root rejection |
| Inspector independent paths, frontend, destruction | inspectorOwnsIndependentDirectoryUntilProfileDestruction: two distinct profiles/paths, real frontend renderer, destroyed signals prove page before profile before directory removal, adjacent sentinel unchanged |
| Inspector failure containment | inspectorFailureDoesNotAttachOrFallBack with owned blocking file; ArticleView catches setup exception at its existing creation operation |
| Existing Inspector behavior | article_inspector_test, article_selection_test and two-process inspector geometry smoke |
| Network + W2 transaction/lifecycle unchanged | portable_network_cache_test, http_client_test, publication_preparation_test and Network preferences/restart smoke |
| Recovery/configuration | application_service_test's actual configuration transaction/recovery cases, user_state_upgrade_test and production reload coordinator smoke |
| W3 migration isolation | W3.1/W3.2 scenarios and full_text_scope_isolation_test including its negative fixture |
| Production target isolation | actual ON/OFF Ninja command closure includes product helper, excludes new test implementation; OFF has no new test target |

## Builds and cumulative tests

Existing independent caches remain `build/Release` (BUILD_TESTS=ON) and
`build/W3-off` (OFF), both Release/Ninja, both point to this exact worktree.
`on-build.log`, `on-final-source-build.log`, `off-build.log` record successful
production and affected test builds. Actual command closures and assertions are in
`on-production-commands.txt`, `off-production-commands.txt`, `target-isolation.json`.

`regressions-1.log`: 16/16 passed. `affected-smokes.log`: 7/7 passed. The combined
23-entry expression passed 23/23 on final source and is repeated on the committed candidate; command,
exits and copied QTest details are retained externally. Required tests must exist
and actually execute. W2 includes its 11 isolated child cases, not just process
startup. Core recovery is covered by the existing application service cases; the
exploratory regex term configuration_transaction_recovery_smoke matched no entry
and is not claimed as a separate executed test.

Existing Inspector NativeLegacyGeometryImport remains explicitly skipped because
its paired native Qt5 capture was not supplied (14 pass, 1 skip). This historical
cross-version geometry evidence is not a P1 path assertion. The new P1 target,
W1, W2 and W3 key cases have no skips. Historical CTest GUI flags are unchanged;
the focused P1 test and ordinary native startup use `--disable-gpu` without
`--no-sandbox`. GLES fallback diagnostics occur without failed load assertions.

## Ordinary production startup

`startup-preflight.md` records the complete path/instance check. External
`ordinary_startup.py` copies the actual product exe/DLLs/resources, creates the
existing `bin/portable` layout, supplies an owned explicit index path and XDXF
fixture via core.conf, and launches with argv containing only the executable.
It uses normal GoldenDict identity, Windows QPA and an owned TEMP/TMP/cwd; no smoke,
Qt test mode, profile-owner substitution or Network/WebEngine bypass.

The first `startup-on` attempt was rejected before MainWindow because the external
Python fixture used CRLF instead of the current format's exact LF header. Exit 1
and files remain intact; `startup-fixture-correction.md` binds the diagnosis. Only
the fixture writer was corrected. `startup-on-lf` entered actual MainWindow,
rendered Welcome to GoldenDict through articleWebContent, showed the owned one-word
dictionary, and exited 0 through external Ctrl+Q. A typed lookup was not visibly
observed and is not additional lookup evidence. Window accessibility captures,
actual renderer image/command line, binary hash, before/after files and directory
notifications are preserved. Exit produced the owned configuration/session/geometry checkpoint; empty
History/Favorites remained absent; no product/renderer process remained at the post-run inspection.

Recursive ReadDirectoryChangesW monitoring was armed/self-checked before launch
and continued past exit. The successful ON run recorded 85 events with no watcher
error/overflow; selected Network/WebEngine/temp/index and exit-checkpoint changes
were observed, and the owned adjacent sentinel was preserved. Monitoring covers
writes from any process in its owned subtree, but not all filesystem accesses,
reads, PID attribution or paths outside it. The accepted Qt constructor analysis
and prior independent probe complement this bounded observation. No daily-root
sentinels, cleanup or restoration were used. Final candidate native startup and
module hashes are recorded separately alongside the review inputs.

No non-portable default-user startup, Linux/macOS runtime, or all-Chromium-feature
zero-write claim is made. Final completion depends on the new independent exact
candidate receipt; the old Fail remains historical and is never relabelled.
