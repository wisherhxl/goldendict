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

The R1-R10 headings are dependency-ordered workstreams, not commit-sized
deliveries. The leaf delivery table below defines the independently auditable
functional units. Each leaf must be developed in its own task worktree,
independently audited, committed, pushed, and integrated before a dependent
leaf starts.

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

Requirements: `CRD-PLATFORM-001` and `CRD-PLATFORM-002`.

- Restore continuous clipboard/selection monitoring, scan flag and popup,
  modifier policy, global hotkeys, tray, close-to-tray, startup, and
  always-on-top behavior behind private platform adapters.
- Complete external-player and Linux speech/TTS behavior. Retain Qt Multimedia
  as the maintained built-in playback path; add another backend only when its
  deployment boundary is justified.
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
| R1.1 | Safe corpus manifest: `CRD-TEST-REAL-001`, read-only half of `002` | none | synthetic corpus unit tests; repeat generation and byte comparison; real-corpus manifest log |
| R1.2 | Paired isolated run workspace and metadata: remainder of `002`, `003` | R1.1 | synthetic confinement/mismatch tests; paired Qt 5/Qt 6 metadata validation |
| R2.1 | MSVC exception-family test diagnosis and harness correction | R1.2 | each affected test serially reproduced; focused corrected tests; no assertion weakening |
| R2.2 | Windows path/profile/process isolation corrections | R2.1 | focused restart/process tests from Unicode and long paths |
| R2.3 | Windows WebEngine serialization and GPU-independent harness | R2.2 | all affected WebEngine tests serially pass; full Windows CTest log retained |
| R3.1 | Configuration and user-state upgrade/rollback matrix: `CRD-STATE-004`, `CRD-COMPAT-002` through `005` | R1.2, R2.3 | generated malformed/failure-injection tests plus disposable Qt 5 profile upgrade |
| R3.2 | Real-corpus discovery, identity, counts, ordering, restart, and rescan: `CRD-TEST-REAL-004`, `CRD-COMPAT-001` | R1.2, R2.3 | paired machine-readable Qt 5/Qt 6 result files and diff |
| R3.3 | Real MDict split-resource acceptance and evidence-confirmed corrections | R3.2 | MDX/three-MDD lookup, resource, restart, and immutable-source checks |
| R3.4 | Real DSL and greater-than-4-GiB resource-ZIP acceptance and corrections | R3.2 | DSL/dictzip/article/resource/restart checks with bounded storage evidence |
| R3.5 | Remaining real-corpus lookup, suggestion, article, media, and management matrix: `CRD-TEST-REAL-005` through `007` | R3.3, R3.4 | paired query catalog and machine-readable result diff |
| R4.1 | Qt 5 Interface Preferences page and backed behavior | R2.3 | focused preference persistence/effect test plus paired page capture |
| R4.2 | Qt 5 Advanced Preferences page and backed behavior | R4.1 | focused persistence/effect test plus paired page capture |
| R4.3 | Qt 5 Audio Preferences page and backed behavior | R4.1 | focused persistence/routing test plus paired page capture |
| R4.4 | Qt 5 Full-text Search Preferences page and backed policy controls | R5.2 | focused policy/persistence test plus paired page capture |
| R4.5 | Qt 5 Scan Popup Preferences page and backed controls | R7.2 | focused adapter/persistence test plus paired page capture |
| R4.6 | Qt 5 Hotkeys Preferences page and backed controls | R7.3 | focused adapter/persistence test plus paired page capture |
| R5.1 | Remaining full-text query/result targeting, excerpt, and highlight parity: `CRD-FTS-001` | R2.3 | focused Core/Widgets/WebEngine tests against pinned Qt 5 evidence |
| R5.2 | Full-text policy, eligibility, scheduling, atomic publication, rebuild, and recovery: `CRD-FTS-002`, `003` | R3.5 | lifecycle, cancellation, corruption, replacement, and failure-injection tests |
| R5.3 | Full-text readiness, progress, waiting/indexed counts, and failure UI | R5.2, R4.4 | focused UI state-machine test and paired state captures |
| R5.4 | Full-text real-format acceptance: `CRD-TEST-REAL-008` | R5.3 | every eligible real format indexed, queried, reused, rebuilt, cancelled, and recovered |
| R6.1 | Remaining dictionary/group management, popup muting, auto-group, ordering, and batch actions | R3.5 | focused model/UI/persistence tests and paired workflow capture |
| R6.2 | Hunspell and Chinese-conversion registration/configuration | R6.1 | real and generated morphology/conversion lookup plus persistence tests |
| R6.3 | Website encoding, iframe, and resource parity | R2.3 | deterministic local HTTP fixtures and WebEngine presentation tests |
| R6.4 | DICT authentication parity | R2.3 | deterministic authenticated local DICT fixture and redaction tests |
| R6.5 | Forvo playback and external-program environment/audio policy | R4.3, R7.5 | deterministic local audio/process fixtures and containment tests |
| R7.1 | Continuous X11 clipboard/selection monitoring and modifier policy | R2.3 | private adapter tests and X11 integration check |
| R7.2 | Scan flag and scan-popup presentation/lifecycle | R7.1 | adapter/UI lifecycle tests and paired captures |
| R7.3 | Global hotkey registration, dispatch, conflict, and teardown | R7.1 | private adapter tests plus X11 session check |
| R7.4 | Tray, close-to-tray, startup, activation, and always-on-top | R7.3 | desktop integration test and paired state capture |
| R7.5 | External player and Linux speech/TTS | R4.3 | controlled process/TTS adapter tests and manual audio check |
| R8.1 | Article resource saving and remaining safe context actions | R3.5 | local resource fixture, cancellation/error tests, and paired menu capture |
| R8.2 | Qt WebEngine DevTools/inspector parity | R2.3 | private page/profile lifecycle tests and manual DevTools check |
| R8.3 | About, homepage, forum, configuration-folder, credits, and attribution workflows | R9.1 | focused action/URL/path tests and paired dialogs |
| R8.4 | Remaining proxy/authentication, request identity, remote-content, and update behavior | R6.3, R6.4 | deterministic local proxy/origin/update fixtures and redaction tests |
| R9.1 | Complete asset provenance/owner/install/consumer inventory and feature-owned imports | R3.5 | inventory validator, installed-runtime checks, and hash provenance |
| R9.2 | Qt 6 context review and enablement in independently reviewable locale batches | R9.1 | `lrelease`, locale smoke, fallback test, and paired localized surface checks |
| R9.3 | Matched empty/synthetic-data visual matrix for all required surfaces | R4.1-R8.4, R9.2 | fixed-environment captures, semantic checklist, and documented narrow masks |
| R9.4 | Matched real-corpus visual matrix: `CRD-TEST-REAL-009` | R5.4, R6.2, R9.3 | private paired captures and retained non-content metadata/results |
| R10.1 | Linux/Windows clean-build and test CI with pinned profiles/locks | R2.3 | clean runner logs for the same commit |
| R10.2 | Linux runtime bundle, DEB/RPM, install/uninstall, launcher, MIME, and registration | R7.5, R8.4, R10.1 | clean package install/launch/uninstall tests |
| R10.3 | Windows runtime bundle/installer, WebEngine deployment, manifest, and native hooks | R7.4, R8.4, R10.1 | clean package install/launch/uninstall and native integration tests |
| R10.4 | Paired startup/index/lookup/render/full-text/memory/storage measurements: `CRD-TEST-REAL-010` | R5.4, R9.4, R10.2, R10.3 | machine-readable same-host results, thresholds, and disposition of every material regression |
| R10.5 | Same-candidate Linux/Windows cutover audit and documentation reconciliation | all prior leaves | complete build/test/install/package/visual/corpus records and independent final audit |

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

