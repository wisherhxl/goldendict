# Qt 6 Baseline Replacement Gap Audit

Status: Active execution baseline

Audit date: 2026-09-03

Audited Qt 6 commit: `d3491598257a542dbc8d8dbf0bd36c3894711419`

Qt 5 evidence commit: `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`

Governing requirements: [Qt 6 Product Baseline CRD](qt6-product-baseline-crd.md)

## Outcome

The Qt 6 line is a usable development baseline, but it is **not ready to
replace Qt 5 as the sole product baseline**. Core dictionary lookup, the major
local formats, article rendering, user-owned history and Favorites, source and
group transactions, the main product shell, initial audio playback, Linux
single-instance lookup, installed help, and initial Russian localization are
present. The CRD cutover gate is still blocked by incomplete product workflows
and incomplete acceptance evidence.

This audit does not change product requirements. Every item below is a
conformance unit under the approved CRD and defaults to the pinned Qt 5
behavior. A new product decision is required only when that behavior cannot be
implemented within the mandatory architecture, security, licensing, or
maintained-dependency constraints.

## Evidence Reviewed

- The frozen Qt 5 source, including 19 Designer UI files and the main-window,
  Preferences, scan, dictionary-management, full-text, and platform sources.
- The Qt 6 application, Core, Network, External, build, package, and registered
  test surfaces at the audited commit.
- The migration, feature-parity, porting-map, architecture, build, and testing
  records.
- The legacy resource inventory: 73 icons, 257 flags, 45 translation sources,
  two help collections, and three desktop/packaging resources.
- The Qt 6 resource inventory: 24 product icon/style payloads plus its resource
  manifest and provenance record, 45 imported application translation sources
  of which only Russian is enabled, and both English/Russian help collections.
- The read-only acceptance corpus at `D:\workspace\goldendict\content`: 88
  files and 9,303,289,246 bytes, including MDX/MDD, compressed DSL, DSL resource
  ZIP, Hunspell, fonts, styles, and images.
- The durable Windows Release record in `docs/testing.md`: 99 of 125 tests
  passed in the earlier broad run, with 26 failures assigned to MSVC QtTest and
  cross-process WebEngine/XDG investigation. More recent local build and
  focused-test observations are not treated as acceptance evidence until their
  commands and logs are retained by the applicable delivery unit.
- No Linux execution environment or repository CI workflow is currently
  available on this host. The repository contains the Linux Qt WebEngine Conan
  profile, but the same-candidate Linux release gate is not continuously
  enforced.

File counts are evidence of audit coverage, not a requirement to reproduce the
Qt 5 source layout. Qt 6 may continue to construct Widgets programmatically.

## Confirmed Complete Or Substantially Complete Areas

| Area | Current evidence | Remaining boundary |
| --- | --- | --- |
| Product identity and build skeleton | Qt 6.11.1, Conan 2, CMake package, `goldendict_core`, install/package foundations | Release matrix and CI remain open |
| Local dictionary foundation | All fifteen required local/specialized source families have bounded discovery, lookup, article, and focused fixture coverage | Advanced format variants and real-corpus proof remain open |
| Daily lookup and article shell | Facade-backed lookup, suggestions, groups, tabs, history, Favorites, dictionary browser, WebEngine rendering, navigation, print/export foundations | Resource saving, DevTools, detailed parity, and platform ingress remain open |
| Product shell | Main regions, menus, toolbar lookup, docks, tab controls, status summary, product icon/style, and matched initial captures | State-by-state visual parity and detailed dialogs remain open |
| Online/external source composition | MediaWiki, websites, Forvo, DICT, and external programs compose transactionally | Narrow protocol, credentials, resources, audio, and process-policy gaps remain open |
| Initial Linux integration | Single-instance lookup/activation, XWayland fallback, middle-click primary selection, desktop launcher/metainfo, help, and Russian locale | Continuous scanning, hotkeys, tray, broader localization, and release registration remain open |

## Remaining Conformance Units

The R1-R10 headings are coverage workstreams, not a strict execution order or
commit-sized deliveries. The leaf delivery table below defines the
independently auditable functional units and their dependency graph. Start a
leaf only after its listed dependencies are integrated and its own readiness
check confirms that the required environment and evidence are available. Each
leaf must be developed in its own task worktree, independently audited,
committed, pushed, and integrated before a dependent leaf starts.

### R1 — Acceptance Corpus Inventory And Harness

Requirements: `CRD-TEST-REAL-001` through `CRD-TEST-REAL-003`.

- Generate a safe manifest from an operator-supplied corpus path with relative
  paths, sizes, classifications, and SHA-256 hashes.
- Enforce read-only corpus handling and separate per-version profiles, indexes,
  caches, logs, and evidence directories.
- Define machine-readable run metadata so Qt 5 and Qt 6 results can be paired
  without recording private dictionary content.
- Add deterministic tests for path confinement, stable ordering, hashing,
  classification, and refusal to write inside the corpus.

This is the first workstream because every remaining dictionary, resource,
performance, and visual acceptance claim depends on trustworthy real data
evidence.

### R2 — Windows Test-Gate Stabilization

Requirements: Section 10.3 and the Windows half of Section 12 of the CRD.

- Reproduce and classify every current Windows failure serially.
- Correct test fixture, path, process, environment, and WebEngine isolation
  assumptions without weakening product assertions.
- Retain genuine product failures in the functional queue rather than masking
  or platform-skipping them.
- Make the complete Windows Release suite a reliable candidate gate.

### R3 — Real-Dictionary Discovery, Lookup, And Resource Acceptance

Requirements: `CRD-TEST-REAL-004` through `CRD-TEST-REAL-007` and
`CRD-DICT-003`, `CRD-STATE-004`, and `CRD-COMPAT-001` through
`CRD-COMPAT-005`.

- Compare clean discovery, warm restart, identity, ordering, counts, lookup,
  suggestions, articles, links, styles, fonts, images, and large resources
  against Qt 5 using the same corpus.
- Prioritize confirmed corpus gaps, especially DSL resource ZIP files larger
  than 4 GiB and split MDict resources.
- Close every remaining `CRD-DICT-003` format variant with generated or
  redistributable fixtures when the operator corpus does not contain that
  variant; real-corpus coverage is not a substitute for the complete format
  contract.
- Implement only evidence-confirmed backend corrections; keep original corpus
  files immutable and use disposable copies for mutation tests.

### R4 — Complete Preferences Product Surface

Requirements: `CRD-PREF-001` through `CRD-PREF-004`.

Qt 5 exposes Interface, Scan Popup, Hotkeys, Audio, Network, Full-text Search,
and Advanced pages. Qt 6 currently exposes only a reduced General and Network
surface. Restore the seven-page structure in legacy order through backed,
working controls. Deliver page families separately: Interface/Advanced,
Audio, Full-text Search, Scan Popup, and Hotkeys. Do not expose inert controls.

### R5 — Full-Text Lifecycle Completion

Requirements: `CRD-FTS-001` through `CRD-FTS-003` and
`CRD-TEST-REAL-008`.

- Finish query-mode equivalence, result targeting/excerpts, and rendered
  highlighting behavior.
- Complete eligibility policy, startup/replacement scheduling, readiness,
  progress, waiting/indexed counts, rebuild, cancellation, failure recovery,
  and Preferences integration.
- Exercise the lifecycle with every supported real format.

### R6 — Dictionary And Source Management Completion

Requirements: `CRD-DICT-001`, `CRD-DICT-002`, `CRD-STATE-003`, and the
portable/default-source decisions in `CRD-DICT-004` and `CRD-DICT-005`.

- Close remaining integrated management actions, batch behavior, morphology
  and Chinese-conversion registration, ordering, popup muting, auto groups,
  rescan, and legacy-compatible defaults.
- Close narrow website encoding/iframe/resource, DICT authentication, Forvo
  playback, and external-program environment/audio execution gaps.

### R7 — Linux Desktop Interaction And Audio

Requirements: `CRD-PLATFORM-001`, `CRD-PLATFORM-002`,
`CRD-PLATFORM-005`, and `CRD-LOOKUP-005`.

- Restore continuous clipboard/selection monitoring, scan flag and popup,
  modifier policy, global hotkeys, tray, close-to-tray, startup, and
  always-on-top behavior behind private platform adapters.
- Complete external-player and Linux speech/TTS behavior. Retain Qt Multimedia
  as the maintained built-in playback path; add another backend only when its
  deployment boundary is justified.
- Complete the supported command-line and single-instance message matrix,
  including option, malformed-input, activation-only, forwarding, queueing,
  and duplicate-writer behavior required by `CRD-LOOKUP-005`.
- Document X11, XWayland, and native Wayland capability differences explicitly.

### R8 — Browser, Network, And Product Integration Details

Requirements: `CRD-LOOKUP-003`, `CRD-PLATFORM-003`, and
`CRD-PLATFORM-004`.

- Restore resource saving, DevTools/inspector, remaining context actions,
  supported proxy/authentication modes, remote-content policy, request
  identity, update checks, and the remaining About/homepage/forum/configuration
  folder workflows.
- Preserve the WebEngine security boundary; do not reintroduce WebKit-only
  active-content behavior merely for structural similarity.

### R9 — Complete Product Resources, Localization, And Visual Matrix

Requirements: `CRD-RES-001` through `CRD-RES-005`,
`CRD-TEST-REAL-009`, and Sections 6 and 10.2.

- Finish the resource owner/source/install/consumer inventory and import every
  feature-owned reusable icon, flag, style, image, and desktop asset.
- Review and enable the remaining imported translation catalogs only after Qt 6
  context validation; complete help geometry and zoom persistence.
- Capture matched Qt 5/Qt 6 states for every required window, menu, Preferences
  page, management workflow, full-text state, and representative real article.

### R10 — Linux And Windows Release Candidate Gate

Requirements: `CRD-TEST-REAL-010` and Section 12 of the CRD.

- Add maintained Linux and Windows profiles and CI for clean dependency
  resolution, configure, build, test, install, launch, package, and uninstall.
- Complete runtime bundle, DEB/RPM, and Windows installer/deployment behavior,
  including WebEngine and native hooks.
- Run the same candidate revision through both release platforms and the real
  corpus, reconcile all parity documents, and perform the final independent
  cutover audit.

macOS restoration remains required post-cutover under the approved CRD. It is
tracked separately and must not be mistaken for Linux/Windows cutover evidence.

## Independently Auditable Delivery Leaves

