# W3.2 bounded batch — bounded migration in progress

Status: DictionaryBarSmoke implementation and scoped behavior validation complete;
normal startup validation deferred under explicit user clarification. Independent
review remains pending until an external receipt binds the frozen candidate.
Base and current HEAD: e77627a9bc99892d440b39acfba93aab56a40bf8.
The only worktree is D:/workspace/goldendict/worktrees/feature-tiger-qt6-migration.
Existing W3.1 migration/coverage/build/guard contracts remain authoritative; see
w3-1-design.md and w3-1-verification.md. W1/W2/W3.1 history is preserved.

## Locked scope

One family: RunDictionaryBarSmokeCheck / --dictionary-bar-smoke /
goldendict_dictionary_bar_smoke. Reuse actual QApplication, scheme, Core/Network
composition, Dictd/external catalog fixtures, MainWindow and presentation source/
resource/MOC/link closure. Reuse the narrow group selection/refresh and request
count observations where suitable, without adding unrelated capabilities to the
full-text access class. QObject discovery supplies toolbar/actions/query/results/
suggestions; existing real Qt slots supply lookup start/completion operations.
No general mutable-state access is needed or authorized.

Preserve action catalog identity/order/checkability/accessibility, toolbar
hierarchy/visibility, reordered group baseline/mute, independent all/group state,
all-off empty results and suggestions, and hidden-bar unfiltered lookup results.
The original 10 ms polling and minimum settling observations must remain test-owned
and follow real completion; retain the 20-second CTest timeout and isolation.
The selected method's two asynchronous stages are one existing scenario, not a
combination of previously isolated families. No lookup algorithm is copied.

Expected removal is only this method/declaration plus its main classification,
fixture predicate and dispatch; shared fixtures remain for unselected consumers.
The cumulative W3.1 guard must extend to the new test target and prove each added
violation in isolated fixture targets. Per-family baseline/equivalent validation,
checkpoint and final cumulative review remain mandatory. No second family, W4,
transaction semantics, public contract or startup architecture change is selected.

## Verified baseline and current blocker

Evidence: D:/workspace/goldendict/evidence/a4-test-extraction-w3-2-20260912.
Initial cwd/root/branch/HEAD matched; working tree was clean. Fetch succeeded;
HEAD is three commits ahead and zero behind origin. Both existing Release caches
resolve to this exact source: build/Release BUILD_TESTS=ON, build/W3-off OFF.
All four installed candidate-profile selector hashes match. W3.1 final receipt
independent-review-2.md binds this HEAD/tree and is preserved, SHA-256
fe3117a5357bbe3160033760e4077c3e80b649cf88f3e08d1fd87041cd8bef42.

Baseline build through run_with_conan.ps1: goldendict,
full_text_dictionary_scope_test, article_page_lifecycle_test and
publication_preparation_test, Release, --parallel 4: exit 0.
run-baseline.ps1 runs four separately isolated exact CTest entries: old dictionary
bar, W3.1 projection, W1 and W2; all four return 0. No migration red is claimed.

W3.1 --webengine-smoke exercises initialized MainWindow and a real HTML load /
toPlainText marker; it is not no-argument startup and cannot satisfy W3.2's new
explicit requirement. On Windows main.cpp:558 onward gates configuration/cache
root overrides on IsSmokeInvocation. Without that, network_cache_root is the
system QStandardPaths cache location. Portable mode redirects configuration only.
NetworkRuntime::Impl constructs its real disk cache and may RemoveOwnedDirectory
when disabled (network_runtime.cc:510-543); setting cache size zero is not safe
isolation of the daily cache.

A read-only probe of the installed Qt package, through the Conan launcher with
APPDATA/LOCALAPPDATA/HOME pointed into build/w32-path-probe, returned
C:/Users/dev/AppData/Local/<APPNAME>/cache (exit 0). This confirms those environment
variables do not isolate the installed Qt Windows cache path. Qt's implementation
uses SHGetKnownFolderPath. No ordinary goldendict launch was attempted against
those daily paths. No startup failure or success is fabricated.

## Isolation clarification (supersedes the earlier migration hold)

The user explicitly permits continuing this family and already isolated tests.
Another Windows account/VM is not required. Normal startup may use an existing
formally supported path setting, without smoke dispatch or skipped initialization.
Unsafe startup remains deferred; default-path/no-argument startup is not covered.