Result: **Ready for R1 and for subsequent units in the recorded order.**

- The gap list conforms to the approved CRD and removes no Qt 5 capability.
- Core product behavior remains behind `goldendict_core`; Widgets and WebEngine
  presentation remain in `apps/goldendict`; Network, process, audio, and
  platform volatility remain behind their existing narrow adapters.
- The plan applies SRP by separating evidence collection, test infrastructure,
  product capabilities, presentation, platform integration, resources, and
  release gates. It applies dependency inversion at existing Core and platform
  boundaries and does not add speculative interfaces.
- R1 can be verified entirely with synthetic temporary corpora before reading
  the operator corpus. No payload is copied, published, modified, or required
  by CI.
- No current product or architecture question blocks R1. Later discoveries are
  added to the issue queue and raised one at a time only when user direction is
  genuinely required.

## Non-Blocking Issue Queue

| ID | Observation | Disposition |
| --- | --- | --- |
| IQ-01 | The current host has no WSL/Linux environment and the repository has no CI workflow. | Continue platform-independent and Windows work; resolve Linux execution in R10 unless an earlier Linux-only unit requires it. |
| IQ-02 | The broad Windows suite retains documented infrastructure/toolchain failures. | R2 owns classification and correction; focused passing tests remain valid for independent units. |
| IQ-03 | The real corpus contains private multi-gigabyte resources unsuitable for repository or CI storage. | R1 records only safe hashes/metadata and uses operator-provided paths. |
| IQ-04 | Several format, WebEngine, and platform gaps may require maintained replacements for obsolete Qt 5 APIs. | Investigate within the existing adapter boundaries; raise only an infeasible parity or intentional divergence decision. |

## Cutover Rule

This document may be closed only when R1 through R10 have accepted evidence,
all remaining supported Qt 5 behavior is complete or explicitly approved as a
documented divergence, and the product owner explicitly declares the parity
phase complete. Individual builds, tests, or functional-unit completions do
not authorize that declaration.