Every row is one functional delivery. A leaf may be split further after its
evidence audit, but it may not be combined with another row merely to reduce
the number of audits or commits. The focused command is recorded exactly in
the leaf's issue record before source edits; C++/GUI checks use the checkout
launcher and Python tooling checks run directly.

| Leaf | Scope and requirement closure | Depends on | Minimum acceptance evidence |
| --- | --- | --- | --- |
| R1.1 | **Complete:** safe corpus manifest: `CRD-TEST-REAL-001`, read-only half of `002` | none | synthetic corpus unit tests; repeat generation and byte comparison; real-corpus manifest log |
| R1.2 | **Complete:** paired isolated run workspace and metadata: remainder of `002`, `003` | R1.1 | synthetic confinement/mismatch tests; paired Qt 5/Qt 6 metadata validation |
| R2.1 | **Complete:** MSVC exception-family test diagnosis and harness correction | R1.2 | each affected test serially reproduced; focused corrected tests; no assertion weakening |
| R2.2 | **Complete:** Windows path/profile/process isolation corrections | R2.1 | focused restart/process tests from Unicode and long paths |
| R2.3 | **Complete:** Windows WebEngine serialization and GPU-independent harness | R2.2 | all affected WebEngine tests serially pass; full Windows CTest log retained |
| R3.1 | **Complete:** configuration and user-state upgrade/rollback matrix: `CRD-STATE-004`, `CRD-COMPAT-002` through `005` | R1.2, R2.3 | generated malformed/failure-injection tests plus disposable Qt 5 profile upgrade |
| R3.2 | **Complete:** real-corpus discovery, identity, counts, ordering, restart, rescan, bounded cancellation, and recovery: `CRD-TEST-REAL-004`, `CRD-COMPAT-001` | R1.2, R2.3 | paired machine-readable Qt 5/Qt 6 result files and diff |
| R3.3 | **Complete:** real MDict split-resource acceptance and evidence-confirmed corrections | R3.2 | MDX/three-MDD lookup, resource, restart, and immutable-source checks |
| R3.4 | Real DSL and greater-than-4-GiB resource-ZIP acceptance and corrections | R3.2 | DSL/dictzip/article/resource/restart checks with bounded storage evidence |
| R3.5 | Remaining real-corpus lookup, suggestion, article, media, and management matrix: `CRD-TEST-REAL-005` through `007` | R3.3, R3.4 | paired query catalog and machine-readable result diff |
| R3.6 | StarDict non-corpus variants and companions: StarDict part of `CRD-DICT-003` | R3.5 | generated fixtures for remaining compression, metadata, resource, identity, indexing, lookup, restart, corruption, and failure variants |
| R3.7 | Dictd non-corpus variants and companions: Dictd part of `CRD-DICT-003` | R3.5 | generated fixtures for remaining index/data, dictzip, metadata, identity, indexing, lookup, restart, corruption, and failure variants |
| R3.8 | SDict non-corpus variants and companions: SDict part of `CRD-DICT-003` | R3.5 | generated fixtures for remaining field encodings/compression, identity, indexing, lookup, article, restart, corruption, and failure variants |
| R3.9 | XDXF non-corpus variants and companions: XDXF part of `CRD-DICT-003` | R3.5 | generated fixtures for remaining compression, markup/link/resource, identity, indexing, lookup, restart, corruption, and failure variants |
| R3.10 | GLS non-corpus variants and resource ZIP: GLS part of `CRD-DICT-003` | R3.5 | generated fixtures for encoding/compression/resource ZIP, identity, indexing, lookup, article, restart, corruption, and failure variants |
| R3.11 | DSL non-corpus advanced variants: DSL part of `CRD-DICT-003` | R3.4, R3.5 | generated fixtures for abbreviations, nested cards, markup/resource, identity, indexing, lookup, restart, corruption, and failure variants not present in the real corpus |
| R3.12 | BGL non-corpus advanced control records: BGL part of `CRD-DICT-003` | R3.5 | generated fixtures for remaining control/code-page/resource, identity, indexing, lookup, restart, corruption, and failure variants |
| R3.13 | MDict non-corpus legacy, compression, and encryption variants: MDict part of `CRD-DICT-003` | R3.3, R3.5 | generated fixtures for supported 1.x/LZO/encryption and companion cases, identity, indexing, lookup, article/resource, restart, corruption, and failure behavior |
| R3.14 | Aard non-corpus multi-volume/icon variants: Aard part of `CRD-DICT-003` | R3.5 | generated fixtures for volume aggregation, icons, identity, indexing, lookup, article/resource, restart, corruption, and failure behavior |
| R3.15 | ZIM non-corpus compression/link/icon variants: ZIM part of `CRD-DICT-003` | R3.5 | generated fixtures for supported LZMA2/Zstd, link rewriting, icons, split identity, indexing, lookup, restart, corruption, and failure behavior |
| R3.16 | SLOB non-corpus compression/conversion/icon variants: SLOB part of `CRD-DICT-003` | R3.5 | generated fixtures for supported LZMA2, conversion, icons, identity, indexing, lookup, restart, corruption, and failure behavior |
| R3.17 | EPWING non-corpus compression/encoding/media/index variants: EPWING part of `CRD-DICT-003` | R3.5 | generated fixtures for supported HONMON compression, mixed encodings, gaiji/media, grouped indexes, identity, indexing, lookup, restart, corruption, and failure behavior |
| R3.18 | LSA non-corpus icon and large-stream variants: LSA part of `CRD-DICT-003` | R3.5 | generated fixtures for icons, bounded large-file streaming, identity, indexing, lookup/playback, restart, corruption, and failure behavior |
| R3.19 | ZIP sound-pack non-corpus ZIP64/split/encryption/icon/stream variants: ZIP-sound part of `CRD-DICT-003` | R3.5 | generated fixtures for every supported remaining archive/icon/stream case, identity, indexing, lookup/playback, restart, corruption, and failure behavior |
| R3.20 | Sound-directory non-corpus icon and large-stream variants: sound-directory part of `CRD-DICT-003` | R3.5 | generated fixtures for icons, bounded large-file streaming, confinement, identity, indexing, lookup/playback, restart, corruption, and failure behavior |
| R4.1 | Qt 5 Interface Preferences page and backed behavior | R2.3 | focused preference persistence/effect test plus paired page capture |
| R4.2 | Qt 5 Advanced Preferences page and backed behavior | R4.1 | focused persistence/effect test plus paired page capture |
| R4.3 | Qt 5 Audio Preferences page and backed behavior | R4.1 | focused persistence/routing test plus paired page capture |
| R4.4 | Qt 5 Full-text Search Preferences page and backed policy controls | R5.2 | focused policy/persistence test plus paired page capture |
| R4.5 | Qt 5 Scan Popup Preferences page and backed controls | R7.2 | focused adapter/persistence test plus paired page capture |
| R4.6 | Qt 5 Hotkeys Preferences page and backed controls | R7.3 | focused adapter/persistence test plus paired page capture |
| R5.1 | Remaining full-text query/result targeting, excerpt, and highlight parity: `CRD-FTS-001` | R2.3 | focused Core/Widgets/WebEngine tests against pinned Qt 5 evidence |
| R5.2 | Full-text policy, eligibility, scheduling, atomic publication, rebuild, and recovery: `CRD-FTS-002`, `003` | R3.6-R3.20 | lifecycle, cancellation, corruption, replacement, and failure-injection tests |
| R5.3 | Full-text readiness, progress, waiting/indexed counts, and failure UI | R5.2, R4.4 | focused UI state-machine test and paired state captures |
| R5.4 | Full-text real-format acceptance: `CRD-TEST-REAL-008` | R5.3 | every eligible real format indexed, queried, reused, rebuilt, cancelled, and recovered |
| R6.1 | Remaining dictionary/group management, popup muting, auto-group, ordering, and batch actions | R3.6-R3.20 | focused model/UI/persistence tests and paired workflow capture |
| R6.2 | Hunspell and Chinese-conversion registration/configuration | R6.1 | real and generated morphology/conversion lookup plus persistence tests |
| R6.3 | Website encoding, iframe, and resource parity | R2.3 | deterministic local HTTP fixtures and WebEngine presentation tests |
| R6.4 | DICT authentication parity | R2.3 | deterministic authenticated local DICT fixture and redaction tests |
| R6.5 | Forvo playback and external-program environment/audio policy | R4.3, R7.5 | deterministic local audio/process fixtures and containment tests |
| R6.6 | Portable-mode application-local content, source-control restrictions, and self-contained state: `CRD-DICT-004` | R3.2, R6.1 | disposable portable installation covering discovery, disabled Add/Remove, path confinement, restart, upgrade, and package relocation |
| R6.7 | New-profile default English Wikipedia source without eager network access: `CRD-DICT-005` | R6.1, R6.3 | isolated first-run persistence/runtime test and local transport proof of zero requests before user lookup |
| R6.8 | Remaining transliteration provider registration and configuration: `CRD-DICT-002`, `CRD-LOOKUP-001` | R6.1 | Hepburn Hiragana/Katakana provider registration, persistence, and lookup tests plus exact disposition of legacy Nihon-shiki/Kunrei-shiki persisted fields without inventing nonexistent Qt 5 runtime mappings or exposing inert controls |
| R7.1 | Continuous X11 clipboard/selection monitoring and modifier policy | R2.3 | private adapter tests and X11 integration check |
| R7.2 | Scan flag and scan-popup presentation/lifecycle | R7.1 | adapter/UI lifecycle tests and paired captures |
| R7.3 | Global hotkey registration, dispatch, conflict, and teardown | R7.1 | private adapter tests plus X11 session check |
| R7.4 | Tray, close-to-tray, startup, activation, and always-on-top | R7.3 | desktop integration test and paired state capture |
| R7.5 | External player and Linux speech/TTS | R4.3 | controlled process/TTS adapter tests and manual audio check |
| R7.6 | Complete command-line and single-instance behavior: `CRD-LOOKUP-005` | R6.1, R7.2, R7.4 | generated argument/message matrix covering supported main/popup group and scan options, URLs, malformed input, activation-only invocation, forwarding before/after facade publication, process contention, and exactly one writer |
| R7.7 | X11, XWayland, and native-Wayland capability contract: `CRD-PLATFORM-005` | R7.1-R7.6 | platform matrix documenting and testing each supported/fallback/unsupported selection, scan, hotkey, tray, activation, and audio path without representing a platform limit as product removal |
| R7.8 | Windows scan/clipboard, hotkey, tray, startup, activation, always-on-top, and single-instance adapters: `CRD-PLATFORM-001`, `005` | R6.1, R7.2-R7.4, R7.6 | private adapter tests and native Windows integration matrix covering registration, conflicts, lifecycle, groups/options, forwarding, and teardown |
| R7.9 | Windows playback, external player, pronunciation, and SAPI speech: `CRD-PLATFORM-002`, `005` | R4.3, R6.5 | controlled process/media/SAPI adapter tests and manual native audio check |
| R8.1 | Article resource saving and remaining safe context actions | R3.6-R3.20 | local resource fixture, cancellation/error tests, and paired menu capture |
| R8.2 | Qt WebEngine DevTools/inspector parity | R2.3 | private page/profile lifecycle tests and manual DevTools check |
| R8.3 | About, homepage, forum, configuration-folder, credits, and attribution workflows | R9.1 | focused action/URL/path tests and paired dialogs |
| R8.4 | Remaining proxy/authentication, request identity, remote-content, and update behavior | R6.3, R6.4 | deterministic local proxy/origin/update fixtures and redaction tests |
| R8.5 | Help window geometry and zoom persistence | R4.6, R8.3 | focused current/legacy persistence, invalid-state fallback, DPI/topology normalization, and paired Help capture |
| R9.1 | Complete asset provenance/owner/install/consumer inventory and feature-owned imports | R3.6-R3.20 | inventory validator, installed-runtime checks, and hash provenance |
| R9.2 | Translation inventory, Qt 6 context validation rules, fallback contract, and batch assignment | R9.1 | deterministic catalog validator, source/context diagnostics, and exact 45-catalog batch manifest |
| R9.3 | Locale batch A enablement: `ar_SA` through `fi_FI` in the R9.2 manifest | R9.2 | `lrelease`, fifteen locale smokes, fallback tests, and paired representative surfaces |
| R9.4 | Locale batch B enablement: `fr_FR` through `qt_es` in the R9.2 manifest | R9.3 | `lrelease`, fifteen locale smokes, fallback tests, and paired representative surfaces |
| R9.5 | Locale batch C enablement: `qt_it` through `zh_TW` in the R9.2 manifest | R9.4 | `lrelease`, fifteen locale smokes, fallback tests, and paired representative surfaces |
| R9.6 | Matched empty/synthetic main-shell visual matrix: main window, menus, toolbars, tabs, lookup, results, Favorites, History, scan popup, and tray | R4.1, R4.5, R7.2, R7.4, R9.5 | fixed-environment captures, per-surface semantic checklist, and documented narrow masks |
| R9.7 | Matched empty/synthetic settings-and-management visual matrix: seven Preferences pages, Sources, Dictionaries, Groups, dictionary info, headword browser, full-text search, Help, About, and message dialogs | R4.1-R6.4, R6.8, R8.2, R8.3, R8.5, R9.5 | fixed-environment captures, per-dialog semantic checklist, state coverage, and documented narrow masks |
| R9.8 | Matched synthetic article/browser visual matrix: Welcome/article states, navigation, search, zoom, context menus, media, print, save, authentication, error, and DevTools states | R5.1, R6.3-R6.5, R8.1, R8.2, R8.4, R9.5 | deterministic local fixtures, fixed-environment captures, semantic checklist, and documented narrow masks |
| R9.9 | Matched real-corpus visual matrix: `CRD-TEST-REAL-009` | R5.4, R6.2, R9.6-R9.8 | private paired captures and retained non-content metadata/results |
| R10.1 | Linux/Windows clean-build and test CI with pinned profiles/locks | R2.3 | clean runner logs for the same commit |
| R10.2 | Linux runtime bundle, launcher, WebEngine, help, translations, and product assets | R7.5, R7.7, R8.4, R9.5, R10.1 | clean staged-runtime dependency scan and launch/lookup/help/media smoke |
| R10.3 | Linux DEB desktop/MIME integration and install/uninstall | R10.2 | clean package build, metadata inspection, install, desktop/MIME launch, upgrade, and uninstall evidence |
| R10.4 | Linux RPM desktop/MIME integration and install/uninstall | R10.2 | clean package build, metadata inspection, install, desktop/MIME launch, upgrade, and uninstall evidence |
| R10.5 | Windows runtime bundle and WebEngine/help/translation/product-asset deployment | R7.8, R7.9, R8.4, R9.5, R10.1 | clean staged-runtime dependency scan and launch/lookup/help/media/native-adapter smoke without a developer environment |
| R10.6 | Windows installer, application manifest, shortcuts, registration, upgrade, and uninstall | R10.5 | clean installer build and metadata inspection plus install, native integration, repair/upgrade, and uninstall evidence |
| R10.7 | Paired startup/index/lookup/render/full-text/memory/storage measurements: `CRD-TEST-REAL-010` | R5.4, R9.9, R10.3-R10.6 | machine-readable same-host results, thresholds, and disposition of every material regression |
| R10.8 | Same-candidate Linux/Windows cutover audit and documentation reconciliation | all prior leaves | complete build/test/install/package/visual/corpus records and independent final audit |