The read-only Qt Core probe uses the installed Qt binary and the exact production
organization/application names. It prints
C:/Users/dev/AppData/Local/GoldenDict/GoldenDict/cache/qt-network-http.
See exact-cache-path.log and path-probe/probe.cpp in external evidence. Initial
probe build wiring failures (missing packaged CMake config and MSVC switches)
are tool setup failures, not product failures; no product was launched.
NetworkCacheStorage::Prepare creates the owned directory and a temporary write
probe when enabled. NetworkRuntime::Impl configures the actual disk cache and
removes that owned subtree recursively when disabled. Cleanup does not remove
the parent cache root or its indexes sibling. Shutdown policy may also clear it.
Portable selects core.conf/history-v1/favorites-v1 and their recovery locations;
legacy import is resolved within portable. Explicit index_directory can isolate
indexes. Neither portable nor --dictionary-root overrides Network's root today.
Windows smoke-only GOLDENDICT_TEST_CONFIG_ROOT is not a normal path setting.
The Qt default WebEngine profile is off-the-record (empty storage name); do not
conflate it with Network's owned HTTP cache or claim whole-process zero writes.

Proposed minimal separate path correction, not implemented by W3.2: resolve the
portable cache root as <application>/portable/cache and pass that existing path
through NetworkRuntime::Prepare; use it for default indexes while retaining an
explicit index_directory. Keep non-portable defaults, publication semantics and
all normal initialization unchanged. Scope would be the app path resolver/main
composition and focused resolver/startup tests, with documented portable cache
semantics. No Network API, new CLI, framework or user-profile mutation is needed.
This changes the portable path contract and requires separate explicit approval.
Until then normal startup validation remains unavailable, not a migration blocker.

## Per-contract migration mapping

| Original observation/entry | Test-owned equivalent |
| --- | --- |
| RunDictionaryBarSmokeCheck / --dictionary-bar-smoke | DictionaryBarTest::dictionaryBarThroughRealWindow / retained goldendict_dictionary_bar_smoke CTest |
| Missing facade/bar or catalog < 2, empty all-actions | Explicit fixture/QObject/catalog/all-action precondition assertions |
| identities | Same catalog count, IDs, order, checked/checkable, label/tooltip and widget accessible-name comparisons |
| hierarchy | Same object name, toggle action, top docking, visibility, movable/floatable/all allowed areas |
| group_baseline | Real SetDictionaryGroups and SelectGroup/RefreshDictionaryBar; same reordered IDs and mute checks |
| group_isolation / all_scope_retained | Same actual action trigger and group round trips; group and all state remain independent |
| all_off | Same real StartLookup/FinishLookup slots; five empty-request observations at 10 ms, no results or suggestions |
| hidden_unfiltered | Same hidden-bar lookup and completion polling, nonempty results, restore toolbar visibility |

There is no fault injector or test-only persistent MainWindow field in this family.
The local flags, timing and assertions leave production together with the method,
header declaration, smoke classification, fixture predicate and main dispatch.
Other consumers retain shared source fixtures and HasSmokeArgument unchanged.
The original scenario already has two sequential phases; no separate cases are
combined. QTimer/QEventLoop replace self-owning recursive callbacks in the test
only. Completion still runs on the GUI thread against the real slots; the CTest
20-second bound is unchanged. No arbitrary private state is assigned.

DictionaryScopeTestAccess moves the three existing W3.1 operations into one test
header: Compose, SelectGroup(optional refresh), read-only RequestCount. The new
family uses the latter two; existing public group/preferences/facade operations,
QObject discovery and registered Qt slots handle the rest. The friend rename
adds no runtime code or new capability. The complete presentation source/resource/
MOC/link closure is reused, without a new implementation variant or .cpp includes.
Each runner constructs its own QApplication and real module composition.
The selected family uses the existing product --smoke child as its original
external-program fixture, preserving exit-zero/empty-output semantics; this
pre-GUI loader probe is not the removed DictionaryBar test entry and does not
perform ordinary initialization. No production dependency on the runner exists.

CTest retains its name, working directory (app build directory), 20-second timeout,
offscreen and Chromium flags. Test-owned QTemporaryDir paths replace the old
fixed CTest HOME/XDG directories, including explicit Network/index/config paths;
each process/family has independent data. Config load/recovery/startup assertions
were not part of this family and are not inferred from test-owned composition.

## Preserved migration failure

The first migrated run failed only the newly added final request-count assertion
(after restoring toolbar visibility): actual 1, expected 0. All original boolean
assertions passed. Source main_window.cpp visibilityChanged calls
ApplyDictionaryParticipation, which starts suggestions/navigation lookup when
restoring the toolbar. The original scenario observed hidden-stage completion
before show(), then returned without asserting post-show quiescence. The test now
records that same pre-show completion boundary; it does not suppress the real
visibility callback, alter product behavior or weaken an original assertion.
See first-migration-qtest-failure.txt and migrated-family-tests.log (CTest exit 8).
The corrected boundary is separately recorded, not presented as product red/green.

