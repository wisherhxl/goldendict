# GoldenDict Qt 6 Product Baseline Change Requirements Document

Status: Approved
Created: 2026-09-02
Product baseline owner: GoldenDict project
Legacy evidence baseline: `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`
Target implementation line: Qt 6 migration branch

## 1. Purpose

This Change Requirements Document defines the product change that turns the
current Qt 6 migration from an internal architecture and feature demonstration
into the continuing GoldenDict product baseline.

The Qt 6 application is not a new or reduced GoldenDict product. It must
reproduce the supported, observable behavior of the pinned Qt 5 baseline,
including its product identity, visual structure, workflows, resources,
settings, user-owned data, platform behavior, and failure behavior. The Qt 6
architecture may differ internally, but an existing user must recognize and be
able to use the result as the same GoldenDict product.

## 2. Problem Statement

The Qt 6 line currently proves a substantial set of new architecture,
dictionary, lookup, article, state, and full-text components. It also builds
and runs successfully. However, the first paired Windows audit found that the
desktop application still presents itself like a demonstration shell:

- the main-window structure, toolbar, panes, status presentation, tabs, and
  welcome state differ substantially from Qt 5;
- Preferences exposes two pages instead of the seven-page Qt 5 product
  surface;
- source, dictionary, and group management use reduced or different
  workflows;
- several File, View, Help, printing, tray, scan, hotkey, audio, network, and
  full-text lifecycle surfaces are absent;
- legacy icons, styles, help, translations, and detailed product information
  are not yet fully present; and
- Windows tests expose both test-harness portability problems and unresolved
  runtime behavior.

A successful build or an isolated working feature is therefore insufficient
to make Qt 6 the product baseline.

## 3. Product Decision

The pinned Qt 5 GoldenDict version is the authoritative product specification.
The Qt 6 line is the replacement implementation.

The following precedence applies when sources disagree:

1. observable behavior and product resources at the pinned Qt 5 commit;
2. product decisions approved in this CRD and its decision log;
3. mandatory Qt 6 architecture and ownership rules;
4. current Qt 6 behavior.

The Qt 6 design must preserve the Qt 5 product behavior through the current
architecture. It must not copy obsolete Qt 5 source structure merely to obtain
visual similarity. A conflict that cannot preserve both approved product
behavior and mandatory architecture must be raised for a product decision; it
must not be resolved through silent omission or redesign.

Qt 6-only capabilities that have no observable counterpart in the pinned Qt 5
product may retain their underlying implementation and tests. During the
parity program they must not alter the Qt 5 default interface, workflow,
defaults, compatibility, or delivery scope. Exposing such a capability in the
product requires a separately approved post-parity enhancement.

Parity decisions use a default-to-Qt-5 rule. When Qt 6 can reproduce the
pinned Qt 5 behavior without violating mandatory architecture, security, or
maintained-dependency constraints, implementation proceeds with that behavior
without a separate product question. Product discussion is required only when
the behavior cannot be reproduced or when there is a clear, material
optimization opportunity whose value may justify an intentional divergence.

## 4. Goals

### 4.1 Primary Goal

Make Qt 6 the sole continuing GoldenDict development baseline after it has
demonstrated approved Qt 5 product parity and release readiness.

### 4.2 User Goals

An existing GoldenDict user must be able to:

- recognize the Qt 6 application as the same product at first launch;
- use the same major menus, toolbars, panes, dialogs, settings, and workflows;
- continue using existing dictionaries, configuration, groups, history,
  favorites, and supported user data;
- obtain equivalent lookup, article, resource, audio, full-text, scan, tray,
  hotkey, printing, and online-source behavior where the platform supports it;
- find the same product help, translations, icons, styles, and attribution;
  and
- upgrade without having to learn a replacement user interface or manually
  reconstruct application state.

### 4.3 Engineering Goals

- Keep product logic behind the existing Qt 6 Core/application boundaries.
- Keep widgets, presentation state, resources, and Qt WebEngine integration in
  `apps/goldendict`.
- Reuse eligible Qt 5 product assets directly, retaining provenance, license,
  translation context, relative lookup behavior, and install location.
- Replace obsolete dependencies and platform APIs behind narrow adapters.
- Establish repeatable functional and screenshot parity gates.
- Leave the frozen Qt 5 checkout clean and unchanged.

## 5. Non-Goals