#### R2.1 issue record and readiness

R2.1 is a minor correction governed by the approved Windows test-gate
requirements in CRD Sections 10.3 and 12. The recorded issue is that the
earlier Windows Release run grouped multiple QtTest process terminations at a
common MSVC exception entry without first reproducing and classifying each
test serially. The conforming outcome is a trustworthy harness diagnosis and,
where the failure is caused by test infrastructure, a correction that keeps
all product assertions intact. Any reproducible product defect remains a
separate functional gap and is not skipped or relabelled as infrastructure.

Impact check: **Ready**. This delivery changes only Windows test execution or
test-fixture mechanics proven to cause the shared exception family. It does
not change product behavior, public interfaces, dictionary data, or the Core/
Widgets ownership boundary. The existing test registration and fixture
patterns remain the design authority; no new abstraction or design pattern is
justified unless the diagnosis demonstrates a repeated lifecycle boundary.

The exact initial reproduction command, run from this leaf's Release build in
a Visual Studio 2026 x64 environment with MSVC 14.44 and the checkout's Conan
environment, is:

```powershell
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j1 --output-on-failure
```

Every member of the exception family is then rerun serially with
`--repeat until-pass:2` disabled and a focused anchored `-R` expression. The
delivery evidence must retain the affected test names, exit/error signatures,
classification, focused post-correction results, and a diff review proving
that no assertion or product contract was weakened.

Completion result: **Complete**. The serial baseline contained 20 affected
QtTest functions in 15 executables, all terminating with Windows status
`0xe06d7363` at `RaiseException`. A clean integration build exposed one more
latent POSIX-only missing-executable fixture in the same external-program
executable, so the corrected gate covers 21 functions. The functions and their
classifications are retained in `docs/testing.md`. After correction, every
affected function passes when run alone. The complete 15-executable set has no
exception-family termination: 11 executables pass and four report only
ordinary, named R2.2 path or failure-injection assertions. The shared BGL,
Dictd, and StarDict fixture consumers also build and their eight focused
executables pass. A full serial 128-test rerun passes 116 tests and leaves 12
ordinary R2.2/R2.3 failures, with no remaining exception-family crash. No
product assertion was removed or weakened; symlink safety assertions still run
whenever the host permits creation of the required symlink.

#### R2.2 issue record and readiness

R2.2 is a minor correction governed by CRD Sections 10.3 and 12. After R2.1,
the clean Windows Release suite passes 116 of 128 tests. Ten executables retain
15 ordinary failures caused by Windows path representation, case-insensitive
filesystem behavior, file-replacement or symlink capabilities, nondeterministic
external network resolution, checkout line endings, or POSIX-only smoke data.
The remaining WebEngine restart/GPU/serialization failures belong exclusively
to R2.3. The original pair is joined intermittently by the proxy restart smoke
when it follows a failed WebEngine restart in the complete serial suite.

Impact check: **Ready**. Portable observable contracts remain unchanged:
transport-neutral paths use their existing generic representation, source
identity checks compare filesystem identity instead of spelling, failure
injection covers operations that exist on the host, and security assertions run
whenever the required symlink capability is available. Tests may use local
deterministic endpoints and platform-valid temporary paths, but may not weaken
redaction, atomicity, confinement, ordering, or migration assertions. A product
change is permitted only if diagnosis proves that an existing cross-platform
contract, rather than a fixture assumption, is broken. Core, Network, and
Widgets ownership boundaries remain unchanged; no new module or abstraction is
justified.

The functional unit covers these executables:

- `aard_dictionary_test`, `application_service_test`, `dsl_reader_test`,
  `hunspell_discovery_test`, `sdict_reader_test`,
  `stardict_dictionary_test`, and `xdxf_reader_test`;
- `http_client_test` and `legacy_catalog_source_inventory_test`; and
- `goldendict_source_directories_smoke`.

Verification requires an independent Release build in this worktree, each
affected function run directly with named output, the ten-executable anchored
gate in serial mode, and the complete 128-test suite through
`run_with_conan.ps1`. R2.2 is complete only when all ten executables pass and
the complete suite leaves no failure outside the documented R2.3 WebEngine
restart/GPU/serialization cluster.

Completion result: **Complete**. A VS 2026 Release build using the pinned MSVC
14.44 toolset and Conan Qt 6.11.1 completed successfully. All ten affected
executables passed three consecutive serial iterations. The HTTP executable
also passed 30 consecutive direct iterations after its deterministic proxy and
transaction-boundary corrections. The previously integrated external-program
gate passed 40 direct and 20 CTest iterations after one non-repeating full-suite
exception observation, and passed in the subsequent complete run. A complete
serial run passed 125 of 128 tests; only
`goldendict_article_click_preferences_smoke`,
`goldendict_proxy_preferences_smoke`, and
`goldendict_interface_language_smoke` remained in the R2.3 cluster. A second
serial run excluding exactly those three passed 125 of 125 tests. Every
executable was launched through `run_with_conan.ps1`; no missing-DLL dialog or
loader failure occurred.

#### R2.3 issue record and readiness

