# P1 verification and remaining validation gate

Status: Network path implementation and isolated regression complete; ordinary
startup and the full required GUI regression matrix remain blocked by the
WebEngine path-isolation gap below. P1 is not claimed fully accepted.

## Identity and authority

- User approval: P1, 2026-09-19; distinct from W3.2, no W3.3, merge or push.
- Worktree: D:/workspace/goldendict/worktrees/feature-tiger-qt6-migration.
- Branch: feature/tiger-qt6-migration.
- Base: d196d6d7c02f17e2ba91e58d34620d4f9d0c4803; initially clean.
- Profile: activated/hash-verified goldendict-candidate-v1; one development writer.
- Governing rules: user/global AGENTS and engineering-delivery/software-design;
  workspace/project AGENTS, docs/agent-workflow.md, project-design-rules.md,
  architecture cache contract, CRD-DICT-004 and this change's approved P1 delta.
- Readiness: independent Ready receipt outside the tree, readiness-review.md,
  SHA-256 4659293a6caf91044999442e278a6fccbd83011ff683729935f1dc6a58c87922.
- Evidence root: D:/workspace/goldendict/evidence/p1-portable-network-cache-20260919.
  Final candidate/review identities live there; prior W1/W2/W3 records unchanged.

## Implementation and call-chain scope

ResolveNetworkCacheRoot is a pure app-private operation beside the existing
configuration-location resolver. Explicit input is preserved byte-for-byte;
portable uses current configuration parent/cache; other profiles preserve the
supplied platform-default string. Network alone appends qt-network-http.
main.cpp calls the selector once before startup Prepare; all five Prepare call
sites use that const root: startup/recovery reconstruction, source reload, group
reload, Preferences, and the existing predecision scenario. No Network, Core,
W1 lifecycle, W2 publication, public API, serialization or worker code changed.

Failure remains caller-specific: startup reports the existing diagnostic and
uses uncached traffic; Preferences rejects unavailable positive preparation and
retains the prior runtime/configuration. No default-root retry was added.
maximum_cache_bytes() reports configured policy, not disk availability.

## Reproducible commands and observed exits

Run from the worktree in PowerShell with the existing Python directory prefixed
to PATH: C:/Users/dev/AppData/Local/Python/pythoncore-3.14-64.
Every build-tree executable/CTest uses ./run_with_conan.ps1 --build-type Release.
Build commands additionally use --with-build-environment before --.

| Phase | Command after launcher `--` | Result / retained log |
| --- | --- | --- |
| Baseline | cmake --build --preset conan-release --target goldendict legacy_configuration_location_test http_client_test --parallel 4 | 0; baseline-build.log |
| Baseline | ctest --preset conan-release -j 1 -R '^(legacy_configuration_location_test\|http_client_test)$' --output-on-failure | 0, 2/2; baseline-tests.log |
| Red | cmake --build --preset conan-release --target portable_network_cache_test --parallel 4 | 0; final-contract-red-build.log |
| Red | ctest --preset conan-release -j 1 -R '^portable_network_cache_test$' --output-on-failure | 8, three portable root mismatches; final-contract-red-tests.log and final-contract-red-qtest.txt |
| Final ON | cmake --build --preset conan-release --target goldendict portable_network_cache_test legacy_configuration_location_test http_client_test --parallel 4 | 0; final-on-build.log |
| Final green | ctest --preset conan-release -j 1 -R '^(portable_network_cache_test\|legacy_configuration_location_test\|http_client_test\|full_text_scope_isolation_test\|full_text_query_composer_test)$' --output-on-failure | 0, 5/5; final-green-tests.log |
| Config build | cmake --build --preset conan-release --target application_service_test --parallel 4 | 0; configuration-build.log |
| Config/recovery | ctest --preset conan-release -j 1 -R '^application_service_test$' --output-on-failure | 0, 1/1; configuration-tests.log |
| Final OFF | cmake --build build/W3-off --target goldendict --parallel 4 | 0; final-off-build.log |
| Earlier regression | ctest --preset conan-release -R '^(portable_network_cache_test\|legacy_configuration_location_test\|http_client_test\|publication_preparation_test\|full_text_scope_isolation_test\|full_text_query_composer_test)$' --output-on-failure | 0, 6/6; green2-tests.log; GUI isolation qualification below |

Table regex pipes are escaped only for Markdown; shell commands use plain `|`.
ON cache: build/Release, BUILD_TESTS=ON, Release/Ninja. OFF: build/W3-off,
BUILD_TESTS=OFF, Release/Ninja. Both CMAKE_HOME_DIRECTORY values are this worktree.
Executables: each build directory's bin/goldendict.exe; new test only in
build/Release/bin/portable_network_cache_test.exe.

The first red run returned 8 but GUI-subsystem QTest output was not captured.
CTest was then configured with QTest -o output, giving actual assertion evidence.
The first green attempt exposed an incorrect test assumption that the policy
accessor became zero on unavailable storage (green-tests.log/green-qtest.txt).
Source investigation showed it intentionally preserves the requested maximum.
The test now checks that policy, unchanged diagnostic, selected path, and absence
of fallback writes. No product failure behavior changed. After this correction,
the complete final test source was rerun against the behavior-preserving old
selection seam: three root mismatches and the explicit/nonportable control Pass.
The exact same final test source then produced six QTest Pass (four test methods
plus init/cleanup), zero failures/skips. The red is an instrumented extraction,
not untouched base. final-contract-red-tracked.patch and final-contract-test.cpp
preserve that distinction. All red paths and sentinels were QTemporaryDir-owned;
no old ordinary product was launched and no daily cache was targeted.