This change does not authorize:

- a voluntary redesign, modernization, simplification, or rebranding of the
  GoldenDict product;
- unrelated new product functionality during the parity program;
- cloning the Qt 5 source layout or bypassing the approved Qt 6 ownership
  boundaries;
- restoring Qt WebKit when equivalent behavior can be implemented safely with
  Qt WebEngine;
- copying bundled third-party binary libraries from the Qt 5 tree when Conan
  or a maintained system dependency is required by project policy;
- pixel-for-pixel equality for operating-system-native borders, font
  antialiasing, native widget rendering, or other unavoidable Qt/platform
  rendering differences; or
- declaring parity complete because the application compiles, launches, or
  passes a single workflow.

## 6. Visual Acceptance Standard

Qt 5 is the hard baseline for layout, logical dimensions, icons, text,
ordering, visibility, default state, and interaction. Qt 6 or operating-system
native rendering differences are acceptable only when they do not alter the
product's structure, information hierarchy, usable geometry, or behavior.

The following must match the Qt 5 baseline unless a documented product
decision says otherwise:

- main-window regions, dock placement, split orientation, default sizing, and
  default visibility;
- menu hierarchy, action order, labels, mnemonics, shortcuts, check state, and
  enablement rules;
- toolbar membership, icon identity, text/icon mode, ordering, grouping, and
  default placement;
- tab shape, controls, ordering, close behavior, restoration, and preferences;
- status-bar and count/status presentation;
- dialog structure, page order, group boxes, field labels, button order,
  minimum/default geometry, and modal/modeless behavior;
- welcome, empty, loading, error, and no-result states;
- product colors, CSS, icons, flags, embedded images, and article resources;
  and
- translated presentation for every locale retained in the supported product.

Native rendering tolerance does not permit missing controls, changed layout,
different icons, different wording, changed defaults, or changed workflows.

## 7. Functional Requirements

### 7.1 Product Shell

- `CRD-SHELL-001`: Restore the Qt 5 main-window information architecture,
  including the word/search area, article tabs and article view, dictionary
  results, favorites, history, navigation controls, toolbar, dictionary bar,
  menu bar, and status presentation.
- `CRD-SHELL-002`: Restore Qt 5 default geometry, dock state, visibility, and
  first-run presentation with safe screen-bound normalization.
- `CRD-SHELL-003`: Restore every supported Qt 5 File, View, Edit, Search, and
  Help action with equivalent state and dispatch behavior.
- `CRD-SHELL-004`: Restore the Qt 5 welcome page and product-resource loading
  behavior without adding a runtime dependency on the frozen checkout.
- `CRD-SHELL-005`: Restore Qt 5 product icons and style resources through the
  Qt 6 resource and install system.

### 7.2 Lookup And Article Workflow

- `CRD-LOOKUP-001`: Preserve typed suggestions, exact lookup, alternate
  writings, morphology, transliteration, result limits, cancellation, and
  stable failure behavior.
- `CRD-LOOKUP-002`: Preserve scoped/group lookup, dictionary participation,
  dictionary muting, result navigation, and no-result presentation.
- `CRD-LOOKUP-003`: Preserve article assembly, links, embedded resources,
  cross-references, external navigation policy, article search, zoom, and
  printing behavior.
- `CRD-LOOKUP-004`: Preserve tab creation, activation, replacement, close,
  reopen/session restoration, background behavior, and configured tab order.
- `CRD-LOOKUP-005`: Preserve command-line lookup and supported single-instance
  message behavior.

### 7.3 User-Owned State

- `CRD-STATE-001`: Preserve history creation, ordering, limits, filtering,
  clearing, import, export, persistence, and visible count behavior.
- `CRD-STATE-002`: Preserve favorites hierarchy, add/remove/move operations,
  import/export, confirmation behavior, group association, and persistence.
- `CRD-STATE-003`: Preserve dictionary groups, ordering, icons, shortcuts,
  muting, popup muting, auto-group behavior, and migration from legacy state.
- `CRD-STATE-004`: Configuration, history, favorites, groups, and other
  user-owned state must be migrated atomically or left intact with a clear and
  recoverable error. Partial silent migration is prohibited.

### 7.4 Preferences

- `CRD-PREF-001`: Restore the seven Qt 5 preference pages in their established
  order: Interface, Scan Popup, Hotkeys, Audio, Network, Full-text Search, and
  Advanced.