R2.3 is a minor correction governed by CRD Sections 10.3 and 12. The Windows
Release suite isolated three reported failures around WebEngine restart
smokes: article-click Preferences failed while Chromium reported an unusable
GPU context, interface-language startup remained alive until the CTest
timeout, and proxy Preferences failed intermittently in the same serial suite.
Investigation showed that the article-click and proxy scripts did not provide
the Windows smoke contract's shared short configuration root. The
interface-language entry point and translation presenter are Linux-only, and
`docs/testing.md` already defines that smoke as a real Linux application test;
registering it on Windows exercised the normal interactive event loop rather
than any interface-language assertion. The article-click smoke also exposed a
real cross-platform edge: Chromium word selection can include trailing
whitespace, and its fixed delay could advance before the asynchronous query
completed.

Impact check: **Ready**. The required product behavior is already explicit:
Preferences must persist across a clean application restart, while the test
harness must be deterministic on supported headless Windows hosts. The
correction shares a short, disposable configuration root across both processes
in each Windows restart test, keeps the existing software-rendered offscreen
path, registers the interface-language smoke only on its implemented Linux
platform, trims whitespace from the selected word and visual selection, and
sequences smoke assertions from query completion. It does not bypass persisted
configuration, weaken restart assertions, change module ownership, or move
presentation responsibilities into Core. No public product interface, module
boundary, dependency, or new design abstraction is justified.

Completion result: **Complete**. A VS 2026 Release build using MSVC 14.44,
Conan, and Qt 6.11.1 completed all 648 remaining build steps. The article-click
and proxy restart smokes each passed ten consecutive serial iterations. The
final Windows-applicable suite passed 127 of 127 tests in 43.17 seconds.
Every executable and CTest run used `run_with_conan.ps1`; Chromium's expected
software-rendering diagnostics did not affect results, and no missing-DLL
dialog, timeout, or orphaned GoldenDict/WebEngine process remained. The
Linux-only interface-language registration remains part of the Linux gate and
was not represented as Windows coverage.

#### R3.1 issue record and readiness

R3.1 is a conformance delivery under approved requirements `CRD-STATE-004`
and `CRD-COMPAT-002` through `CRD-COMPAT-005`. The current startup path
migrates configuration, History, and Favorites independently. A later parse
or publication failure can therefore leave only part of the Qt 6 profile
published, and a current-configuration load failure can continue with default
state that a later save may publish over the unread file. The current
line-oriented Qt 6 format also rejects every unknown field instead of
round-tripping syntactically valid future fields. These behaviors do not meet
the approved atomicity, forward-compatibility, diagnostic, or rollback-safety
requirements.

Readiness result: **Ready**. Core application persistence owns one aggregate
startup-upgrade use case. It will parse and stage every missing current state
file before publishing any of them, expose a recoverable pending marker while the
multi-file publication is incomplete, and complete or diagnose that exact
transaction before any state becomes visible to the application. Existing
current files retain per-file precedence and legacy inputs remain byte-for-byte
unchanged. The application composition root consumes the aggregate result and
stops startup on an unresolved load or upgrade error; it does not substitute
defaults. The current configuration representation will carry bounded,
well-formed unknown records through load and save while continuing to reject
malformed records and invalid known fields.

This design applies a transaction/coordinator pattern at the demonstrated
three-file consistency boundary. It keeps parsing and persistence in Core,
leaves Widgets responsible only for presenting a clear startup error, and
adds no module, format-specific dependency, or speculative service layer.
Disabled legacy website definitions whose iframe or query-encoding behavior
belongs to R6.3 may be retained as configuration state, but they may not be
activated until their runtime behavior is supported. An enabled unsupported
definition remains a detectable migration failure instead of being silently
discarded.

The functional unit includes the aggregate upgrade API and startup wiring,
forward-compatible current-configuration preservation, generated valid,
malformed, interrupted-publication, retry, and failure-injection tests, and a
disposable copy of a genuine Qt 5 profile. Verification uses the focused Core
and application migration executables, a direct disposable-profile run,
`git diff --check`, a clean Visual Studio 2026 Release build, and the complete
Windows-applicable CTest suite through `run_with_conan.ps1`. No source edit or
test may modify the frozen Qt 5 checkout or an operator profile.

#### R3.1 delivery verification

The implementation candidate now coordinates configuration, History, and
Favorites preparation and publication in Core. A bounded pending record makes
every publication checkpoint recoverable by exact forward completion before
startup can observe state. Preparation failures remove all staging artifacts;
published destinations are never replaced by defaults; current files retain
precedence; and the legacy sources remain untouched. Startup reports the
underlying failure to the diagnostic log, presents a clear data-loss-prevention
message, and stops. Current-format saves retain bounded, syntactically valid
future records. Qt 5 compatibility also preserves disabled iframe and legacy
query-encoding website records, plus a disabled Forvo definition with an empty
language list, without activating behavior that still belongs to R6.3.

Windows verification used Visual Studio 2026 with the Conan Release environment:

- the clean configured build completed all 791 compile and link steps;
- the focused configuration, legacy-location, user-state-upgrade, runtime
  composition, and source-management selection passed 5/5;
- the generated upgrade test covered malformed configuration, History, and
  Favorites plus failures at pending-marker publication, each of the three
  destination publications, and pending-marker removal, with successful exact
  restart recovery; it also pins Qt 5's default-true behavior when a disabled
  website omits `inside_iframe`;
- the source-management smoke preserves and reapplies a disabled Forvo record
  whose Qt 5 language list is empty;
- a disposable copy of the genuine Qt 5 portable profile at SHA-256
  `B613C5DAA34E26F1BA63ED8F481566C229F35C4561504FD3BEB02F0FB78CECF0`
  upgraded successfully, and the source hash and frozen Qt 5 checkout remained
  unchanged;
- the complete Windows-applicable suite passed 128/128 serially through
  `run_with_conan.ps1`; and
- `goldendict_article_tabs_smoke` passed 20/20 consecutive isolated repeats.

Two parallel full-suite attempts exposed the existing shared WebEngine GPU
resource contention: one or both WebEngine article-tab processes could not
create a shared graphics context while multiple GUI smokes ran concurrently.
The failing processes passed when isolated and in the complete serial suite,
and no R3.1 code owns WebEngine rendering or CTest resource scheduling. The
result is retained as non-blocking test-infrastructure evidence rather than
being hidden or bundled into this functional delivery.

#### R3.2 issue record and readiness

R3.2 is a conformance delivery under approved requirements
`CRD-TEST-REAL-003`, `CRD-TEST-REAL-004`, and `CRD-COMPAT-001`. R1.1 and
R1.2 already bind an immutable payload-free manifest, matched run conditions,
exact Qt 5 and Qt 6 revisions, and disjoint writable profiles around the same
read-only corpus. They deliberately stop before observing product discovery.
The remaining gap is a version-neutral, machine-readable record of successful
and failed discovery, stable identity, configured enablement and order, article
and headword counts, index publication, warm reuse, explicit rescan, and the
recovery cases required by the CRD.

Readiness result: **Ready, split into independently auditable functional
units**. The frozen Qt 5 product exposes the required state inside its existing
dictionary collection and initialization callback but has no machine-readable
acceptance entry point. The Qt 6 Core exposes successful identities through
`DictionaryService::GetCatalog`; discovery/open failures are currently visible
only as diagnostics attached to lookup and suggestion responses, and initial
index work has no purpose-built progress/cancellation observation contract.
Those limitations are evidence to record, not permission to infer successful
parity or to rewrite format implementations before a paired run identifies an
actual difference.

The delivery uses an adapter-and-orchestrator design:

1. R3.2a defines one bounded canonical JSON observation schema, a strict
   structural/semantic validator, deterministic normalization and comparison,
   and synthetic tests. It extends the existing paired workspace rather than
   creating another authority for corpus, revision, condition, or directory
   identity. Result files contain metadata and diagnostics only, never article
   text, dictionary payload, credentials, or absolute retained corpus paths.
2. R3.2b supplies version-specific read-only observers behind that contract.
   The Qt 6 observer uses the installed headless Core boundary. The Qt 5
   observer is built from the exact frozen revision in a disposable source
   copy with a narrow acceptance-only instrumentation patch; it never edits or
   relinks the frozen checkout and is not a shipping product binary. Both
   observers acknowledge the R1.2 pair and conditions before a result can be
   accepted.
3. R3.2c runs clean discovery, warm restart, and explicit rescan against the
   immutable real corpus, then runs changed-source, cancellation, and missing-
   companion cases only against generated fixtures or bounded disposable
   copies. It retains paired canonical result files and a complete difference
   report. A confirmed Qt 6 product defect returns to implementation as its own
   coherent correction and test; an intentional divergence or unsupported Qt
   5 behavior still requires explicit approval.

The common result contract records the pair ID, exact revision and version,
condition and manifest hashes, scenario, monotonic phase sequence, completion
outcome, normalized dictionary identity and source-relative component list,
enabled/order disposition, article and headword counts, typed diagnostics, and
bounded index-file metadata. The comparator treats version-specific generated
index names and timings as evidence rather than equality keys. It compares
logical source identity, order, enablement, counts, outcome, and diagnostic
class, while preserving both raw version-specific values for audit.

The design keeps discovery and indexing in Core, orchestration in repository
test tooling, and presentation out of the headless observer. Production API
changes are permitted only where a real required state cannot otherwise be
observed or controlled, and must remain transport-neutral, bounded, explicitly
cancellable, and independent of Qt Widgets. Repeated scenario sequencing uses
a state-machine coordinator; format-specific parsing remains private and no
new shared-library module is justified.

The initial verification set is `python` unit tests for schema validation,
canonicalization, comparison, path redaction, output bounds, acknowledgement,
failure atomicity, and corpus immutability; focused Core tests for any added
observation/cancellation contract; a clean Visual Studio 2026 Release build;
and the complete serial CTest suite through `run_with_conan.ps1`. Real runs use
`D:\workspace\goldendict\content` and the approved manifest whose SHA-256 is
`b7e91878649b61388ecb1a3713709685e243f57e60d2b8eb23838a91bba816d2`.
Before and after every scenario, the runner revalidates the corpus and rejects
an incomplete acknowledgement or any missing, partial, or non-canonical final
result. Repository-owned observers must publish through the contract's atomic
same-directory writer; post-exit validation does not claim to infer the write
method used by an arbitrary child process.

#### R3.2a delivery verification

R3.2a implements the shared observation and comparison contract without
changing product discovery. The existing paired runner can now require a fresh
`observation.json`, projects its exact destination to the child, and validates
the result against the selected version and all four R1.2 identity bindings
after the child exits and after the corpus is revalidated. The reusable result
module validates canonical structure, bounds, normalized relative paths,
unique identities and order, canonical diagnostic classification tokens,
contiguous phase sequences, index dispositions, and field-specific comparison
values, valid UTF-8 text, and unique ordered records that describe actual
Qt 5/Qt 6 differences. Result reads stop at the size bound plus one byte; both
observation and comparison publication use same-directory atomic replacement.