A second run retained the same post-show assertion because an exact-text edit did
not match its clang-format line wrapping. second-migration-qtest-failure.txt
preserves the repeated failure. After replacing the actual wrapped assertion with
the recorded pre-show value, family-tests-final.log reports both the family and
cumulative guard passing (2/2, exit 0). No original assertion was removed.

## Verification commands and result scope

All commands use this Qt6 worktree and its existing Release Conan environment.
Prefix for builds is `./run_with_conan.ps1 --build-type Release
--with-build-environment --`; runtime/CTest prefix omits `--with-build-environment`.

| Check | Command after prefix / evidence | Actual result |
| --- | --- | --- |
| Baseline production and W1/W2/W3.1 targets | cmake --build --preset conan-release --target goldendict full_text_dictionary_scope_test article_page_lifecycle_test publication_preparation_test --parallel 4; baseline-build.log | exit 0 |
| Pre-migration cases | run-baseline.ps1; baseline-tests.log | 4 exact cases, 4 passes, script exit 0 |
| ON current production and migrated targets | cmake --build --preset conan-release --target goldendict dictionary_bar_test full_text_dictionary_scope_test article_page_lifecycle_test publication_preparation_test --parallel 4; migration-build.log | exit 0; subsequent test-only rebuilds recorded separately |
| Corrected complete family and guard | ctest --preset conan-release -j 1 -R '^(goldendict_dictionary_bar_smoke|full_text_scope_isolation_test)$' --output-on-failure; family-tests-final.log | 2/2, exit 0; DictionaryBar 3 QTest passes including init/cleanup, no skip |
| Cumulative regression | run-regression.ps1; cumulative-regression.log | 10 separately isolated exact entries, all exit 0 |
| Configuration regression | cmake build application_service_test user_state_upgrade_test http_client_test full_text_query_composer_test; configuration-regression.log | build exit 0; application_service_test and user_state_upgrade_test 2/2, exit 0 |
| OFF production | cmake --build build/W3-off --target goldendict --parallel 2; off-build.log | exit 0 |
| OFF existing startup entry | ctest --test-dir build/W3-off -j 1 -R '^goldendict_webengine_smoke$' --output-on-failure; off-webengine-smoke.log | 1/1, exit 0; smoke only, NOT ordinary startup |
| Both temporary volumes | ctest exact DictionaryBar entry with fresh TEMP/TMP on C: and D:; temp-volume-checks.json | each 1/1, exit 0 |
| OpenSpec / whitespace | openspec validate architecture-remediation-review --strict; git diff --check | exit 0 |

Cumulative entries: migrated DictionaryBar, W3.1 projection, W1 lifecycle,
W2 publication preparation, http_client_test (including queued callable thread/
completion/non-reentry), full_text_query_composer_test, cumulative isolation guard,
full-text dialog, configuration reload coordinator and Preferences predecision.
W1 reports 17 and W2 parent suite 13 passes, zero failures/skips. Child-process
QTest summaries in the W2 log are not counted as extra independent parent cases.

ON and OFF CMake caches both bind this worktree, Release/Ninja, shared Qt 6.11.1,
and the existing build/Release/generators/conan_toolchain.cmake. Actual ninja -t
commands goldendict evidence shows real main_window.cpp compilation and product
link closure, without either migrated runner/source or Qt6Test. OFF target listing
contains neither migrated runner; dumpbin /imports has no Qt6Test import.
Configure guard closure reports are captured for both builds. Guard fixtures
produce three clean successes and twelve targeted rejections across both runners
and their shared access boundary. No negative edit is left in a real product target.
The named-member guard cannot detect arbitrarily renamed/copied scenarios or
semantically overbroad test access; independent review remains required.

The new target compiles all real presentation sources/resources/MOC under the
same existing conditions; no shared production compile/link structure is changed.
OFF reuses its separate previously validated cache and rebuilds affected product
objects. Neither build was switched between ON and OFF. Build outputs and runtime
profiles are ignored, external evidence is preserved with source identity/hashes.

## Delivery boundaries and remaining verification

Only one complete family is migrated; the final local candidate is the single
family checkpoint. Its exact commit/tree and cumulative base diff are external
because adding a receipt must not mutate a reviewed candidate. Rollback consists
of this W3.2 change, not W1/W2/W3.1 or persistent user data.
No ordinary Windows production launch was performed. No smoke-only success is
substituted for it. Existing supported settings leave the Network path gap
identified above; the portable-path proposal is not implemented. Default-path
startup, Linux/macOS, full historical suite, external CI and packaging remain
unverified. The migrated family proves toolbar/lookup contracts, not main.cpp
persistence/recovery wiring; actual existing coordinator and configuration tests
remain regression evidence. W3/A4 stays in progress with 51 pending MainWindow
methods and the separately inventoried main-only/shared historical dependencies.
No second family, lookup ownership, W1/W2 behavior, public contract, merge or push.