- `CRD-PREF-002`: Restore each supported setting's label, control type,
  bounds, default, dependency, persistence, restart behavior, and immediate
  application behavior.
- `CRD-PREF-003`: A visible preference must have an implemented effect. A
  nonfunctional compatibility control may be shown only after an explicit
  product decision defines its disabled or informational behavior.
- `CRD-PREF-004`: Existing Qt 6 configuration fields must not redefine legacy
  behavior merely because their current demonstration UI differs.

### 7.5 Dictionary And Source Management

- `CRD-DICT-001`: Restore the integrated Qt 5 Sources, Dictionaries, and
  Groups workflow, including search/filter, ordering, drag and drop, enable
  state, rescan, rename, removal, and batch actions.
- `CRD-DICT-002`: Restore all supported source families and their settings,
  including files, sound directories, morphology, MediaWiki/Wikipedia,
  websites, DICT servers, external programs, Forvo, transliteration, and text
  to speech where present on the target platform.
- `CRD-DICT-003`: Preserve supported legacy local dictionary formats, archive
  companions, discovery rules, identity, indexing, lookup, article/resource
  behavior, and failure behavior.
- `CRD-DICT-004`: Portable mode must preserve the Qt 5 self-contained
  contract: dictionaries are loaded from the application-local `content`
  directory, source Add/Remove operations are disabled, and configuration and
  user-owned state remain within the portable installation's established
  locations. Any future portable-mode expansion requires separate approval.
- `CRD-DICT-005`: A new non-portable profile must create the Qt 5 default
  English Wikipedia source. Creating the source must not itself initiate a
  network request; requests begin only through user-triggered lookup behavior.

### 7.6 Full-Text Search

- `CRD-FTS-001`: Preserve the Qt 5 full-text query modes, options, dictionary
  scope, result activation, article highlighting, cancellation, and error
  behavior.
- `CRD-FTS-002`: Restore index eligibility, readiness, creation, rebuild,
  progress, waiting/indexed counts, lifecycle status, and persisted policy.
- `CRD-FTS-003`: Index publication and replacement must be atomic and safe on
  every supported platform.

### 7.7 Platform And Integration Behavior

- `CRD-PLATFORM-001`: Restore scan popup, clipboard or selection monitoring,
  global hotkeys, tray behavior, close-to-tray, startup behavior, and
  always-on-top behavior through private platform adapters.
- `CRD-PLATFORM-002`: Restore supported audio playback, external-player,
  pronunciation, and text-to-speech behavior with maintained dependencies.
- `CRD-PLATFORM-003`: Restore proxy modes, authentication, remote-content
  policy, online-source behavior, cache policy, request identification, and
  update checks where supported by the baseline.
- `CRD-PLATFORM-004`: Restore configuration-folder, reference help, homepage,
  forum, About, credits, contributor, translator, desktop, MIME, manifest, and
  installer integration.
- `CRD-PLATFORM-005`: Document platform differences explicitly. Platform
  limitations must not be represented as general product removals.

## 8. Resource Requirements

The Qt 5 product-resource inventory is eligible for direct reuse within the
same GoldenDict product, subject to provenance and license verification.

- `CRD-RES-001`: Inventory every legacy icon, flag, stylesheet, translation,
  help file, image, desktop asset, manifest, and packaging resource.
- `CRD-RES-002`: Map each retained resource to a Qt 6 owner, resource path,
  install path, runtime consumer, and acceptance check.
- `CRD-RES-003`: Preserve established resource names and relative lookup
  behavior when user dictionaries, article content, help, styles, or
  translations depend on them.
- `CRD-RES-004`: Do not substitute placeholder or demonstration assets when a
  reusable product asset exists.
- `CRD-RES-005`: Record any resource that cannot be reused and obtain approval
  for its compatible replacement.

## 9. Compatibility Requirements

- `CRD-COMPAT-001`: Existing supported dictionary data must remain usable
  without conversion whenever the Qt 5 product did not require conversion.
- `CRD-COMPAT-002`: Existing configuration must migrate without losing
  supported settings. Unknown or future fields must not be destroyed solely
  by loading and saving through Qt 6.
- `CRD-COMPAT-003`: Existing history, favorites, groups, muted dictionaries,
  source definitions, shortcuts, and persisted UI state must survive upgrade.