The comparator reports material logical differences while retaining generated
dictionary IDs and complete index metadata as version-specific evidence. It
does not compare generated IDs, index filenames, index bytes, or elapsed time
as product-equivalence keys. The repository-wide dependency-free script gate,
`python -m unittest discover -s scripts/tests -p "*_test.py"`, passes 58 tests
with one host-capability symlink skip: 18 result, 23 workspace, 11 manifest,
and 6 Conan-launcher tests. Coverage includes stale-result rejection,
identity mismatch, path confinement, bounded reads, size and schema bounds,
failed atomic replacement for both output types, generated-index tolerance,
and typed material-difference reporting, including hostile Unicode input and
false-difference rejection and index-evidence cross-checking.
`py_compile` and `git diff --check` also pass. No C++ target, dependency,
shipping binary, or frozen Qt 5 file changes in this functional unit.

#### R3.2b observer delivery split and Qt 6 verification

R3.2b is split at the version-adapter boundary so each observer is independently
reviewable and the frozen Qt 5 instrumentation cannot obscure changes to the
Qt 6 acceptance path. R3.2b1 provides the shared raw-to-canonical adapter and a
test-only Qt 6 observer. R3.2b2 adds the disposable-source Qt 5 observer and
reuses the same adapter contract. Neither target is a shipping product feature,
and neither changes dictionary discovery ownership.

The R3.2b1 adapter validates every paired-workspace identity binding before it
launches a probe, snapshots the isolated index directory before and after the
run, confines observed components to the immutable corpus and approved
manifest, expands known DSL and MDict companion sets, converts transient
free-form failures to retained typed diagnostics, and publishes an atomic final
observation followed by an atomic acknowledgement commit marker. Either final-
write failure removes both outputs. Failed probes, malformed raw data, path
escape, or binding mismatch publish no final evidence. The raw result is
bounded and deleted when the adapter exits. Probe-supplied arguments cannot
override the adapter-owned corpus, index, locale, condition, scenario, or
output options, and the Qt 6 process independently rejects duplicate or unknown
options.

This unit deliberately implements only `clean-discovery`. It accepts the bound
all-enabled group with no preset dictionary order, the clean-default preference
profile, no queries, and a host-matching operating system and architecture; it
applies the bound locale to Core and requires an empty isolated index directory.
It rejects all other scenarios before launch.
The raw observer, rather than the adapter, records authoritative enablement,
catalog order, condition hash, scenario, outcome, and actual discovery phase
completion. Warm restart, explicit rescan, changed-source, cancellation, and
unavailable-companion state sequencing remain R3.2c work and cannot be
misreported as completed by R3.2b1.

`qt6_real_dictionary_observer` is built only with `BUILD_TESTS`, is not
installed, and calls `CreateDictionaryService` plus the public catalog and
lookup boundaries. It therefore observes the product's headless Core behavior
without adding acceptance concerns to production APIs or duplicating format
discovery. The repository-wide Python gate passes 68 tests with one
host-capability symlink skip, including ten adapter tests for successful
projection, manifest-bound DSL/MDict companions, pre-launch condition binding,
reserved-option rejection, unsupported-scenario rejection, both final-write
failures, nonempty clean-discovery index rejection, path escape, and child
failure. Black, Ruff, `py_compile`, and
`git diff --check` pass. A Visual Studio 2026/MSVC 19.44 Release build completes,
and a clean Conan-launched serial CTest run passes all 130 tests, including the
two strict C++ option-parser rejection tests. An initial
`external_program_source_test` process crash passed immediately in isolation
and in the complete clean rerun; no adapter or Core observer path participates
in that test.

The Conan-launched end-to-end run against a generated real DSL fixture reports
the expected name, English-to-German languages, one article, one headword, and
one newly created isolated index, and retains no absolute corpus path. The full
approved corpus remains the R3.2c paired-run gate after both version adapters
exist.

#### R3.2b2 Qt 5 observer delivery verification

R3.2b2 supplies the frozen-version adapter without modifying the pinned Qt 5
checkout. `prepare_qt5_acceptance_source.py` first requires the exact frozen
commit and tree and a completely clean checkout, exports that commit with
`git archive`, rejects output paths inside the checkout and unsafe archive
members, and applies exact-anchor instrumentation only to the disposable copy.
The resulting provenance binds
the frozen commit and tree, the checked-in observer include, and SHA-256 hashes
of every instrumented source file. Its qmake version resource is fixed to the
frozen revision instead of probing a surrounding worktree. The runtime wrapper
validates those bindings again before launch.

The injected observer reads the dictionary objects produced by the existing
Qt 5 discovery and group-construction path. It emits only bounded identity,
relative component, language, count, phase, outcome, and index metadata for the
shared adapter; it neither parses dictionary formats independently nor retains
article text. The acceptance-only patch also redirects indexes to the paired
workspace, bypasses single-instance forwarding only for an acceptance run, and
terminates after atomic raw-result publication. Two narrowly scoped Windows
compatibility shims exist only in the disposable source: the legacy
`CHANGEFILTERSTRUCT` fallback is disabled when current MinGW headers provide it,
and accessibility `WM_GETOBJECT` requests are ignored to keep old Qt 5 UI
automation from destabilizing the observer process.

The Python wrapper creates a disposable profile under the paired configuration
root, writes the legacy platform-specific configuration location, supplies the
exact corpus and index bindings, rejects portable state,
confines raw output to the adapter's evidence directory, and prepends explicit
Qt 5 runtime and plugin paths. It suppresses Windows loader/fault dialogs while
preserving the process error mode. Windows uses the native platform plugin
because this Qt 5 WebKit build crashes in the offscreen plugin; other platforms
remain offscreen. This is a harness constraint and does not change observed
dictionary behavior.

The focused 16-test suite covers checkout identity and cleanliness, safe export,
source/output separation, changed anchors, complete instrumented-file
provenance, provenance tampering, cross-platform profile placement,
runtime/plugin projection, output confinement, portable-state refusal,
revision mismatch, and failed-process cleanup. A fresh
MSYS2 UCRT64 Qt 5 build
from the prepared source completes with the frozen checkout still clean. An
end-to-end run using the real corpus file `中文/古汉语常用字字典.dsl.dz` observes
the expected dictionary name, Chinese-to-Chinese languages, 3,889 articles and
headwords, and one newly created isolated index. The shared validator accepts
the canonical result, and no absolute corpus path or dictionary payload is
retained. Full-corpus paired scenarios remain R3.2c work.

The repository-wide dependency-free Python gate passes 84 tests with one
host-capability symlink skip. Black and Ruff pass for all R3.2b2 Python files,
and `py_compile` plus `git diff --check` pass.

#### R3.2c implementation plan

R3.2c is executed as four independently reviewable functional units so a
confirmed product gap does not invalidate unrelated acceptance evidence:

1. Extend both version observers and the shared adapter with the immutable
   corpus lifecycle states `clean-discovery`, `warm-restart`, and
   `explicit-rescan`. A resumable coordinator owns their order, requires
   stable dictionary and index identity, requires created indexes on the clean
   run and reused indexes thereafter, archives every canonical observation,
   and produces one comparison per state plus an aggregate summary.
2. Run that lifecycle for Qt 5 and Qt 6 against all 88 files in the approved
   read-only corpus and retain the complete local comparison. Each confirmed
   Qt 6 defect becomes its own correction delivery with focused fixture
   coverage before the full lifecycle is repeated.
3. Add changed-source and unavailable-companion lifecycle states using bounded
   disposable copies or generated fixtures. The immutable operator corpus is
   never edited, copied wholesale, or used for destructive cases.
4. Add bounded cancellation/progress observation at the existing Core
   orchestration boundary, pair it with the Qt 5 behavior, and retain recovery
   evidence. Any required production API remains transport-neutral,
   cancellable, and independent of Qt Widgets.

Units 1 through 3 are integrated. Unit 4 is completed by the cancellation and
recovery delivery described below. R3.2 is complete when this delivery is
independently audited and integrated.

The Unit 1 delivery was exercised end to end with the real corpus file
`中文/古汉语常用字字典.dsl.dz`. Both versions completed clean discovery, warm
restart, and explicit rescan. Qt 5 created one headword index and reused it in
the two later states; Qt 6 created one full-text index and likewise reused it.
The Qt 6 rescan state publishes a distinct unchanged replacement through the
Core facade-activation owner used by production source reloads, while the
separate File-menu rescan action remains owned by R7.2.
The aggregate comparison therefore reports two semantic index-role differences
per state rather than incorrectly declaring equivalence. This is actionable
Unit 2 parity evidence, not a lifecycle-runner failure. The disposable Qt 5
source with the actual `on_rescanFiles_triggered` path builds successfully,
the complete Qt 6 Release build succeeds, all 130 registered Qt 6 tests pass
through the Conan runtime launcher, and the repository-wide Python gate passes
102 tests with one documented host-capability skip. A focused Core test proves
that an unchanged source configuration publishes a distinct facade through the
same activation owner and leaves the replaced service stopped.

Interrupted clean discovery is resumable without trusting an unarchived
observation: the coordinator safely clears the validated version workspace's
mutable state back to clean-run preconditions and reruns the scenario. It
refuses links, Windows reparse points, and non-regular entries while doing so.
Every resumed archive is rebound to the validated pair's separate corpus-
manifest and acceptance-condition hashes before it can contribute evidence.
The real DSL Qt 6 smoke was then repeated after removing its archive prefix
while retaining the generated index and other run state; the coordinator reset
the isolated state, recreated the index, and completed all three scenarios.

The first Unit 2 correction closes the confirmed Oxford split-MDD aggregate
loading defect without changing the approved requirements or public
architecture. The Qt 6 reader now indexes immutable MDD key and record-block
metadata and reads only the requested bounded resource range, including exact
assembly across block boundaries and consecutive volumes. Generated tests pin
cross-block bytes, changed-source rejection, and common error translation. A
Conan-launched smoke against the approved corpus completes clean discovery,
warm restart, and explicit rescan with 17 dictionaries, zero errors, and the
same Oxford MDict ID and 238,766 article/headword counts in every state. This is
correction evidence within Unit 2; R3.2 and R3.3 remain incomplete until the
paired Qt 5 lifecycle and actual resource-query catalog are accepted.