## Acceptance mapping

| Contract | Actual evidence |
| --- | --- |
| Portable detection and real preparation | SelectsPortableBeforeRealPreparation: real ResolveConfigurationLocations/ProbePath, pure selector, Prepare and Create, exact child path and 1 MiB policy |
| Explicit/nonportable compatibility | PreservesExplicitAndNonPortableWithoutIo: exact supplied strings, no directory creation; no real platform-default cache access |
| Unavailable portable root/no fallback | UnavailablePortableNeverFallsBack: cache root blocked by test file, failed preparation diagnostic, Create retains selected path/policy, simulated default untouched |
| Enable/disable/reapply/clear-on-exit | ReapplyAbandonAbortAndCleanupStayInOwnedChild: real PrepareCandidate/Reserve/Abort/Commit, disabled and enabled states, shutdown, second disabled startup |
| Abandon/retire/token handling | Same test: abandoned/aborted candidate leaves active sentinel intact; consumed candidate rejected; real runtime reused then replaced |
| Cleanup containment | Owned child removed; sibling and simulated-default sentinels retain exact content; no recursive parent deletion |
| Configuration/recovery regression | application_service_test and legacy_configuration_location_test pass; main call-chain inspection confirms fixed root reused |
| W2 | publication_preparation_test executed and passed in green2; this is behavioral evidence, not proof of complete WebEngine path isolation |
| W3 guard and composer | full_text_scope_isolation_test and full_text_query_composer_test pass |
| W1/W3 GUI and product configuration smoke reruns | Not rerun after discovering unresolved WebEngine temporary-data path; prior accepted receipts remain historical, not current P1 passes |

## ON/OFF target isolation

Captured real Ninja `-t commands goldendict` for both directories and
`dumpbin /dependents` on both executables, all exit 0. Neither command closure
contains portable_network_cache_test nor Qt6Test. OFF build.ninja has no new
portable_network_cache_test target. ON compiles the same production location
source; test source is only in the BUILD_TESTS conditional target. Existing
historical test membership is not changed. Evidence: on/off-production-commands.txt,
on/off-dependents.txt and target-isolation.txt. No GUI launch is implied by builds.

## Ordinary production startup: NOT EXECUTED

| Writable domain | Proposed portable fixture / actual source evidence | Gate |
| --- | --- | --- |
| Configuration, history, favorites | ResolveConfigurationLocations uses application-directory/portable; current files and selected legacy import stay there | Supported existing selection |
| Recovery records and config checkpoint | Derived from the same selected configuration/history paths, startup convergence and aboutToQuit persistence | Source inspected; ordinary run not performed |
| Dictionary indexes | Default remains platform CacheLocation/indexes; a fixture must supply existing explicit index_directory beneath its own portable root | Supported existing config; no changed index policy |
| Network cache | P1 selects portable/cache; storage owns only qt-network-http | Actual isolated tests passed |
| WebEngine HTTP cache/cookies/prefs | Default profile off-record, HTTP cache path empty and prefs memory-backed | Does not prove all transient writes isolated |
| WebEngine temporary data | Qt 6.11.1 ProfileAdapter::dataPath() still derives AppDataLocation/QtWebEngine/OffTheRecord; application supplies no override | BLOCKED, outside P1 |
| TEMP/TMP and process working directory | Can be process-local fixture directories; Windows APPDATA/LOCALAPPDATA env does not redirect Qt Known Folders | Not used as a false substitute |
| Instance forwarding | main.cpp single-instance service/forwarding enclosed in Q_OS_LINUX; no Windows forwarding path found | Static inspection only |

Installed Qt source inspected under
C:/Users/dev/.conan2/p/b/qt7a37ca086ace9/b/src/qtwebengine/src/core:
profile_adapter.cpp:53 (path construction), :318 (off-record dataPath and explicit
warning about Chromium temporary directories), pref_service_adapter.cpp:64
(memory prefs), and main app defaultProfile call sites. The AppData-derived
profile path is not proven redirected by portable mode. This is a potential write
location from source, NOT an observed daily-file modification. The separate
browser_main_parts_qt.cpp:235 AppData use is Linux-only and is not asserted as a
Windows effect. Resource/locales environment overrides are not profile-data
settings. No supported product data-path injection was found in the inspected
startup surface. No environment-variable retry, daily-cache probe/write/delete,
profile override, new startup flag, or WebEngine policy change was performed.

The earlier W2 test run used its existing temporary configuration/Network roots
and passed; inspection later showed its WebEngine isolation cannot be certified
from those environment settings alone. No claim of complete write tracing or
retroactive correction of historical results is made. Further GUI runs withheld.

A separate minimal prerequisite would choose a portable WebEngine data path in
application composition before profile creation, preserving off-record semantics,
and validate transient writes. That changes another path contract and is not
implemented or implicitly approved by P1. Ordinary portable startup, normal
main-window/exit observation, nonportable default startup, and the full GUI
regression gate remain unverified. This record preserves that limitation rather
than labeling compilation or a smoke as ordinary launch.