- `CRD-COMPAT-004`: Generated indexes and caches may be rebuilt when their
  format changes, but user data must not be treated as disposable cache data.
- `CRD-COMPAT-005`: Migration failure must be detectable, diagnosable, and
  recoverable without silently replacing the user's working legacy data.

## 10. Verification Requirements

### 10.1 Behavior Evidence

Every migrated capability must have:

1. exact Qt 5 source or runtime evidence;
2. documented user-visible, data, edge-case, and failure behavior;
3. a mapped Qt 6 ownership boundary;
4. focused automated tests or a reproducible manual check;
5. an updated parity record; and
6. direct comparison with the pinned Qt 5 evidence.

### 10.2 Screenshot Evidence

- Use fixed logical window size, DPI, locale, style, theme, fixture data, and
  application state for paired captures.
- Maintain authoritative Qt 5 captures for the main window, every menu, every
  preference page, dictionary/source/group management, full-text search,
  history, favorites, About/help, and each major state.
- Generate the corresponding Qt 6 captures and image-difference evidence.
- Review geometry, control inventory, order, text, icon, state, and workflow
  differences. Raw pixel thresholds alone are not sufficient.
- Keep approved native-rendering masks or tolerances narrow and documented.

### 10.3 Build And Test Evidence

- Clean configure, dependency resolution, build, test, install, launch, and
  package checks must be reproducible on every release-gating platform.
- GUI tests must use platform-appropriate Qt and Qt WebEngine environments;
  Linux-only offscreen assumptions must not invalidate Windows results.
- Path, case-sensitivity, file-locking, symlink, process, and authentication
  tests must define portable expectations or explicit platform capability
  gates.
- Sanitizers and static analysis supplement but do not replace normal Release
  verification.

### 10.4 Real-Dictionary Acceptance Corpus

Small generated or redistributable fixtures remain mandatory for deterministic
unit, component, installation, package, and CI testing. They are not sufficient
evidence for product-baseline acceptance. Batch acceptance and final cutover
must also use the same representative real dictionary corpus in Qt 5 and Qt 6.

The initial user-provided Windows corpus is stored outside the repository at
`D:\workspace\goldendict\content`. Its 2026-09-02 inventory contains 88 files
and 9,303,289,246 bytes. It currently exercises:

- one large MDict dictionary with an MDX, three MDD resource volumes, external
  CSS, four fonts, and an image;
- seventeen compressed DSL-family data files, five DSL abbreviation or
  companion files, annotations and dictionary images;
- six large DSL resource ZIP archives, including archives larger than 4 GB;
- Chinese and English dictionaries in nested directories with non-ASCII,
  whitespace, punctuation, and mixed-case path components; and
- seven Hunspell `.aff`/`.dic` morphology pairs covering multiple languages,
  with related notices and metadata.

The location is an operator-provided acceptance input, not an application or
test-code constant. Windows uses the path above by default for this audit
environment. Linux receives the same corpus through a separately configured
external path. The dictionary files are not repository content, package
content, uploadable artifacts, or CI prerequisites unless their owners later
provide explicit redistribution authorization.

- `CRD-TEST-REAL-001`: Generate a local inventory manifest containing relative
  path, file size, format classification, and a stable content hash before a
  full acceptance run. Store only the manifest when its metadata is safe to
  retain; never copy dictionary payloads into the repository or build package.
- `CRD-TEST-REAL-002`: Treat the corpus as read-only. Application indexes,
  caches, configuration, logs, and test outputs must be written to isolated
  per-version test profiles outside the corpus. Destructive and malformed-file
  tests use generated fixtures or disposable copies, never the user corpus.
- `CRD-TEST-REAL-003`: Run Qt 5 and Qt 6 against the same corpus with matched
  locale, group, preferences, query, and platform conditions. Record any
  difference in discovery, identity, ordering, enablement, duplicate handling,
  article count, or source error.
- `CRD-TEST-REAL-004`: Verify first discovery/index creation, clean restart and
  index reuse, explicit rescan, changed-source detection through a disposable
  copy, cancellation, progress/status presentation, and recoverable handling
  of unavailable companions.
- `CRD-TEST-REAL-005`: Maintain a representative query catalog for each real
  dictionary family. It must cover exact lookup, suggestions, alternate
  writings where applicable, missing words, Unicode input, punctuation,
  multi-word input, and morphology-assisted lookup.