The refreshed Unit 2 lifecycle at Qt 6 revision `0ff83d24` discovers the same
17 logical dictionaries as Qt 5 with zero diagnostics in all three states. It
removes both prior missing-dictionary differences and confirms one remaining
MDict metadata defect: the Oxford file has an empty header title, for which Qt
5 uses the filename before its first extension while Qt 6 removed only the
final `.mdx` extension. The focused follow-up correction applies the general
legacy invalid-title fallback and generated coverage; it does not change the
still-open ordering, DSL headword-count, index-role, or R3.3 query evidence.

The next focused Unit 2 correction replaces Qt 6 stable-ID catalog sorting
with the frozen Qt 5 clean-profile discovery order. A private Core ordering
strategy retains configured-root order, legacy depth-first directory and
format precedence, case-insensitive names, and the later sound-directory,
Hunspell, and runtime-source phases. Generated mixed-format coverage and the
paired lifecycle corpus are the acceptance evidence; index-role interpretation
and R3.3 query evidence remain open.

The following focused Unit 2 correction closes the DSL headword-count defect at
the format-adapter boundary. The private preprocessor now follows the frozen
Qt 5 ordering for unsorted-zone removal, optional expansion without duplicate
elision, per-source-line result limits, the 500-code-point guard, escaped
syntax, normalization, and alternate `~`/`^~` replacement. Per-line traversal
terminates when its 32-result budget is full, and each successive alternate
resolves tildes against the current first merged record. Supplementary-plane
case inversion retains the legacy 16-bit uppercase predicate. The separate
article display path retains unsorted-zone contents for body-tilde replacement
and uses escape-aware traversal for literal backslash and doubled-bracket
forms, non-breaking escaped spaces, and compound terminators. Generated
exact-record, adversarial-bound, lookup, rendering, and boundary coverage
passes. Full-text canonical headword ownership and the pre-insertion record
ordinal remain bound to the first nonempty expansion from the first source
line, independent of lookup-record size filtering and merged order. Empty
legacy expansions remain in the reported count but are excluded from lookup
and enumeration. A clean discovery of the complete approved corpus, launched
through Conan, reports zero diagnostics and exact Qt 5 article and headword
counts for all 16 DSL dictionaries, including all eight dictionaries that
previously differed. This correction does not claim abbreviation expansion,
resource ZIP support, nested cards, index-role equivalence, or R3.3 query
acceptance.

The next Unit 2 correction resolves the remaining cross-version index-role
interpretation at the acceptance-contract boundary. This is a minor
correction under the already approved R3.2 requirements: Qt 5 and Qt 6 use
different private index implementations, and the CRD requires observable
creation and reuse rather than identical private roles, filenames, bytes, or
counts. The current comparator nevertheless keys material index differences by
`(logical dictionary, private role)`, so a Qt 5 headword index and a Qt 6
full-text index with the same successful lifecycle are reported as two product
differences.

Readiness result: **Ready**. The observation contract already retains each
version's complete role-specific metadata, while the lifecycle coordinator
independently requires stable artifact identity, clean-run creation, and reuse
on restart and rescan. The correction therefore changes only the
cross-version projection: it compares lifecycle dispositions only when both
versions publish the same semantic role for the same logical dictionary, and
retains all version-only roles, filenames, hashes, sizes, and timings under
`version_specific`. Each version's coordinator remains authoritative for
whether its own private artifacts were created and reused. Focused tests must
cover differing or version-only private roles, mismatched dispositions for a
shared role, multiple private roles, canonical ordering, and validator
cross-check rejection. The repository-wide Python gate and a fresh paired
real-corpus lifecycle comparison are the acceptance evidence; no product API,
format reader, index serialization, or shipping binary changes are in scope.

Completion result: **Complete**. The focused comparison suite passes 21 tests
and the repository-wide dependency-free gate passes 105 tests with one
documented host-capability skip. A clean VS 2026/MSVC 14.44 Release configure
and 802-step build succeeds, and the complete Conan-launched serial CTest suite
passes 130/130 in 165.29 seconds from a fresh short Windows temporary root. The
fresh paired workspace
`evidence/qt5-qt6-lifecycle-index-equivalence-96a04/` validates the immutable
88-file corpus before and after every state. Both versions discover the same
17 dictionaries with matching identity, order, enablement, article and
headword counts, phases, outcomes, and diagnostics in clean discovery, warm
restart, and explicit rescan. Each version's complete observed index set is
created on the clean run and reused byte-for-byte thereafter. All three
canonical comparisons report zero material differences while retaining the
17 Qt 5 headword artifacts and 15 Qt 6 full-text artifacts separately.

A bounded diagnostic probe confirms that the Qt 6 Britannica and Oxford
full-text artifacts are absent because each corpus exceeds the current private
256-MiB full-text byte bound. This does not invalidate discovery/index
lifecycle equivalence: Qt 5's artifacts are compact headword indexes, not
equivalent full-text artifacts. The limitation remains visible in the retained
version-specific evidence and must be exercised as user-visible query behavior
under R3.3 and R3.4; this correction neither hides it nor expands the private
index format or memory contract prematurely.

The Unit 3 recovery delivery adds a bounded mutation adapter and a separate
four-state coordinator for clean discovery, changed source, unavailable
companion, and companion recovery. The adapter is confined to a marker-bearing
disposable corpus, permits only normalized regular files up to 64 MiB, touches
only source time metadata, quarantines one companion outside the corpus, and
restores it on every normal or failed child exit. The generated StarDict
fixture supplies the marker and language-aligned comparison metadata; the
operator's 88-file corpus remains untouched. Both version observers project
the new phases through the existing raw-to-canonical adapter. Internal index
snapshots now retain modification time so a deterministic Qt 5 rewrite is not
misclassified as reuse, without changing the canonical evidence schema.

The first paired run exposed that frozen Qt 5 removes stale generated indexes
when the dictionary disappears, while Qt 6 retained them. The focused product
correction restores the legacy service-level cleanup after all local and
runtime dictionary identities are known. It removes only regular `.gdidx` and
`.gdfts` files whose stem is not active, preserves unrelated files and
directories, reports cleanup failures without aborting unrelated dictionary
loading, and requires recovery to recreate the artifacts. Generated Core
coverage and the real executables confirm removal and recreation for both
headword and full-text indexes.

The fresh paired development workspace reports zero material differences for
all four lifecycle states. While a StarDict companion is unavailable, Qt 6 now
matches the frozen Qt 5 loader by skipping the dictionary without exposing a
structured lookup error; stale generated indexes are still removed and later
recreated after recovery. Unit 3 does not change public APIs, index formats,
dictionary identity, Widgets, or the frozen Qt 5 checkout.

The repository-wide dependency-free Python gate passes 116 tests with one
documented host-capability skip. A VS 2026/MSVC 14.44 Release build succeeds,
and the complete Conan-launched serial CTest suite passes 130/130 after the
diagnostic-alignment correction. The paired development
workspace is `evidence/qt5-qt6-recovery-workspace-u3-final4/`, bound to pair
`b5b4e1017415c8485733f622b0dbe1871f39c50193f548812f355edf1ec5ed13`;
it contains only the generated fixture, isolated runtime state, and canonical
metadata evidence.

The Unit 4 delivery adds a bounded deterministic 100,000-article Aard fixture
and a resumable two-state coordinator for cancellation and recovery. Both
version observers publish the same dictionary-bound `started` transition,
followed by `cancelled` or `completed` as appropriate. The coordinator requires
an empty full-text-index precondition before cancellation and a current artifact
after recovery while retaining implementation-private index roles and bytes as
version-specific evidence.

The first Qt 6 attempt exposed a product lifecycle defect: Aard construction
synchronously built missing, stale, or corrupt full-text artifacts before the
facade and its lifecycle state could be observed or cancelled. The focused
correction makes startup load only a validated current artifact and leaves all
other work to the existing serial Core executor after activation. Executor
shutdown now cancels both active and still-requested generations. The private
inspection seam used by the acceptance observer remains inside Core, does not
alter the installed service interface, and preserves the transport-neutral,
headless architecture. Installed direct service and facade factories retain
owned automatic activation and joined shutdown, while prepared replacement
candidates keep their explicit publish-time activation boundary.

The fresh paired Windows workspace
`evidence/qt5-qt6-cancellation-workspace-u4-final3/` is bound to pair
`e72c873d6717e7ff10595f65b4d12844bd132bc0cfbf64f9d63921ee6731b332`.
Its Qt 6 cancellation observation waits for active lifecycle work before
requesting cancellation. The cancellation and recovery comparisons each report
zero material differences. The generated fixture, immutable manifest, isolated
state, phase records, and version-specific index metadata are retained outside
the repository. Focused Aard, application-service, full-text lifecycle,
fixture, observer, and coordinator tests cover current reuse,
missing/stale/corrupt background work, shutdown, cancellation, recovery,
confinement, and contract validation. Resumability includes deterministic
cleanup of an unarchived recovery attempt that already produced an index before
failing. R3.2 is therefore complete after the audited delivery reaches the Qt
6 baseline; R3.3 and later query/resource acceptance remain separate open
leaves.

### R3.3 development readiness

Readiness result: **Ready** (2026-09-04).

R3.3 implements the already approved `CRD-DICT-003`, `CRD-COMPAT-001`, and
real-corpus acceptance requirements. It does not introduce a new product
requirement or intentional divergence. The scope is the single MDict corpus
in the approved external manifest: its MDX, base MDD, `.1.mdd`, and `.2.mdd`
components; exact lookup and article selection; representative referenced
resource retrieval; warm restart; and source immutability. The broader
suggestion, alternate-writing, missing-word, Unicode, punctuation, multi-word,
morphology, rendered-media, and management matrices remain assigned to R3.5.
Non-corpus MDict compression, encryption, and legacy variants remain R3.13.

Acceptance orchestration and canonical comparison stay in bounded Python
tools. Version-specific observers call the existing Qt 5 dictionary contract
and the installed Qt 6 headless `DictionaryService`; they do not add a public
Core interface or move backend behavior into Widgets. The private corpus is
an operator input, query/resource catalogs and run results stay under the
isolated evidence root, and the repository retains only generated-fixture
tests plus non-content hashes and summaries. Every run validates the existing
manifest and conditions bindings, requires the four ordered MDict components,
confines mutable profile/index/output state outside the corpus, and verifies
all source sizes and SHA-256 values again after observation.

The pre-pair catalog is frozen at
`evidence/r3.3-mdict-query-resource-catalog-v1.json`, SHA-256
`7ececd4a6d2fae85c9ddbf6b0bf5042354e8023870916cfb86916a76147f2d29`.
It is bound to the approved real-corpus manifest SHA-256
`b7e91878649b61388ecb1a3713709685e243f57e60d2b8eb23838a91bba816d2`
and to `evidence/qt5-qt6-acceptance-conditions-r3.3-v1.json`, whose
canonical acceptance-condition SHA-256 is
`a58bb158d54c1d0d6e60f3efc29c19023273da14261f296bcff04cd4be0319f7`.
The conditions file contains the same three exact queries rather than the
empty lifecycle-query set used by R3.2.
The catalog fixes three exact headword probes and three article-referenced
resource IDs before either product result is collected. Read-only MDD key
enumeration confirms that one selected resource is unique to the base MDD,
one to `.1.mdd`, and one to `.2.mdd`; the catalog records each component,
resource size, and resource SHA-256. A repeated 88-file corpus manifest after
that preflight remains byte-identical to the approved manifest.

Delivery is split at evidence-driven functional boundaries:

1. Add the paired MDict lookup/resource result contract, coordinator, Qt 5 and
   Qt 6 observer adapters, generated contract tests, and the reproducible
   external-corpus run procedure. The minimum retained evidence is all three
   frozen exact MDict lookups with deterministic article identity and content
   hashes; successful retrieval and expected byte/hash validation of the
   cataloged base-MDD, `.1.mdd`, and `.2.mdd` resources through each product's
   normal resource API; a clean-to-warm restart comparison; complete ordered
   four-component provenance; unchanged manifest bindings; and zero
   unexplained material differences.
2. If the paired run exposes product behavior differences, deliver each
   coherent backend correction separately under the same approved
   requirements, with generated regression coverage and a repeated paired
   run. Do not weaken the comparator or normalize away a product difference.
3. Close R3.3 only after the final paired evidence is equivalent, the Qt 6
   Release build and focused MDict/application tests pass, the cumulative
   CTest and Python suites pass, and the completion and integration audits
   accept every delivery.

Unit 1 implementation result: **Acceptance implemented; product difference
confirmed** (2026-09-05). The bounded coordinator and version adapters now
collect all three exact articles and their cataloged base-MDD, `.1.mdd`, and
`.2.mdd` resources through each product's normal APIs in clean and warm states.
The first pair retained exact resource sizes and SHA-256 values in all four
observations, complete ordered four-component provenance, stable same-version
clean/warm observations, and unchanged manifest/conditions/catalog bindings.
The published comparison (external SHA-256
`948f14e7f539e1733d240cd0b693b065f5667e70386347fb43594c77af51b45f`)
correctly reports a material difference: Qt 6's current sanitizer escapes the
Oxford dictionary's custom article tags into visible preformatted text, while
Qt 5 renders the custom markup. This is an explained product gap, not an
acceptance-harness failure. It remains assigned to the next separate R3.3
backend correction; the comparator and frozen expectations remain unchanged,
and R3.3 remains open until a repeated pair is equivalent.

Unit 2 issue and readiness: **Ready** (2026-09-05). Unit 1 proves that the
Oxford article body is well-formed custom HTML-like markup containing
hyphenated and namespace-qualified element names plus valueless attributes.
The current sanitizer rejects that syntax before applying its allowlist and
therefore exposes the entire source as escaped preformatted text. The
conforming correction extends lexical recognition of those two safe syntax
forms and browser-compatible recovery for unmatched closing tags. Unknown
elements remain transparent, every unknown attribute is dropped,
active-content elements remain suppressed, and recognized links and resources
pass through typed rewrite and confinement rules. A closing tag that cannot
correspond to any tracked open safe or suppressed element is ignored, while an
actual tracked nesting mismatch or unterminated element still becomes inert
text. MDict `sound://` references are rewritten to confined internal resource
URLs and collected through the same bounded resource-reference path as images
and media sources. No public interface or ownership boundary changes. Focused
generated tests must pin the accepted custom syntax, recovery behavior,
audio-reference rewrite, and existing security behavior. Completion requires
a repeated paired real-corpus run whose visible text is equivalent while all
three exact split-MDD resources and clean/warm bindings remain unchanged.
The first corrected comparison isolated one final difference: every Qt 6
article retained the MDX record's trailing NUL terminator, while the Qt 5
conversion omitted it. The same unit therefore removes only trailing record
terminators at the MDict decoding boundary, preserves embedded NUL bytes and
raw record offset/size provenance, and pins that distinction in a generated
reader test.

Unit 2 implementation result: **Equivalent** (2026-09-05). The final fresh
paired workspace is
`evidence/qt5-qt6-mdict-r33-unit2-final3`; its pair ID is
`e7c84bdf6f64b0798fd0b614a63a66e11f78ae07daac2e96cd03dac45e1c0923`.
The comparison has external SHA-256
`307d65659b95ce880eea577df6d3392d7ec6c9c6cc3c08e84827a771cc5b946d`
and reports `equivalent: true` with no differences. Qt 5 and Qt 6 have equal
visible text for all three exact articles in clean and warm states; the base
MDD, `.1.mdd`, and `.2.mdd` resources retain their exact cataloged sizes and
SHA-256 values; same-version clean/warm results are stable; and both products
published valid condition acknowledgements. The Qt 6 Visual Studio 2026
Release build, all 131 CTest tests, and all 129 Python tests passed with one
intentional skip. R3.3 is complete when this audited delivery reaches the Qt 6
baseline; the broader R3.5 and R3.13 matrices remain open and unchanged.

### R3.4 real-DSL resource-ZIP acceptance readiness

Readiness result: **Ready, split into evidence-driven functional units**
(2026-09-05).

R3.4 implements the already approved `CRD-DICT-003`, `CRD-COMPAT-001`, and
`CRD-TEST-REAL-004` through `CRD-TEST-REAL-006`; it does not change product
requirements. R3.2 is complete and provides the manifest-bound, read-only,
paired-workspace lifecycle required by this leaf. The frozen Qt 5 DSL backend
discovers adjacent `.dsl.files.zip` and `.dsl.dz.files.zip` archives and reads
their indexed members through the dictionary resource contract. The current
Qt 6 DSL backend discovers only adjacent resource directories, so the first
pair is expected to expose a product gap rather than prove equivalence.

The approved corpus contains five resource archives that have matching DSL
sources and one orphan archive that must remain unowned. One matching archive
uses a ZIP64 end record for 117,421 entries; three classic archives have full
central directories while their 16-bit entry counts wrap modulo 65,536. The
largest archive is 4,090,975,853 bytes (greater
than 4 GB decimal, but less than 4 GiB); its 4,482,738,078-byte aggregate
uncompressed payload is greater than 4 GiB. R3.4 therefore avoids the ambiguous
earlier description "archive larger than 4 GiB" and tests the exact bounded
large-archive and ZIP64-count properties present in the approved corpus.

Delivery is divided at coherent, independently auditable boundaries:

1. Add a canonical DSL query/resource catalog, a bounded paired coordinator,
   version-specific observer adapters, generated contract tests, and the
   reproducible clean/warm real-corpus procedure. The comparator must retain
   missing resources and component-ownership differences as structured product
   evidence; it must not fail before publishing the pair or normalize a
   difference away.
2. Correct Qt 6 DSL resource-archive ownership and lazy bounded member access
   through the existing private DSL backend. Archive indexing must not
   materialize aggregate member data, resource reads remain size-bounded and
   source-change safe, and unrelated or orphan archives remain unattached.
3. Close R3.4 only after the final paired evidence is equivalent for all five
   exact article/resource probes in clean and warm states, archive/source
   provenance and the orphan assertion are stable, the corpus manifest is
   unchanged, focused generated regressions pass, and the cumulative build and
   test gates plus completion and integration audits pass.

The design retains the Adapter boundary for Qt 5 and Qt 6 evidence collection
and a coordinator for validation, normalization, comparison, and atomic
publication. Production work remains inside the DSL backend and depends on a
small archive-reader abstraction rather than exposing ZIP mechanics through
Core or Widgets. This satisfies current ownership and dependency direction,
keeps each responsibility cohesive, and creates no public interface or broad
architecture change. Required corpus access, frozen Qt 5 observer preparation,
Qt 6 Conan runtime activation, and external evidence storage are available.
No unresolved product or architecture decision blocks Unit 1.

Unit 1 implementation result: **Acceptance implemented; product differences
confirmed** (2026-09-05). The final Unit 1 pair is retained at
`evidence/qt5-qt6-dsl-r34-unit1-final2`, pair ID
`6e9d0aa16771daf93d9deed152457824a60320de803187c7a0983f44fd374c5c`.
The published comparison has external SHA-256
`4a6834299fa27e8dbce42944264b466618d37727ae16c1cefbd3159759559e42`.
Qt 5 retrieves the five frozen article-referenced resources byte-for-byte from
their adjacent archives and reports every archive as an owned component in
both clean and warm states. Qt 6 returns the five articles stably but reports
none of those resources or archive ownership. The pair also exposes visible
article-text differences for all five probes and a platform MIME-name
difference for WAV resources. These are retained product differences, not
acceptance-harness failures. They remain assigned to separate R3.4 correction
units; R3.4 stays open until a fresh repeated pair is equivalent.

Unit 2 implementation result: **Resource-archive parity complete; remaining
article, abbreviation, and MIME differences retained** (2026-09-05). The
private DSL `ResourceZip` component performs bounded central-directory-only
discovery, supports classic wrapped entry counts and ZIP64 metadata, and
lazily retrieves one stored or deflated member with path confinement, size,
CRC, and source-revision checks. Directory resources retain precedence, and
archives without a matching DSL source remain unattached. This preserves the
existing Core resource contract and exposes no archive-specific public API.

The final Unit 2 pair is retained at
`evidence/qt5-qt6-dsl-r34-unit2-final`, pair ID
`a57d3b9887a577d39fead612fd4bff50b2f145e3d79ac93d2e21f730c1664059`.
Its comparison has external SHA-256
`d24b4bf9a4a13b1db620632c9419b6cc9104bee601ec66a3bf0ffc5a43d6647b`.
All five archive resources match Qt 5 by exact size and SHA-256 in clean and
warm observations, all five matching archives are owned, and the orphan is
unowned. The comparison difference count fell from 38 to 16 per Qt 6
observation. Unit 3 owns the retained three abbreviation-component, five
visible-text hash/size, and three WAV media-type differences. R3.4 therefore
remains open without weakening any equality key.