- `CRD-TEST-REAL-006`: Compare article selection, ordering, title/headword,
  rendered text, internal links, cross-references, styles, fonts, images,
  audio/media references, and other embedded resources. Large split MDD and
  large ZIP resource access are explicit acceptance cases.
- `CRD-TEST-REAL-007`: Exercise group creation and ordering, dictionary
  enable/mute state, popup mute state, source rescan, dictionary information,
  dictionary headword browsing, and persistence using real discovered
  dictionaries.
- `CRD-TEST-REAL-008`: Exercise full-text eligibility, index creation/reuse,
  progress, cancellation, representative searches, result activation,
  article highlighting, rebuild, and failure recovery on every supported real
  format.
- `CRD-TEST-REAL-009`: Capture paired screenshots for representative main
  window, article, resource, dictionary-management, group, full-text, history,
  and favorite workflows using the real corpus. Dictionary content must not be
  published outside the authorized local audit environment.
- `CRD-TEST-REAL-010`: Record startup, discovery, first-index, warm-start,
  lookup, article-render, full-text, memory, and generated-storage measurements
  for Qt 5 and Qt 6. An unexplained material regression blocks cutover until it
  is fixed or approved as an intentional difference.

Real-data results supplement rather than replace parser boundary, corruption,
resource-limit, cancellation, failure-injection, and migration tests. A real
dictionary passing one lookup does not prove complete format parity.

## 11. Delivery Plan

### Development Readiness Record

Result: **Ready** (2026-09-02).

- The CRD is consistent with the frozen Qt 5 product baseline and does not
  authorize a reduced product or an unapproved redesign.
- Product presentation remains owned by `apps/goldendict`; product workflows,
  dictionary behavior, persistence, and data contracts remain behind the
  existing Core/application boundaries. This preserves the documented shared-
  library and GUI boundary and applies single responsibility, interface
  segregation, and dependency inversion without adding an unnecessary binary
  abstraction.
- Delivery proceeds as the functional batches below. A batch may be divided
  into smaller, coherent slices that each have explicit Qt 5 evidence, focused
  automated tests, matched visual or manual evidence when applicable, updated
  parity documentation, and an independent completion audit before commit.
- The initial Batch A slice is limited to the application icon and core shell
  resources, 653 by 538 default logical size, canonical menu and navigation-
  toolbar presentation, Welcome tab/page, yellow article style, status bar,
  removal of demonstration controls from the default surface, and the default
  left-search/right-stacked-pane layout.
- The initial slice is verified by a Release build, the product-shell,
  article-tab, File, Edit, Search, View, History, and Favorites smoke tests,
  and matched Qt 5/Qt 6 captures under the visual standard in Section 10.2.
  Later dictionary and workflow slices additionally use the external real-
  dictionary corpus defined in Section 10.4.
- The Visual Studio 2026, Conan, CMake, Qt 6, frozen Qt 5 reference, and local
  test corpus required to begin delivery are available. Platform-specific
  tool gaps discovered in later batches are recorded and resolved without
  weakening their acceptance criteria.
- No unresolved product or architecture decision blocks the initial slice.

### Batch A — Product Shell And Assets

Restore the main-window structure, menus, toolbars, docks, tabs, status bar,
welcome state, default geometry, icons, styles, and product resources. This
batch removes the demonstration-shell appearance and establishes the visual
container for all remaining work.

### Batch B — Daily Lookup And User State

Complete lookup/navigation, article behavior, tabs, history, favorites, and
session persistence using the restored shell.

### Batch C — Preferences And Configuration

Restore all seven preference pages, setting dependencies, platform wiring,
legacy migration, and atomic persistence.

### Batch D — Dictionary, Source, And Group Management

Restore the integrated management workflow, all source families, rescan and
indexing behavior, ordering, muting, drag/drop, and group operations.

### Batch E — Full-Text And Platform Features

Complete full-text lifecycle UI, scan popup, hotkeys, tray, audio, online and
proxy behavior, printing, help, translations, and platform integration.

### Batch F — Release And Baseline Cutover

Close the cross-platform test matrix, installers/packages, migration testing,
visual audit, documentation, and release-candidate acceptance. Linux and
Windows must pass the complete gate against the same candidate baseline. Only
then may Qt 6 become the sole continuing product baseline. macOS restoration
follows the cutover and remains tracked product work.