Unit 3 abbreviation result: **Companion ownership and tooltip behavior
complete; article and MIME differences retained** (2026-09-05). A private
resolver follows the frozen Qt 5 adjacent filename precedence for plain and
gzip `_abrv` files while discovery continues to suppress them as standalone
dictionaries. The reader reuses bounded legacy headword expansion, expands
definition tildes against the final merged first key before unescaping it,
joins physical lines across DSL comments, removes DSL markup from values,
preserves escaped-space
nonbreaking semantics, renders exact `[p]` keys as `dsl_p` spans, applies the
short-tooltip nonbreaking whitespace rules, and fails open when the optional
companion is corrupt. The primary-only full-text document and revision contract
remains unchanged.

The final pair at `evidence/qt5-qt6-dsl-r34-unit3-final3`, pair ID
`1f741371eebfacf6ced90523b35ffaf01a75e62abd054181b102a696ca3a2207`,
has comparison SHA-256
`9cf46ccb237049f073b73d9d524335be8e36ac436d9a5eda0b464e11f45ff229`.
Clean and warm observations now match all three abbreviation components and
retain only 13 differences per Qt 6 observation: five visible-text hashes and
sizes plus three WAV media types. R3.4 remains open for those independently
committable correction units; no equality key was removed or weakened.

Unit 4 media-type result: **Deterministic WAV evidence parity complete;
article differences retained** (2026-09-05). Investigation confirmed that the
private DSL backend already returns the required `audio/wav` product media
type for both directory and archive resources. The remaining discrepancy came
from the Qt 6 acceptance adapter falling back to the host `QMimeDatabase`,
which reports the platform alias `audio/vnd.wave` on this Windows runtime when
the lookup entry does not enumerate its referenced resource. The adapter now
uses the same deterministic, extension-based Core media-type policy as the
product resource boundary while continuing to prefer explicit entry metadata.
This is an evidence-adapter correction, not a product-behavior exception.

The repeated pair at `evidence/qt5-qt6-dsl-r34-unit4-final`, pair ID
`e131bb9d8ef1f6f5467c1725469ceb12631f66eff93b7898c70fbee9aa78234f`,
has comparison SHA-256
`5a1054972b8013eb7f5f500ad6731175d32b2a585d0e54f04310316595626a57`.
All three WAV media-type fields now match in clean and warm states. The strict
comparison retains exactly ten differences per Qt 6 observation: the hash and
size of visible article text for each of the five probes. R3.4 remains open for
the independently committable article-rendering correction; no equality key
was removed or weakened.

The design uses the existing Adapter boundary for version-specific evidence
collection and a coordinator for paired-run state and publication. These
patterns have concrete value because Qt 5 and Qt 6 expose different APIs while
the canonical contract, bounds, atomic publication, and corpus-safety policy
must remain shared. Responsibilities stay cohesive, dependencies continue to
point toward transport-neutral Core contracts, and no speculative service or
binary boundary is added. Required Windows/Qt 5/Qt 6 toolchains, Conan runtime
activation, frozen source preparation, real manifest, corpus access, and
isolated evidence storage are available. No unresolved product or architecture
decision blocks implementation.

### CRD closure cross-check

This cross-check prevents a completed leaf graph from silently leaving an
approved requirement open. A requirement marked as already implemented still
participates in the listed acceptance/cutover leaves; that label is not a
waiver of regression evidence.

| Approved requirement set | Existing status and remaining closure owner |
| --- | --- |
| `CRD-SHELL-001` through `CRD-SHELL-005` | The structural shell is substantially implemented; remaining state-by-state behavior and appearance close through R9.6 and R9.9. |
| `CRD-LOOKUP-001` through `CRD-LOOKUP-004` | Existing lookup/tab/history/Favorites slices are retained; remaining real-format, full-text, management, transliteration, browser, and visual behavior closes through R3.2-R3.20, R5.1-R6.2, R6.8, R8.1-R8.2, and R9.6-R9.9. |
| `CRD-LOOKUP-005` | R7.6, R7.8, R10.5, and R10.6. |
| `CRD-STATE-001` and `CRD-STATE-002` | Existing history and Favorites behavior is retained; upgrade and complete visual/workflow acceptance closes through R3.1, R9.6, R9.7, and R9.9. |
| `CRD-STATE-003` | R6.1 and R6.6. |
| `CRD-STATE-004` | R3.1 and R6.6. |
| `CRD-PREF-001` through `CRD-PREF-004` | R4.1-R4.6 and R9.7. |
| `CRD-DICT-001` and `CRD-DICT-002` | R6.1-R6.5, R6.8, R7.5, and R7.9. |
| `CRD-DICT-003` | R3.2-R3.20 and R5.4. |
| `CRD-DICT-004` | R6.6 and the relevant Linux/Windows package leaves R10.2-R10.6. |
| `CRD-DICT-005` | R6.7. |
| `CRD-FTS-001` through `CRD-FTS-003` | R4.4 and R5.1-R5.4. |
| `CRD-PLATFORM-001` | R4.5, R4.6, R7.1-R7.4, R7.7, and R7.8. |
| `CRD-PLATFORM-002` | R4.3, R6.5, R7.5, R7.7, and R7.9. |
| `CRD-PLATFORM-003` | R6.3-R6.5 and R8.4. |
| `CRD-PLATFORM-004` | R8.3, R8.5, and R10.2-R10.6. |
| `CRD-PLATFORM-005` | R7.7-R7.9 and the platform-specific R10.2-R10.6 acceptance leaves. |
| `CRD-RES-001` through `CRD-RES-005` | R8.3, R9.1-R9.9, R10.2, and R10.5. |
| `CRD-COMPAT-001` through `CRD-COMPAT-005` | R3.1-R3.20, R6.6, R10.3, R10.4, and R10.6. |
| `CRD-TEST-REAL-001` through `CRD-TEST-REAL-010` | R1.1-R1.2, R3.2-R3.5, R5.4, R9.9, and R10.7. |

Standard verification command forms are:

```powershell
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -R "<focused-pattern>" --output-on-failure
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release --output-on-failure
python scripts/tests/<tool-test>.py
```

Linux uses `./run_with_conan.sh` with the same CTest arguments. Release-package
leaves additionally use the exact configure, install, CPack, and `conan create`
commands in `docs/build.md` and retain their logs with the candidate identity.

## Readiness And Architecture Audit

Result: **R1.1 is integrated and R1.2 is complete in this delivery. No later leaf is declared ready by this result.**

R1.2 readiness record (2026-09-03): R1.1 is integrated at
`18f3cfc5c308d730e9b52f5b69c579d585b73c14`; the paired-workspace leaf is
platform-independent, requires no product or architecture decision, and uses
the focused verification command
`python scripts/tests/real_dictionary_acceptance_workspace_test.py`.

R1.2 implementation evidence (2026-09-03): the synthetic suite validates
creation, path confinement, per-version environment isolation, pair identity,
live corpus-to-manifest equality, external expected-revision anchors, metadata
mismatch rejection, atomic publication, child-process projection, exact
dictionary-root binding, post-run corpus verification, and condition
acknowledgement.
The external real-corpus workspace validates as pair
`4a5dabba704a08472a1bbd838c16db05024927c2f828052287431aa2532d9ce0`
for Qt 5 `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, Qt 6
`18f3cfc5c308d730e9b52f5b69c579d585b73c14`, and the accepted R1.1
manifest. Its configuration, cache, indexes, logs, temporary files, and
evidence roots are disjoint and outside the corpus. Both projected version
runs acknowledged the same condition and corpus hashes with their distinct
expected revisions, and the corpus still matched the R1.1 manifest afterward.

- The gap list conforms to the approved CRD and removes no Qt 5 capability.
- Core product behavior remains behind `goldendict_core`; Widgets and WebEngine
  presentation remain in `apps/goldendict`; Network, process, audio, and
  platform volatility remain behind their existing narrow adapters.
- The plan applies SRP by separating evidence collection, test infrastructure,
  product capabilities, presentation, platform integration, resources, and
  release gates. It applies dependency inversion at existing Core and platform
  boundaries and does not add speculative interfaces. This structural review
  does not constitute readiness approval for leaves after R1.1/R1.2.
- R1 can be verified entirely with synthetic temporary corpora before reading
  the operator corpus. No payload is copied, published, modified, or required
  by CI.
- The dependency-free R1.1 tests pass. Two read-only passes over the approved
  external corpus produced byte-identical 88-file, 9,303,289,246-byte
  manifests with SHA-256
  `b7e91878649b61388ecb1a3713709685e243f57e60d2b8eb23838a91bba816d2`.
  The evidence remains outside the repository and contains no dictionary
  payload or absolute corpus path.
- Before each later leaf starts, perform and record a focused readiness check
  against its dependencies, platform, fixtures, tools, and
  acceptance evidence. A leaf that requires unavailable Linux/X11 execution
  remains not ready until that environment or an approved equivalent runner is
  available. Later discoveries are added to the issue queue and raised one at
  a time only when user direction is genuinely required.

## Non-Blocking Issue Queue

| ID | Observation | Disposition |
| --- | --- | --- |
| IQ-01 | The current host has no WSL/Linux environment and the repository has no CI workflow. | Continue platform-independent and Windows work; resolve Linux execution in R10 unless an earlier Linux-only unit requires it. |
| IQ-02 | The broad Windows suite retained documented infrastructure/toolchain failures. | Resolved by R2.1-R2.3; the complete Windows-applicable Release suite passes 127 of 127 tests. |
| IQ-03 | The real corpus contains private multi-gigabyte resources unsuitable for repository or CI storage. | R1 records only safe hashes/metadata and uses operator-provided paths. |
| IQ-04 | Several format, WebEngine, and platform gaps may require maintained replacements for obsolete Qt 5 APIs. | Investigate within the existing adapter boundaries; raise only an infeasible parity or intentional divergence decision. |

## Cutover Rule

This document may be closed only when R1 through R10 have accepted evidence,
all remaining supported Qt 5 behavior is complete or explicitly approved as a
documented divergence, and the product owner explicitly declares the parity
phase complete. Individual builds, tests, or functional-unit completions do
not authorize that declaration.