Each batch must be independently reviewable, buildable, testable, and backed
by updated parity evidence. A batch may be split into small implementation
slices, but that sequencing must not reduce its product scope.

## 12. Baseline Cutover Gate

Qt 6 may replace Qt 5 as the continuing development baseline only when:

- the same candidate revision passes the complete Linux and Windows product,
  compatibility, build, test, install, launch, package, and visual gates;
- all Qt 5 capabilities in the approved supported-product scope are complete
  or have an explicitly approved and documented divergence;
- no critical or major user workflow remains represented only by a
  placeholder, demonstration UI, or nonfunctional control;
- configuration and user-owned data migration pass upgrade, rollback-safety,
  malformed-input, and failure-injection checks;
- paired visual review accepts all required surfaces under the standard in
  Section 6;
- release-gating platforms pass their clean build, test, install, launch, and
  package checks;
- the application has no runtime dependency on the frozen Qt 5 checkout;
- dependency and resource provenance is complete;
- the parity, migration, architecture, testing, and known-difference documents
  agree; and
- the product owner explicitly declares the parity phase complete and approves
  the baseline cutover.

After cutover, new product work must start from Qt 6. The pinned Qt 5 checkout
remains read-only historical evidence and is not advanced with later upstream
changes without a separate reconciliation decision. macOS restoration remains
required post-cutover work, but it does not block the Linux-and-Windows
baseline decision.

## 13. Risks And Controls

| Risk | Control |
| --- | --- |
| Demo UI becomes the accidental product design | Treat Qt 5 screenshots, resources, and interaction evidence as hard acceptance inputs |
| Visual copying violates Qt 6 architecture | Recreate presentation in `apps/goldendict` and route intent through existing Core/application boundaries |
| Existing Qt 6 features regress during shell restoration | Retain focused behavior tests and add shell-level state/dispatch tests before replacing presentation |
| Legacy settings appear in the UI without working behavior | Require implemented effect or an approved compatibility decision for every visible control |
| Cross-platform assumptions hide real failures | Run platform-native builds/tests and classify capability, harness, and runtime failures separately |
| Resource import loses provenance or runtime paths | Maintain a resource inventory with ownership, license, source, destination, and consumer checks |
| Real dictionary files are large, private, or not redistributable | Keep the corpus external and read-only, retain only safe metadata and local evidence, and use redistributable fixtures in CI |
| Long parity program drifts into unrelated development | Enforce the CRD scope and require explicit approval for additions or intentional divergences |

## 14. Decision Log

| Date | Decision | Status |
| --- | --- | --- |
| 2026-09-02 | Qt 5 layout, logical dimensions, icons, wording, defaults, and interaction are the hard product baseline. Unavoidable Qt 6/OS native rendering differences are acceptable. | Approved |
| 2026-09-02 | Write and approve this CRD before resuming product-shell source changes. | Approved |
| 2026-09-02 | Formal Qt 6 baseline cutover requires complete Linux and Windows acceptance against the same candidate revision. macOS restoration follows cutover. | Approved |
| 2026-09-02 | Qt 6-only capabilities may retain implementation and tests but must not change the Qt 5 default product surface during parity. Product exposure requires separate post-parity approval. | Approved |
| 2026-09-02 | Qt 6 portable mode preserves the Qt 5 self-contained contract: application-local `content` dictionaries and disabled source Add/Remove operations. Enhancements require a later product change. | Approved |
| 2026-09-02 | A new non-portable profile creates the Qt 5 default English Wikipedia source without initiating a request until user-triggered lookup. | Approved |
| 2026-09-02 | When Qt 5 behavior can be reproduced within mandatory constraints, implement it by default without item-by-item confirmation. Raise only infeasible parity or a material optimization opportunity. | Approved |
| 2026-09-02 | Product acceptance uses the user-provided external real dictionary corpus in addition to committed deterministic fixtures. The corpus remains read-only, local, and outside repository/package artifacts. | Approved |
| 2026-09-02 | The complete CRD is approved as the governing product-change and acceptance basis; implementation may begin. | Approved |

## 15. Open-Decision Policy

There is no current open product decision.

Future questions are raised only when pinned Qt 5 behavior cannot be reproduced
within mandatory constraints or when a clear, material optimization may
justify an intentional divergence. Such questions are presented one at a time.
