# GoldenDict Qt 6 Migration

## Provenance

This migration combines two read-only source baselines:

- GoldenDict product source: commit
  `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` from the legacy GoldenDict
  repository.
- Tiger project template: commit
  `2b36f6bdd52ff2fe81572893b311b6b33752c075` from the Tiger repository.

The GoldenDict baseline defines product identity and behavior. The Tiger
baseline supplies the CMake, Conan, module, test, install, and packaging
structure. Reusable Tiger implementation names, including `Tiger*.cmake` and
`ti_*` commands, remain unchanged unless a product-facing interface requires a
GoldenDict name.

## Governing Rules

- GoldenDict remains the product. Its behavior, identity, and migrated source
  remain licensed under GPLv3 or later.
- Tiger Platform remains separately licensed under its existing MIT license.
  Preserve its copyright and license notices instead of relicensing it as
  GoldenDict product code.
- The distributed GoldenDict application must satisfy GPLv3-or-later terms;
  the permissively licensed Tiger Platform source remains independently
  available under MIT.
- Tiger owns the project, dependency, build, test, install, and package
  structure. GoldenDict product behavior is migrated into that structure.
- Runtime entry points, GUI presentation, Qt WebEngine view integration,
  application resources, translations, and the composition root belong in
  `apps/goldendict`.
- Product logic belongs behind the public `goldendict_core` shared-library
  boundary. The GUI only displays state and forwards user intent. The library
  must also support a future headless dictionary-service executable through a
  transport-neutral API that does not depend on Qt Widgets, Qt Gui, or Qt
  WebEngine. Its primary future consumer is AI lookup, so return bounded,
  structured results with stable dictionary provenance rather than requiring
  GUI HTML scraping. Keep domain responsibilities as tested internal
  components; add another DLL only for a demonstrated deployment, dependency,
  platform, or plugin boundary, never merely for each layer, format, or legacy
  file.
- Linux with Qt 6 is the first supported target. Keep platform dependencies
  behind narrow boundaries so Windows and macOS can be restored later.
- Conan resolves all external dependencies. Official Conan `*/system`
  packages are allowed for Linux display and graphics ABI integration.
- Compilation alone is not a migration gate. Every migrated capability needs
  focused automated tests or a documented manual parity check.
- Keep the legacy GoldenDict worktree clean and unchanged as the source and
  behavior reference.
- Freeze migration scope at GoldenDict commit `3d93dd66` until Linux behavior
  parity is complete. Do not continuously chase later legacy `master` changes;
  reconcile them as a separately reviewed update after the pinned baseline
  passes the Linux acceptance gate.

## Optimized Migration Plan

### Phase 1 — Baseline, Isolation, And License Boundary

- Pin the GoldenDict and Tiger source commits.
- Keep the legacy GoldenDict worktree clean and create the Tiger-based
  migration worktree.
- Inventory legacy build options, bundled code, generated files, external
  dependencies, and platform-specific code.
- Establish the repository license layout before importing legacy source:
  GoldenDict product code under GPLv3-or-later and Tiger Platform under MIT,
  with component notices preserved.

Gate: both source baselines and worktrees are independently verifiable, and
the product/component license boundary is documented and represented in the
repository.

Status: complete. The source baselines and worktree isolation are recorded,
and the repository represents the product/component boundary through the root
GPL license, `LICENSES/README.md`, preserved Tiger MIT notices, SPDX headers on
new product entry points, and package metadata.

### Phase 2 — Reproducible Tiger And Qt 6 Skeleton

- Apply GoldenDict product and package identity without renaming reusable Tiger
  internals.
- Remove unused template applications, protobuf examples, and dependencies.
- Resolve Qt 6.11.1 and Qt WebEngine through Conan.
- Build a minimal `apps/goldendict` Qt Widgets application backed by Qt
  WebEngine.
- Keep memory-constrained WebEngine build policy in the Conan profile rather
  than relying on operator-only command-line knowledge.
- Capture the exact clean Git commit in Conan export metadata so package source
  builds reproduce the migration revision instead of cloning a default branch.
- Verify a clean Conan install, configure, build, focused tests, install, and
  initial package behavior.

Gate: a clean Linux environment can reproduce the minimal GoldenDict Qt 6
skeleton, and Qt WebEngine is demonstrably enabled.

Status: complete. A clean pushed revision reproduces the exact migration source
and Tiger submodule through Conan, builds and packages `goldendict/1.6.0`, and
passes the generated C consumer in `test_package`. Direct Release configure,
build, smoke test, install, and TGZ package checks also pass.

### Phase 3 — Porting Map And First-Usable Definition

Classify every legacy source and resource by:

- ownership in the public core facade, a private core component, a justified
  optional integration module, or presentation-only `apps/goldendict` code;
- dependencies and direction of dependency;
- required Qt 6 API changes;
- operating-system coupling;
- source license and notices;
- available tests or required fixtures;
- priority for the first usable Linux release.

Produce a dependency graph, feature-parity matrix, and prioritized backend
batches. StarDict is the approved representative dictionary format for the
Phase 4 vertical slice. Batching controls migration and verification order; it
does not reduce the formal Linux release scope. The first formal Linux release
must restore every dictionary format supported by the pinned legacy
GoldenDict baseline.

Gate: the ownership/dependency map, first-usable feature set, representative
backend, fixtures, and deferred-feature list are reviewed and approved before
broad source movement.

Working documents:

- [porting-map.md](porting-map.md): source/resource ownership, dependency
  direction, Qt 6 risks, backend batches, and fixture policy;
- [feature-parity.md](feature-parity.md): formal Linux parity matrix;
- [first-usable.md](first-usable.md): the bounded Phase 4 StarDict vertical
  slice and its acceptance gate.

Status: complete. The architecture, ownership boundaries, backend order,
fixture policy, and Phase 4 gate were reviewed and approved before broad
source movement.

### Phase 4 — Minimal Vertical Slice

Migrate the smallest complete user path:

1. the `goldendict_core` shared-library facade and private components;
2. application configuration;
3. dictionary abstraction and asynchronous request path;
4. one representative dictionary backend;
5. dictionary discovery and indexing;
6. headword lookup and browser-independent article HTML generation;
7. a headless lookup consumer that exercises the public core API; and
8. Qt WebEngine rendering in a presentation-only application UI.

Keep scope deliberately narrow. This phase validates dependency direction,
the WebEngine bridge, and the end-to-end migration strategy before expanding
the foundation or format count.

Gate: a fixture dictionary can be discovered, indexed, searched, and retrieved
through the public headless API, then rendered through the real Qt 6
application path, with automated checks for non-visual behavior and a
documented rendering smoke test.

Status: in progress. The automated vertical-slice gate is complete. The
installed `goldendict_core` headless API now exercises configuration,
discovery, generated indexing, asynchronous exact lookup, stable provenance,
language metadata, inert article assembly, typed resource retrieval, and clean
missing results against a generated StarDict fixture. The private StarDict
adapter supports bounded uncompressed and gzip/dictzip-compatible data,
validates source and index corruption, and rebuilds stale generated indexes.
The presentation-only Qt Widgets shell performs lookup through the desktop
facade, renders local articles with Qt WebEngine, resolves internal lookup and
resource URLs through typed core APIs, and denies external embedded
navigation. Release tests, install/export, an independent installed consumer,
and exact-SCM Conan package creation pass. The documented visible Linux GUI
check remains before Phase 4 can be declared complete.

### Phase 5 — Non-UI Foundation Hardening

- Port common utilities, configuration, logging, and error handling.
- Harden dictionary interfaces and asynchronous request ownership.
- Port compression, persistent storage, indexing, text folding,
  transliteration, and encoding behavior in dependency order.
- Replace obsolete Qt APIs deliberately, including regular expressions,
  codecs, containers, and signal/slot usage where required.
- Keep non-UI behavior behind the core facade and preserve internal dependency
  direction as the foundation grows.

Gate: focused tests pass without requiring the GUI, and the Phase 4 vertical
slice remains functional.

Status: in progress. The first hardening increment publishes headless query
limits and rejects oversized, malformed UTF-8, embedded-NUL, and invalid
filter inputs before backend dispatch. UTF-8 validation is a private,
browser-independent foundation primitive with data-driven tests. The second
increment adds private Unicode compatibility normalization, full case folding,
diacritic removal, and whitespace/punctuation folding. StarDict exact lookup
uses the canonical folded form while preserving the source headword, and the
headless response publishes that canonical form as match metadata. ICU is a
private implementation dependency and does not enter the public core ABI.
The third increment activates the existing transport-neutral `kPrefix` match
mode without adding another public module or format-specific API. StarDict
prefix lookup uses the same canonical Unicode form, ranks exact canonical
matches first and then shorter candidates, publishes deterministic match
scores, observes cancellation/deadline checkpoints during index scans, and is
covered by the installed headless consumer. Fuzzy matching remains deferred.
The fourth increment adds a private, bounded text-encoding primitive on the
existing ICU dependency. It strictly decodes legacy dictionary bytes to UTF-8
and encodes UTF-8 query text for format adapters, rejects malformed input,
unrepresentable output, unknown or oversized encoding names, and output-limit
violations, and covers Latin-1, UTF-16LE, GB18030, and EUC-JP. This establishes
one encoding policy for later SLOB, MDict, Babylon, Hunspell, and EPWING ports
without exposing a codec library through the public core ABI.
The fifth increment adds a dedicated bounded headword-suggestion contract.
StarDict ranks and deduplicates matching index headwords without reading
article bodies, while the public headless response preserves dictionary and
language identity, canonical match metadata, filtering, cancellation,
deadlines, partial errors, global cross-dictionary ranking, and deterministic
result limits. The installed consumer exercises this lightweight path
independently of prefix article lookup.

### Phase 6 — Dictionary Backends In Priority Batches

- Port backends in approved core, common optional, and deferred/high-cost
  batches.
- Give every backend small legal or generated fixtures.
- Verify discovery, indexing, headword enumeration, lookup, embedded resource
  loading, corruption handling, cancellation, and useful diagnostics.
- Keep optional backends behind explicit CMake/Conan feature checks.
- Do not enable a backend by default until its gate passes.

Gate per backend: fixture-based build and lookup parity. Internal milestones
may contain only the formats already migrated, but the first formal Linux
release is blocked until every format supported by the pinned legacy baseline
passes its gate. Original dictionary files must remain directly usable without
format conversion; implementation-generated indexes may be rebuilt when
binary compatibility cannot be preserved safely.

Status: in progress. The first text-batch increment adds a private Dictd
adapter for original `.index` plus `.dict` or gzip/dictzip-compatible
`.dict.dz` files. It validates base-64 article ranges and UTF-8 headwords,
honors the optional original-headword column, reads `00databaseshort` title
metadata, reuses Unicode-folded exact/prefix/suggestion ranking, and exposes
bounded inert plain-text articles through the existing headless and desktop
facades. Discovery reports incomplete companion sets, compressed input is
checksum-checked and bounded, and scan checkpoints preserve cancellation and
deadline behavior. Legacy full-text indexing and presentation-specific Dictd
markup conversion remain separately gated Phase 5/7 work.

The second text-batch increment adds SDict `.dct` discovery and a bounded
private reader for the packed little-endian container. It validates header,
full-index, headword, and article ranges; preserves source/target language
metadata; supports the legacy plain, zlib, and bzip2 compression modes; and
reuses common Unicode-folded exact/prefix/suggestion ranking. SDict structural
markup and word references are converted into sanitized HTML and typed lookup
URLs before reaching desktop or headless consumers. Full-text indexing remains
separately gated Phase 5 work.

The third text-batch increment adds recursive XDXF `.xdxf` and `.xdxf.dz`
discovery with bounded stored and decompressed input. A private Expat stream
reader validates UTF-8 and XML structure, preserves dictionary and language
metadata, enumerates article aliases, and reuses common folded
exact/prefix/suggestion ranking. Logical markup, word references, and image
references enter the common sanitized-HTML and typed-resource path; resource
loads are size-bounded and confined to safe relative paths beside the source
or in its `.files` directory. Legacy resource ZIP archives and full-text
indexing remain later parity increments.

### Phase 7 — Articles, WebEngine, And Networking

Run three separately gated workstreams:

- article construction and transformation;
- Qt WebEngine navigation, JavaScript, resources, link handling, printing,
  zoom, search, and external URL bridge;
- resource networking and online dictionaries using Qt 6 APIs.

Stabilize local article rendering and embedded resources before enabling
online dictionary workflows. Treat WebKit-to-WebEngine behavior as a redesign,
not a mechanical API rename.

The first WebEngine interaction increment provides bounded in-article search,
back/forward/reload controls, clamped zoom controls, application-routed lookup
links, and an allowlisted `http`/`https`/`mailto` external URL bridge. Embedded
content cannot navigate the article view directly. An offscreen WebEngine
interaction smoke verifies real Chromium text matching and zoom behavior in
addition to the existing local-rendering smoke. Advanced context menus and the
remaining JavaScript integration stay behind later Phase 7/8 gates.

The following article export increment exposes native copy, atomic sanitized
HTML save, and asynchronous PDF print actions. The WebEngine interaction smoke
also verifies serialized HTML and a real Chromium-generated PDF payload.
Advanced context-menu customization and system print-dialog integration remain
later Phase 8 parity work.

The bounded Phase 8 R4 residual adds a custom article context menu for
facade-resolved lookup links, allowlisted external links, short selection
lookup, input-line transfer, native/plain-text copy, image copy, and select-all.
The menu snapshots its originating tab and routes navigation through the
existing tab contracts; it does not add resource saving, inspector access,
article scripts, or per-dictionary context navigation. Credential-bearing and
unsupported external targets receive no menu action. Printing uses Qt
PrintSupport's platform print dialog where available and provides a
cross-platform preview while retaining the separate asynchronous PDF and HTML
exports. Cancelled, unavailable, overlapping, and failed print paths do not
mutate article or session state.

The article composition increment combines every returned dictionary entry in
one bounded inert document, retaining per-dictionary headings and the resource
URLs already emitted by the core assemblers. Composition remains inside the
core facade: the desktop application receives presentation-ready HTML and does
not depend on format implementations or concatenate trusted markup itself.

The core-controlled article shell adds responsive typography, bounded media,
scrollable tables, wrapped preformatted text, dictionary separators, and
light/dark color support. Inline styles remain stripped from dictionary input;
the CSP permits only the fixed inline stylesheet emitted by the core, while
scripts and all network content remain disabled.

The networking foundation increment establishes an internal optional network
module so Qt Network does not become an installed core dependency or enter its
public headers. This split is justified by the independently deployable
headless core consumer. The module's HTTP client accepts only credential-free
HTTP(S) URLs, follows a bounded number of explicit redirects without permitting
HTTPS downgrade, and enforces aggregate timeout, response-size, cancellation,
and successful-status policies. Tests use a loopback HTTP fixture and never
contact a public service.

Gate: representative local articles render correctly, links and resources
work, and approved online dictionary scenarios pass without weakening the
local rendering gate.

### Phase 8 — Complete Application UI

- Port the main window, tabs, dictionary and group controls, preferences,
  history, favorites, inspector, and scan popup.
- Preserve the pinned legacy GoldenDict UI layout and interaction behavior.
  This migration does not include a voluntary UI redesign; make only the
  smallest behavior or layout changes required by Qt 6, Qt WebEngine, or Linux
  platform compatibility, and document those differences.
- Keep UI code dependent only on the `goldendict_core` application facade.

The tab-state foundation adds a bounded transport-neutral article-tab session
behind the desktop facade. It provides deterministic create, activate, close,
close-others, reuse, and explicit-new-tab policies plus per-tab back/forward
state containing query, group, title, and internal-link context. The existing
main window continues to use one article view and records ordinary lookups in
the active core tab. Multi-tab widgets, session persistence and migration, and
tab preferences remain separate Phase 8 increments.

The multi-tab presentation increment mirrors that session with visible Qt
Widgets tabs and retained per-tab WebEngine views. It supports foreground and
background creation, activation, close and close-others commands, middle-click
close, independent rendered/query/group state, and facade-backed back/forward
restoration. Ordinary internal links reuse the active tab; Ctrl-click and
middle-click open a background tab, while Shift-click opens a foreground tab.
Session persistence, migration, placement preferences, and configurable
foreground/background policy remain separate Phase 8 work.

The first tab-persistence increment adds a complete transport-neutral core
session DTO and optional canonical current-configuration records. Export and
atomic restore preserve ordered stable IDs, the active tab, every bounded
navigation history and cursor, group/internal-link identity, and a
deterministic collision-free next ID. Older current files retain the existing
single empty tab. The following application increment restores that DTO before
Widgets synchronization, rebuilds views from current entries without adding
history, and atomically saves facade exports after successful mutations and on
orderly shutdown. An isolated two-start GUI smoke pins restart identity and ID
continuation. Tab-opening preferences, geometry, and matching legacy migration
remain a later bounded increment.

The first Phase 8 state-migration increment imports dictionary paths and named
sound directories from a bounded legacy XML configuration when no current
configuration exists. The core owns parsing and atomic persistence; the legacy
file is never modified, a current configuration always takes precedence, and
malformed input leaves no partial replacement. A subsequent bounded slice
imports ordered dictionary groups and their stable transport-neutral metadata,
retaining unknown nonempty dictionary IDs without catalog resolution. Online
sources and preferences remain separately gated migration work; group-aware
lookup behavior and group editing remain later Phase 8 work.

The P3 local-source editor replaces the single-folder bootstrap chooser with
one bounded dialog for ordered dictionary paths and named sound directories.
Widgets provide add, remove, and explicit reorder controls, while the core
owns validation and atomic persistence. The composition root saves a complete
candidate before swapping facades and restores the exact article session, so
groups, preferences, geometry, history, favorites, and unrelated configuration
survive success and failure. Duplicates and empty sound names remain accepted;
empty sound paths are rejected. The minimal Qt 6 dialog intentionally omits
online sources, recursive flags, and sound icons absent from the current core
model; explicit reorder buttons make ordering deterministic.

The P4a online-source current-persistence foundation adds ordered bounded
MediaWiki, website, Forvo, and DICT records to the transport-neutral core
configuration. It canonically stores stable identity, display name, enabled
intent, and only fields accepted by the Phase 7 adapters. Forvo credentials,
website iframe and legacy query-encoding settings, runtime composition, UI,
and network activity remain excluded. Older current files receive fixed safe
defaults. The P4c closure imports all P4a-supported legacy source families in
the same bounded Expat pass as the rest of the configuration and persists only
after the complete candidate validates. MediaWiki and credential-free Forvo
records map directly. Websites require the supported `%GDWORD%` template and
explicit non-iframe intent. DICT URLs map to a credential-free host and port,
with the legacy empty database/strategy defaults, only when each list contains
at most one value. Icons, API keys, userinfo, legacy query encodings,
iframe-enabled websites, and multi-database or multi-strategy DICT records are
not imported; recognized unrepresentable intent aborts the entire migration.

The P4b external-program current-persistence increment adds ordered bounded
shell-free program records with stable identity, enabled intent, plain-text,
HTML, or prefix-match output, absolute executable paths, ordered argument
templates, and optional absolute working directories. Canonical collection and
argument counts distinguish explicit empty state and reject malformed parent or
argument ordering. Audio programs, icons, environment and process policy,
runtime composition, UI, and execution remain excluded. P4c parses legacy
external command lines with the pinned GoldenDict grammar and stores the
absolute executable plus ordered arguments without a shell. Only plain-text,
HTML, and prefix-match programs are representable. Audio, relative, empty,
shell-dependent, and otherwise invalid commands abort the entire migration;
icons remain excluded. This closes P4 and unlocks P5 source composition without
activating adapters, processes, networking, or UI here.

The P5a.1 runtime-composition foundation exports one transport-neutral core
dictionary-source extension contract and an overload of the existing headless
service factory that atomically owns supplied runtime sources. The optional
network module wraps the existing MediaWiki and website adapters, includes only
enabled records, preserves each persisted family order and stable identity, and
appends those families after the unchanged local catalog. Core continues to own
query validation, cancellation, article assembly, sanitization, groups, and
public result conversion. Loopback tests cover real adapter calls without public
network access.
The generic core seam does not require runtime identities to exist in persisted
configuration, so later composers may generate deterministic derived identities.
The P5a.1 network composer owns DTO consistency: it validates the complete
candidate and emits only enabled configured records. Core atomically rejects
null sources, empty IDs, and local/runtime identity collisions. Every composed
method rejects pre-cancelled or pre-expired requests; transport operations also
poll cancellation and deadline state and recheck it before returning, with the
existing bounded adapter timeout as the fallback between polling opportunities.

P5a.2 composes the existing Forvo and DICT adapters after MediaWiki and websites.
Forvo credentials are supplied only by an explicit in-memory configured-source-ID
map and never enter persistence, logs, identities, diagnostics, or errors.
Missing credentials are recoverable source-ID-only diagnostics; malformed,
unknown, non-Forvo, or identity-colliding input fails before a result is returned.
Persisted Forvo family and language order is preserved through deterministic
length-framed child identities that retain configured-source provenance. Forvo
lookup exposes audio through typed resources, DICT exposes definitions and
suggestions, and both preserve the P5a.1 request contract.

P5a.3 appends enabled external programs after DICT in persisted order and closes
the application composition path. Programs retain configured identity and run
the P4b absolute executable plus ordered argument templates directly, without a
shell. Plain-text and HTML output becomes an untrusted article; prefix-match
output becomes bounded ordered line suggestions; programs expose no resources.
All wrappers honor zero limits, cancellation, absolute deadlines, bounded
process output, and translated runtime errors. The composition root now builds
the complete local-plus-runtime facade, restores its session, atomically saves
the candidate, and only then rebinds presentation objects. Any construction,
restore, or save failure preserves the active facade and configuration.

The P5b presentation increment extends the bounded source dialog with ordered
MediaWiki, website, Forvo, and DICT editors. Widgets stage transport-neutral
records, retain stable hidden identities, expose enabled intent and explicit
reordering, and report core validation or application replacement failures
without closing the dialog. Apply constructs one complete configuration,
restores the exact article session into a fully composed replacement, saves it
atomically, and only then rebinds presentation state. Exact empty collections
are preserved, including the Forvo presence marker. Credentials, external
program editing, icons, iframe and legacy encoding settings remain excluded.

The P5c presentation increment adds ordered external-program editing to the
same dialog and complete-candidate apply path. Widgets retain hidden stable
identities and enabled intent, expose only plain-text, HTML, and prefix-match
results, select absolute executables and optional working directories, and
stage each ordered argument template separately with explicit add, remove, and
reorder controls. `%GDWORD%` remains an argument-template substitution; the UI
never reconstructs a command line, invokes a shell, or executes a program.
Core validation and canonical persistence remain unchanged. Apply composes and
restores a complete replacement before atomic save and presentation rebinding,
so validation, composition, restore, or save failure leaves the active
configuration and facade intact while the dialog remains open for correction.

The current-preferences foundation adds deterministic portable defaults and
bounded optional preference records to the core-owned current configuration.
It round-trips locale, appearance, window, hotkey, scan, audio, proxy policy,
network, zoom, history, favorites, article, and lookup limits without adding
Widgets dependencies or wiring behavior. The following bounded migration
increment imports the matching portable, non-secret legacy XML preferences
with strict conversions and current-default fallback. Existing current state
still wins, persistence is atomic, and the legacy source remains immutable;
credentials, layout/session state, source definitions, UI, and runtime wiring
remain excluded. This preference migration unlocks T3; P3 remains independently
ready from the current-preferences foundation.

P8-PREF-5 activates the credential-free manual HTTP CONNECT subset of that
model. Enabled MediaWiki, website, and Forvo adapters receive a request-local
Qt HTTP proxy; enabled raw-TCP DICT sources establish a private bounded CONNECT
tunnel with the same candidate. Disabled mode remains direct. The DICT
handshake shares its request deadline and cancellation polling with the
protocol, maps proxy authentication and transport failures deterministically,
and redacts endpoint and credential data. System, SOCKS5, HTTP GET, WebEngine,
external-program, and application-global proxy behavior remain excluded.

The bounded T3c increment adds `open_new_tabs_after_current` (default false)
and `open_new_tabs_in_background` (default true), migrated from their exact
legacy XML names. New tabs append or insert after the active core tab; default
gestures use configured activation while explicit commands and Shift versus
Ctrl/middle-click overrides remain explicit. It also stores only opaque main-
window geometry, with strict legacy `mainWindowGeometry` Base64 migration and
a 64 KiB decoded limit. Widgets alone calls Qt geometry APIs; malformed,
oversized, empty, or Qt-rejected data preserves the default layout.
`mainWindowState` and dock state remain excluded.

The following unlabelled Phase 8 layout-state increment adds bounded current-
format persistence for the Qt 6 main window's history and favorites docks and
article toolbar. Core stores at most 64 KiB of opaque state; Widgets uses a
current-only state version, restores transactionally, validates floating
widgets against the available screen topology, and returns to the previous
usable layout on rejection. The pinned legacy `mainWindowState` is not replayed:
its search, results, favorites, and history panes plus navigation and dictionary
toolbars do not match the current widget hierarchy, so partial opaque replay
would be incompatible rather than parity. Legacy-compatible shell layout and
the final opaque-state acceptance audit remained later Phase 8 work.

The next unlabelled Phase 8 shell increment aligns the already-functional
favorites and history docks with the pinned legacy `favoritesPane` and
`historyPane` identities. Both panes are visible by default in a deterministic
vertical split on the right, with favorites above history and usable article
content retained in the center. The private current Qt state version advances
while transactionally accepting the immediately preceding `favoritesDock` and
`historyDock` state; legacy version-1 `mainWindowState` remains excluded.
Search and results panes, navigation and dictionary toolbars, menus, and the
opaque-state acceptance audit remained separate Phase 8 leaves.

The following unlabelled Phase 8 shell increment adds the pinned `navToolbar`
hierarchy for the existing Back, Forward, group, query, and lookup controls.
It preserves their real facade-backed workflows, deterministic order, usable
expanding query input, and Alt+D/Ctrl+L query focus while constraining movement
to the legacy-compatible top and bottom areas. The private current Qt state
version advances transactionally through the immediately preceding layout and
the older dock-identity transition; version-1 legacy bytes remain excluded.
Results navigation, `dictionaryBar`, menus, pronunciation, scan controls, the
remaining article toolbar audit, and the opaque-state acceptance audit remained
separate unlabelled Phase 8 leaves.

The following unlabelled Phase 8 shell increment adds the pinned `dictsPane`
results-navigation hierarchy for completed bounded lookups. Its ordered
`dictsList` mirrors the active article tab's real dictionary results and scrolls
keyboard- or mouse-activated rows to the corresponding composed article
section. Empty, failed, cancelled, superseded, background, and closed-tab state
cannot leak stale rows into the active pane. The pane is visible above the
completed favorites/history hierarchy on the right by default. The private Qt
state version advances while transactionally accepting the three immediately
preceding Qt 6 layouts; legacy version 1 remains excluded. The prefix/suggestion
`searchPane`, `dictionaryBar`, menus, and the opaque-state acceptance audit
remained separate leaves.

The next unlabelled Phase 8 shell increment adds the pinned `searchPane` and
`wordList` identities as a visible left-side prefix-suggestion pane. A single
Widgets-owned serial worker submits only the existing bounded suggestion
contract, cancels replaced work, and rejects stale tab generations; empty,
failed, cancelled, superseded, and closed-tab requests clear their presentation
without backend enumeration. Ordered suggestions remain group-aware and
activate the existing lookup/tab/history workflow. The private Qt state version
advances while transactionally accepting versions 5, 4, 3, and 2 and seeding
the absent pane at its deterministic default; version 1 remains excluded.
Full-text and compound/expression search, `dictionaryBar`, menus, complete shell
auditing, and the opaque-state acceptance audit remained separate leaves.

The following Phase 8 dictionary-participation prerequisite makes the existing
bounded lookup and suggestion dictionary-ID filters explicit when their lists
are empty. Existing callers retain the empty-means-unfiltered default, while an
active empty filter completes successfully without consulting a backend. This
transport-neutral contract enables a later Widgets-owned `dictionaryBar` to
represent the legacy all-off state without fake identities, persisted group or
source mutation, or presentation-owned lookup semantics. Toolbar hierarchy,
local participation state, Qt layout-state integration, and action exposure
remain the dependent unlabelled Phase 8 leaf.

The dependent unlabelled Phase 8 Widgets leaf adds the legacy-compatible
`dictionaryBar` as a real catalog-backed toolbar. Ordered, per-group check
state is ephemeral for one application run, is seeded from existing group
muting metadata without rewriting it, and becomes an explicit lookup and
suggestion filter only while the toolbar is visible. Active changes replace
the active tab's work while background requests retain their submitted
snapshot; all-off completes through the existing empty-filter contract. The
private Qt state version advances to 7 and transactionally accepts versions
6 through 2 while seeding only the absent toolbar default; version 1 remains
excluded. Menus, persistent group/source editing, and the opaque-state
acceptance audit remained separate leaves.

The next unlabelled Phase 8 menu leaf adds the pinned `menubar` and `menuView`
identities for the already-complete shell. The View menu reuses the exact
`toggleViewAction()` instances for `searchPane`, `dictsPane`, `favoritesPane`,
`historyPane`, `dictionaryBar`, and `navToolbar`, retaining their synchronized
visibility and checked state plus the legacy pane shortcuts and relative
ordering. It adds no proxy actions or unsupported entries. Qt main-window
state remains at private version 7 because menus do not alter the saved
dock/toolbar hierarchy. File, Edit, Search, History, Favorites, and Help menus,
View styling and always-on-top controls, and the opaque-state acceptance audit
remained separate leaves.

The following unlabelled Phase 8 menu leaf adds the pinned `menuHistory`
identity after View. It reuses the exact `historyPane` toggle action and exposes
the existing export, import, and clear workflows in legacy order with their
pinned action identities and separator. Pane buttons and menu entries share one
action path; empty and in-progress state cannot diverge between them. Widgets
continues to own native file dialogs while application composition owns the
compatibility text export and atomic destination replacement. Qt main-window
state remains at private version 7. The other menu branches and the opaque-state
acceptance audit remained separate leaves.

The next independent Phase 8 menu leaf adds the pinned `menuFile` before View
and History. It reuses the existing preference-backed new-tab, print-preview,
print, and atomic HTML-save actions, adds orderly application quit, and retains
the legacy identities, supported relative order, separators, roles, and
shortcuts.
The tab corner control and article toolbar dispatch through those same action
instances. Page setup, file rescanning, and close-to-tray remain absent because
the current Qt 6 application has no backed implementation for them; the
existing PDF export remains toolbar-only because it has no pinned File-menu
entry. Widgets owns native dialogs and presentation busy state, while the
facade and composition root retain tab/session mutation and orderly-shutdown
persistence. Qt main-window state remains at private version 7. The other menu
branches and the opaque-state acceptance audit remained separate leaves.

The following independent Phase 8 menu leaf adds the pinned `menu_Edit`
identity between View and History. It exposes only the backed `dictionaries`
action, retaining the legacy text, F3 shortcut, and no-role behavior while
sharing one action path with the existing source-configuration button.
Widgets owns modal and busy presentation; the existing complete-candidate
application path continues to own validation, session restoration, atomic
persistence, failure preservation, and facade replacement. The pinned
`preferences` action remains absent because Preferences UI is not implemented.
Qt main-window state remains at private version 7. The other menu branches and
the opaque-state acceptance audit remained separate leaves.

The dependent bounded Preferences leaf adds the pinned `preferences` action
after Dictionaries with `&Preferences...`, F4, and `PreferencesRole`, and a
legacy-shaped General page containing only the backed Tabs group. Its two
controls configure whether new tabs open after the current tab and whether
they open in the background. Widgets only collect and display the complete
preferences candidate; the composition root validates it, recomposes the
application, restores the complete article session, persists atomically, and
then replaces the live facade and presentation preferences. Cancel and failed
validation leave configuration and runtime state unchanged. Preferences for
appearance, tray, hotkeys, scan popup, audio, network/proxy, full-text search,
history, favorites, and advanced behavior remain independent Phase 8 leaves
because their current runtime application contracts are absent; Phase 8
acceptance remains blocked on explicitly approved backed subsets for them.

The subsequent Preferences leaf extends General with the pinned History
group: `Store history` defaults on and `Maximum history size` defaults to 500
within the legacy `0..99999` range. Disabling storage affects only ordinary
future lookups; explicit import still replaces history and is bounded by the
configured maximum. Lowering the maximum immediately removes oldest entries,
including clearing retained history at zero. Validation, recomposition,
complete-session restoration, atomic file replacement, rollback on a later
configuration-save failure, and final facade/presentation replacement remain
composition-root responsibilities. Cancel and failures preserve prior state.
Appearance, tray, hotkeys, scan popup, audio, network/proxy, full-text search,
favorites, and advanced Preferences remain separate Phase 8 leaves.

The subsequent Preferences leaf extends General with the pinned
Favorites deletion-confirmation checkbox, defaulting on. It confirms removal
of the selected word or complete folder subtree before the existing path-based
command is dispatched; rejection is a pure cancellation, while acceptance and
disabled confirmation retain immediate atomic favorites persistence and
failure preservation. Applying the complete preference candidate preserves
favorites ordering, selection and expansion, selected-folder targeting, and
the article session. The pinned favorites save interval remains unexposed
because the current model has no delayed-save contract. Appearance, tray,
hotkeys, scan popup, audio, network/proxy, article, full-text search, and
advanced Preferences remain separate Phase 8 leaves.

The following independent Preferences leaf extends General with the backed
Articles controls. `Collapse articles more than` defaults off and enables a
`1..100000` symbol limit that defaults to 2000 and advances by 50; the lower
bound follows the current validated configuration contract rather than the
legacy widget's ineffective zero value. Core composition collapses only
multi-dictionary results whose sanitized plain text is strictly longer than
the limit, while a sole dictionary result stays expanded. Applying the policy
recomposes the runtime and immediately replays current non-empty tabs after
complete-session restoration. Widgets restores per-tab scroll and in-page
search presentation after each replay; cancel and failures leave the existing
facade and presentation unchanged. Appearance, optional-parts, input-filter,
diacritic, synonym, audio, network/proxy, full-text-search, and advanced
Preferences remain separate Phase 8 leaves.

The following independent Preferences leaf extends General with the backed
Input phrase length controls. `Ignore input phrases longer than` defaults off
and enables a `1..1000000` symbol limit that defaults to 1000 and advances by
10. The configured application/core boundary counts Unicode scalar values and
rejects over-limit manual, suggestion, history/favorites, article-link,
selection, restored-session, and dictionary-browser-activated lookups before
backend or navigation mutation; input is never truncated. Rejection retains
the entered text, reports the limit non-modally, and preserves group, history,
tabs/session, results, cancellation/deadline, and stale-response guarantees.
The current bound and Unicode-scalar rule intentionally replace the pinned
legacy widget's `9999999` maximum and Qt UTF-16-code-unit count. The next
independent Preferences leaf is General/Ignore diacritics.

The General/Ignore diacritics leaf adds the pinned default-off checkbox and
keeps the original query in history and restored tab navigation. Exact local
lookup first obtains folded-index candidates, then compares NFC, default
Unicode case fold, NFC; when enabled it additionally applies NFD, removes all
Mn/Mc/Me marks, and returns to NFC. Punctuation and whitespace remain
significant in this collision check. Prefix lookup, suggestions, aliases,
redirect targets, group participation, and exported headword enumeration keep
their existing behavior. Runtime sources use a capability-aware public request
contract; unsupported configured sources fail explicitly rather than producing
source-dependent matches.

The next independent Phase 8 menu leaf adds the pinned `menuSearch` identity
between Edit and History. It exposes only the existing in-article
`searchInPageAction`, with its legacy text, role, and Ctrl+F shortcut, and
shares that canonical action with the article-toolbar search controls. Widgets
retains per-tab query and match presentation and rejects stale WebEngine find
completions without changing facade navigation or session state. The pinned
full-text action is absent because no full-text workflow is backed; compound,
scan, and global-search placeholders are likewise excluded. Qt main-window
state remains at private version 7. The remaining menu branches and the
opaque-state acceptance audit remained separate leaves.

The following independent Phase 8 menu leaf adds the pinned `menuFavorites`
identity after History. It reuses the exact `favoritesPane` toggle and the
existing XML export, XML import, and active-query Add actions in pinned
supported order, with one separator and unique Ctrl+I/Ctrl+E ownership. The
article toolbar and menu share the same action instances; query, tree
selection, empty-tree, and busy state therefore cannot diverge. Widgets owns
dialogs and presentation state, while the composition root and core retain
selected-folder targeting, validation, atomic persistence and transfer, and
failure preservation. The pinned plain-list export is absent because no
authoritative Qt 6 implementation exists, and selected-item removal remains
toolbar-only because the legacy menu has no removal entry. Qt main-window
state remains at private version 7. The Help branch and the opaque-state
acceptance audit remained separate leaves.

The final independent Phase 8 menu leaf adds the pinned `menu_Help` identity
after Favorites. It exposes only the supported legacy Homepage, Configuration
Folder, and About actions in their relative order and roles. Homepage dispatch
accepts only the fixed credential-free `https://goldendict.org/` target;
Configuration Folder uses the already-resolved current profile directory; and
About reads the CMake-synchronized application version, runtime Qt version,
and shipped GPL license identity. Widgets owns validation, desktop dispatch,
dialog presentation, and private deterministic seams. The F1 reference action
is absent because no current help collection is shipped, and Forum is absent
because no working HTTPS target is verified. No session, layout, persistence,
public API, installed boundary, or network-fetch behavior changes.

The final Phase 8 legacy opaque-main-window-state acceptance audit closes
without importing version-1 bytes. Bounded legacy `mainWindowGeometry`
migration remains supported, but the pinned repository contains no authentic
legacy `mainWindowState` artifact and does not pin an exact Qt runtime whose
opaque `QMainWindow::saveState(1)` representation can be proven compatible
with Qt 6. Windows-specific normal/maximized rectangles and their explicit
window-mode behavior are likewise excluded by the available evidence.
Inventing, translating, or opportunistically replaying those undocumented
bytes would risk accepting an incompatible layout. Current Qt 6 state versions
2 through 7 remain bounded and transactionally supported, with rollback on
malformed, incompatible, oversized, or unusable input and semantic defaults
for shell objects absent from older current versions.

The Phase 8 R6b legacy article-session audit closes without a parser or import
surface. At pinned legacy commit
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, the XML configuration has no tab,
active-article, navigation-history, dictionary-binding, or scroll-position
records. Startup creates one fresh tab and shows the welcome article, while
shutdown saves configuration, history, and favorites but not article views;
per-view navigation and scroll state are memory-only. There is therefore no
legacy session source format, version, discovery trigger, or accepted unit to
migrate. A first migrated startup keeps the current deterministic no-session
fallback, and later startups use the already-bounded atomic current-format
session store. Inventing a legacy representation would not be parity work.

The Phase 8 R6c application increment discovers the pinned legacy
configuration location without moving parsing policy into Widgets. Portable
mode exclusively selects `portable/config` beside the executable. Otherwise,
Linux/Unix selects `~/.goldendict/config` when the old directory exists and
falls back to the XDG configuration root's `goldendict/config`; Windows first
recognizes `~/Application Data/GoldenDict/config` and otherwise uses roaming
`APPDATA/GoldenDict/config`; macOS uses `~/.goldendict/config`. The legacy
directory gate is preserved exactly, so a selected missing, unreadable,
malformed, or unsafe candidate never falls through to another profile.
Existing current state wins before the legacy path is inspected, portable
current configuration is stored beside the portable source, and discovered
legacy files remain read-only.

The Phase 8 R6d companion increment reuses that one selected legacy profile
for the exact lowercase `history` and `favorites` files. Portable, Windows,
and macOS companions remain beside the selected legacy `config`; Linux/Unix
favorites does likewise, while Linux/Unix history preserves the pinned XDG
split by using profile-local `history` when it exists and otherwise selecting
`goldendict/history` below the generic data root. Current `history-v1` and
`favorites-v1` files live beside the resolved `core.conf` and take precedence
independently. Missing, malformed, unreadable, oversized, symlink, directory,
or special companion inputs never trigger another profile search. Each valid
companion migrates through its existing bounded core parser and its own atomic
destination transaction, so either companion may succeed without changing the
other or either legacy source.

The following user-state increment adds a transport-neutral core history store
with bounded group-aware UTF-8 entries and a recoverable legacy line-format
migration. Current state takes precedence, new state is written atomically,
and the legacy history is never modified. The application now records submitted
lookups, deduplicates them case-insensitively, persists them atomically, and
exposes a reusable history dock. Filtering and explicit clearing are also
available; export remains later Phase 8 work.

The first favorites increment adds a transport-neutral hierarchical store for
folders, expansion state, ordered headwords, and Unicode content. It imports
the bounded legacy XML tree only when current favorites are absent, rejects
entities and malformed or over-limit input, writes the upgraded representation
atomically, and never modifies the legacy source. The application presents the
tree with preserved hierarchy and expansion state, supports re-lookup and
case-insensitively deduplicated root-level additions, removes a selected item
or folder through its stable tree path, and keeps persistence in the
composition root. Nested editing and import/export remain later Phase 8 work.

The dictionary-browser increment restores a reusable dictionary-information
dialog backed only by the public desktop facade. It lists the loaded catalog,
shows stable identity, edition, and source provenance, and performs bounded
per-dictionary prefix suggestions without reading articles in the GUI.
Activating a suggested headword routes through the existing lookup workflow.
The displayed bounded result can also be exported atomically as a UTF-8 text
file with a compatibility BOM and one sanitized headword per line. Descriptions
from metadata-bearing Aard, MDict, SLOB, and ZIM dictionaries are exposed by
the core catalog and rendered as plain text. Backend-owned article and headword
counts are also exposed for every migrated local format without loading article
content in the GUI. The browser can copy a dictionary's source path and open
its containing folder through desktop services. Descriptions for formats
without equivalent metadata, unbounded full-list export, and wildcard and
regular-expression filtering remain later Phase 8 parity work.

The bounded R2 residual adds wildcard and PCRE2-compatible regular-expression
filter modes to the dictionary browser. Advanced patterns must begin with a
non-empty literal prefix: core uses that prefix to obtain at most 100 existing
lightweight suggestions and applies Unicode-aware matching without enumerating
the dictionary. Wildcards support `*`, `?`, `[abc]`, `[!abc]`, and backslash
escaping; regular expressions use PCRE2 syntax. Matching is case-insensitive by
default with an explicit case-sensitive option. Patterns are limited to 256
UTF-8 bytes, and PCRE2 match, depth, and heap limits bound each candidate.
Invalid, unseeded, or resource-exhausting patterns return no partial results.
Plain prefix browsing, result ordering, dictionary identity, lookup activation,
and displayed-headword export remain unchanged.

The bounded R5 residual completes the dictionary-information surface rather
than restoring the legacy WebKit developer inspector. The core catalog now
propagates source and target languages and bounded plain-text descriptions for
BGL, Dictd, DSL annotations, EPWING, GLS, SDict, StarDict, and XDXF in addition
to the existing Aard, MDict, SLOB, and ZIM metadata. The Widgets dialog presents those
details in selectable read-only controls. Public source provenance removes URL
credentials, queries, and fragments; raw source configuration, process
arguments, multi-file inventories, article inspection, and DevTools remain
excluded.

The Phase 8 R3a.1 prerequisite adds a bounded transport-neutral contract for
enumerating one dictionary's complete headword index. It reproduces the pinned
legacy list semantics: exact duplicate removal followed by case-sensitive,
non-locale-aware UTF-16 code-unit ordering. Opaque authenticated cursors are
bound to one immutable core-service snapshot and dictionary. StarDict lazily
builds a cancellation-aware sorted ordinal index and then traverses all pages
linearly without copying a complete word list.

The bounded R3a.2 leaf extends that ordinal mechanism to Aard, BGL, Dictd,
DSL, EPWING, GLS, MDict, SDict, SLOB, XDXF, and ZIM. Enumeration uses each
backend's immutable natural article-key records, including indexed aliases,
expanded DSL headwords, MDict link keys, and ZIM redirect titles. It excludes
metadata-only values, resource keys, and synthetic article text.
Unsupported local and runtime sources fail before source I/O.

The bounded R3a.3 leaf applies the same mechanism to LSA, configured sound
directories, and ZIP sound dictionaries. It enumerates the already validated
immutable record names: LSA strips a case-insensitive terminal `.wav`, sound
directories use the basename without its supported audio extension, and ZIP
sounds retain safe member directories while stripping the supported audio
extension and its preceding trailing spaces. Exact derived-name collisions are
deduplicated while case variants remain distinct. Enumeration does not revisit
the filesystem, read audio resources, decode LSA Vorbis data, or decompress ZIP
members. Runtime sources remain unsupported. R3b export UI and file writing
remain a separate leaf.

The history-management increment adds case-insensitive live filtering to the
reusable history pane and an explicit clear action. Clearing remains a
composition-root command: it atomically persists an empty bounded history
through the core store and refreshes the presentation only after success. The
pane also exports the complete history as an atomically replaced UTF-8 text
file with a compatibility BOM and one sanitized headword per line. History
import accepts a bounded UTF-8 text file with an optional BOM, ignores blank
lines, trims surrounding whitespace, preserves file order, and atomically
replaces current history only after complete validation. Group-selection
controls remain later Phase 8 work.

Group-aware lookup resolves a selected nonzero group against the discovered
core catalog and queries its available dictionaries in configured order.
Stale member IDs are skipped, duplicate resolved identities are used once, and
an empty resolved group remains empty. Group zero retains all-dictionary
behavior; for legacy compatibility, an unknown nonzero group also falls back
to all dictionaries. The selected group is carried by transport-neutral lookup
and suggestion requests and is preserved when history entries are recorded and
activated. Group-selection and editing controls remain separate UI work.

The dictionary-group presentation increment adds a visible all-dictionaries
selector followed by configured groups, including configured icons and
shortcuts. Selection persists across ordinary lookups and history activation;
a deleted or missing selection deterministically falls back to group zero. A
bounded Qt Widgets editor manages ordered groups and catalog membership,
muted and popup-muted membership, icons, favorites folders, and shortcuts.
The composition root applies the edited collection through existing core
validation and atomic persistence while preserving unrelated configuration.

Favorites can be organized without leaving the main window: a new folder is
created at the root or inside the selected folder, and Add to Favorites targets
the selected folder (or a selected headword's parent). The composition root
applies each tree mutation to a copy, persists it atomically through core, and
refreshes the tree only after success. Selected folders and headwords can also
be renamed or reordered among their siblings through the same validated
copy-and-persist command path; nested selections can also be moved back to the
root. The pane imports and atomically exports the bounded legacy-compatible
UTF-8 XML tree; import replaces current state only after complete XML
validation and successful current-format persistence.

The bounded R1 residual adds arbitrary drag-and-drop moves for one favorite
headword or complete folder subtree. Core validates source and destination
paths, exact insertion ordering, duplicate siblings, stale paths, and cycles,
then atomically persists the complete candidate before Widgets refresh. Drops
onto folders insert first, drops between siblings preserve the indicated
position, and successful refreshes retain the moved selection and folder
expansion state. The current and legacy-compatible file formats are unchanged;
copy and multi-selection behavior remain outside this increment.

The subsequent Phase 8 Preferences leaf restores General/Extra search
via synonyms. The existing default-on persisted preference now controls an
exact-lookup-only core workflow: private StarDict `.syn`, Babylon BGL
alternate, and GLS secondary-headword mappings resolve primary forms, which
are searched across the already participating dictionaries without rewriting
the original query or changing suggestions and other match modes. The private
StarDict cache format advances and older implementation-generated caches
rebuild automatically. The subsequent Preferences leaf is
General/Expand optional parts.

The General/Expand optional parts leaf adds the pinned default-off checkbox and
restores DSL `[*]` zones end to end. Disabled pages initially hide every
optional zone behind one script-free article control and exclude hidden text
from large-article threshold measurement; enabled pages expose the zones and
omit the control. Core owns bounded DSL semantics, sanitization, and desktop
composition, while Widgets edits the complete candidate through the existing
atomic recomposition/session-restoration transaction. Plain-text results,
headless visible rendering, queries, filters, ordering, bounds, cancellation,
deadlines, suggestions, and non-DSL formats remain unchanged. The repository
task graph continues with the audited Preferences leaves below.

The Phase 8 Preferences acceptance audit compares every control in pinned
legacy `preferences.ui` and its `preferences.cc`, `config.hh`, and
`mainwindow.cc` backing with the current DTO, migration, runtime, Widgets, and
tests. Phase 8 Preferences is not yet accepted. Tab placement, history size and
recording, Favorites deletion confirmation, article collapsing, input-phrase
limits, diacritic policy, synonym expansion, and optional parts are fully
backed and acceptance-covered.

The dependency-ordered ready graph is:

1. `P8-PREF-1 General/Tabs — Hide single tab` is complete. The installed
   transport-neutral preferences DTO now carries the default-off boolean, with
   canonical current persistence, strict `hideSingleTab` migration, the pinned
   checkbox, and Widgets-owned Qt tab-bar auto-hide through the existing atomic
   complete-candidate transaction. Configuration and offscreen transaction,
   one/multiple-tab, and restart coverage preserve active identity, ordering,
   session, layout, and unrelated preferences. This deliberate DTO layout
   evolution requires installed C++ consumers to rebuild; the unchanged 1.6.0
   version is distinguished by Conan SCM recipe and package revisions.
2. `P8-PREF-2 General/Tabs — MRU Ctrl-Tab order` is complete. The installed
   preferences DTO carries the default-off boolean with canonical current
   persistence and strict `mruTabOrder` migration; consumers must rebuild for
   the second deliberate layout evolution. Widgets owns stable runtime MRU
   identities under the existing 32-tab session capacity and reconstructs
   them after replacement without persisting or reordering tabs. Disabled
   traversal stays positional; enabled Ctrl+Tab and Ctrl+Shift+Tab traverse a
   chord-stable MRU sequence symmetrically, intentionally correcting the
   pinned legacy reverse inconsistency. Configuration, transaction,
   lifecycle, keyboard, and restart coverage pin stale-ID cleanup and order
   preservation.
3. `P8-PREF-3 General/Interface — ESC hides main window` is complete. It reuses
   the existing default-off, persisted, and strictly migrated
   `escape_hides_main_window` field and exposes the pinned checkbox. Widgets
   hides the main window only when a focused child declines plain ESC; disabled
   behavior is unchanged and modal dialogs retain precedence. Offscreen
   transaction, query/article focus, child-consumption, modal, and two-process
   restart coverage pins cancel/failure preservation and successful persistence
   without adding tray, close-to-tray, scan, or hotkey behavior.
4. `P8-PREF-4 General/Interface — Article click interaction` is complete. It
   reuses the existing persisted and strictly migrated booleans and exposes the
   pinned controls. Eligible pointer events run only a fixed application-owned
   DOM query in WebEngine's isolated application world; bounded results enter
   the existing selection-lookup command exactly once, while links, inputs,
   editable content, stale callbacks, and disabled modes cannot dispatch.
   Configuration and isolated two-process WebEngine coverage pins all four
   combinations, exclusions, transaction failure, and restart without changing
   CSP, sanitization, navigation, history, tabs, or article security.
5. `P8-PREF-5 General/Network — Manual HTTP CONNECT proxy` is complete. It uses
   the existing
   credential-free proxy DTO/migration, `HttpRequest::Proxy`,
   `QNetworkAccessManager::setProxy`, and atomic runtime recomposition. It
   exposes disabled/manual HTTP CONNECT host and port only; credentials, system
   proxy, SOCKS5, legacy HTTP GET proxying, WebEngine policy, and process-global
   mutation remain excluded. Configuration/migration tests, loopback
   origin/proxy composition tests, and an offscreen transaction/restart smoke
   prove strict validation, direct disabled traffic, proxied enabled traffic,
   live-facade preservation, and secret-free persistence/diagnostics.
6. `P8-PREF-6 General/Network — Qt Network cache policy` is complete after its
   separately audited Phase 7 runtime owner. It exposes only the pinned cache
   size and clear-on-exit controls and reuses the neutral persisted fields plus
   validate/prepare/persist/activate transaction. Focused configuration,
   runtime lifecycle, offscreen transaction, and two-process restart coverage
   pins exact MiB behavior, failure rollback, request-quiescent owned-directory
   cleanup, and WebEngine isolation, subject only to disposable-byte eviction.
7. `P8-PREF-7 General/Interface — Dictionary context-menu limit` is complete
   after the accepted Phase 7 navigation prerequisite. It reuses the installed
   field and restores the pinned label, tooltip, `0..9999` range, unit step,
   default 20, zero-to-overflow behavior, and `.........` results-pane handoff.
   Successful apply atomically revises every private tab snapshot; cancel,
   failure, and stale actions leave presentation and session state unchanged.

The following controls remain blocked on named non-Preferences prerequisites:
languages, help, and appearance on Phase 9 shipped translations/help/styles;
tray and autostart on Phase 9 desktop integration; hotkeys on Phase 9 global
hotkey and clipboard integration; Scan Popup on Phase 9 scan/clipboard/X11/
Wayland contracts, with Windows technologies in Phase 10; Audio on Phase 7
audio-link routing and Phase 9 playback/process integration; system proxy and
credentials on Phase 7/9 policy, and SOCKS5/HTTP GET on Phase 7 transport
support; full-text controls on Phase 5 indexing/contracts, Phase 6 per-format
support, and the Phase 8 workflow; and update checks on Phase 9 release
integration.

The `Phase 7 network-cache ownership audit` and runtime-owner prerequisite are
complete. Qt Network is the
exclusive owner of the GoldenDict-managed HTTP/HTTPS response cache;
WebEngine remains outside the preference contract. P8-PREF-6 uses the
documented prepare, persist, and activate boundary, with the explicit
exception that evicted disposable cache bytes cannot be reconstructed. It
does not change WebEngine profile policy or clear WebEngine data.

The dependent Phase 8 General/Network cache leaf is complete. It restores the
pinned 0--2000 MiB maximum-size editor and clear-on-exit checkbox, including
the zero-limit enablement rule and injected owned-directory tooltip, while
reusing the existing transport-neutral fields and prepare/persist/activate
transaction. Cancel and every apply failure preserve configuration, runtime
ownership, facade, session, and layout; successful apply and clean restart
publish the exact MiB policy. Reducing or disabling may still irreversibly
evict disposable bytes. WebEngine profile data, cookies, storage, proxy policy,
and cache-now operations remain outside this leaf.

History and Favorites save intervals are intentionally excluded because
current mutations persist immediately. Web plugins have no supported Qt
WebEngine equivalent. The mandatory CSP/sanitizer replaces the optional
cross-site-content control with a stronger security boundary. Header-identity
suppression has no approved transport policy and must not appear as an inert
control. Windows-only Scan Popup technologies remain Phase 10. Legacy fields
not presented in the pinned Preferences dialog, including menu visibility,
always-on-top, search-pane mode, and zoom state, do not create Preferences
parity requirements.

The post-navigation completeness audit finds no independently ready Preferences
leaf after P8-PREF-7. Every remaining dialog control is either blocked by the
named Phase 5/6/7/9/10 prerequisite above or intentionally excluded. The
overall Preferences gate therefore remains open without naming a speculative
successor.

The Phase 7 `bounded per-dictionary article-context navigation` leaf is
accepted and now backs the dependent Preferences control. Widgets
derives one private per-tab presentation snapshot from ordered lookup entries
and shares first-result indexes between the article context menu and existing
results pane; no public core API, persistence field, or inert control was
added.

The context menu lists dictionaries in first-result order, deduplicates
repeated identities, uses the catalog name with an ID fallback, and navigates to
the first represented article for the selected dictionary. The list uses the
persisted `0..9999` bound, whose pinned default is 20, and the pinned legacy
`.........` overflow label. Zero skips dictionary actions and hands any
represented results to the pane through that overflow action.
Overflow exposes the existing results pane instead of creating an unbounded
menu. Empty, failed, stale, and non-lookup pages show no dictionary entries,
and tab, view, presentation, and document identities make captured actions
harmless after replacement or closure. Existing link, selection, copy, image,
print, tab, and results-pane behavior remains unchanged.

The fresh post-Phase-7/8 next-leaf audit selects the Phase 5 `bounded
full-text indexing and query contract` as the sole next independently
implementable prerequisite. The existing core already provides bounded and
cancellable requests, Unicode folding, stable dictionary identity, complete
paged headword enumeration for the migrated article formats, structured
plain-text article retrieval, generated-index lifecycle patterns, persisted
full-text settings, and a reserved full-text match mode. The leaf can therefore
establish and test a transport-neutral contract with a generated reference
corpus before any format or UI depends on it.

The leaf owns bounded document ingestion, index creation, reuse and stale-index
rebuild, whole-word/plain-text/wildcard/regular-expression queries, case and
diacritic policy, word order and distance, dictionary and result limits,
deterministic provenance and match metadata, cancellation, deadlines,
corruption handling, and resource limits. It does not wire a concrete
dictionary format, add the Phase 8 search workflow or Preferences controls, or
publish inert UI. Its gate is a headless generated corpus that proves those
behaviors, including malformed and oversized input and stale or corrupt index
failures, while the installed consumer covers any evolved public contract.

The Phase 5 contract leaf is complete. The installed service exposes only the
bounded transport-neutral query/result/error operation. A private generated
corpus pins versioned create/reuse/stale/corrupt lifecycle behavior, the four
query modes, Unicode case/diacritic policy, word order/distance, filters,
deterministic provenance, limits, cancellation, deadlines, and malformed or
oversized input. Index paths, serialization, ingestion, and lifecycle state
remain private. No Phase 6 adapter or Phase 8 workflow/control is wired.

The first Phase 6 full-text task is P6-FT-1, the private StarDict adapter. Its
prerequisites are the completed Phase 5 contract and StarDict's accepted
discovery, generated-index, primary/synonym record, article-assembly,
enumeration, provenance, and generated-fixture seams. The adapter ingests one
document per validated primary `.idx` record, derives plain text through the
existing article assembler for both supported `sametypesequence` forms, and
excludes `.syn` aliases from duplicate document creation. A distinct private
artifact in the configured index directory tracks every applicable StarDict
source and exercises create, reuse, stale rebuild, and corrupt rebuild. The
application service dispatches and deterministically merges only private
full-text-capable backends while preserving bounded results and typed
unsupported or unavailable errors.

P6-FT-1 changes no installed API, public capability flag, preference, widget,
or presentation behavior. Other formats, resource and metadata text, legacy
full-text index compatibility, highlighting, dependency additions, and
unrelated refactors are excluded. Later per-format adapters depend on the
private capability and service-aggregation pattern established by this leaf;
the Phase 8 full-text workflow remains downstream. P6-FT-1 is complete with
generated plain/HTML, synonym, compressed-data, lifecycle, mixed-service, and
installed-consumer coverage.

The next Phase 6 task is P6-FT-2, the private SDict adapter. Its prerequisites
are the completed Phase 5 contract, P6-FT-1 private capability and service
dispatch, and SDict's accepted discovery, bounded plain/zlib/bzip2 decoding,
complete full-index enumeration, safe HTML/link conversion, article assembly,
stable provenance, and generated fixtures. Each distinct validated article
offset contributes one document. The first full-index record referencing that
offset supplies the canonical headword and a document ID derived from its
record ordinal and article offset; later records sharing the offset are aliases
and do not duplicate the document. Ingestion passes the converted SDict HTML
through the existing inert article assembler and indexes only its bounded plain
text. Metadata, link targets, resources, and raw markup are excluded.

P6-FT-2 owns a distinct private `.gdfts` artifact in the configured index
directory whose complete source-revision input is the discovered `.dct` file.
It must preserve create, reuse, stale-rebuild, corrupt-rebuild, cancellation,
deadline, resource-limit, and contained storage-failure behavior from the
accepted private index lifecycle. In a mixed service, StarDict and SDict
results merge deterministically within the existing global bound; adapted
dictionaries with no match add no error, non-adapted local and runtime formats
return typed unsupported errors, and requested missing IDs return typed
unavailable errors. Filtering and aggregation remain service-owned.

The leaf changes no installed `SearchFullText` API or DTO, public capability
flag, runtime-source interface, configuration, preference, widget, or
presentation behavior. Other adapters, legacy `_FTS` compatibility, index
format changes, new dependencies, resource or metadata indexing, highlighting,
the Phase 8 workflow, and unrelated refactors are excluded. Its later
implementation gate is the focused generated-fixture and mixed-service
acceptance suite, the unchanged installed consumer, the complete Linux Release
test/install path, and clean exact-SCM Conan package creation.

P6-FT-2 is complete. SDict now contributes one private full-text document per
distinct validated article offset, preserves first-record canonical provenance,
indexes only inert assembled plain text, and participates in the accepted
`.gdfts` lifecycle and deterministic mixed-service dispatch without changing
installed or runtime-source surfaces.

The fresh independent post-P6-FT-2 audit selects P6-FT-3, the private XDXF
adapter, as the next bounded leaf. Its prerequisites are the completed Phase 5
contract, P6-FT-1 and P6-FT-2 private capability/service dispatch, and XDXF's
accepted complete record/article retention, multi-key article ownership,
bounded plain/gzip decoding, safe markup/link conversion, inert article
assembly, stable identity, single-source ownership, and generated fixtures.
Pinned legacy XDXF explicitly advertises full-text support. The audit's Dictd
exclusion was based on incomplete legacy evidence and is superseded by the
post-P6-FT-3 audit below.

Each validated XDXF article ordinal contributes exactly one document. The
first validated `<k>` record for that article supplies the canonical headword;
later keys referencing the same article are aliases and do not duplicate it.
The private document ID is derived from the first key's record ordinal and the
article ordinal with an XDXF-specific prefix. Ingestion passes the existing
sanitized `text/html` representation through the inert article assembler and
indexes only its bounded plain text. Metadata, resource contents or paths,
link targets, image references, raw XML or markup, and alias text not present
in the assembled article are excluded.

P6-FT-3 owns a distinct private `.gdfts` artifact in the configured index
directory. The one discovered `.xdxf` or `.xdxf.dz` file is the complete
source-revision input and controls create, reuse, stale rebuild, and corrupt
rebuild. No configured index directory leaves the dictionary typed
unsupported. Cancellation, deadlines, resource limits, corruption, and
storage failures retain the accepted contained per-dictionary behavior. In a
mixed service, StarDict, SDict, and XDXF results merge deterministically within
the existing global bound; an adapted no-match dictionary adds no error,
requested non-adapted local or runtime dictionaries return typed unsupported
errors, and missing requested IDs return typed unavailable errors.

The leaf changes no installed `SearchFullText` API or DTO, public capability
flag, runtime-source interface, preference, widget, presentation, dependency,
or private index format. Legacy `_FTS` compatibility, metadata or resource
indexing, highlighting, the Phase 8 workflow, other adapters, and unrelated
refactors are excluded. The implementation gate is focused generated-fixture
and mixed-service coverage, the unchanged installed consumer, the complete
Linux Release test/install path, and clean committed exact-SCM Conan package
creation. GLS, DSL, BGL, MDict, Aard, ZIM, SLOB, and EPWING remain later
per-format leaves because their synonym, optional-headword, redirect,
multi-source, resource-bearing, or specialized-container semantics require
larger contracts.

P6-FT-3 is complete. XDXF now contributes one private full-text document per
validated article ordinal, preserves first-key canonical provenance, excludes
alias-only and non-article text, and participates in the accepted `.gdfts`
lifecycle and deterministic mixed-service dispatch without changing installed
or runtime-source surfaces.

The fresh independent post-P6-FT-3 audit selects P6-FT-4, the private Dictd
adapter, as the next bounded leaf. Direct pinned legacy evidence supersedes the
earlier Dictd exclusion: `dictdfiles.cc` at `3d93dd6` initializes and gates
Dictd full-text indexing and implements `_FTS` creation and search. The
migrated reader provides complete source-order `.index` traversal, optional
original-headword aliases, validated article byte ranges, bounded plain and
dictzip data loading, `text/plain` article assembly, stable identity, the
complete companion source set, and generated fixtures. The completed Phase 5
contract and P6-FT-1 through P6-FT-3 private capability/service dispatch are
its prerequisites.

Each distinct validated non-metadata `(article_offset, article_size)` range
contributes exactly one document. Records named `00databaseshort`,
`00-database-short`, `00databaseinfo`, or `00-database-info` remain metadata
and are excluded. The first remaining source record owning the range supplies
the canonical headword; its optional original headword and later records
sharing the range are aliases and do not duplicate it. The private document ID
is `dictd-index:<record-ordinal>:<article-offset>:<article-size>`. Ingestion
passes the existing `text/plain` article through the inert assembler and
indexes only its bounded plain text, excluding metadata, alias-only text,
resource data, and generated link or markup interpretation.

P6-FT-4 owns a distinct private `.gdfts` artifact in the configured index
directory. Its complete source revision is the `.index` file plus the actually
selected `.dict` or `.dict.dz` companion; changing either file or switching
the selected companion forces stale rebuild. No configured index directory
leaves Dictd typed unsupported. Cancellation, deadlines, resource limits,
corruption, and storage failures remain contained per dictionary. In a mixed
service, StarDict, SDict, XDXF, and Dictd results merge deterministically under
the existing global bound; adapted no-match dictionaries add no error,
requested non-adapted sources remain typed unsupported, and missing requested
IDs remain typed unavailable.

The leaf changes no installed `SearchFullText` API or DTO, public capability
flag, runtime-source interface, configuration, preference, widget,
presentation, dependency, or private index format. Legacy `_FTS`
compatibility, metadata or resource indexing, highlighting, the Phase 8
workflow, other adapters, and unrelated refactors are excluded. GLS, DSL, BGL,
MDict, Aard, ZIM, SLOB, and EPWING remain later because their larger
format-specific contracts make them less bounded than Dictd. LSA, ZIP sound
packs, and sound directories remain outside textual article ingestion. The
implementation gate is focused generated-fixture, lifecycle, and mixed-service
coverage, the unchanged installed consumer, the complete Linux Release
test/install path, and clean committed exact-SCM Conan package creation.

P6-FT-4 is complete. Dictd now contributes one private full-text document per
distinct validated non-metadata article range, with source-record canonical
provenance, alias deduplication, inert plain-text assembly, two-source
`.gdfts` lifecycle, and deterministic mixed-service dispatch. No subsequent
format adapter was selected by that implementation.

The fresh independent post-P6-FT-4 audit selects P6-FT-5, the private GLS
adapter, rather than following migration order. Pinned legacy `gls.cc` at
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8` explicitly creates and searches
`_FTS`, initializes `can_FTS`, and
honors the `GLS` disabled-type gate. The migrated reader retains complete
source-order headword records and materialized articles: pipe-separated aliases
share one article index and primary headword. It also provides bounded
UTF-8/UTF-16 and plain/gzip decoding, sanitized `text/html`, stable single-file
identity, and generated fixtures. DSL, BGL, MDict, Aard, ZIM, SLOB, and EPWING
remain later because expanded-headword, binary alias, redirect-chain,
multi-volume, resource-namespace, item/bin, or grouped-index ownership makes
their contracts larger. No additional migrated textual article format is
eligible; runtime sources lack a per-format ingestion contract, while LSA, ZIP
sounds, and sound directories are resource-only and remain excluded.

Each validated GLS article ordinal contributes exactly one document. The first
source-order record referencing it supplies the canonical primary headword;
later pipe-separated aliases do not duplicate it. The private document ID is
`gls-index:<first-record-ordinal>:<article-ordinal>`. Ingestion passes the
existing sanitized `text/html` through the inert article assembler and indexes
only its bounded plain text. Alias-only headword text, glossary metadata,
resource paths and bytes, image/link targets, raw markup, `.files` contents,
and future resource ZIP contents are excluded.

P6-FT-5 owns a distinct private `.gdfts` artifact in the configured index
directory. Its complete source revision is the discovered `.gls` or `.gls.dz`
file; source mutation or switching the discovered plain/compressed file forces
a stale rebuild, while external resource changes do not. No configured index
directory leaves GLS typed unsupported. In a mixed service, StarDict, Dictd,
SDict, XDXF, and GLS merge in existing backend order under the global bound;
adapted no-match dictionaries add no error, requested non-adapted local or
runtime sources remain typed unsupported, and missing requested IDs remain
typed unavailable. Per-dictionary cancellation, deadlines, corruption,
resource limits, and storage failures remain contained.

The leaf changes no installed `SearchFullText` API or DTO, runtime-source
interface, public capability flag, configuration, preference, dependency, or
private `.gdfts` format. Other adapters, legacy `_FTS` compatibility, metadata
or resource indexing, highlighting, the Phase 8 workflow, and unrelated
refactors are excluded. The implementation gate is focused generated-fixture,
lifecycle, contained-failure, and five-format mixed-service coverage; the
unchanged installed consumer; Linux Release build and
`ctest --preset conan-release`; library install and installed `test_package`
consumer; and clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile. No leaf after P6-FT-5 is selected.

P6-FT-5 is complete with per-article GLS ingestion, first-record canonical
ownership, inert assembled text, single-source `.gdfts` lifecycle,
mixed-service dispatch, and installed-consumer coverage. No successor is
selected.

The fresh independent post-P6-FT-5 audit selects P6-FT-6, the private Aard
adapter, and no later leaf. The complete remaining matrix is DSL, BGL, MDict,
Aard, ZIM, SLOB, and EPWING; each is explicitly full-text eligible at pinned
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` through its
format-specific disabled-type gate and `can_FTS` initialization in `dsl.cc`,
`bgl.cc`, `mdx.cc`, `aard.cc`, `zim.cc`, `slob.cc`, and `epwing.cc`. LSA, ZIP
sounds, and sound directories are resource-only, while runtime sources have no
private per-format ingestion contract. Aard is the smallest dependency-ready
candidate because its migrated source-order records already map directly to
deduplicated article ordinals, its redirect-only articles are materialized as
safe internal links, its bounded 32/64-bit and raw/zlib/bzip2 reader is
complete, and one `.aar` file plus generated fixtures settles its lifecycle.
DSL resource snapshots, BGL binary aliases, MDict redirect chains and MDDs,
ZIM split files and namespaces, SLOB item/bin ownership, and EPWING grouped
indexes remain larger unselected contracts; this is not a successor ranking.

Each unique validated Aard article ordinal contributes exactly one document.
The first source-order record referencing it supplies the canonical headword;
later aliases do not duplicate it, and a redirect-only article contributes its
already-safe target text. The private document ID is
`aard-index:<first-record-ordinal>:<article-ordinal>`. Only bounded plain text
from the existing inert assembly of migrated `text/html` is indexed. Metadata,
alias-only text, link targets, raw markup, icons, resources, and future
multi-volume data are excluded. The distinct `.gdfts` artifact uses the sole
discovered `.aar` file as its complete source revision, so mutation or
replacement forces stale rebuild; no configured index directory leaves Aard
typed unsupported.

Mixed-service behavior retains stable dictionary-ID ordering, the global
bound, dictionary filters, adapted no-match behavior, typed unsupported and
unavailable errors, and contained cancellation, deadline, corruption,
resource-limit, and storage failures. P6-FT-6 changes no installed
`SearchFullText` API or DTO, runtime interface, public capability flag,
configuration, preference, dependency, GUI/Phase 8 workflow, or private
`.gdfts` format. Other adapters, legacy `_FTS` compatibility,
metadata/resource indexing, highlighting, and unrelated refactors remain
excluded. P6-FT-6 is complete with unique-article ingestion, first-record
provenance, inert assembled text, sole-source lifecycle, deterministic
six-format dispatch, and installed-consumer coverage. No leaf after P6-FT-6
is selected.

The fresh independent post-P6-FT-6 audit selects P6-FT-7, the private DSL
adapter, and no later leaf. The complete remaining matrix is DSL, BGL, MDict,
ZIM, SLOB, and EPWING. Each remains explicitly full-text eligible at pinned
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`: its format source
contains both the full-text preference gate and `can_FTS` initialization, and
legacy DSL additionally implements `_FTS` creation and article traversal at
`dsl.cc:1337` and `dsl.cc:1368`, with search dispatch at `dsl.cc:2094`. Actual
migrated registration confirms that no other textual local format remains.
BGL control records, MDict redirect/MDD ownership, ZIM split
volumes and namespaces, SLOB reference/item content types, and EPWING grouped
subbook indexes remain larger unselected contracts; this is not a successor
ranking.

DSL is decision-complete because the migrated reader traverses all articles
and expanded records in source order, assigns exactly one ordinal to each
materialized sanitized article, and makes optional expansions, tilde
expansions, and alternate headword lines reference that ordinal. P6-FT-7
creates one document per article ordinal. The first expanded record from the
first headword line owns the canonical headword; all other records for that
article are aliases. Provenance is
`dsl-index:<first-record-ordinal>:<article-ordinal>`. Only bounded plain text
from inert assembly of the existing `text/html` article is searchable.
Directives, annotations, alias-only text, resource paths/bytes, link and image
targets, raw markup, abbreviation dictionaries, future resource ZIPs, and
unsupported nested cards are excluded.

The distinct private `.gdfts` artifact snapshots only the selected `.dsl` or
`.dsl.dz` source. Mutation, replacement, or switching the discovered
plain/compressed file forces a stale rebuild; `.ann` and `.files` changes do
not because they provide metadata and resources rather than article text.
Without an index directory DSL remains typed unsupported. StarDict, SDict,
XDXF, Dictd, GLS, Aard, and DSL retain stable dictionary-ID aggregation under
the global bound, no error for adapted no-match dictionaries, typed
unsupported for requested non-adapted local/runtime sources, typed unavailable
for missing IDs, and per-dictionary containment of cancellation, deadline,
corruption, resource-limit, and storage failures.

P6-FT-7 changes no installed API/DTO, runtime interface, public capability,
configuration, preference, dependency, GUI/Phase 8 workflow, or private
serialization. Legacy `_FTS` compatibility, metadata/resource indexing,
highlighting, other adapters, and unrelated refactors are excluded. No leaf
after P6-FT-7 is selected.

P6-FT-7 is complete with one private document per DSL article ordinal,
first-expanded-record ownership, inert assembled visible text, selected-source
`.gdfts` lifecycle, deterministic seven-format dispatch, and installed-consumer
coverage. No successor is selected.

The fresh independent post-P6-FT-7 audit selects only P6-FT-8, the private BGL
adapter. Actual migrated registration leaves exactly BGL, MDict, ZIM, SLOB,
and EPWING as textual candidates. Pinned legacy BGL declares full-text
construction at `bgl.cc:253`, gates it at `bgl.cc:255-259`, initializes
`can_FTS` and its source-backed `_FTS` lifecycle at `bgl.cc:308-314`, and
builds the index at `bgl.cc:475-499`, all at revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.

BGL is decision-complete because every supported source-order entry block
materializes one bounded decoded article ordinal and every retained primary or
alternate record identifies that ordinal. P6-FT-8 creates one document per
referenced ordinal; the first retained record owns the canonical headword and
later records are aliases. This also defines ownership when an empty nominal
primary is discarded. Provenance is
`bgl-index:<first-record-ordinal>:<article-ordinal>`. Only inert assembled
visible text from the existing sanitized `text/html` is indexed. Metadata,
alias-only text, resources, image/link targets, raw blocks or markup, and
code-page control data are excluded.

The sole discovered `.bgl` is the complete source revision. Mutation or
replacement, including an embedded-resource-only change, stales the private
artifact because entries and resources share one container; resource content
remains unindexed. MDict redirect/MDD ownership, ZIM namespace/split-volume
ownership, SLOB reference/item content types, and EPWING grouped subbook
ownership remain unresolved and unselected, without successor ranking.

P6-FT-8 preserves installed `SearchFullText` APIs and DTOs, runtime
interfaces, public capability flags, configuration/preferences, dependencies,
GUI/Phase 8 behavior, and private `.gdfts` serialization. Legacy `_FTS`
compatibility, metadata/resource indexing, highlighting, other adapters, and
unrelated refactors are excluded. The adapted service set grows to eight with
unchanged ordering, filtering, global bounds, typed unsupported/unavailable
errors, no-match behavior, and per-dictionary failure containment. No leaf
after P6-FT-8 is selected or ranked.

P6-FT-8 is complete. BGL now contributes one private full-text document per
referenced nonempty article ordinal with first-retained-record ownership,
exact source/article provenance, inert assembled text, a complete sole-`.bgl`
revision, and deterministic eight-format service dispatch. No successor is
selected.

The fresh independent post-P6-FT-8 audit covers the exact remaining migrated
textual registry: MDict, ZIM, SLOB, and EPWING. It selects only P6-FT-9, the
private SLOB adapter. At pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, `slob.cc:633-643` declares and
gates full-text support, `slob.cc:667-714` owns the single-source `_FTS`
lifecycle, and `slob.cc:1148-1263` traverses source-order references and
deduplicates physical item/bin articles. Migrated SLOB preserves the same
complete source-order reference evidence, bounded content-type and store
decoding, item/bin identity, safe article materialization, sole `.slob`
source, private dependencies, and generated fixtures.

P6-FT-9 retains only references whose normalized content type begins with
`text/html` or `text/plain`. It assigns zero-based `record_ordinal` values in
retained textual-reference order; excluded resources do not consume them. The
first encounter of each distinct unsigned zero-based `(item_index, bin_index)`
pair assigns a zero-based `article_ordinal`, creates one document, and owns the
canonical headword. Later references to the pair are aliases. Exact provenance
is `slob-index:<first-record-ordinal>:<article-ordinal>:<item-index>:<bin-index>`,
using unpadded unsigned base-10 components. This encodes both first-retained
logical ownership and the physical SLOB article identity.

Only inert assembled visible text from sanitized HTML or escaped,
`<pre>`-wrapped plain text is indexed. Metadata tags, alias-only text,
non-text bins, resources, icons, raw markup, and advanced conversion remain
inert or excluded. The sole discovered `.slob` is the complete source
revision; any mutation or replacement stales the private `.gdfts` artifact.
MDict redirect/MDD ownership, ZIM namespace/split-volume ownership, and EPWING
grouped-subbook ownership remain unresolved and unselected, without successor
ranking.

P6-FT-9 preserves installed `SearchFullText` APIs and DTOs, runtime
interfaces, public capability flags, configuration/preferences, dependencies,
GUI/Phase 8 behavior, and private `.gdfts` serialization. Implementation,
legacy `_FTS` compatibility, metadata/resource indexing, highlighting, other
adapters, and unrelated refactors are excluded. The future adapted service set
grows to nine with unchanged ordering, filtering, global bounds, typed
unsupported/unavailable errors, no-match behavior, and per-dictionary failure
containment. No leaf after P6-FT-9 is selected or ranked.

P6-FT-9 is complete. The private SLOB adapter now uses retained-textual
reference order and first-encounter item/bin ownership to build inert
documents with exact four-component provenance. Its sole-container lifecycle
and deterministic nine-format service integration preserve every public and
installed boundary. No successor is selected.

The fresh independent post-P6-FT-9 audit covers the exact remaining migrated
textual registry: MDict, ZIM, and EPWING. At pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, MDict gates full-text support at
`mdx.cc:264-274` and rebuilds against its dictionary filename set at
`mdx.cc:489-517`; ZIM gates support at `zim.cc:724-733`, initializes its
filename-set-backed `_FTS` lifecycle at `zim.cc:752-796`, and discovers and
deduplicates linked articles at `zim.cc:1094-1195`; EPWING gates support at
`epwing.cc:138-148` and rebuilds against its dictionary filename set at
`epwing.cc:372-397`.

Migrated MDict parses source-order keys, bounded record data, folded redirects,
styles, and companion MDD resources, but assigns one article slot per key
before redirect resolution; redirect-target ownership and the exact MDX/MDD
revision therefore require a prerequisite. Migrated EPWING traverses
`CATALOGS`, subbooks, and supported word-index records and renders bounded safe
text, but its current deduplication includes the headword and it does not expose
the complete mutable subbook-tree revision; grouped-index ownership and source
lifecycle require a prerequisite. Neither candidate is selected or ranked.

ZIM is decision-complete and is the sole selected P6-FT-10 leaf. Migrated
evidence at `modules/core/src/formats/zim/zim_reader.cc:212-245` preserves
directory-table order and terminal cluster/blob identity,
`zim_reader.cc:301-348` provides bounded redirect resolution,
namespace/MIME eligibility, and terminal-entry deduplication, and
`modules/core/src/formats/zim/zim_discovery.cc:16-44` owns sole-file and
consecutive split-volume discovery. Generated fixtures cover raw, zlib, and
bzip2 clusters, 32/64-bit offsets, redirects, resources, and safe HTML/plain
materialization without a new dependency.

P6-FT-10 retains only namespace `A` sources, plus namespace `C` sources for
ZIM 6.1 and later, whose resolved terminal MIME begins with `text/html` or
`text/plain`. Zero-based `record_ordinal` values follow retained eligible
source order; excluded entries consume none. The zero-based terminal
directory-table index is the deduplication identity. Its first retained source
assigns the zero-based first-encounter `article_ordinal`, owns the canonical
nonempty title-or-URL headword and `first_record_ordinal`, and creates one
document; later sources resolving to it are aliases.

Exact provenance is
`zim-index:<first-record-ordinal>:<article-ordinal>:<target-entry-index>:<cluster-index>:<blob-index>`,
with every component in canonical unsigned base-10 without signs or padding.
The five components encode the first logical owner, deduplicated ordinal,
terminal directory identity, and physical cluster/blob identity and are
collision-safe within the dictionary revision. Only inert assembled visible
text from validated HTML or escaped, `<pre>`-wrapped plain text is indexed.
Metadata, alias-only text, resource names/bytes, non-text blobs, icons, raw
markup, unsupported compression/conversion, and new link rewriting are
excluded.

The complete ordered source revision is the sole `.zim`, or every consecutive
split part from `.zimaa` through the last part before the first missing suffix.
Any included-part mutation, replacement, addition, removal, or order change,
including a resource-only change, stales the private `.gdfts` artifact.
Without an index directory ZIM remains typed unsupported. The future
ten-format service preserves stable ordering, filtering, global bounds,
adapted no-match behavior, typed unsupported/unavailable errors, and contained
per-dictionary failures.

P6-FT-10 preserves installed `SearchFullText` APIs and DTOs, runtime
interfaces, public capability flags, configuration/preferences, dependencies,
GUI/Phase 8 behavior, and private `.gdfts` serialization. Legacy `_FTS`
compatibility, metadata/resource indexing, highlighting, other
adapters, and unrelated refactors are excluded.
No leaf after P6-FT-10 is selected or ranked.

P6-FT-10 is complete with terminal-entry deduplication, first-source ownership,
exact five-component provenance, inert assembled text, complete ordered ZIM
source revisions, deterministic ten-format dispatch, and installed-consumer
coverage. No successor is selected.

The fresh independent post-P6-FT-10 audit covers the exact remaining migrated
textual registry: MDict and EPWING. At pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, MDict gates full-text support at
`mdx.cc:264-274`, rebuilds against its dictionary filename set at
`mdx.cc:489-517`, and extracts text at `mdx.cc:519-533`; EPWING gates support
at `epwing.cc:138-148`, rebuilds against its dictionary filename set at
`epwing.cc:372-397`, and extracts physical article page/offset at
`epwing.cc:400-420`.

Neither adapter is decision-complete. Migrated MDict parses source-order MDX
keys, bounded record ranges, folded redirects, styles, and consecutive MDD
companions, but assigns per-key article slots before redirect resolution and
does not expose terminal physical ownership or an exact MDX/MDD revision.
Migrated EPWING traverses `CATALOGS`, subbooks, and supported word indexes and
renders bounded safe text, but deduplicates by a tuple that includes the
headword and does not expose the complete mutable `CATALOGS`/subbook tree.
EPWING remains unselected and unranked.

P6-FT-11 is complete as the bounded private MDict ownership prerequisite; it
is not the MDict adapter. Its immutable reader view assigns zero-based
`record_ordinal` values to accepted nonempty MDX keys in
source order before bounded folded `@@@LINK=` resolution. Missing-target and
cyclic redirects consume an ordinal but report explicit nonterminal outcomes;
MDD entries consume none. Successful resolution exposes the terminal
zero-based MDX key ordinal and exact decoded record offset and size. That
physical identity alone controls deduplication. The first resolving source
owns the canonical headword and `first_record_ordinal`, later resolving keys
are aliases, and zero-based `article_ordinal` follows first-terminal encounter
order.

The following adapter audit is locked to exact provenance
`mdict-index:<first-record-ordinal>:<article-ordinal>:<terminal-key-ordinal>:<record-offset>:<record-size>`.
All components are canonical unsigned base-10 without signs or padding, making
the logical and physical identity collision-safe within the revision. The
view supplies only the resolved terminal's existing bounded decoded/styled
HTML for later inert assembly. It excludes missing targets, cycles, unresolved
redirect text, empty inert output, metadata, MDD resource names/bytes, and
aliases as separate searchable content.

The complete ordered revision is MDX followed by the discovered base MDD and
every consecutive numbered MDD companion. Mutation or replacement of any
member and companion addition or removal changes the revision, including a
resource-only MDD change; MDD files never own text documents. Generated
fixtures prove direct and chained redirects, folded collisions, duplicate
aliases, distinct physical records with equal content, missing targets,
cycles, empty materialization, multi-digit identity components, consecutive
MDD topology, and checkpoints/cancellation.

Until a separate adapter audit, both remaining formats stay typed unsupported
and the existing ten-format mixed service remains unchanged. P6-FT-11 does not
change installed APIs/DTOs, runtime interfaces, capability flags,
configuration/preferences, dependencies, GUI/Phase 8 behavior, or private
`.gdfts` serialization. This completed prerequisite excludes adapter
implementation, legacy `_FTS`,
metadata/resource indexing, highlighting, other formats, dependency work, and
unrelated refactors. No adapter or successor after this prerequisite is
selected or ranked.

The fresh post-P6-FT-11 readiness audit covers exactly the two remaining
migrated textual candidates, MDict and EPWING, against pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Legacy MDict gates full-text at
`mdx.cc:264-274`, rebuilds from its complete filename set at `mdx.cc:489-517`,
and extracts text at `mdx.cc:519-533`. Legacy EPWING gates full-text at
`epwing.cc:138-148`, rebuilds from its dictionary filename set at
`epwing.cc:372-397`, and extracts physical page/offset at `epwing.cc:400-420`.

P6-FT-11 closes MDict's prior gap completely: its immutable view provides
source-order ordinals, terminal/missing/cycle outcomes, folded redirect
resolution, exact terminal key/range identity, first-source and article
ownership, aliases, checkpoints, bounded decoded/styled materialization, and
the ordered MDX/base-MDD/consecutive-MDD snapshot. P6-FT-12 is therefore
selected as the sole decision-complete private adapter leaf. EPWING remains
unselected and unranked because its current physical deduplication includes
the headword and it exposes no complete mutable `CATALOGS`/subbook-tree
revision.

P6-FT-12 is complete and consumes the prerequisite without revising it: one
document per distinct terminal
`(terminal-key-ordinal, record-offset, record-size)`, owned
by the first resolving source, with aliases retained and zero-based
first-encounter `article_ordinal`. Provenance is exactly
`mdict-index:<first-record-ordinal>:<article-ordinal>:<terminal-key-ordinal>:<record-offset>:<record-size>`
using canonical unsigned base-10 components. Only inert text assembled from
the terminal's bounded decoded/styled HTML is indexed. Missing/cyclic
redirects, unresolved payloads, empty output, metadata, MDD names/bytes,
aliases as independent documents, and active markup are excluded.

The complete revision remains the discovered MDX followed by base MDD and all
consecutive numbered MDD companions. Mutation, replacement, addition, removal,
or order change of any member, including a resource-only MDD, stales the
artifact; MDD files never own documents. The eleven-format mixed service must
retain ordering, filtering, global bounds, adapted no-match behavior, typed
unsupported for EPWING and other non-adapted sources, typed unavailable for
missing IDs, and contained failures. Public APIs/DTOs, runtime interfaces,
capability flags, configuration/preferences, dependencies, GUI/Phase 8, and
private `.gdfts` serialization remain unchanged. Legacy `_FTS`,
metadata/resource indexing, highlighting, other formats, dependencies, and
unrelated refactors are excluded. The accepted implementation provides
generated adapter/lifecycle coverage, deterministic eleven-format service
coverage, and exact-provenance installed C++ consumption; the installed C
consumer remains unchanged. EPWING remains unselected and unranked, and no
successor after P6-FT-12 is selected or ranked.

The final remaining textual-format audit selected only P6-FT-13, the private
EPWING reader ownership/revision prerequisite. The audit is pinned to legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`: `epwing.cc:138-148`
in `/home/log/Workspace/GoldenDict` gates full-text support,
`/home/log/Workspace/GoldenDict/epwing.cc:372-397` owns its rebuild,
`/home/log/Workspace/GoldenDict/epwing.cc:400-423` reads physical page/offset
articles, and `/home/log/Workspace/GoldenDict/epwing.cc:954-1023` assembles
`CATALOGS` plus selected-subbook filenames. The
migrated reader traverses the catalog and supported word indexes at
`modules/core/src/formats/epwing/epwing_reader.cc:468-607`, but its
`epwing_reader.cc:645-655` identity includes the headword and
`epwing_reader.h:34-81` supplies no complete revision. Both suspected blockers
remain; the adapter is not decision-complete.

P6-FT-13 is complete with a private immutable ingestion view ordered by catalog
subbook, supported index table, index page, and entry. Records carry
`record_ordinal`, headword, and headword-independent
`(text_file_ordinal, page, offset)` identity. The first record for an identity
owns the canonical headword and `article_ordinal`; later records are aliases.
Articles carry that ownership, bounded rendered HTML, and checkpoints. Exact
future provenance is
`epwing-index:<first-record-ordinal>:<article-ordinal>:<text-file-ordinal>:<page>:<offset>`
with canonical unsigned base-10 components. Internal references remain inert
links and neither redirect ownership nor create documents.

The view's complete ordered `dictionary::SourceSnapshot` consists of
`CATALOGS`, optional decoding-affecting `LANGUAGE`, then every regular
non-symlink file under every catalog-selected subbook/content tree, with
subbooks in catalog order and files in relative-path byte order. Mutation,
replacement, addition, removal, path/order change, topology change, and
resource-only changes stale the future artifact; unrelated siblings and
generated/cache files do not. Only inert plain text derived from bounded
rendering may later be indexed. Empty output, metadata/copyright, resource
names/bytes, aliases, unindexed reference targets, active markup, scripts,
media, and gaiji payloads are excluded. Limits, errors, checkpoints,
cancellation, and deadlines cannot publish a partial view.

The accepted private prerequisite preserves all installed APIs/DTOs, runtime
interfaces, capabilities, configuration/Preferences, dependencies, GUI/Phase
8 behavior, `.gdfts` serialization, and eleven completed format adapters. It
excludes the EPWING adapter, legacy `_FTS`, metadata/resource indexing,
highlighting, other adapters, builds for this documentation leaf, and unrelated
refactors. EPWING remains typed unsupported pending a later audit. No adapter,
successor, or leaf after P6-FT-13 is selected or ranked.

The bounded post-prerequisite audit is complete. It rechecked pinned legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` at `epwing.cc:138-148`,
`epwing.cc:372-423`, and `epwing.cc:954-1023` against migrated revision
`1935cc2c7f11efa64f597621bc375ca71402dd56`. P6-FT-13 now exposes both the
headword-independent physical owner and complete ordered immutable revision;
no blocker remains. The audit therefore selects exactly P6-FT-14, the private
EPWING full-text adapter, and selects or ranks no successor.

P6-FT-14 is complete and creates one document per retained physical ingestion
article whose bounded rendered HTML assembles to non-empty inert plain text.
First-record
ownership supplies the canonical headword, aliases, `first_record_ordinal`,
`article_ordinal`, `text_file_ordinal`, page, and offset. Aliases are not
documents, and equal content at different physical identities remains
distinct. Provenance is exactly
`epwing-index:<first-record-ordinal>:<article-ordinal>:<text-file-ordinal>:<page>:<offset>`
with canonical unsigned decimal components. Internal references remain inert
and cannot redirect ownership or create documents.

The generated artifact uses the ingestion view's complete `SourceSnapshot`:
`CATALOGS`, optional decoding-affecting `LANGUAGE`, and every regular
non-symlink file in each selected subbook tree, ordered by catalog then
relative-path bytes. All selected-member, order, path, and topology changes,
including resource-only changes, stale it; unselected siblings and
generated/cache files do not. Creation, reuse, stale/corrupt rebuild,
reader/index/storage failures, limits, checkpoints, cancellation, and
deadlines are contained and publish no partial state.

Application composition dispatches EPWING as the twelfth adapted format while
preserving dictionary ordering/filtering, the global bound, adapted no-match,
typed unavailable IDs, and contained failures. Generated fixtures and the
installed C++ consumer pin exact provenance; the installed C consumer remains
unchanged. The leaf preserves installed APIs/DTOs, capabilities,
configuration/Preferences, dependencies, GUI/Phase 8, `.gdfts`
serialization, and eleven completed adapters. It excludes legacy `_FTS`,
metadata/resource indexing, highlighting, other adapters, unsupported EPWING
parity, and unrelated refactors.

No successor after P6-FT-14 is selected or ranked.

The fresh Phase 6 milestone and Phase 8 workflow audit is pinned to migrated
revision `f5547edfc3d5464d2182d5196df669b63765b568` and legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It confirms that all twelve
private textual-format adapters now dispatch through the installed bounded
`SearchFullText` contract. The remaining user workflow is not one leaf: the
legacy dialog separately composes request lifetime, query options, group and
muting selection, result presentation and activation, highlighting, persisted
format/size policy, index readiness, and background indexing.

P8-FT-1, the private cancellable asynchronous full-text request controller, is
complete. The existing synchronous installed call and cancellation token are
sufficient, and the migrated private suggestion worker supplies the applicable
Widgets lifetime pattern. The controller
accepts an immutable query and generation, replaces older work by
cancellation, runs the service call off the GUI thread, converts a boundary
exception to a terminal internal error, returns the unchanged response to the
GUI thread, rejects stale completions, and cancels and joins before dialog
destruction, facade replacement or shutdown can invalidate the borrowed
service. Core retains all query, dictionary, limit, deadline, result and typed
error semantics; the controller exposes no invented per-dictionary progress.

P8-FT-1 excludes every visible full-text control and result behavior,
dictionary/group selection, activation, highlighting, persistent Preferences
application, index availability and background lifecycle. It also preserves
installed APIs/DTOs, configuration compatibility, dependencies, all twelve
adapters and private `.gdfts` serialization. Legacy `_FTS`, metadata/resource
indexing, platform integration and unrelated Phase 7/8/9/10 work remain out of
scope. Its focused private QTest accepts the intentional registered-test
increase from 103 to 104. No successor after P8-FT-1 is selected or ranked.

The fresh documentation-only post-P8-FT-1 audit is pinned to migrated revision
`66c53d263ebfb735f898b950786bc43f9081821b` and the unchanged legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It compares exactly the modeless
dialog shell/query controls, dictionary/group/muting selection, result
model/presentation/activation, article-highlighting handoff, persistent
Preferences controls, and index availability/status/background lifecycle.
It selects P8-FT-2, the smallest independent prerequisite: lossless four-mode
query-mode persistence. No visible workflow leaf or successor is selected or
ranked.

P8-FT-2 is complete with only the additive installed/public
`FullTextSearchMode::kPlainText = 3` enumerator. Existing migrated meanings
remain `0` whole words, `1` wildcard and `2` regular expression; migrated `3`
means plain text. Legacy XML's distinct `0` whole words, `1` plain text, `2`
wildcard and `3` regular expression are translated explicitly. The enum
underlying type, existing DTO field and layout, and scalar configuration wire
shape remain unchanged. Unknown current or legacy values remain atomic load
errors.

The implementation is limited to the enum, current configuration validation,
legacy translation, and focused fixtures/tests. It changes no
installed full-text query/result DTO, controller behavior, other persisted
full-text field, twelve private adapters, `.gdfts`, or dependency. It has no
request, asynchronous lifecycle, cancellation or visible behavior and excludes
action/menu, dialog/query widgets and query construction, selection,
presentation, activation, highlighting, Preferences UI/policy application,
index status/background lifecycle, legacy `_FTS` compatibility,
metadata/resource indexing, platform work, and unrelated Phase 8/9/10
behavior. No successor after P8-FT-2 is selected or ranked.

The fresh documentation-only post-P8-FT-2 readiness audit is pinned to
migrated revision `fcc1eec921a5e564b9b49cefdbf00f4846d71e21` and legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks exactly the
modeless dialog/query controls, dictionary/group/muting selection, results and
activation, highlighting, Preferences integration, and index
visibility/status/background lifecycle. P8-FT-3 is now complete as the
installed query DTO and aggregator prerequisite; no visible workflow leaf or
later successor is selected or ranked.

P8-FT-3 preserves `FullTextQuery::result_limit` as the unchanged global hard
safety/output cap and adds the distinct bounded installed
`maximum_articles_per_dictionary` field. Its default is `100` and its valid
range is `1..100000`, matching the persisted legacy preference. The two limits
are validated independently and are never multiplied or derived from one
another. Each selected dictionary may contribute at most
`maximum_articles_per_dictionary` accepted articles, while aggregation also
stops at the existing global `result_limit`; the backend request bound is the
minimum of the per-dictionary limit and remaining global capacity.

The legacy dialog preference controls only the new per-dictionary field. The
global limit remains an internal or advanced safety contract, not a relabeled
legacy control. Existing callers retain behavior because the new default is no
smaller than any valid global cap. Dictionary ordering/filtering,
cancellation, deadlines, typed errors and partial responses remain unchanged.
The additive public DTO member is authorized as source-compatible but requires
consumer rebuild, install checks and exact-SCM package verification.

The completed P8-FT-3 changes no controller, persistence mapping, adapter,
private `.gdfts` format or dependency and adds no UI. It excludes
dictionary/group/muting policy, results/activation, highlighting, Preferences widgets or policy,
index visibility/background lifecycle, legacy `_FTS`, metadata/resource
indexing, platform work and unrelated migration behavior. No successor after
P8-FT-3 is selected or ranked.

The fresh documentation-only post-P8-FT-3 readiness audit is pinned to
migrated revision `bbad53ecebee93419caa3acf9c560bea128c3a83` and the
unchanged read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks exactly the
modeless dialog/query controls, dictionary/group/muting selection, results and
activation, highlighting, Preferences integration, and index
visibility/status/background lifecycle. P8-FT-4 completes the installed DTO,
validation, and aggregator contract prerequisite selected as the sole next
leaf. Current evidence is `dictionary_service.h:28,151-161` and
`dictionary_service.cc:633-655,1136-1153`; pinned legacy evidence is
`config.hh:156-176` and
`fulltextsearch.cc:249-256,387-394,438-446`.

P8-FT-4 changes `FullTextQuery::maximum_articles_per_dictionary` to
`std::optional<std::size_t>` with an engaged default of `100`, preserving
P8-FT-3 callers. Engaged values validate in `1..100000`; disengaged means no
per-dictionary truncation, not a sentinel, and remains bounded by the global
cap. `FullTextQuery::result_limit` keeps its existing caller default of `20`
and independent global hard-cap meaning while its accepted range widens from
`1..100` to `1..1000000`.

With an engaged per-dictionary limit, Core requests at most
`min(maximum_articles_per_dictionary, remaining global capacity)` from each
selected dictionary. With a disengaged limit, Core requests at most the
remaining global capacity. Aggregation always stops at `result_limit`; neither
limit is derived from or multiplied by dictionary count or the other limit.
The later dialog composer maps checked to the engaged persisted value,
unchecked to `std::nullopt`, and uses a fixed application global cap of
`100000`, independent of dictionary count and the per-dictionary value.

The optional installed field and widened validation range are an authorized
public ABI expansion verified through consumer rebuild, install checks, and
exact-SCM package verification. P8-FT-4 adds no UI and changes no controller,
persistence mapping, adapter, private `.gdfts` format, or dependency. It
excludes dialog composition, dictionary/group/muting policy,
results/activation, highlighting, Preferences widgets and index policy, index
visibility/background lifecycle, legacy `_FTS`, metadata/resource indexing,
platform work, and unrelated migration behavior. The completed P8-FT-4 selects
or ranks no successor.

The fresh documentation-only post-P8-FT-4 readiness audit is pinned to clean
pushed revision `e594de3fc6c0b6e912387d78f873eb9dd3e5a749` and the unchanged
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It reviews and separates private query composition, modeless dialog/action
integration, dictionary/group/muting selection, results and activation,
highlighting, Preferences/index policy, and index
visibility/status/background lifecycle. P8-FT-5 selects only the private
Widgets query composer as the smallest dependency-ready prerequisite; no
visible application entry point or inert result workflow is added.

P8-FT-5 is complete. Its private, non-integrated Widgets composer constructs
one `FullTextQuery` without changing an installed interface.
It copies UTF-8 query text; explicitly maps persisted whole words, plain text,
wildcard, and regular-expression modes to the corresponding query enum; and
maps match case, ignore diacritics, and ignore word order directly. Checked
word distance produces an engaged `maximum_word_distance` in `0..1000`, and
unchecked produces `std::nullopt`. Checked maximum articles per dictionary
produces the engaged persisted value in `1..100000`, and unchecked produces
`std::nullopt`. Every composed request uses the independent fixed global cap
`result_limit = 100000` and the existing default timeout. Until a separate selection leaf, it supplies no
dictionary IDs and leaves `dictionary_filter_active` false.

Whole-word and plain-text modes enable word-order and optional-distance
controls. Wildcard and regular-expression modes compose
`ignore_word_order = false` and `maximum_word_distance = std::nullopt` while
retaining their private values for a later mode change. Repeated composition is
deterministic, does not mutate persisted configuration, and does not submit
backend work. Widgets owns only this control-to-DTO mapping;
Core continues to own validation, search semantics, ordering, filtering,
errors, cancellation, deadlines, and aggregation, and the completed private
controller remains the later submission boundary.

Current evidence is `dictionary_service.h:28-32,47-52,149-160`,
`application.h:193-198,268-277`, and
`apps/goldendict/src/full_text_request_controller.h/.cpp` with its focused
test. Pinned legacy evidence is `fulltextsearch.ui` and
`fulltextsearch.cc:232-315,387-446`. The modeless dialog and Search-menu action,
group/dictionary/muting projection, submission/completion presentation,
results/activation, highlighting, Preferences enablement/index policy, index
visibility/status/background lifecycle, public DTO or persistence changes,
adapters, `.gdfts`, legacy `_FTS`, dependencies, and unrelated behavior remain
excluded. No successor after P8-FT-5 is selected or ranked.

The focused offscreen Widgets QTest verifies all four modes, direct booleans,
checked and unchecked optionals at their exact bounds, retained values across
mode transitions, the fixed independent global cap, UTF-8 preservation,
unchanged timeout and dictionary-filter defaults, repeated composition, and
persisted-preference immutability. The composer has no service or controller
dependency and cannot submit work. Its registration intentionally increases
the suite baseline from 104 to 105 tests. No successor is selected or ranked.

The completed P8-FT-6 implementation started from clean pushed migrated
revision `ac8a01c6b6212d6313364e2107a3bcb8b13df535`
and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes without preselection
the remaining modeless dialog/action integration, dictionary/group/muting
projection, request states, results/activation, highlighting,
Preferences/index policy, and index visibility/background lifecycle. It
selects exactly one smallest dependency-ready prerequisite, P8-FT-6, and
selects or ranks no successor.

P8-FT-6 is complete with only the additive installed C++ catalog capability
`DictionaryIdentity::supports_full_text_search`, default `false`. Core maps it
to `true` exactly for a backend implementing the private `FullTextBackend`
contract and keeps the catalog identity and every full-text result identity
consistent. The twelve completed textual adapters map true; LSA, ZIP sounds,
sound directories, online sources, external programs, and unsupported future
backends map false. The value is recomputed with service composition, is not
persisted, and does not represent private-index ready/building/error state.

This prerequisite is forced by pinned legacy `fulltextsearch.cc:613-659`,
which filters `canFTS()` before group muting. Migrated
`dictionary_service.h:85-96` exposes no equivalent, while
`dictionary_service.cc:1113-1119` discovers support only through a private
backend cast and `main_window.cpp:5535-5770` can otherwise project group and
ephemeral dictionary-bar participation. Widgets must not inspect private
backends or probe by submitting a search.

Focused Core tests cover supported/unsupported mappings, catalog/result
consistency, stable order, and service replacement. The installed C++ consumer
reads both values; the installed C consumer remains unchanged. The registered
suite remains 105 tests. Release build and full tests, install and standalone
installed-consumer checks, and clean committed exact-SCM Conan package
verification complete the implementation gate. The public DTO expansion is
the only authorized ABI change.

Dialog/action wiring, group and muting application, submission/completion UI,
results and activation, highlighting, Preferences and index policy, index
visibility/status/background lifecycle, persistence, adapters, `.gdfts`,
legacy `_FTS`, dependencies, and unrelated behavior are excluded. A later
leaf may consume the capability but is neither selected nor ranked here. No
successor after P8-FT-6 is selected or ranked.

The fresh documentation-only post-P8-FT-6 readiness audit is pinned to clean
pushed migrated revision `9801ebdb99e09600efc0fad32405bee02dd4971e` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes without preselection
the remaining dictionary/group selection and muting projection, modeless
dialog/MainWindow action integration, request submission/completion states,
results presentation and activation, highlighting, Preferences/index policy,
and index visibility/background lifecycle. Exactly one smallest dependency-
ready leaf is selected: P8-FT-7, the private Widgets dictionary-participation
projection. No successor after P8-FT-7 is selected or ranked.

P8-FT-7 is complete. It consumes the completed P8-FT-5 composer, the P8-FT-6
public capability, and the accepted MainWindow group and `dictionaryBar`
participation state. For All Dictionaries it preserves catalog order; for a named group it preserves
configured member order, rejects unresolved IDs, and applies configured
`muted_dictionary_ids`. Both paths retain only identities with
`supports_full_text_search == true`. A visible dictionary bar further retains
only checked IDs in that same order; when the bar is hidden, its ephemeral
checks do not filter the baseline. The projection always sets
`dictionary_filter_active = true` and supplies the resulting ordered IDs,
including an empty vector. It recomputes current state and changes no
configuration or preference.

MainWindow owns selection, group membership/muting, and ephemeral bar state;
Core owns capability and service-side filtering; the new seam owns only their
private Widgets-to-query mapping. Focused tests cover both scopes, order,
unsupported and unresolved dictionaries, configured muting, visible and
hidden bar behavior, empty active filtering, current-state recomputation,
unchanged P8-FT-5 fields, and absence of controller or persistence effects. A
focused offscreen MainWindow smoke exercises the real selector and bar without
opening a dialog or submitting work.

The implementation gate is focused tests, Linux Release configure/build
and full `ctest --preset conan-release` with only an intentional test-count
delta, then clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers. Installed interfaces are
unchanged, and the dedicated smoke raises the registered suite from 105 to
106 tests, so install and standalone consumer checks are unnecessary.
Modeless dialog/action integration,
submission/completion UI, results/activation, highlighting, Preferences/index
policy, index visibility/status/background lifecycle, public APIs,
persistence, adapters, `.gdfts`, legacy `_FTS`, dependencies, and unrelated
behavior are excluded. Evidence is migrated
`full_text_query_composer.h/.cpp`, `dictionary_service.h:85-96,149-160`,
`main_window.cpp:5535-5770`, the P8-FT-5/P8-FT-6 focused tests, and pinned
legacy `fulltextsearch.cc:613-659`. No successor after P8-FT-7 is selected or
ranked.

The fresh documentation-only post-P8-FT-7 readiness audit is pinned to clean
pushed migrated revision `1eeea0a73ff29b832f68012bd2b99b7f6208cf87` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes without preselection
the remaining modeless dialog/MainWindow Search-action integration, request
submission/cancellation/replacement/completion states, results presentation
and activation, highlighting, Preferences/index policy, and index
visibility/status/background lifecycle. Exactly one smallest dependency-ready
leaf is selected: P8-FT-8, the private modeless full-text dialog shell and
MainWindow/Search-action integration. No successor after P8-FT-8 is selected
or ranked.

P8-FT-8 is complete. It consumes the completed P8-FT-1 controller lifetime,
P8-FT-5 composer,
P8-FT-7 participation projection, existing MainWindow group/dictionary-bar
state, and attached desktop facade. It adds `fullTextSearchAction` immediately
after `searchInPageAction` in `menuSearch`, with the pinned text, shortcut,
shortcut context, and menu role. The action is enabled only while a usable
facade is attached; applying persisted full-text enablement or index readiness
remains excluded. Triggering creates at most one non-modal MainWindow-owned
dialog or shows, raises, and activates the existing instance. The shell uses
the pinned title and absent window-context-help button, hosts the existing
composer, copies and selects the current main lookup text on creation, and
recomputes the current participation projection on every show.

Closing destroys the instance and clears MainWindow ownership. Close,
MainWindow destruction, and facade replacement detach and stop the controller
before its borrowed service can become invalid; reopening creates a fresh
shell. The leaf submits no query, processes no completion, presents or
activates no result, hands off no highlight, and persists neither geometry nor
edited controls.

Focused private Widgets tests and an offscreen MainWindow smoke cover exact
action placement and properties, facade-dependent enablement, singleton
show/raise lifecycle, initial query transfer and selection, current-state
reprojection, close/reopen freshness, and safe close/facade-replacement/window
teardown with zero service calls. The implementation gate is those tests,
Linux Release configure/build, full `ctest --preset conan-release` with only
the intentional test-count delta, and clean committed exact-SCM `conan create`
with the Release Qt WebEngine host profile and packaged consumers. Installed
interfaces are unchanged, so install and standalone consumer checks are not
required.

Affected components are the private Widgets dialog shell, MainWindow
menu/composition/lifetime wiring, focused tests, smoke, and test registration.
Request submission/cancellation/replacement/completion UI, results/activation,
highlighting, Preferences/index policy, index visibility/status/background
lifecycle, public APIs, persistence, adapters, `.gdfts`, legacy `_FTS`,
dependencies, and unrelated work are excluded. Evidence is migrated
`main_window.cpp:425-433,6064-6086`, `full_text_query_composer.h/.cpp`,
`full_text_dictionary_projection.h/.cpp`, and
`full_text_request_controller.cpp:143-197`, plus pinned legacy
`mainwindow.ui:614-627`, `mainwindow.cc:4754-4791`, and
`fulltextsearch.cc:195-340`. No successor after P8-FT-8 is selected or ranked.

P8-FT-9 is complete as the private dialog request submission, cancellation,
replacement and terminal-state integration leaf selected by the post-P8-FT-8
audit. Its implementation is based on clean migrated revision
`750177f4f8a2d9fd709a2c27efe3e254505308d6` and the unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. No successor after
P8-FT-9 is selected or ranked.

P8-FT-9 uses only completed contracts. Search composes current P8-FT-5
controls, reapplies the latest P8-FT-7 dictionary projection, clears any prior
private terminal response, advances a dialog-owned generation and submits via
P8-FT-1. A later submission replaces running or pending work, and generation
matching prevents stale completion from changing the dialog. The private
submission entry point supports replacement while the visible Search control
is disabled. Explicit Cancel
is idempotent, invalidates the active generation and restores idle state.
Active work shows indeterminate progress and exposes cancellation; a current
terminal completion stores the unchanged response privately, hides progress
and restores Search. P8-FT-8 close, facade-replacement and teardown ordering is
preserved.

Focused private Widgets tests and an offscreen dialog/MainWindow smoke cover
exact composed/projected queries, monotonically increasing generations,
running state, replacement, cancellation, stale/cancelled completion
suppression, unchanged response retention, terminal restoration and safe
teardown. P8-FT-9 extends existing registrations, so the Release suite remains
108 tests. The implementation gate is focused tests, Linux Release build, full
Release CTest with only the intentional test delta, and clean committed
exact-SCM Conan creation with packaged consumers. No install or standalone
consumer gate is needed because installed interfaces do not change.

P8-FT-9 does not define result projection, merging, ordering, metadata,
counts, selection, article activation, highlighting, beeps, or user-facing
validation/error/partial-response policy. It excludes Preferences and index
policy, index readiness/visibility/status/background lifecycle, persistence,
public APIs, adapters, `.gdfts`, legacy `_FTS`, dependencies and unrelated
behavior. Results and activation remain downstream of this response path;
highlighting additionally needs activation and a reviewed WebEngine handoff.
Preferences/index work still requires product policy and a Core lifecycle
contract. Evidence is migrated `full_text_request_controller.h/.cpp`,
`full_text_query_composer.h/.cpp`,
`full_text_dictionary_projection.h/.cpp` and
`full_text_search_dialog.h/.cpp`, plus pinned legacy
`fulltextsearch.cc:338-570` and `fulltextsearch.ui:99-238`. No successor after
P8-FT-9 is selected or ranked.

The fresh documentation-only post-P8-FT-9 readiness audit was pinned to clean
migrated revision `ee632c2e470b1f24e73d311dd0eec8727d5b5c15` and unchanged
clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It audits every remaining
full-text result projection/presentation, result UI/selection/empty/error,
activation/navigation/lookup, highlighting/WebEngine, Preferences/index
policy, and index readiness/visibility/status/background-lifecycle surface
without advance ranking. Exactly one smallest independent prerequisite was
selected: P8-FT-10, the private non-integrated per-document response projection
model. No successor after P8-FT-10 is selected or ranked.

P8-FT-10 projects the completed `FullTextResponse::results` into one immutable
Qt model row per result, preserving Core order, duplicate headwords, dictionary
identity, document ID, match metadata, excerpt, and byte matches exactly.
`Qt::DisplayRole` provides the UTF-8 headword; private typed access provides the
unchanged result. Atomic reset replaces the snapshot, and an empty response
has zero rows. Widgets owns only projection and snapshot lifetime; Core retains
ordering, bounds, errors, and partial-response ownership. The model does not
merge, sort, filter, truncate, localize, interpret errors, or attach to the
dialog.

The leaf depends only on the installed result DTO and P8-FT-9 terminal-response
retention. Acceptance covers exact order/count, distinct equal headwords,
field-for-field metadata, UTF-8 display, empty/reset/replacement behavior,
source-response independence, and absence of controller/service/persistence
effects. Its focused gate is a private offscreen Widgets model QTest; its full
implementation gate is Linux Release configure/build, full
`ctest --preset conan-release` with only the intentional registration delta,
and clean committed exact-SCM `conan create` with the Release Qt WebEngine host
profile and packaged consumers. Installed surfaces do not change, so install
and standalone installed-consumer checks are unnecessary.

Result-view layout/counts, selection, empty/error/partial presentation,
article activation/navigation and lookup handoff, highlighting/WebEngine,
Preferences enablement/index policy, index readiness/visibility/status and
background lifecycle, public APIs, persistence, adapters, `.gdfts`, legacy
`_FTS`, dependencies, and unrelated behavior are excluded. Evidence is
migrated `dictionary_service.h:164-196`,
`application/dictionary_service.cc:1078-1157`, and
`full_text_search_dialog.h/.cpp`, plus pinned legacy
`fulltextsearch.hh:41-65,135-156`,
`fulltextsearch.cc:129-186,518-610,685-750`, and
`fulltextsearch.ui:99-238`. No successor after P8-FT-10 is selected or ranked.

P8-FT-10 is complete. Its private Qt model deterministically snapshots ordered
results with duplicate headwords and complete typed metadata intact, while
leaving response errors and partial state unprojected and defining no visible
presentation. No successor is selected or ranked.

The fresh documentation-only post-P8-FT-10 readiness audit is pinned to clean
migrated revision `f177cb2915a1c0e4618b44713e40c4eb1cf4c600` and the unchanged
clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It audits without advance ranking
the dialog's retained response and private projection model, visible result
presentation and states, activation/navigation/lookup, highlighting/WebEngine,
Preferences enablement/index policy, and index
readiness/visibility/status/background lifecycle. Exactly one smallest
independently decision-complete leaf is selected: P8-FT-11, private dialog
response-model integration. No successor after P8-FT-11 is selected or ranked.

P8-FT-11 makes the dialog own one child `FullTextResponseModel`. A replacement
submission clears both the P8-FT-9 retained response and model rows. Only a
generation-current completion is retained unchanged and copied into the
P8-FT-10 model's independent ordered result snapshot; response errors and
`partial` remain owned by the retained response. Stale and cancelled
completions update neither. Cancellation, service replacement, controller
detachment, and teardown preserve their completed P8-FT-9 behavior.

The leaf depends only on P8-FT-9 response retention and P8-FT-10 projection.
Acceptance covers initial and replacement emptiness, exact current-response
projection for success, empty, contained-error, and partial responses, atomic
row replacement, unchanged retained response-level state, stale/cancelled
suppression, and no new controller/service/configuration/persistence effects.
Its focused gate extends the existing offscreen dialog QTest, so the Release
baseline remains 109 tests. The implementation gate is Linux Release
configure/build, full `ctest --preset conan-release` with no unintended
registration delta, and clean committed exact-SCM `conan create` with the
Release Qt WebEngine host profile and packaged consumers. Installed interfaces
do not change; Release install and standalone installed C and C++ consumers are
nevertheless required as a stronger verification gate for this leaf.

Visible list/table/tree presentation, columns/additional roles, counts,
selection, empty/error/partial presentation, article activation/navigation and
lookup handoff, highlighting/WebEngine behavior, Preferences enablement/index
policy, index readiness/visibility/status/background lifecycle, public APIs,
persistence, adapters, `.gdfts`, legacy `_FTS`, dependencies, and unrelated
behavior remain excluded. Evidence is migrated
`full_text_search_dialog.h/.cpp`, `full_text_response_model.h/.cpp`, and their
focused tests, plus pinned legacy `fulltextsearch.hh:135-156`,
`fulltextsearch.cc:518-610,685-750`, and `fulltextsearch.ui:99-238`.
No successor after P8-FT-11 is selected or ranked.

P8-FT-11 is complete. The private dialog-owned model synchronizes only with
generation-current accepted responses, replacement submissions clear both
snapshots, and stale or cancelled completions update neither. Complete retained
response errors and partial state remain unchanged, projected results preserve
Core order, and no visible result view or successor is selected or ranked.

The fresh documentation-only post-P8-FT-11 readiness audit is pinned to clean
migrated revision `649e5001712a50c719b681fba11565f2bcef4c71` and unchanged
clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It audits every remaining
full-text workflow surface without advance ranking: visible result presentation
and decoration; selection; counts and empty/error/partial states;
activation/navigation/lookup; highlighting/WebEngine; Preferences/index
policy; and index readiness/visibility/status/background lifecycle.

Exactly one smallest dependency-ready and independently decision-complete leaf
is selected: P8-FT-12, private attachment of one visible `QListView` to the
completed dialog-owned `FullTextResponseModel`. It depends only on P8-FT-10
projection and P8-FT-11 synchronization. Widgets owns the view and attachment;
Core retains ordering, bounds, result metadata, errors, partial-state meaning,
and index behavior. The list exposes the model's existing ordered UTF-8
headwords, including duplicates. Replacement submission presents zero rows
immediately; only a generation-current completion presents synchronized rows.

Acceptance requires exactly one dialog-owned visible list using the existing
child model; zero rows initially and for replacement-pending, empty, and
error-only responses; exact Core-order rows for current successful and partial
responses; atomic repeated replacement; stale/cancelled suppression; and no
controller, service, configuration, or persistence change. The focused future
gate extends the existing offscreen dialog QTest, so the Release baseline stays
109 tests. The full future gate is Linux Release configure/build, full
`ctest --preset conan-release` without registration drift, clean exact-SCM
`conan create` with the Qt WebEngine host profile and packaged consumers,
Release install, and standalone installed C and C++ consumer checks.

Dictionary tooltip/name decoration, columns, extra roles, richer delegates,
selection/focus/retention, counts, and empty/error/partial presentation are
separate surfaces. Activation by click, double-click, or Return/Enter; lookup
construction, dictionary scoping, and MainWindow navigation are separate.
Highlighting remains downstream of activation and reviewed WebEngine handoff.
Preferences enablement, format exclusions, size policy, and persistence remain
behind product policy. Index readiness, visibility, status, progress, and
background lifecycle remain behind a Core lifecycle contract. Public APIs and
DTOs, adapters, `.gdfts`, legacy `_FTS`, dependencies, build-system changes,
and unrelated behavior are excluded.

Evidence is migrated `full_text_search_dialog.h/.cpp`,
`full_text_response_model.h/.cpp`, their focused tests, and completed
P8-FT-10/P8-FT-11 contracts, plus pinned legacy
`fulltextsearch.hh:135-156`, `fulltextsearch.cc:518-610,685-750`, and
`fulltextsearch.ui:99-238`. The legacy list view establishes the presentation
choice without coupling activation or status policy.
No successor after P8-FT-12 is selected or ranked.

P8-FT-12 is complete. The dialog now owns one private visible `QListView`
attached to its existing response model. Focused offscreen coverage proves
initial, replacement-pending, empty, and error-only zero-row behavior; exact
Core-order UTF-8 success and partial rows including duplicates; atomic repeated
replacement; and stale/cancelled suppression. The Release suite remains 109
registered tests, and no public, persistence, adapter, dependency, index, or
build-system surface changes. No successor is selected or ranked.

### Phase 8 full-text result activation intent (complete)

The independent documentation-only post-P8-FT-12 audit is pinned to clean
migrated revision `32b1fba41ee4b7b8e145acf41256e7c393b2764e` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It selects P8-FT-13 as the sole smallest dependency-ready,
decision-complete leaf: private Widgets-owned activation intent from the
P8-FT-12 result list. A valid result activates exactly once on a single
primary-button click or Return/Enter. Invalid, empty, stale, or reset indexes
do nothing; a double click must not add a second activation. Return/Enter
requires an already-current valid index and defines no selection or focus
policy.

The dialog resolves the row through `FullTextResponseModel::ResultAt()` and
delivers a safe value snapshot of the existing `FullTextResult` through a
private application-facing callback/signal. Core retains result identity and
metadata ownership. Model replacement, cancellation, service replacement, and
teardown cannot invalidate an accepted intent. No MainWindow navigation or
installed interface change belongs to this leaf.

Every other remaining surface stays explicitly decomposed. Initial/current
selection, focus, and retention are separate. Dictionary tooltip/name
decoration, columns, extra roles, and richer delegates are separate. Counts
and empty/error/partial presentation are separate. MainWindow lookup,
per-result dictionary scoping, tab/history/navigation mutation, highlighting,
and WebEngine handoff are downstream and separate; current tab navigation
cannot retain exact result-dictionary scope without a separate contract.
Preferences enablement, format exclusions, size policy, and persistence remain
behind product policy. Index readiness, visibility, status, progress, and
background lifecycle remain behind a Core lifecycle contract. Public APIs and
DTOs, persistence, adapters, `.gdfts`, legacy `_FTS`, dependencies,
build-system changes, and unrelated behavior are excluded.

Focused acceptance extends the existing offscreen dialog QTest with valid
single-click and current-index Return/Enter activation, exact one-delivery and
exact copied result metadata, double-click non-duplication, and invalid/empty/
reset-index suppression while retaining stale/cancelled and teardown coverage.
The registered Release baseline remains 109 tests. The future full gate remains
Linux Release configure/build, full `ctest --preset conan-release`, clean
exact-SCM `conan create` with the Qt WebEngine host profile and packaged
consumers, Release install, and standalone installed C and C++ consumers.
Evidence is migrated `full_text_search_dialog.h/.cpp`,
`full_text_response_model.h/.cpp`, and focused tests plus pinned legacy
`fulltextsearch.cc:292-293,594-610,664-673` and
`fulltextsearch.hh:227,232-233`. No successor after P8-FT-13 is selected or
ranked.

P8-FT-13 is complete. `FullTextSearchDialog` now translates a valid primary
single click or current-row Return/Enter into exactly one private by-value
activation intent containing the unchanged `FullTextResult` returned by the
existing model. Invalid, reset, stale, and cancelled rows remain inert, and a
double click does not add another activation. MainWindow and all lookup,
dictionary-scope, and navigation behavior remain untouched. The Release test
baseline remains 109, and no successor is selected or ranked.

### Phase 8 full-text scoped navigation prerequisite (complete)

The independent post-P8-FT-13 audit was pinned to clean migrated revision
`6ac1912965a60f6d8c9b5614752fa97f3426d80f` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selected only
P8-FT-14, the Core-owned scoped lookup-navigation prerequisite. The audit
rejects a direct activation-to-lookup connection as the next leaf: current
navigation stores only query and group, so that connection would widen the
full-text dictionary scope on replay, restore, or participation refresh.

P8-FT-14 adds an optional authoritative ordered dictionary scope to lookup
navigation, represented by an active flag and dictionary IDs. Active-empty is
distinct from absent and performs no dictionary lookup. Core owns validation,
tab history, and backward-compatible session serialization. A scope permits at
most `kMaximumLookupDictionaryFilters` nonempty IDs, each valid UTF-8 without
NUL and bounded by `kMaximumLookupFilterBytes`. New navigation records append
the active flag, count, and encoded IDs; existing ten-field records load as
unscoped, and inconsistent counts are rejected. MainWindow consumes a retained
scope unchanged when constructing `LookupQuery` and otherwise preserves the
current dictionary-bar projection behavior. Invalid scoped state fails
atomically under the existing navigation/session limits. P8-FT-14 is complete.
Its only public contract change is the additive `dictionary_filter_active` and
`dictionary_ids` fields on `TabNavigationState`; the C ABI remains unchanged.
Core preserves explicit presence, active-empty, and ordered non-empty scope
through identity, history, session persistence, and restoration.

This prerequisite makes later activation handoff bounded but does not perform
it. The approved downstream behavior will query the activated result headword
across the complete dictionary scope submitted for that full-text response;
it will not narrow to `result.dictionary.id`. Activation connection, tab
disposition, query-edit synchronization, history emission, exact
`document_id` selection, and highlighting/WebEngine handoff remain separate.
Selection/focus/retention, result decoration, counts/status presentation,
Preferences/index policy, index lifecycle, adapters, index formats and legacy
`_FTS`, dependencies, build-system changes, and unrelated work remain separate.

Focused acceptance covers scoped navigation validation, active-empty behavior,
MainWindow lookup-query construction, history back/forward retention, session
round trips, old-session compatibility, and atomic rejection. Full acceptance
uses the existing Linux Release build/test/package/install/consumer gate.
Evidence is migrated
`desktop_facade.h`, Core tab/session tests, `main_window.cpp` lookup/navigation
construction, P8-FT-7 projection, and pinned legacy
`fulltextsearch.cc:594-610` plus `mainwindow.cc:3002-3014`.
No successor after P8-FT-14 is selected or ranked.

### Phase 8 full-text accepted-response activation context (complete)

The independent post-P8-FT-14 audit was pinned to clean migrated revision
`e3f2ac70814ea8166af747ffee2e3718b5323ac6` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes all
remaining full-text surfaces without advance ranking and selects only
P8-FT-15, private accepted-response activation-context retention. P8-FT-15 is
now complete.

Connecting P8-FT-13 directly through MainWindow is not dependency-ready.
Before P8-FT-15, the activation signal carried the result but not the complete
scope submitted for its response, and the dialog's projected query could
change independently of retained response rows. P8-FT-15 snapshots the
authoritative active flag and ordered dictionary IDs at submission, associates
them only with the generation-current accepted response, clears them with
response/model replacement, and emits them by value with a valid result
activation. Stale or cancelled completion, service replacement, detach, and
teardown cannot restore or deliver obsolete context. Authoritative-empty
remains distinct from absent. This is a private Widgets change with no public
API/DTO or persistence change.

The later connection is decision-complete but remains unselected. It looks up
the exact result headword in the current activated article tab, leaves the main
query edit unchanged, and builds `kLookup` navigation with headword query/title,
the active MainWindow group, and the accepted response's scope copied
unchanged. Successful tab mutation follows existing MainWindow sequencing:
tab synchronization, `ArticleTabSessionMutated`, and
`StartNavigationLookup(..., true)`. This preserves Core tab history, session
identity, ordinary lookup-history emission, replay, restoration, and
authoritative-empty no-work behavior. Missing context/facade, invalid
activation/navigation, and tab-limit failure are no-ops for lookup and
history/session mutation and retain existing failure-status behavior.

Exact `document_id`, source-dictionary targeting, match/excerpt metadata,
highlighting, ignore-diacritics, and WebEngine handoff remain deferred.
Selection/focus/retention; decoration; counts and empty/error/partial states;
activation-to-MainWindow connection; Preferences/index policy; index
readiness/visibility/status/progress/background lifecycle; adapters and index
formats including legacy `_FTS`; persistence beyond existing navigation;
dependencies, build-system work, and unrelated behavior remain independent.

Focused acceptance covers absent, ordered nonempty, and authoritative-empty
scope snapshots, exact accepted-generation association, replacement clearing,
stale/cancelled suppression, service/detach safety, by-value lifetime, and
immunity to later `SetProjectedQuery()` changes. The implementation gate is the
Linux Release test/package/install/consumer workflow. Evidence is migrated
dialog submission/completion/activation code and tests, P8-FT-7,
P8-FT-13, P8-FT-14, and MainWindow lookup/navigation conventions, plus pinned
legacy `fulltextsearch.cc:594-610` and `mainwindow.cc:3002-3014`.
No successor after P8-FT-15 is selected or ranked.

### Phase 8 full-text scoped result navigation connection (complete)

The independent post-P8-FT-15 audit is pinned to clean migrated revision
`bb7298ab1fd04c302fb74d2903d3fa92a8c63bc6` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes every
remaining full-text workflow surface without advance ranking and selects only
P8-FT-16, the private activation-to-MainWindow scoped current-tab navigation
connection.

P8-FT-16 is dependency-ready because P8-FT-13 delivers the exact result,
P8-FT-14 retains authoritative optional dictionary scope through Core
navigation history/session/replay, and P8-FT-15 delivers the immutable accepted-
response scope with activation. MainWindow owns the private signal connection,
navigation construction, current-tab mutation, failure status, and lookup
handoff. Core retains navigation validation, bounded history/session ownership,
and scoped replay. No public API, persistence, dependency, adapter, index-
format, or build-system change belongs to this leaf.

The activated result's exact headword becomes the `kLookup` navigation query
and title. Navigation targets and activates the current article tab, creates no
new or background tab, leaves the main query edit unchanged, captures the
active MainWindow group, and copies the accepted dictionary scope unchanged,
including authoritative empty. A successful `OpenArticleTab` is followed in
the existing order by tab synchronization, `ArticleTabSessionMutated`, and
`StartNavigationLookup(..., true)`, preserving one identity for tab history,
session restoration/replay, ordinary lookup-history emission, and lookup.
Missing facade, dialog, or accepted context; invalid activation/navigation; or
tab-limit failure starts no lookup and causes no lookup-history or session
mutation while retaining existing MainWindow failure behavior.

Focused acceptance covers exact headword/query/title, current-tab activation
without tab creation, unchanged main query text, activation-time group capture,
ordered nonempty and authoritative-empty scopes, successful sequencing,
history/session identity and replay without widening, and missing-context,
missing-facade/dialog, invalid-activation/navigation, and tab-limit no-ops. The
completed implementation gate is Linux Release configure/build, full
`ctest --preset conan-release`, clean exact-SCM `conan create`, packaged
consumers, Release install, and standalone installed consumers.

Exact `document_id` and source-dictionary targeting, match/excerpt metadata,
highlighting, ignore-diacritics, and WebEngine handoff remain deferred.
Selection/focus/retention; decoration; counts and empty/error/partial states;
Preferences/index policy; index readiness/visibility/status/progress/background
lifecycle; adapters and index formats including legacy `_FTS`; persistence
beyond existing navigation; dependencies/build work; and unrelated parity
remain independent surfaces. They are decomposed only and no later leaf is
selected or ranked.

Evidence is migrated dialog activation/context code and focused tests,
P8-FT-7, P8-FT-13, P8-FT-14 and its Core tab/session coverage, P8-FT-15, and
MainWindow `OpenArticleTab`/synchronization/session-mutation/failure-status/
`StartNavigationLookup` conventions, plus pinned legacy
`fulltextsearch.cc:594-610` and `mainwindow.cc:3002-3014`.
P8-FT-16 is complete through a private lifetime-safe MainWindow connection and
the existing Core-owned current-tab navigation contract. It adds no test
executable or public/installed interface.
No successor after P8-FT-16 is selected or ranked.

### Phase 8 full-text accepted-result count presentation (complete)

The independent post-P8-FT-16 audit is pinned to clean migrated revision
`b7cfd864b85df4a7ee36d4e08e36287c4fabfd7b` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes
every remaining full-text workflow surface without advance ranking and selects
only P8-FT-17, private accepted-result count presentation.

P8-FT-11 supplies the generation-current retained response, P8-FT-12 supplies
the atomic ordered visible model, and replacement submission already clears
both. P8-FT-17 therefore adds a dialog-owned `Articles found: N` label whose
value follows the accepted model row count. It starts and resets to zero;
accepted success and partial responses count every retained row, including
duplicates; accepted empty and error-only responses show zero. A partial
count describes retained results without promising completeness. Stale or
cancelled completions, service replacement, detach, and teardown cannot
overwrite the current count. Core retains ownership of ordering, duplicates,
errors, and `partial` semantics.

Focused acceptance covers initial and replacement zero, nonempty and duplicate
success, empty and contained-error responses, partial responses with and
without rows, repeated accepted responses, and stale/cancelled or detached
completion safety. The focused gate extends the existing private dialog
test and application smoke. The full implementation gate remains Linux Release
configure/build, full `ctest --preset conan-release`, clean exact-SCM
`conan create`, packaged consumers, Release install, and standalone installed
consumers. This documentation-only audit requires no build.

Exact `document_id` lookup and source-dictionary targeting; initial/current
selection, keyboard focus, and retention; dictionary/result decoration,
tooltips, and metadata; empty/error/partial messaging beyond the numeric
retained-result count; match/excerpt presentation; highlighting,
ignore-diacritics transfer, and WebEngine handoff; Preferences enablement,
format exclusions, size/index policy, and persistence; index readiness,
visibility, status, progress, background lifecycle, rebuild, and failure UI;
adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies, builds, and
unrelated parity remain independent and unranked. P8-FT-17 changes no public
API, DTO, persistence, adapter, index, dependency, or build-system surface.

Evidence is migrated `full_text_search_dialog.cpp`,
`full_text_response_model.cpp`, and their focused tests, plus pinned legacy
`fulltextsearch.cc:290,448-449,570-571` and
`fulltextsearch.ui:99-129`, where `articlesFoundLabel` is initialized and
updated from retained result count.
P8-FT-17 is complete through the private dialog-owned label and the retained
accepted-response/model contract. It adds no test executable or
public/installed interface, and the registered Release baseline remains 109
tests.
No successor after P8-FT-17 is selected or ranked.

### Phase 8 full-text result dictionary-name tooltip (complete)

The independent post-P8-FT-17 audit is pinned to clean migrated revision
`7c8fc16e55844b2712f3257a78b7f6b8e6cc3b5b` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes
every remaining full-text workflow surface without advance ranking and selects
only P8-FT-18, private result dictionary-name tooltip projection.

P8-FT-10 already retains each complete immutable `FullTextResult` in the
private model, P8-FT-11 synchronizes it only with the accepted response, and
P8-FT-12 attaches that model to the visible result list. P8-FT-18 therefore
adds `Qt::ToolTipRole` for a valid row using the exact UTF-8
`FullTextResult::dictionary.name`. Duplicate-headword rows retain independent
dictionary-name tooltips. An empty name produces no visible tooltip, with no
fallback to ID, source, edition, or other metadata. Invalid, foreign,
out-of-range, nonzero-column, and unsupported-role requests return no value.
Display text, order, duplicates, metadata, activation, synchronization, and
count behavior remain unchanged.

Focused acceptance covers exact Unicode names, distinct tooltips for
duplicate-headword rows from different dictionaries, empty-name suppression,
copied/moved response lifetime, deterministic reset replacement, invalid and
foreign indexes, unsupported roles, and unchanged display and result metadata.
The focused command is
`ctest --preset conan-release -R '^full_text_response_model_test$'` after the
Release target has been built. The full implementation gate is
Linux Release configure/build, full `ctest --preset conan-release`, clean
exact-SCM `conan create`, packaged consumers, Release install, and standalone
installed consumers.

Exact `document_id` navigation and source-dictionary targeting; initial/current
selection, keyboard focus, and selection retention; non-tooltip decoration,
columns, delegates, icons, and additional metadata roles; empty/error/partial
messaging beyond the numeric retained-result count; match ranges and excerpt
presentation; highlighting, ignore-diacritics transfer, and WebEngine handoff;
Preferences enablement, format exclusions, size/index policy, and persistence;
index readiness, visibility, status, progress, background lifecycle, rebuild,
and failure UI; adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies,
builds, and unrelated parity remain independent and unranked. P8-FT-18 changes
no public API, DTO, persistence, Core, adapter, index, dependency, or
build-system surface.

Evidence is migrated `full_text_response_model.h/.cpp`, its focused tests, and
the P8-FT-11/P8-FT-12 dialog synchronization and attachment, plus pinned legacy
`fulltextsearch.cc:690-721`, where `HeadwordsListModel::data()` supplies
dictionary names through `Qt::ToolTipRole`.
P8-FT-18 is complete through the private response-model tooltip projection and
focused tests. It adds no test executable or public/installed interface, and
the registered Release baseline remains 109 tests.
No successor after P8-FT-18 is selected or ranked.

### Phase 8 full-text result edit-role projection (complete)

The independent post-P8-FT-18 audit is pinned to clean migrated revision
`d7d2f76a397f5adf0a546ef3885216b35f82753c` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes
every remaining full-text workflow surface without advance ranking and selects
only P8-FT-19, private result headword edit-role projection.

P8-FT-10 already retains each complete immutable `FullTextResult` in the
private model, P8-FT-11 synchronizes it only with the accepted response, and
P8-FT-12 attaches that model to the visible result list. P8-FT-19 therefore
adds `Qt::EditRole` for a valid row using the exact UTF-8
`FullTextResult::headword`, decoded identically to `Qt::DisplayRole`. Duplicate
rows retain independent headwords. Invalid, foreign, out-of-range,
nonzero-column, and unsupported-role requests return no value. Display,
tooltip, order, duplicates, metadata, activation, synchronization, count,
selection, focus, and retention behavior remain unchanged.

Focused acceptance covers exact Unicode edit-role headwords, duplicate
rows, equality with the display role, copied/moved response lifetime,
deterministic reset replacement, invalid and foreign indexes, unsupported
roles, and unchanged tooltip and result metadata. The focused command is
`ctest --preset conan-release -R '^full_text_response_model_test$'` after the
Release target has been built. The full implementation gate is
Linux Release configure/build, full `ctest --preset conan-release`, clean
exact-SCM `conan create`, packaged consumers, Release install, and standalone
installed consumers.

Exact `document_id` navigation and source-dictionary targeting; initial/current
selection, keyboard focus, and selection retention; non-edit-role decoration,
columns, delegates, icons, and additional metadata roles; empty/error/partial
messaging beyond the numeric retained-result count; match ranges and excerpt
presentation; highlighting, ignore-diacritics transfer, and WebEngine handoff;
Preferences enablement, format exclusions, size/index policy, and persistence;
index readiness, visibility, status, progress, background lifecycle, rebuild,
and failure UI; adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies,
builds, and unrelated parity remain independent and unranked. P8-FT-19 changes
no public API, DTO, persistence, Core, adapter, index, dependency, or
build-system surface.

Evidence is migrated `full_text_response_model.h/.cpp`, its focused tests, and
the P8-FT-10/P8-FT-11/P8-FT-12 model ownership, synchronization, and attachment,
plus pinned legacy `fulltextsearch.cc:690-721`, where
`HeadwordsListModel::data()` supplies the exact headword through both
`Qt::DisplayRole` and `Qt::EditRole`.
P8-FT-19 is complete through the private response-model edit-role projection
and focused tests. It adds no test executable or public/installed interface,
and the registered Release baseline remains 109 tests.
No successor after P8-FT-19 is selected or ranked.

### Phase 8 full-text result-list selection contract (complete)

The independent post-P8-FT-19 audit is pinned to clean migrated revision
`a60f258e9226ebc7e1ee2115055d2ee531dc097a` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes
every remaining full-text workflow surface without advance ranking and selects
only P8-FT-20, the private result-list selection and reset contract. P8-FT-20
is complete through dialog-owned deterministic reset behavior and focused
tests.

P8-FT-10 owns the immutable result snapshot, P8-FT-11 synchronizes only the
accepted response, P8-FT-12 attaches it to one visible `QListView`, and P8-FT-13
requires a valid current row for activation. P8-FT-20 therefore stays within
the private Widgets dialog: the list has at most one current and selected row;
initial, empty, and error-only states have neither; and a generation-current
successful or partial response does not select a row or steal keyboard focus.
Ordinary user interaction may establish one current and selected row.

Starting a replacement clears rows, current index, and selection atomically.
The accepted replacement remains unselected even when it contains the same
headword or row position. Stale or cancelled completions cannot restore
selection, current index, or focus. Reset preserves whether the list or another
widget already owns keyboard focus. Existing click, Return, and Enter
activation remains unchanged and still requires a valid current row. The
response model continues to own only ordered result data; Core continues to own
result identity and navigation semantics.

Focused acceptance extends `full_text_search_dialog_test` for initial,
successful, partial, empty, and error-only responses; explicit user selection;
replacement clearing and same-row non-retention; stale and cancelled
completion suppression; list-owned and other-widget-owned focus; and unchanged
activation. The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The full implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers.

Exact `document_id` navigation and source-dictionary targeting; non-selection
decoration, columns, delegates, icons, and additional metadata roles;
empty/error/partial messaging beyond the numeric retained-result count; match
ranges and excerpt presentation; highlighting, ignore-diacritics transfer, and
WebEngine handoff; Preferences enablement, format exclusions, size/index policy,
and persistence; index readiness, visibility, status, progress, background
lifecycle, rebuild, and failure UI; adapters, `.gdfts`, legacy `_FTS`, index
formats, dependencies, builds, and unrelated parity remain independent and
unranked. No public API, DTO, persistence, Core, adapter, index, dependency, or
build-system surface belongs to P8-FT-20.

Evidence is migrated `full_text_search_dialog.h/.cpp`,
`full_text_response_model.h/.cpp`, their focused tests, and completed
P8-FT-10 through P8-FT-13, plus pinned legacy
`fulltextsearch.cc:287-315,448-449,567-579,662-673` and
`fulltextsearch.ui:99`. The legacy dialog attaches one list model and delegate,
clears the model before replacement, adds accepted rows without programmatic
selection or focus transfer, and activates only a clicked or current valid row.
No successor after P8-FT-20 is selected or ranked.

### Phase 8 full-text bidirectional result rendering (complete)

The independent post-P8-FT-20 audit is pinned to clean migrated revision
`53281651f9f882cfb9364a55a908f7104d760456` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes
every remaining full-text workflow surface without advance ranking and
selected only P8-FT-21, private per-row bidirectional painting and elision for
the existing result list. P8-FT-21 is complete.

P8-FT-12 owns one visible `QListView`, P8-FT-18 and P8-FT-19 expose the exact
dictionary name and headword roles, and P8-FT-20 fixes selection, current-index,
reset, and focus behavior. P8-FT-21 therefore stays within a private Widgets
delegate. The response model continues to own ordered result data; Core
continues to own result identity and navigation semantics.

For each painted result row, the delegate derives direction independently from
the exact displayed headword. Right-to-left text paints with
`Qt::RightToLeft` and uses `Qt::ElideLeft` when elision is enabled. All other
text paints with `Qt::LeftToRight` and uses `Qt::ElideRight` when elision is
enabled. `Qt::ElideNone` remains unchanged. Mixed Unicode follows Qt's
per-string direction result. Model roles and values, ordering, tooltips,
retained-result count, selection, focus, activation, response ownership, and
accepted-generation synchronization remain unchanged.

Focused acceptance covers left-to-right, right-to-left, and mixed
Unicode headwords; enabled and disabled elision; duplicate rows; accepted
replacement; and unchanged tooltips, selection, focus, and activation. The
focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The full implementation gate remains
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-21 adds no test executable or
public/installed interface, so the registered Release baseline remains 109
tests.

Exact `document_id` navigation and source-dictionary targeting; columns,
icons, additional metadata roles, and non-bidirectional decoration;
empty/error/partial messaging beyond the numeric retained-result count; match
ranges and excerpt presentation; highlighting, ignore-diacritics transfer, and
WebEngine handoff; Preferences enablement, format exclusions, size/index policy,
and persistence; index readiness, visibility, status, progress, background
lifecycle, rebuild, and failure UI; adapters, `.gdfts`, legacy `_FTS`, index
formats, dependencies, builds, and unrelated parity remain independent and
unranked. No public API, DTO, persistence, Core, adapter, index, dependency, or
build-system surface belongs to P8-FT-21.

Evidence is migrated `full_text_search_dialog.h/.cpp`,
`full_text_response_model.h/.cpp`, their focused tests, and completed
P8-FT-12/P8-FT-18/P8-FT-19/P8-FT-20. Pinned legacy evidence is
`fulltextsearch.cc:287-315`, `delegate.hh`, `delegate.cc:5-31`, and
`fulltextsearch.ui:99`: the dialog installs a private word-list delegate whose
paint path derives direction per displayed string, chooses left or right
elision accordingly, and preserves an explicit no-elision setting.
No successor after P8-FT-21 is selected or ranked.

### Phase 8 full-text partial-response status (complete)

The independent post-P8-FT-21 audit is pinned to clean migrated revision
`b47e96630f2b4f9bb702442b9f563dc0719eec04` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes
every remaining full-text workflow surface without advance ranking and selects
only P8-FT-22, private presentation of the generation-current accepted
response's authoritative partial state.

P8-FT-9 accepts only the current terminal generation, P8-FT-11 retains the
complete response, and P8-FT-17 presents its retained-result count. P8-FT-22
therefore adds only one private Widgets status label. It reads
`Results may be incomplete.` exactly when the accepted response has
`partial == true`. Initial state and replacement submission hide it. Complete
responses keep it hidden even when empty or error-containing; partial responses
show it with zero or nonzero retained rows and with or without errors.

Core remains authoritative for the partial flag, results, ordering, and errors.
Widgets does not infer partiality from counts, errors, cancellation, or error
codes and does not display dictionary IDs, backend messages, or error details.
Stale or cancelled completions, controller detachment, service replacement,
and teardown cannot introduce or overwrite the current status. Result rows,
count, selection, focus, activation, response ownership, and accepted-generation
synchronization remain unchanged.

Focused acceptance covers initial and replacement reset; complete and
partial responses with zero and nonzero rows; partial responses with and without
errors; complete error-containing responses; repeated accepted responses; and
stale, cancelled, detached, replaced-service, and teardown completion safety.
The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The completed implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-22 adds no test executable or
public/installed interface, so the registered Release baseline remains 109
tests.

Empty-result messaging; error summaries and details; exact `document_id`
navigation and source-dictionary targeting; columns, icons, additional metadata
roles, and other decoration; match ranges and excerpt presentation;
highlighting, ignore-diacritics transfer, and WebEngine handoff; Preferences
enablement, format exclusions, size/index policy, and persistence; index
readiness, visibility, status, progress, background lifecycle, rebuild, and
failure UI; adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies,
builds, and unrelated parity remain independent and unranked. No public API,
DTO, persistence, Core, adapter, index, dependency, or build-system surface
belongs to P8-FT-22.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, and completed P8-FT-9/P8-FT-11/P8-FT-17.
Pinned legacy `fulltextsearch.cc:448-449,499-586` silently contains individual
dictionary failures while updating retained rows and count; it supplies no
safe structured error-detail presentation contract. P8-FT-22 is complete. No
successor after P8-FT-22 is selected or ranked.

### Phase 8 full-text empty-result status (complete)

The independent documentation-only post-P8-FT-22 readiness audit is pinned to
clean migrated revision `8a79669095166821e6361f24bf02a27d8bb6a2fb` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes every remaining
full-text workflow surface without advance ranking and selects only P8-FT-23,
private presentation of a conclusive empty accepted response.

P8-FT-9 accepts only the current terminal generation, P8-FT-11 retains the
complete response, P8-FT-17 presents its retained-result count, and P8-FT-22
independently presents authoritative partiality. P8-FT-23 therefore adds only
one private Widgets status. It reads `No matches` exactly when an accepted
response has zero retained results, `partial == false`, and no errors. Initial
state and replacement submission hide it. Nonempty, partial, and
error-containing responses hide it, and the existing partial status remains
independent.

Core remains authoritative for results, partiality, and errors. Widgets does
not claim a conclusive empty search after incomplete or failed work and does
not display dictionary IDs, backend messages, or error details. Stale or
cancelled completions, controller detachment, service replacement, and
teardown cannot introduce or overwrite the current status. Result rows, count,
selection, focus, activation, response ownership, and accepted-generation
synchronization remain unchanged.

Focused acceptance covers initial and replacement reset; conclusive
empty responses; nonempty, partial-empty, partial-nonempty, error-only, and
result-plus-error responses; repeated accepted transitions; and stale,
cancelled, detached, replaced-service, and teardown completion safety. The
focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The completed implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-23 adds no test executable or
public/installed interface, so the registered Release baseline remains 109
tests.

Error summaries and details; exact `document_id` navigation and
source-dictionary targeting; columns, icons, additional metadata roles, and
other decoration; match ranges and excerpt presentation; highlighting,
ignore-diacritics transfer, and WebEngine handoff; Preferences enablement,
format exclusions, size/index policy, and persistence; index readiness,
visibility, status, progress, background lifecycle, rebuild, and failure UI;
adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies, builds, and
unrelated parity remain independent and unranked. No public API, DTO,
persistence, Core, adapter, index, dependency, or build-system surface belongs
to P8-FT-23.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, and completed
P8-FT-9/P8-FT-11/P8-FT-17/P8-FT-22. Pinned legacy
`fulltextsearch.cc:448-449,499-586` clears retained rows before replacement,
silently contains individual dictionary failures, and exposes only the
accepted zero count for an empty result. Migrated
`main_window.cpp:5122-5136,7706-7723` already uses exact text `No matches` for
a conclusive zero-match article search. P8-FT-23 is complete. No successor
after P8-FT-23 is selected or ranked.

### Phase 8 full-text terminal failure status (complete)

The independent documentation-only post-P8-FT-23 readiness audit is pinned to
clean migrated revision `3b67ce9413cba3555115779ddd48d70e927a7fd4` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes every remaining
full-text workflow surface without advance ranking and selects only P8-FT-24,
private presentation of a terminal failed accepted response.

P8-FT-9 accepts only the current terminal generation, P8-FT-11 retains the
complete response, P8-FT-17 presents its retained-result count, P8-FT-22
presents authoritative partiality, and P8-FT-23 distinguishes conclusive
empty work. P8-FT-24 therefore adds only one private Widgets status. It reads
`Full-text search failed` exactly when an accepted response has zero retained
results, `partial == false`, and one or more errors. Initial state and
replacement submission hide it. Conclusive empty, nonempty, and partial
responses hide it.

Core remains authoritative for results, partiality, and errors. Widgets does
not label retained or incomplete work as a terminal failure and does not
display dictionary IDs, error codes, backend messages, or raw details. The
result count, partial status, and empty status remain independent. Stale or
cancelled completions, controller detachment, service replacement, and
teardown cannot introduce or overwrite the current status. Result rows,
selection, focus, activation, response ownership, and accepted-generation
synchronization remain unchanged.

Focused acceptance covers initial and replacement reset; terminal
error-only responses with each existing error-code category and multiple
errors; conclusive empty, nonempty, partial-empty, partial-nonempty, and
result-plus-error responses; repeated accepted transitions; and stale,
cancelled, detached, replaced-service, and teardown completion safety. The
focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The completed implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-24 adds no test executable or
public/installed interface, so the registered Release baseline remains 109
tests.

Result-plus-error and partial-error summaries and all error details; exact
`document_id` navigation and source-dictionary targeting; columns, icons,
additional metadata roles, and other decoration; match ranges and excerpt
presentation; highlighting, ignore-diacritics transfer, and WebEngine handoff;
Preferences enablement, format exclusions, size/index policy, and persistence;
index readiness, visibility, status, progress, background lifecycle, rebuild,
and failure UI; adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies,
builds, and unrelated parity remain independent and unranked. No public API,
DTO, persistence, Core, adapter, index, dependency, or build-system surface
belongs to P8-FT-24.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, completed
P8-FT-9/P8-FT-11/P8-FT-17/P8-FT-22/P8-FT-23, and the migrated generic
`Suggestion lookup failed` presentation in `main_window.cpp:6776-6791`.
Pinned legacy `fulltextsearch.cc:499-586` silently contains individual
dictionary failures and supplies no safe structured error-detail presentation
contract. P8-FT-24 therefore presents only the migrated response's bounded,
terminal failure fact. P8-FT-24 is complete. No successor after P8-FT-24 is
selected or ranked.

### Phase 8 full-text mixed-result error summary (complete)

The independent documentation-only post-P8-FT-24 readiness audit is pinned to
clean migrated revision `e76e6ec326ff6cbb8a33969d22fb72327261abbd` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-25: privately summarize accepted work
that retains results while also containing one or more errors.

P8-FT-25 is dependency-ready because P8-FT-9 accepts only the current terminal
generation, P8-FT-11 retains the complete response, P8-FT-17 presents its
retained-result count, P8-FT-22 presents authoritative partiality, and
P8-FT-24 reserves terminal-failure presentation for result-free, nonpartial
errors. One private Widgets-owned status consumes only the existing response's
result and error collections. Core remains authoritative for both collections
and for `partial`. No public API, DTO, persistence, dependency, adapter,
index-format, or build-system change is required.

The status reads `Some dictionaries could not be searched` exactly when a
generation-current accepted response has one or more retained results and one
or more errors. Initial state and replacement submission hide it. Result-free
and error-free responses hide it. The status neither infers nor changes
partiality, so it may coexist with `Results may be incomplete.` only when the
accepted response's authoritative `partial` flag is true. Widgets exposes no
dictionary ID, error code, backend message, or raw detail. The result count,
partial, empty, and terminal-failure statuses remain independent. Stale or
cancelled completions, controller detachment, service replacement, and
teardown cannot introduce or overwrite the current status. Result rows,
selection, focus, activation, response ownership, and accepted-generation
synchronization remain unchanged.

Focused acceptance covers initial and replacement reset; one and
multiple retained results with each existing error-code category and multiple
errors; authoritative partial true and false combinations; result-only,
error-only, empty, and conclusive-empty responses; repeated accepted
transitions; and stale, cancelled, detached, replaced-service, and teardown
completion safety. The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The completed implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-25 adds no test executable or
public/installed interface, so the registered Release baseline remains 109
tests.

Partial-without-results summaries and all error details; exact `document_id`
navigation and source-dictionary targeting; columns, icons, additional
metadata roles, and other decoration; match ranges and excerpt presentation;
highlighting, ignore-diacritics transfer, and WebEngine handoff; Preferences
enablement, format exclusions, size/index policy, and persistence; index
readiness, visibility, status, progress, background lifecycle, rebuild, and
failure UI; adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies,
builds, and unrelated parity remain independent surfaces. They are decomposed
only; none is selected or ranked. No public API, DTO, persistence, Core,
adapter, index, dependency, or build surface belongs to P8-FT-25.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, and completed
P8-FT-9/P8-FT-11/P8-FT-17/P8-FT-22/P8-FT-24. Pinned legacy
`fulltextsearch.cc:499-586` silently contains individual dictionary failures
while retaining successful headwords and supplies no safe structured error-
detail presentation contract. P8-FT-25 therefore presents only the migrated
response's bounded mixed-result fact. P8-FT-25 is complete. No successor after
P8-FT-25 is selected or ranked.

### Phase 8 full-text partial-without-results status (complete)

The independent documentation-only post-P8-FT-25 readiness audit is pinned to
clean migrated revision `6796df215f28404ccb4f1cc04c6bb7538320ae27` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-26: privately distinguish an accepted
partial response that retains no results from a conclusive empty response.

P8-FT-26 is dependency-ready because P8-FT-9 accepts only the current terminal
generation, P8-FT-11 retains the complete response, P8-FT-17 presents its
retained-result count, P8-FT-22 presents authoritative partiality, and
P8-FT-23 reserves `No matches` for a complete error-free response. One private
Widgets-owned status consumes only the existing response's result collection
and `partial` flag. Core remains authoritative for both. No public API, DTO,
persistence, dependency, adapter, index-format, or build-system change is
required.

The status reads `No matches in searched dictionaries` exactly when a
generation-current accepted response has zero retained results and
authoritative `partial == true`. Initial state and replacement submission hide
it. Complete and nonempty responses hide it. It coexists with
`Results may be incomplete.` and neither infers nor changes partiality. Widgets
exposes no dictionary ID, error code, backend message, or raw detail. The
result count, conclusive-empty, terminal-failure, and mixed-result statuses
remain independent. Stale or cancelled completions, controller detachment,
service replacement, and teardown cannot introduce or overwrite the current
status. Result rows, selection, focus, activation, response ownership, and
accepted-generation synchronization remain unchanged.

Focused acceptance covers initial and replacement reset; zero-result
partial responses with no, one, and multiple errors across each existing
error-code category; nonempty partial responses; complete empty, error-only,
result-only, and mixed-result responses; repeated accepted transitions; and
stale, cancelled, detached, replaced-service, and teardown completion safety.
The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The completed implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-26 adds no test executable or
public/installed interface, so the registered Release baseline remains 109
tests.

All error details; exact `document_id` navigation and source-dictionary
targeting; columns, icons, additional metadata roles, and other decoration;
match ranges and excerpt presentation; highlighting, ignore-diacritics
transfer, and WebEngine handoff; Preferences enablement, format exclusions,
size/index policy, and persistence; index readiness, visibility, status,
progress, background lifecycle, rebuild, and failure UI; adapters, `.gdfts`,
legacy `_FTS`, index formats, dependencies, builds, and unrelated parity remain
independent surfaces. They are decomposed only; none is selected or ranked. No
public API, DTO, persistence, Core, adapter, index, dependency, or build surface
belongs to P8-FT-26.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, and completed
P8-FT-9/P8-FT-11/P8-FT-17/P8-FT-22/P8-FT-23. Pinned legacy
`fulltextsearch.cc:448-449,499-586` reports only the aggregate result count and
silently contains individual dictionary failures, so it supplies no safe
partial-empty presentation contract. P8-FT-26 therefore presents only the
migrated response's bounded partial-without-results fact. P8-FT-26 is complete.
No successor after
P8-FT-26 is selected or ranked.

### Phase 8 full-text accepted-error count presentation (selected)

The independent documentation-only post-P8-FT-26 readiness audit is pinned to
clean migrated revision `e6c85cb99f350eb1b82bd83ad138d4f2695b8bfe` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-27: privately present the authoritative
accepted full-text error count.

P8-FT-27 is dependency-ready because P8-FT-9 accepts only the current terminal
generation and P8-FT-11 retains its complete `FullTextResponse`, including the
bounded ordered error collection. One private Widgets-owned status consumes
only that collection's size. Core remains authoritative for the collection.
No public API, DTO, persistence, dependency, adapter, index-format, or build-
system change is required.

The status reads `Errors: %1` with `%1` replaced by the decimal error count
exactly when a generation-current accepted response contains one or more
errors. Initial state, replacement submission, and accepted error-free
responses hide it. It may coexist with the terminal-failure, mixed-result,
partial, or partial-empty statuses without changing their predicates or the
authoritative `partial` flag. Widgets neither deduplicates errors nor infers a
dictionary count and exposes no dictionary ID, error code, backend message, or
raw detail. Stale or cancelled completions, controller detachment, service
replacement, and teardown cannot introduce or overwrite the current status.
Results, ordering, selection, focus, activation, count, response ownership,
and existing status behavior remain unchanged.

Focused acceptance covers zero, one, and multiple accepted errors
across every existing error-code category; error-only, mixed-result, partial-
empty, partial-nonempty, and error-free responses; repeated accepted
transitions and replacement reset; and stale, cancelled, detached, replaced-
service, and teardown completion safety. It also proves that no dictionary ID,
error code, backend message, or raw detail is displayed. The focused
command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The full implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-27 adds no test executable or
public/installed interface, so the registered Release baseline remains 109
tests.

Error-detail contents; exact `document_id` navigation and source-dictionary
targeting; columns, icons, additional metadata roles, and other decoration;
match ranges and excerpt presentation; highlighting, ignore-diacritics
transfer, and WebEngine handoff; Preferences enablement, format exclusions,
size/index policy, and persistence; index readiness, visibility, status,
progress, background lifecycle, rebuild, and failure UI; adapters, `.gdfts`,
legacy `_FTS`, index formats, dependencies, builds, and unrelated parity remain
independent surfaces. They are decomposed only; none is selected or ranked. No
public API, DTO, persistence, Core, adapter, index, dependency, or build surface
belongs to P8-FT-27.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, and completed P8-FT-9/P8-FT-11/P8-FT-22
through P8-FT-26. Pinned legacy `fulltextsearch.cc:448-449,499-586` reports only
the aggregate result count and silently contains individual dictionary
failures, so it supplies no error-detail presentation contract. P8-FT-27
therefore presents only the migrated response's bounded aggregate error count.
P8-FT-27 is complete.
No successor after P8-FT-27 is selected or ranked.

### Phase 8 full-text accepted-query activation context (complete)

The independent documentation-only post-P8-FT-27 readiness audit is pinned to
clean migrated revision `e7cac3de2fc190e97ca3340a853f972efab3c528` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-28: privately bind the exact submitted
query's highlighting-relevant context to the accepted response and deliver it
by value with result activation.

P8-FT-28 is dependency-ready because P8-FT-9 accepts only the current terminal
generation, P8-FT-11 retains its complete response, P8-FT-15 already binds
immutable submitted dictionary scope to that response, and P8-FT-13 delivers
the exact result by value on activation. The dialog can retain the submitted
query text and authoritative `ignore_diacritics` value beside the existing
private activation scope. No public API, DTO, persistence, Core, dependency,
adapter, index-format, or build-system change is required.

Each generation captures the exact submitted UTF-8 query text and
`ignore_diacritics` value. Only acceptance of that current terminal generation
makes the captured pair activatable. Result activation delivers the accepted
pair by value with the existing exact result and immutable dictionary scope.
Later composer or projected-query changes cannot rewrite it. Replacement
submission clears the previously accepted pair before starting work. Stale or
cancelled completion, controller detachment, service replacement, and teardown
cannot introduce, restore, or overwrite activation context. Results, ordering,
selection, focus, count, statuses, response ownership, and current scoped
navigation behavior remain unchanged.

Focused acceptance covers distinct submitted and subsequently edited
query text; `ignore_diacritics` true and false; repeated accepted generations;
ordered, authoritative-empty, and absent dictionary scope; exact by-value
result and context delivery; replacement reset; copied-lifetime independence;
and stale, cancelled, detached, replaced-service, and teardown completion
safety. The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The completed implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-28 adds no test executable or
public/installed interface, so the registered Release baseline remains 109
tests.

Consuming the retained context for highlighting or ignore-diacritics transfer,
WebEngine handoff, match-range or excerpt presentation, and exact `document_id`
navigation or source-dictionary targeting remain independent successors.
Columns, icons, additional metadata roles, and other decoration; Preferences
enablement, format exclusions, size/index policy, and persistence; index
readiness, visibility, status, progress, background lifecycle, rebuild, and
failure UI; adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies,
builds, and unrelated parity also remain separate. They are decomposed only;
none is selected or ranked. No navigation, article presentation, WebEngine,
public API, DTO, persistence, Core, adapter, index, dependency, or build surface
belongs to P8-FT-28.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextQuery` and `FullTextResult` contracts, completed
P8-FT-9/P8-FT-11/P8-FT-13/P8-FT-15/P8-FT-16, migrated
`main_window.cpp:5887-5920`, and pinned legacy
`fulltextsearch.cc:499-586`. Legacy activation transfers the search expression
and ignore-diacritics choice with the selected headword, while the migrated
activation intentionally consumes neither today. P8-FT-28 completes only the
accepted private prerequisite; it does not select a consumer. No successor
after P8-FT-28 is selected or ranked.

### Phase 8 full-text accepted-query article-search handoff (complete)

The independent documentation-only post-P8-FT-28 readiness audit is pinned to
clean migrated revision `c6c8a8cc6943de8ef2ecd85a0dc152e0c6459f05` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-29: privately hand the exact accepted
query text to the existing per-tab article-search and Qt WebEngine
presentation after full-text result activation.

P8-FT-29 is dependency-ready because P8-FT-28 delivers the accepted query text
by value with the exact result and immutable dictionary scope, P8-FT-16 opens
that result through current-tab scoped navigation, and MainWindow already owns
per-tab article-search state and literal `QWebEngineView::findText` dispatch.
The handoff remains private to Widgets. No public API, DTO, persistence, Core,
dependency, adapter, index-format, or build-system change is required.

After successful activation and navigation acceptance, the target tab replaces
any prior article-search query and status with the exact accepted UTF-8 query
text. Only successful loading of that activation's nonempty lookup page may
dispatch the same text through Qt WebEngine's existing literal find operation.
The dispatch is bound to the target tab, lookup/presentation generation, and
live `ArticleView`; a stale load or callback cannot highlight or report status
for a later page, another tab, or a replacement view. The resulting match count
uses the existing article-search status behavior and the active tab projects
that private per-tab state through the existing search-in-page controls.

Navigation rejection leaves existing article-search state unchanged. Failed or
empty lookup, tab closure or replacement, facade replacement, and teardown
cannot dispatch or revive the pending full-text query. Ordinary lookup,
internal-link navigation, history replay, session restoration, and manual
search-in-page keep their existing behavior and do not infer full-text context.
The main query edit, full-text dialog state, navigation identity and history,
dictionary scope, article composition, result metadata, selection, focus,
counts, and response statuses remain unchanged.

Focused acceptance covers exact UTF-8 query transfer; replacement of a
prior per-tab article-search query and status; one post-load literal find on the
activated current tab; match and no-match status projection; inactive-tab
isolation; and stale lookup, stale load, stale find callback, closed/replaced
view, failed navigation, failed/empty lookup, facade replacement, and teardown
safety. Existing full-text activation, scoped navigation, article search,
WebEngine interaction, tab, history, and session coverage remains green. The
focused command is
`ctest --preset conan-release -R '^(goldendict_full_text_dialog_smoke|goldendict_webengine_interaction_smoke)$'`
after the Release target has been built. The completed implementation gate is
Linux Release configure/build, full `ctest --preset conan-release`
without an unintended registration delta, clean exact-SCM `conan create` with
the Release Qt WebEngine host profile and packaged consumers, Release install,
and standalone installed C and C++ consumers. P8-FT-29 adds no test executable
or public/installed interface, so the registered Release baseline remains 109
tests.

The authoritative `ignore_diacritics` value remains delivered but unconsumed:
Qt WebEngine's literal find interface exposes no matching diacritics policy,
and P8-FT-29 does not emulate one or claim legacy regular-expression
equivalence. Ignore-diacritics semantics, legacy search-expression/regular-
expression construction, match-range or excerpt rendering, and exact
`document_id` navigation or source-dictionary targeting remain independent
successors. Columns, icons, additional metadata roles, and other decoration;
Preferences enablement, format exclusions, size/index policy, and persistence;
index readiness, visibility, status, progress, background lifecycle, rebuild,
and failure UI; adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies,
builds, and unrelated parity also remain separate. They are decomposed only;
none is selected or ranked.

Evidence is completed P8-FT-16/P8-FT-28, migrated
`full_text_search_dialog.h/.cpp`, `main_window.cpp:5887-5920,7556-7730`, the
existing full-text-dialog and WebEngine interaction smokes, and pinned legacy
`fulltextsearch.cc:499-586`. Legacy activation transfers a regular expression
and ignore-diacritics choice into article presentation; the migrated tree has
only a private literal per-tab article-search boundary ready independently.
P8-FT-29 completes that bounded handoff only. No successor after P8-FT-29 is
selected or ranked.

### Phase 8 full-text accepted-completion notification (complete)

The independent documentation-only post-P8-FT-29 readiness audit is pinned to
clean migrated revision `d63d6f8cfcdd305166c3974a95811046344d30e2` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-30: privately notify the user once when
a generation-current full-text response is accepted. P8-FT-30 is complete.

P8-FT-30 uses the P8-FT-9 boundary, which already accepts at most one
terminal response for the current generation, rejects stale and cancelled
completions, and restores the dialog to idle only after acceptance. The dialog
can invoke the existing Widgets-owned `QApplication::beep()` notification at
that boundary without inspecting or changing response contents. No public API,
DTO, persistence, Core, dependency, adapter, index-format, or build-system
change is required.

Every generation-current accepted response produces exactly one beep,
including nonempty success, conclusive empty, partial-empty, partial-nonempty,
error-only, and mixed-result responses. Initial state, submission, progress,
explicit cancellation, stale or duplicate completion, controller detachment,
service or facade replacement, dialog destruction, and MainWindow teardown
produce none. A later accepted replacement generation may notify once. The
notification changes no results, ordering, selection, focus, activation,
counts, statuses, navigation, article-search state, or response semantics.

Focused acceptance covers each accepted response shape, repeated accepted
generations, exactly-once delivery, and suppression for initial, pending,
cancelled, stale, duplicate, detached, replaced-service, destroyed-dialog, and
teardown paths. The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The full implementation gate is Linux Release
configure/build, full `ctest --preset conan-release` without an unintended
registration delta, clean exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers, Release install, and standalone
installed C and C++ consumers. P8-FT-30 adds no test executable or installed
surface, so the registered Release baseline remains 109 tests.

Ignore-diacritics semantics and legacy regular-expression equivalence; match-
range or excerpt rendering; exact `document_id` navigation and source-
dictionary targeting; columns, icons, additional metadata roles, and other
decoration; Preferences enablement, format exclusions, size/index policy, and
persistence; index readiness, visibility, status, progress, background
lifecycle, rebuild, and failure UI; adapters, `.gdfts`, legacy `_FTS`, index
formats, dependencies, builds, and unrelated parity remain independent
surfaces. They are decomposed only; none is selected or ranked. No public API,
DTO, persistence, Core, adapter, index, dependency, or build surface belongs
to P8-FT-30.

Evidence is completed P8-FT-9 and the extended focused dialog tests, migrated
`full_text_search_dialog.cpp:203-253`, and pinned legacy
`fulltextsearch.cc:547-579`, where the dialog beeps after all outstanding
requests reach terminal completion regardless of retained results. P8-FT-30
completes only the migrated accepted-completion notification contract. No
successor after P8-FT-30 is selected or ranked.

### Phase 8 full-text idle dialog dismissal (complete)

The independent documentation-only post-P8-FT-30 readiness audit is pinned to
clean migrated revision `17b124f79f0dfb1a794b9dabbba4e87804dd885b` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-31: privately dismiss the modeless full-
text dialog through its existing Cancel control while no request is active.

P8-FT-31 is dependency-ready because P8-FT-8 already owns the modeless dialog,
singleton MainWindow lifetime, close/destruction path, and safe controller
detach, while P8-FT-9 already distinguishes active request cancellation from
idle state. The existing control can therefore dispatch either behavior from
Widgets without changing Core or an installed interface.

The Cancel control remains available while idle. Activating it initially,
after an accepted completion, or after explicit request cancellation closes
the dialog through the completed P8-FT-8 destruction path, clears MainWindow's
guarded ownership, and permits a later action trigger to create a fresh dialog.
While a generation is active, the same control retains P8-FT-9 semantics: it
cancels only that request, restores idle state, and leaves the dialog open.
Submission, accepted completion, stale or duplicate completion, service or
facade replacement, controller detachment, and teardown cannot themselves
dismiss the dialog. Window-manager close behavior, results, notification,
navigation, article-search, and response semantics remain unchanged.

Focused acceptance covers initial idle dismissal; dismissal after accepted
completion and after active cancellation; active cancellation without
dismissal; repeated commands; MainWindow ownership clearing and clean reopen;
and stale, detached, service-replacement, destroyed-dialog, and teardown
safety. The focused command is
`ctest --preset conan-release -R '^(full_text_search_dialog_test|goldendict_full_text_dialog_smoke)$'`
after the Release targets have been built. The full implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-31 adds no test executable or
installed surface, so the registered Release baseline remains 109 tests.

Ignore-diacritics semantics and legacy regular-expression equivalence; match-
range or excerpt rendering; exact `document_id` navigation and source-
dictionary targeting; columns, icons, additional metadata roles, and other
decoration; Preferences enablement, format exclusions, size/index policy, and
persistence; index readiness, visibility, status, progress, background
lifecycle, rebuild, and failure UI; adapters, `.gdfts`, legacy `_FTS`, index
formats, dependencies, builds, and unrelated parity remain independent
surfaces. They are decomposed only; none is selected or ranked. No public API,
DTO, persistence, Core, adapter, index, dependency, or build surface belongs
to P8-FT-31.

Evidence is completed P8-FT-8/P8-FT-9, migrated
`full_text_search_dialog.cpp:184-218,285-289`, the focused dialog and MainWindow
full-text tests, and pinned legacy `fulltextsearch.cc:582-592`, where Cancel
stops active work without closing and otherwise saves and dismisses the dialog.
P8-FT-31 completes only the migrated idle-dismissal contract. No successor
after P8-FT-31 is selected or ranked.

### Phase 8 full-text dialog geometry persistence prerequisite (complete)

The independent documentation-only post-P8-FT-31 readiness audit is pinned to
clean migrated revision `4bc3184e578d98793d944a6f1eb6c6fd23f637d3` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-32: the transport-neutral persistence
prerequisite for the full-text dialog's opaque geometry.

P8-FT-32 is complete. It extends `CoreConfiguration` with one independently
optional opaque full-text dialog geometry value. Missing data remains empty.
Canonical current configuration uses the established binary-value encoding
pattern, and legacy migration maps exactly
`preferences/fullTextSearch/dialogGeometry`. Core bounds decoded data at 64
KiB and atomically rejects duplicate, malformed, or oversized recognized input.
It stores bytes without interpreting Qt geometry. This follows the existing
main-window geometry boundary and changes no package dependency or build
surface. The added `CoreConfiguration` field is an authorized installed/public
ABI expansion identified by Conan's exact SCM and package revisions; it changes
no runtime interface.

The leaf deliberately stops before Widgets integration. It does not restore
geometry when the dialog is created, capture it on idle dismissal or a
window-manager close, or trigger configuration persistence. P8-FT-31
dismissal/cancellation behavior and every request, response, presentation,
activation, navigation, article-search, service-replacement, and teardown
contract remain unchanged. A later private Widgets capture/restore connection
is decomposed but remains unselected and unranked.

Focused acceptance covers empty defaults, current round-trip and canonical
save, valid legacy migration, duplicate/malformed/oversized rejection, the
exact 64 KiB boundary, atomic failure preservation, and installed C and C++
consumer access to the expanded DTO. The focused command is
`ctest --preset conan-release -R '^application_service_test$'` after the
Release target has been built. The full implementation gate is Linux Release
configure/build, full `ctest --preset conan-release` without an unintended
registration delta, clean exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers, Release install, and standalone
installed C and C++ consumers. The registered Release baseline remains 109
tests because P8-FT-32 adds no test executable.

Evidence is the existing bounded `CoreConfiguration::main_window_geometry`
current/legacy contract and `application_service_test`, plus pinned legacy
`config.hh:156-181`, `config.cc:1008-1044,1990-2027`, and
`fulltextsearch.cc:195-221,387-399`. Widgets geometry integration;
ignore-diacritics and regular-expression equivalence; excerpts and match
ranges; exact document/source targeting; decoration; Preferences/index policy;
index lifecycle UI; adapters/index formats; dependencies, builds, and
unrelated parity remain separately decomposed and unranked. P8-FT-32 completes
only the persistence prerequisite. No successor after P8-FT-32 is selected or
ranked.

### Phase 8 full-text dialog geometry Widgets connection (complete)

The independent documentation-only post-P8-FT-32 audit is pinned to clean
migrated revision `6ff78e84f2b5f0395283a00f0632174672828625` and unchanged
clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking all remaining
full-text parity gaps and prerequisites without advance ranking, it selects
exactly P8-FT-33: connect the persisted bounded opaque geometry to the existing
modeless full-text dialog. P8-FT-33 is complete.

Core already owns bounded atomic persistence and exact legacy migration, the
dialog owns Qt geometry, and the composition root owns configuration saves.
The shared-library/GUI rule therefore keeps Core opaque: Widgets now attempts one
restore for a nonempty value at new-dialog creation, preserves default geometry
on absence or Qt rejection without rewriting the stored bytes, and adds no
placement policy. Idle Cancel and window-manager close capture exact geometry
before destruction for the next established atomic save. Active Cancel remains
cancellation-only and performs no capture or save. Completion, activation,
replacement, service replacement, detachment, and teardown do not
independently persist geometry.

Acceptance extends the existing dialog test and full-text application smoke
with absent, valid, and invalid restore; exact capture on both idle dismissal
paths; reconstruction round-trip; active-cancellation exclusion; and existing
workflow/lifecycle regression coverage. The focused Release command is
`ctest --preset conan-release -R '^(full_text_search_dialog_test|goldendict_full_text_dialog_smoke)$'`
after the Release build. The full implementation gate is Linux Release
configure/build, exactly 109 registered tests and full
`ctest --preset conan-release`, clean committed exact-SCM `conan create` with
the Release Qt WebEngine host profile and packaged consumers, Release install,
and standalone installed C and C++ consumers. P8-FT-33 adds no installed
interface, executable, dependency, or test registration; installed consumers
remain unchanged and source-compatible but are retained as the stronger gate.

The implementation changes only the bounded private Widgets/application
connection, its existing tests, and these governing documents. Screen/topology
normalization, placement fallback, other dialog state, query semantics,
excerpts, exact document/source targeting, decoration, Preferences/index
policy, index lifecycle, adapters/index formats, dependencies, builds, and
unrelated parity remain independently decomposed, unselected, and unranked.
No successor after P8-FT-33 is selected or ranked.
The Release registration baseline remains exactly 109 tests.

### Phase 8 full-text dialog minimum-size contract (complete)

The independent documentation-only post-P8-FT-33 audit is pinned to clean
migrated revision `eb4911bd8fd82402f0dc5b861da65aecb0927633` and unchanged
clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking all remaining
full-text parity gaps and prerequisites without advance ranking, it selects
exactly P8-FT-34: preserve the legacy full-text dialog's private Widgets
minimum size.

P8-FT-7 already owns the modeless dialog and its layout, while P8-FT-33 keeps
Qt geometry interpretation in Widgets. Pinned legacy
`fulltextsearch.ui:13-18` therefore supplies a dependency-ready exact contract:
every newly constructed dialog has a minimum width of 430 and minimum height
of 450 logical pixels. Direct resizing and restored geometry cannot leave the
dialog below either bound. Larger valid restored geometry, absent or rejected
geometry, capture, idle dismissal, active cancellation, and all other existing
workflow and lifecycle behavior remain unchanged.

Acceptance extends `full_text_search_dialog_test` with the exact minimum,
undersized direct-resize and restored-geometry clamping, larger valid geometry
preservation, and P8-FT-33 restoration/lifecycle regressions. The focused
Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release build. The full implementation gate is Linux Release configure/build,
exactly 109 registered tests and full `ctest --preset conan-release`, clean
committed exact-SCM `conan create` with the Release Qt WebEngine host profile
and packaged consumers, Release install, and standalone installed C and C++
consumers. P8-FT-34 adds no installed interface, executable, dependency, or
test registration; installed consumers remain unchanged and source-compatible
but are retained as the stronger package gate.

The completed implementation sets the exact private Widgets minimum before the
existing one-time geometry restoration and extends only the existing focused
dialog test. It changes no persistence or public surface. Default or initial
size, maximum size,
aspect ratio, screen/topology normalization, placement fallback, DPI policy
beyond Qt logical sizing, other dialog state, query semantics, excerpts, exact
document/source targeting, decoration, Preferences/index policy, index
lifecycle, adapters/index formats, dependencies, builds, and unrelated parity
remain independently decomposed, unselected, and unranked. No successor after
P8-FT-34 is selected or ranked. Implementation must stop before editing on
ref/worktree drift, legacy dirtiness, ambiguous sizing evidence or acceptance
behavior, an architectural choice requiring HTTP GET policy, or scope
expansion; the same pinned-state checks must pass again before commit and push.


Phase 6 per-format full-text support follows that contract, then the Phase 8
workflow and its Preferences controls. Audio is the next foundation candidate,
but typed resources do not yet settle ownership between WebEngine delivery, a
private audio service, optional playback backends, and presentation. System
proxy, credentials, SOCKS5, and HTTP GET require transport and security policy
beyond accepted credential-free HTTP CONNECT. Phase 9 translations, help,
styles, tray/autostart, hotkeys, scan/clipboard, audio playback, and updates
retain their asset, desktop, platform, release, or ownership prerequisites.
Their Windows-native Phase 10 behavior remains downstream of the corresponding
cross-platform contracts.

  Concrete local formats remain private to the core library; the executable
  composition root may reference only justified optional integration modules.
- Provide a compatible migration path for legacy configuration, dictionary
  groups, history, and favorites. Do not silently discard these user-owned
  states. Implementation-generated dictionary indexes may be rebuilt
  automatically instead of preserving their binary representation.

Gate: the Linux application can load, index, search, and render the approved
dictionary set through the intended user workflows.

### Phase 8 full-text dialog initial-size contract (complete)

The independent documentation-only post-P8-FT-34 audit was pinned to clean
migrated revision `7ff83942ab6b71dda1a0a798eb0dcbe8ef1ccd24` and unchanged
clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking all remaining
full-text parity gaps without advance ranking, it selected exactly P8-FT-35:
preserve the legacy full-text dialog's private Widgets initial logical size.

P8-FT-7 owns the dialog and layout, P8-FT-33 owns one-time restoration, and
P8-FT-34 owns the minimum. Pinned legacy `fulltextsearch.ui:5-12` supplies the
dependency-ready contract: absent or Qt-rejected stored geometry initializes
every new dialog to exactly 492 by 593 logical pixels after layout construction.
Widgets sets that size after the 430-by-450 minimum and before restore. Valid
restored geometry still wins, undersized restored geometry remains clamped,
and later resizing, persistence, and lifecycle behavior remain unchanged.

The completed implementation extends `full_text_search_dialog_test` only,
covering exact absent and rejected initialization, valid larger restoration,
undersized restoration
clamping, later direct resizing, and P8-FT-33/P8-FT-34 regressions. The focused
Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
implementation gate remains Linux Release configure/build, exactly 109 tests,
full Release CTest, clean committed exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. No installed interface, executable,
dependency, or test registration changes; consumers remain unchanged and
source-compatible.

Help integration, maximum/aspect constraints, screen/topology normalization,
placement fallback, DPI policy beyond Qt logical sizing, other dialog state,
query semantics, excerpts, document/source targeting, decoration,
Preferences/index policy, index lifecycle, adapters/index formats,
dependencies, builds, and unrelated parity remain excluded and unranked. No
successor after P8-FT-35 is selected or ranked. Implementation must stop on
ref/worktree drift, legacy dirtiness, ambiguous sizing evidence or acceptance,
an architectural choice requiring HTTP GET policy, or scope expansion; pinned
checks repeat before commit and push.

### Phase 8 full-text Help activation intent (complete)

The independent documentation-only post-P8-FT-35 audit is pinned to migrated
revision `89019d32113e4c68985756aa7a04e158951e1893`, its identical upstream and
live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After auditing the governing
documents and current and pinned legacy code without advance ranking, it
selects exactly P8-FT-36: the private full-text Help activation intent.

Pinned legacy `fulltextsearch.ui:250-255` and
`fulltextsearch.cc:300-309,676-680` provide the bounded parity contract: one
`Help` button and one F1 action with `Qt::WidgetWithChildrenShortcut` produce
the same dialog help request. The pre-P8-FT-36 private dialog had neither.
Under the shared-library/GUI boundary, P8-FT-36 adds those two private Widgets
activation paths: `fullTextHelpButton` with exact text `Help`,
`fullTextHelpAction` with
exact F1 shortcut and `Qt::WidgetWithChildrenShortcut`, and one argument-free
private `HelpRequested()` signal. Each activation emits exactly once, leaves
the modeless dialog open, and does not submit or cancel a search, alter
query/result/accepted-response state or selection, impose focus behavior beyond
normal Qt activation, capture geometry, or mutate configuration.

Actual help content, help-window/engine construction, URLs or dispatch,
composition-root consumption, Help-menu changes, embedded documentation, HTTP
GET policy, Core/public contracts, dependencies, installed interfaces, and all
other full-text or migration parity remain excluded and unranked. The completed
focused acceptance extends `full_text_search_dialog_test` for exact button,
shortcut/context, single-emission, repeated activation, and idle/active state,
lifecycle, and geometry regressions.

The full implementation gate is Linux Release configure/build, exactly 109
registered tests, full Release CTest, clean committed exact-SCM `conan create`
with the
Release Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. There is no test registration,
installed header, DTO, ABI, dependency, CMake export, or Conan requirement
change; both consumers remain unchanged and source-compatible. No successor
after P8-FT-36 is selected or ranked. Implementation must stop on ref/worktree
drift, legacy dirtiness, ambiguous help evidence or acceptance, selection of a
help destination or transport, a public/Core or composition-root contract,
dependency or installed-surface change, an architectural decision requiring
HTTP GET policy, or scope expansion.

### Phase 8 full-text Search button default policy (complete)

The independent documentation-only post-P8-FT-36 audit is pinned to clean
migrated revision `ca6f206f1913a7941d8dcf3599704353e27e3c4e`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It audits all remaining full-text
parity gaps without advance ranking and selects exactly one smallest
dependency-ready leaf, P8-FT-37: preserve the private Search button's explicit
default and non-auto-default policy.

P8-FT-37 is dependency-ready because the completed dialog shell and request
workflow already own `fullTextSearchButton`, while P8-FT-31 fixes its active
and idle behavior. Pinned legacy `fulltextsearch.ui:204-214` sets
`default == true` and `autoDefault == false`. The pre-P8-FT-37
`full_text_search_dialog.cpp:133-136` set the explicit default only, and the
focused test did not cover either property. Under the shared-
library/GUI boundary, this remains private Widgets behavior.

The completed implementation keeps `isDefault() == true`, sets
`autoDefault() == false`, and preserves both properties across construction,
idle restoration, submission, completion, and active cancellation. It does
not independently submit or cancel, close the dialog, change focus, or alter
query, response, model, selection, geometry, configuration, or Help behavior.
Tab order, initial/transferred focus, broader Return/Enter dispatch, other
buttons' default policy, help consumption, query and result semantics,
navigation, index lifecycle/UI, Preferences/index policy, adapters and index
formats, dependencies, builds, and unrelated parity remain separately
decomposed, unselected, and unranked. No successor after P8-FT-37 is selected
or ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` with exact
button identity, both property values across the specified transitions, and
Search/Cancel/Help regressions. The focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
implementation gate remains Linux Release configure/build, exactly 109
registered tests, full Release CTest, clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers, Release install, and standalone installed C and C++ consumers. No
installed header, DTO, ABI, dependency, CMake export, Conan requirement,
executable, or test registration changes; both consumers remain unchanged and
source-compatible.

Implementation must stop on ref/worktree drift, legacy dirtiness, ambiguous
default-button evidence or acceptance semantics, a broader focus, tab-order,
or keyboard-policy decision, public/Core or composition-root expansion,
dependency or installed-surface change, an architectural decision requiring
HTTP GET policy, or scope expansion. The audit itself is documentation-only,
so compiled verification is intentionally skipped; exact four-file scope,
cross-document consistency, Phase terminology, successor language, and
`git diff --check` are its verification gate.

### Phase 8 full-text dialog tab sequence (complete)

The independent documentation-only post-P8-FT-37 audit is pinned to clean
migrated revision `9be9e1d8928c25f312b075fdda1674bb44d96013`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly P8-FT-38:
restore the private full-text dialog's legacy-authenticated keyboard tab
sequence.

Pinned legacy `fulltextsearch.ui:274-285` defines this consecutive forward
chain: `fullTextQueryText`, `fullTextSearchResults`,
`fullTextUseMaximumWordDistance`, `fullTextMaximumWordDistance`,
`fullTextQueryMode`, `fullTextUseMaximumArticles`,
`fullTextMaximumArticlesPerDictionary`, `fullTextMatchCase`,
`fullTextSearchButton`, and `fullTextCancelButton`. Current Widgets has all
mapped controls but no equivalent dialog-level order. P8-FT-7, P8-FT-31, and
P8-FT-37 provide the dialog, Cancel lifecycle, and Search default-policy
prerequisites. The shared-library/GUI boundary keeps this policy private to
Widgets and leaves Core, composition-root, public, and installed contracts
unchanged.

The completed implementation establishes the chain once at dialog construction
and keeps it stable through idle, submission, completion, and active
cancellation, including while controls are temporarily disabled.
Initial or transferred focus, focus policies, traversal endpoints and
wraparound, the placement of Ignore Diacritics, Ignore Word Order, Help, or
other omitted controls, Return/Enter behavior, shortcuts, button default
policy, search/result behavior, index lifecycle/UI, Preferences/index policy,
adapters/index formats, dependencies/builds, and unrelated parity remain
excluded and unranked. No successor after P8-FT-38 is selected or ranked.

Completed focused acceptance extends only
`full_text_search_dialog_test` with exact forward-chain inspection across the
stated transitions and regressions for initial query focus, Search default
policy, Cancel, Help, and request lifecycle. The focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
gate remains Linux Release configure/build, exactly 109 registered tests, full
Release CTest, clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers, Release install, and unchanged
standalone installed C and C++ consumers. No executable, registration,
installed interface, dependency, export, or Conan requirement changes.

No successor after P8-FT-38 is selected or ranked. Omitted-control order, focus
policies, endpoints, wraparound, initial/transferred focus, key dispatch,
shortcuts, public/Core or composition-root expansion, dependencies, installed
surfaces, and unrelated parity remain separately reviewed and unranked.

### Phase 8 full-text result-count minimum height (completed)

The independent documentation-only post-P8-FT-38 audit was pinned to clean
migrated revision `55777c188049b6de1d84a85db2b2db2b3d71a1e8`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selected exactly P8-FT-39:
preserve the private full-text result-count label's explicit minimum height.

Pinned legacy `fulltextsearch.ui:103-115` gives `articlesFoundLabel` a minimum
height of 21 logical pixels. Current `full_text_search_dialog.cpp:82-86`
created the mapped `fullTextArticlesFoundLabel` without that minimum.
Completed P8-FT-17 owns its count presentation, while P8-FT-34/P8-FT-35 own
the enclosing dialog's minimum and initial geometry. The shared-library/GUI
boundary keeps this correction in private Widgets and leaves Core,
composition-root, public, and installed contracts unchanged.

The completed implementation retains
`fullTextArticlesFoundLabel->minimumHeight() == 21` after construction and
through idle, submission, accepted completion, active cancellation,
replacement, and service/controller lifecycle transitions. Count text and
visibility, response statuses, progress behavior, layout ownership, sizing
above the minimum, and established dialog geometry remain unchanged.

Wording/localization, width policy, fixed or maximum height, font/style/DPI
policy, count/progress rearrangement, response-status layout, progress
alignment, dialog geometry, focus/tab/key behavior, search/results, index
lifecycle/UI, Preferences, adapters/index formats, dependencies/builds, and
unrelated parity are excluded and unranked. No successor after P8-FT-39 is
selected or ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` across
the stated transitions. The focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
gate remains Linux Release configure/build, exactly 109 registered tests, full
Release CTest, clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers, Release install, and unchanged
standalone installed C and C++ consumers. No executable, registration,
installed interface, dependency, export, or Conan requirement changes.

The implementation changes only private dialog construction, its existing
focused test, and the four governing documents. The Release registration
baseline remains exactly 109 tests; no successor after P8-FT-39 is selected or
ranked.

### Phase 8 full-text progress-bar alignment (complete)

The independent documentation-only post-P8-FT-39 audit is pinned to clean
migrated revision `6dc40e40038395ecb8f1c912aab720ada078ee93`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly P8-FT-40:
preserve the private full-text search progress bar's explicit centered
alignment.

Pinned legacy `fulltextsearch.ui:117-128` gives `searchProgressBar` the
explicit alignment `Qt::AlignCenter`. Current `full_text_search_dialog.cpp`
creates the mapped `fullTextSearchProgress`, preserves that alignment, and
sets its indeterminate range. The completed private dialog and request workflow already own
the widget and its lifecycle. The shared-library/GUI boundary keeps this
correction in private Widgets and leaves Core, composition-root, public, and
installed contracts unchanged.

The completed implementation retains
`fullTextSearchProgress->alignment() == Qt::AlignCenter` after construction
and through idle, submission, generation-current accepted completion, active
cancellation, replacement, and service/controller lifecycle transitions.
Existing indeterminate range, visibility, request lifecycle,
result/count/status presentation, layout ownership, dialog geometry, search
semantics, and cancellation behavior remain unchanged.

Progress text/format and text visibility, value/range policy, orientation,
inversion, style/animation, size policy, layout rearrangement, indexing
progress UI/lifecycle, platform-specific styling, public/Core contracts,
dependencies/builds, and unrelated parity are excluded and unranked. No
successor after P8-FT-40 is selected or ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` across
the stated transitions while retaining progress range/visibility and workflow
regressions. The focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
gate remains Linux Release configure/build, exactly 109 registered tests, full
Release CTest, clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers, Release install, and unchanged
standalone installed C and C++ consumers. No executable, registration,
installed interface, dependency, export, or Conan requirement changes.

The implementation stops on ref/worktree drift, legacy dirtiness, ambiguous
alignment evidence or acceptance semantics, any broader progress, layout, or
style choice, public/Core or composition-root expansion, dependency or
installed-surface change, an architectural decision requiring HTTP GET policy,
or scope expansion. Its gate is exact production and focused-test scope,
exactly four governing documentation updates, cross-document consistency,
Phase terminology, successor language, and the full Release, install,
consumer, and exact-SCM package verification described above.

### Phase 8 full-text result-count/progress row (complete)

The independent documentation-only post-P8-FT-40 audit is pinned to clean
migrated revision `0b6a081699947168959884a940870d8a741c1d74`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly P8-FT-41:
restore the private result-count/progress horizontal row.

Pinned legacy `fulltextsearch.ui:102-129` places `articlesFoundLabel` first and
`searchProgressBar` second in one `QHBoxLayout`. Current
`full_text_search_dialog.cpp` adds the mapped private
`fullTextArticlesFoundLabel` and `fullTextSearchProgress` separately to the
dialog's vertical layout, with response-status widgets between them.
P8-FT-17, P8-FT-39, and P8-FT-40 already own count presentation, minimum
height, and progress alignment. The shared-library/GUI boundary keeps this
correction in private Widgets and leaves Core, composition-root, public, and
installed contracts unchanged.

The completed implementation places one private horizontal layout in the
enclosing vertical dialog layout, with the unique direct dialog children
`fullTextArticlesFoundLabel` first and `fullTextSearchProgress` second. The
relationship remains stable through construction, idle, submission,
generation-current accepted completion, active cancellation, replacement,
service replacement, and controller detachment. Count text and minimum height,
progress alignment/range/visibility, response-status order and behavior,
request lifecycle, dialog geometry, focus chain, search semantics, and
cancellation behavior remain unchanged.

Spacing, margins, stretch factors, size policies, widths, status-widget
rearrangement, broader layout redesign, progress behavior/style, geometry,
keyboard behavior, indexing lifecycle/UI, Preferences, adapters/index formats,
dependencies/builds, public/Core or composition-root changes, and unrelated
parity are excluded and unranked. No successor after P8-FT-41 is selected or
ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` to
prove the unique horizontal row, exact label-then-progress order, enclosing-
layout attachment, unchanged widget properties, and lifecycle regressions. The
focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
gate remains Linux Release configure/build, exactly 109 registered tests, full
Release CTest, clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers, Release install, and unchanged
standalone installed C and C++ consumers. No executable, registration,
installed interface, dependency, export, or Conan requirement changes.

The implementation stops on ref/worktree drift, legacy dirtiness, ambiguous
layout evidence or acceptance semantics, any spacing, style, or broader layout
choice, public/Core or composition-root expansion, dependency or installed-
surface change, an architectural decision requiring HTTP GET policy, discovery
of another required file, or scope expansion. The completed implementation
changes only private dialog construction, its existing focused test, and the
four governing documents; its gate is exact scope, cross-document consistency,
Phase terminology, successor language, and the full Release, install, consumer,
and exact-SCM verification described above.

### Phase 8 full-text button-row spacer sequence (complete)

The independent documentation-only post-P8-FT-41 audit is pinned to clean
migrated revision `2a1f8e6dfbcb72368ea8e7a1c27179bb5ff3b5fb`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly P8-FT-42:
restore the private button-row spacer sequence.

Pinned legacy `fulltextsearch.ui:190-270` places expanding horizontal spacers
before Search, between Search and Cancel, between Cancel and Help, and after
Help in one `QHBoxLayout`. Current `full_text_search_dialog.cpp` preserves the
three mapped private buttons in that order but adds stretches only before
Search and after Help. P8-FT-36 through P8-FT-38 already own Help intent,
Search default policy, and the exact tab sequence. The shared-library/GUI
boundary keeps this correction in private Widgets and leaves Core,
composition-root, public, and installed contracts unchanged.

The completed implementation retains one private horizontal button layout with
the exact item sequence expanding horizontal spacer,
`fullTextSearchButton`, expanding horizontal spacer,
`fullTextCancelButton`, expanding horizontal spacer, `fullTextHelpButton`, and
expanding horizontal spacer. The three buttons remain unique direct dialog
children, and the relationship remains stable through construction, idle,
submission, generation-current accepted completion, active cancellation,
replacement, service replacement, and controller detachment. Button identity,
text, order and parentage, Search default policy, tab chain, Help intent,
Cancel lifecycle, request/response behavior, and geometry remain unchanged.

Exact spacer size hints, stretch factors, margins, layout spacing, button sizes
or size policies, button reordering, broader layout/style work, indexing
lifecycle/UI, Preferences, adapters/index formats, dependencies/builds,
public/Core or composition-root changes, HTTP GET policy, and unrelated parity
are excluded and unranked. No successor after P8-FT-42 is selected or ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` to
prove the unique button row, exact seven-item spacer/button sequence,
horizontal expansion of all four spacers, enclosing-layout attachment,
unchanged button contracts, and lifecycle regressions. The focused Release
command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
gate remains Linux Release configure/build, exactly 109 registered tests, full
Release CTest, clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers, Release install, and unchanged
standalone installed C and C++ consumers. No executable, registration,
installed interface, dependency, export, or Conan requirement changes.

Implementation must stop on ref/worktree drift, legacy dirtiness, ambiguous
layout evidence or acceptance semantics, any exact spacer-size, stretch-factor,
margin, spacing, style, or broader layout choice, public/Core or composition-
root expansion, dependency or installed-surface change, an architectural
decision requiring HTTP GET policy, discovery of another required file, or
scope expansion. This selection audit is documentation-only, so compiled
verification is intentionally skipped; exact four-file scope, cross-document
consistency, Phase terminology, successor neutrality, and `git diff --check`
are its verification gate.

### Phase 8 full-text Search group-box boundary (complete)

The independent documentation-only post-P8-FT-42 audit is pinned to clean
migrated revision `a491c72f7d1e2c2fa4b37151addccabdbf1a9b9d`, its identical
upstream and fresh live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly P8-FT-43:
restore the private full-text Search group-box boundary.

Pinned legacy `fulltextsearch.ui:23-96` places the query field and search-option
controls in one `QGroupBox` titled exactly `Search`. Current
`full_text_search_dialog.cpp:65-70` instead adds the mapped private
`FullTextQueryComposer` directly to the dialog's enclosing vertical layout.
P8-FT-1 through P8-FT-42 already own query composition and control behavior,
the exact tab sequence, result presentation, surrounding layout, geometry, and
request lifecycle. The shared-library/GUI boundary keeps this presentation-only
correction in private Widgets and leaves Core, composition-root, public, and
installed contracts unchanged.

The completed implementation adds exactly one private `QGroupBox` titled
`Search` between the enclosing dialog layout and the unique existing
`FullTextQueryComposer`. The composer is directly contained by that group box,
and the relationship remains stable through construction, idle, submission,
generation-current accepted completion, active cancellation, replacement,
service replacement, and controller detachment. Query values, labels, control
ordering and enablement, composition semantics, focus and tab chain,
submission and response behavior, result-count/progress row, statuses, button
row, geometry, and lifecycle behavior remain unchanged.

Group-box margins, spacing, size policy, alignment, styling, checkability,
flatness, mnemonic policy, broader composer-layout parity, indexing lifecycle
or UI, Preferences, adapters/index formats, dependencies/builds, public/Core or
composition-root changes, HTTP GET policy, and unrelated parity are excluded
and unranked. No successor after P8-FT-43 is selected or ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` to prove
the unique group box, exact title, unique directly contained composer,
attachment to the enclosing dialog layout, unchanged query-control contracts,
and lifecycle regressions. The focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
gate remains Linux Release configure/build, exactly 109 registered tests, full
Release CTest, Release install, packaged consumers, unchanged standalone
installed C and C++ consumers, and clean committed exact-SCM creation with:

```sh
conan create . --build=missing \
  -pr:h=profiles/qt-webengine -pr:b=default \
  -s:h build_type=Release
```

No executable, registration, installed interface, dependency, export, or Conan
requirement changes.

P8-FT-43 is complete. The implementation changes only private dialog
construction, its existing focused test, and these four governing documents.

Implementation must stop on ref/worktree drift, legacy dirtiness, ambiguous
mapping or acceptance semantics, any broader grouping, layout, style, or
mnemonic choice, public/Core or composition-root expansion, dependency or
installed-surface change, an architectural decision requiring HTTP GET policy,
discovery of another required file, or scope expansion. Its gate is exact
six-file scope, cross-document consistency, Phase terminology, successor
neutrality, and the full Release, install, consumer, and exact-SCM verification
described above.

### Phase 8 full-text ignore-words-order label parity (complete)

The independent documentation-only post-P8-FT-43 audit is pinned to clean
migrated revision `f514fde0289fdb9d8139df03c0c4e81c5c6545a7`, its identical
upstream and fresh live remote, and clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly P8-FT-44:
restore the private full-text ignore-words-order label.

Pinned legacy `fulltextsearch.ui:83-89` gives the existing checkbox the exact
translatable text `Ignore words order`; before P8-FT-44,
`full_text_query_composer.cpp:89-92` used `Ignore word order` for the mapped
unique private `fullTextIgnoreWordOrder` checkbox. P8-FT-1 through P8-FT-43
supply its query, persistence, control, tab, layout, and request-lifecycle
prerequisites. The shared-library/GUI boundary keeps this text-only correction
in private Widgets and leaves Core, composition-root, public, and installed
contracts unchanged.

The implementation changes only that checkbox's text to exactly
`Ignore words order`. Identity, parentage, checked/enabled state, ordering,
focus/tab behavior, mode-dependent behavior, query composition, submission,
responses, geometry, and lifecycle behavior remain unchanged. The exact text
stays stable through construction, mode changes, option toggles, composition,
submission, generation-current accepted completion, active cancellation,
replacement, service replacement, and controller detachment.

Layout, mnemonic policy, translation-catalog work, other labels, grammar
modernization, indexing lifecycle/UI, Preferences, adapters/index formats,
dependencies/builds, public/Core or composition-root changes, HTTP GET policy,
and unrelated parity are excluded and unranked. No successor after P8-FT-44
is selected or ranked.

Completed focused acceptance extends only `full_text_query_composer_test` to
prove the unique checkbox's exact text and retain its identity, state,
enablement, composition semantics, and relevant control-transition regressions.
The focused Release command is
`ctest --preset conan-release -R '^full_text_query_composer_test$'`. The full
gate remains Linux Release configure/build, exactly 109 registered tests, full
Release CTest, Release install, packaged consumers, unchanged standalone
installed C and C++ consumers, and clean committed exact-SCM creation with:

```sh
conan create . --build=missing \
  -pr:h=profiles/qt-webengine -pr:b=default \
  -s:h build_type=Release
```

P8-FT-44 adds no executable, test registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. The implementation
changes only the private composer, its existing focused test, and these four
governing documents.

P8-FT-44 is complete. No successor is selected or ranked.

Implementation must stop on ref/worktree drift, legacy dirtiness, ambiguous
text mapping or acceptance semantics, any second label or behavior change,
layout, mnemonic, translation-catalog, public/Core or composition-root
expansion, dependency or installed-surface change, an architectural decision
requiring HTTP GET policy, discovery of another required file, or scope
expansion. Its gate is exact six-file scope, cross-document consistency, Phase
terminology, successor neutrality, and the full Release, install, consumer,
and exact-SCM verification described above.

### Phase 8 full-text query-mode label parity (complete)

The documentation-only post-P8-FT-44 audit is pinned to clean migrated
revision `c771e6a47bf8fda61d57dd241d751c6ead8ce454`, its identical upstream and
fresh live remote, and clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly P8-FT-45:
restore the private full-text query-mode label.

Pinned legacy `fulltextsearch.ui:41-53` associates exact translatable text
`Mode:` with the unique search-mode selector. Before P8-FT-45,
`full_text_query_composer.cpp:60-74,131-134` maps that selector to the unique
private `fullTextQueryMode` combo box but labeled its `QFormLayout` row `Mode`.
P8-FT-1 through P8-FT-44 supply its persistence, four modes, query composition,
control, layout, focus/tab, and request-lifecycle prerequisites. The shared-
library/GUI boundary keeps this text-only correction in private Widgets and
leaves Core, composition-root, public, and installed contracts unchanged.

The implementation changes only the associated form label to exactly
`Mode:`. Label-field association, selector identity, values, ordering,
current/enabled state, focus/tab behavior, persistence, query composition,
submission, responses, geometry, and lifecycle behavior remain unchanged. The
exact label and association stay stable through construction, mode and option
transitions, repeated composition, submission, generation-current accepted
completion, active cancellation, replacement, service replacement, and
controller detachment.

Layout restructuring, any other label, mnemonic policy, translation-catalog
work, grammar modernization, indexing lifecycle/UI, Preferences,
adapters/index formats, dependencies/builds, public/Core or composition-root
changes, HTTP GET policy, and unrelated parity are excluded and unranked. No
successor after P8-FT-45 is selected or ranked.

Completed focused acceptance extends only `full_text_query_composer_test`. It uses
`QFormLayout::labelForField()` to prove the unique selector has exactly one
associated label with exact text `Mode:` and preserves selector identity,
values, state, mode transitions, and query composition. Existing dialog tests
retain request and lifecycle regressions. Add no executable or registered
test. The focused Release command is
`ctest --preset conan-release -R '^full_text_query_composer_test$'`.

The full implementation gate remains Linux Release configure/build,
exactly 109 registered tests, full Release CTest, Release install, packaged
consumers, unchanged standalone installed C and C++ consumers, and clean
committed exact-SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-45 adds no executable, test registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. The implementation changes only
the private composer, its existing focused test, and these four governing
documents.

P8-FT-45 is complete. No successor is selected or ranked.

Implementation must stop on ref/worktree drift, legacy dirtiness, ambiguous
label mapping or acceptance semantics, any second label or behavior change,
layout restructuring, mnemonic or translation-catalog work, public/Core or
composition-root expansion, dependency or installed-surface change, an
architectural decision requiring HTTP GET policy, discovery of another
required file, or scope expansion. This audit's gate is exact four-file scope,
cross-document consistency, Phase terminology, successor neutrality,
`git diff --check`, and clean pinned refs and worktrees.

### Phase 8 full-text query-field label parity (complete)

The documentation-only post-P8-FT-45 audit is pinned to clean migrated
revision `05e2f6b7ca3a657d1c0fe57bea7e47e691762054`, its identical upstream and
fresh live remote, and clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly P8-FT-46:
remove the private full-text query field's unmatched label.

Pinned legacy `fulltextsearch.ui:28-31` places the unique query `QLineEdit`
directly in the Search group without a label. Before P8-FT-46,
`full_text_query_composer.cpp:57-58,131-133` mapped that field to the unique
private `fullTextQueryText` line edit but added a `QFormLayout` label with text
`Query`. P8-FT-1 through P8-FT-45 supply the field, composition, persistence,
containing-layout, focus/tab, request-lifecycle, and adjacent mode-label
prerequisites. The shared-library/GUI boundary keeps this one-label correction
in private Widgets and leaves Core, composition-root, public, and installed
contracts unchanged.

The completed implementation changes only the query field's form row from
labeled to unlabeled full-width placement while retaining `fullTextQueryText`. Field
identity, parentage, value, ordering, focus/tab behavior, query composition,
submission, responses, geometry, and lifecycle remain unchanged. The absent
association and unique field stay stable through construction, text mutation,
mode and option transitions, repeated composition, submission, generation-
current accepted completion, active cancellation, replacement, service
replacement, and controller detachment.

Any second label or behavior, broader layout restructuring, spacing, margins,
mnemonic policy, translation-catalog work, indexing lifecycle/UI, Preferences,
adapters/index formats, dependencies/builds, public/Core or composition-root
changes, HTTP GET policy, and unrelated parity are excluded and unranked. No
successor after P8-FT-46 is selected or ranked.

Completed focused acceptance extends only `full_text_query_composer_test`. It
uses `QFormLayout::labelForField()` to prove the unique query field has no associated
label while preserving identity, parentage, value, text mutation, mode and
option transitions, and repeated composition. Existing dialog tests retain
request and lifecycle regressions. Add no executable or registered test. The
focused Release command remains
`ctest --preset conan-release -R '^full_text_query_composer_test$'`.

The implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, Release install, packaged consumers,
unchanged standalone installed C and C++ consumers, and clean committed exact-
SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-46 adds no executable, test registration, installed header, DTO,
ABI, dependency, CMake export, or Conan requirement. Its implementation
is limited to the private composer, its existing focused test, and these four
governing documents.

P8-FT-46 is complete. No later successor is selected or ranked. Implementation
must stop on ref/worktree drift, legacy dirtiness,
ambiguous mapping or acceptance semantics, any second label or behavior,
broader layout work, mnemonic or translation-catalog work, public/Core or
composition-root expansion, dependency or installed-surface change, an
architectural decision requiring HTTP GET policy, discovery of another
required file, or scope expansion. This documentation audit's gate is exact
four-file scope, cross-document consistency, Phase terminology, successor
neutrality, the exact Conan command, `git diff --check`, and clean pinned refs
and worktrees. Builds and tests are intentionally skipped for this
documentation-only audit.

### Phase 8 full-text wildcard mode-text parity (complete)

The documentation-only post-P8-FT-46 audit is pinned to clean migrated
revision `08837e18ecef39bb97ffddc5842fe0560e2d326c`, its identical upstream and
fresh live remote, and clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly P8-FT-47:
restore the private wildcard query-mode item text.

Pinned legacy `fulltextsearch.cc:232-236` adds the third item of the unique
search-mode selector with exact translatable text `Wildcards` and maps it to
the legacy wildcard mode. Before P8-FT-47,
`full_text_query_composer.cpp:60-74` mapped the
same third item of the unique private `fullTextQueryMode` selector to
`FullTextSearchMode::kWildcard` but displayed `Wildcard`. P8-FT-1 through
P8-FT-46 supply the selector, four-item order and values, persistence, query
composition, containing layout, focus/tab, and request-lifecycle prerequisites.
The shared-library/GUI boundary keeps this one-item correction in private
Widgets and leaves Core, composition-root, public, and installed contracts
unchanged.

The completed implementation changes only the third selector item's displayed
text to exactly `Wildcards`. Selector identity, parentage, four-item count and
order, enum data, selected index, persisted mode, query composition, option
enablement, focus/tab behavior, submission, responses, geometry, and lifecycle
remain unchanged. The exact text, unique selector, and wildcard data mapping
stay stable through construction, every mode and option transition, repeated
composition, submission, generation-current accepted completion, active
cancellation, replacement, service replacement, and controller detachment.

The regular-expression item, any other label or behavior, maximum-distance or
article-limit captions and bounds, broader layout restructuring, spacing,
margins, mnemonic policy, translation-catalog work, indexing lifecycle/UI,
Preferences, adapters/index formats, dependencies/builds, public/Core or
composition-root changes, HTTP GET policy, and unrelated parity are excluded
and unranked. No successor after P8-FT-47 is selected or ranked.

Completed focused acceptance extends only `full_text_query_composer_test`. It proves
that the unique selector's third item has exact text `Wildcards` and retains
its `FullTextSearchMode::kWildcard` data through construction, mode and option
transitions, and repeated composition. Existing dialog tests retain request
and lifecycle regressions. Add no executable or registered test. The focused
Release command remains
`ctest --preset conan-release -R '^full_text_query_composer_test$'`.

The implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, Release install, packaged consumers,
unchanged standalone installed C and C++ consumers, and clean committed exact-
SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-47 adds no executable, test registration, installed header, DTO,
ABI, dependency, CMake export, or Conan requirement. Its implementation
is limited to the private composer, its existing focused test, and these four
governing documents.

P8-FT-47 is complete. No later successor is selected or ranked. Implementation
must stop on ref/worktree drift, legacy dirtiness,
ambiguous mapping or acceptance semantics, any second item, label, or behavior,
caption-bound policy, broader layout work, mnemonic or translation-catalog
work, public/Core or composition-root expansion, dependency or installed-
surface change, an architectural decision requiring HTTP GET policy, discovery
of another required file, or scope expansion. This documentation audit's gate
is exact four-file scope, cross-document consistency, Phase terminology,
successor neutrality, the exact Conan command, `git diff --check`, and clean
pinned refs and worktrees.

### Phase 8 full-text regular-expression mode-text parity (complete)

The documentation-only post-P8-FT-47 audit is pinned to clean migrated
revision `f4bf4d3fc40de94913563d49f1fa824261c3c1d4`, its identical upstream and
fresh live remote, and clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly P8-FT-48:
restore the private regular-expression query-mode item text.

Pinned legacy `fulltextsearch.cc:232-236` adds the fourth item of the unique
search-mode selector with exact translatable text `RegExp` and maps it to the
legacy regular-expression mode. Before P8-FT-48,
`full_text_query_composer.cpp:60-75` mapped the same fourth item of the unique
private `fullTextQueryMode` selector to
`FullTextSearchMode::kRegularExpression` but displayed `Regular expression`.
P8-FT-1 through P8-FT-47 supply the selector, four-item order and values,
persistence, query composition, containing layout, focus/tab behavior, and
request lifecycle. The shared-library/GUI boundary keeps this one-item
correction in private Widgets and leaves Core, composition-root, public, and
installed contracts unchanged.

The completed implementation changes only the fourth selector item's displayed
text to exactly `RegExp`. Selector identity, parentage, four-item count and
order, enum data,
selected index, persisted mode, query composition, option enablement,
focus/tab behavior, submission, responses, geometry, and lifecycle remain
unchanged. The exact text, unique selector, and regular-expression data mapping
must stay stable through construction, every mode and option transition,
repeated composition, submission, generation-current accepted completion,
active cancellation, replacement, service replacement, and controller
detachment.

The wildcard item, any other label or behavior, maximum-distance or
article-limit captions and bounds, broader layout restructuring, spacing,
margins, mnemonic policy, translation-catalog work, indexing lifecycle/UI,
Preferences, adapters/index formats, dependencies/builds, public/Core or
composition-root changes, HTTP GET policy, and unrelated parity are excluded
and unranked. No successor after P8-FT-48 is selected or ranked.

Completed focused acceptance extends only `full_text_query_composer_test`. It
prove that the unique selector's fourth item has exact text `RegExp` and
retains its `FullTextSearchMode::kRegularExpression` data through construction,
mode and option transitions, and repeated composition. Existing dialog tests
retain request and lifecycle regressions. Add no executable or registered
test. The focused Release command remains
`ctest --preset conan-release -R '^full_text_query_composer_test$'`.

The implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, Release install, packaged consumers,
unchanged standalone installed C and C++ consumers, and clean committed exact-
SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-48 adds no executable, test registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. Its implementation is
limited to the private composer, its existing focused test, and these four
governing documents.

P8-FT-48 is complete. No later successor is selected or ranked.
Implementation must stop on ref/worktree drift, legacy dirtiness, ambiguous
mapping or acceptance semantics, any second item, label, or behavior,
caption-bound policy, broader layout work, mnemonic or translation-catalog
work, public/Core or composition-root expansion, dependency or installed-
surface change, an architectural decision requiring HTTP GET policy, discovery
of another required file, or scope expansion. This audit's gate is exact
four-file scope, cross-document consistency, Phase terminology, successor
neutrality, the exact Conan command, `git diff --check`, and clean pinned refs
and worktrees.

### Phase 8 full-text ignore-options row parity (selected)

The documentation-only post-P8-FT-48 audit is pinned to clean migrated
revision `6c3cde5e56542a9f34603ab0a673cabdde4f6636`, its identical upstream and
fresh live remote, and clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly P8-FT-49:
restore the private horizontal row and legacy order of the two existing ignore
options.

Pinned legacy `fulltextsearch.ui:75-99` places exactly
`checkBoxIgnoreWordOrder` followed by `checkBoxIgnoreDiacritics` in one
`QHBoxLayout`. Current `full_text_query_composer.cpp:84-93,137-141` already
owns the corresponding private `fullTextIgnoreWordOrder` and
`fullTextIgnoreDiacritics` controls and query mappings, but adds them as
separate vertical items in the opposite order. P8-FT-1 through P8-FT-48 supply
the composer, both controls, their exact text and state, mode behavior, query
composition, containing layout, focus/tab behavior, and request lifecycle.
The shared-library/GUI boundary keeps this presentation correction in private
Widgets and leaves Core, the composition root, public, and installed contracts
unchanged.

P8-FT-49 will reuse the existing controls in one private horizontal layout,
with ignore-word-order first and ignore-diacritics second, and add only that
row to the existing composer layout. Widget identity, parentage, object names,
text, checked/enabled state, query composition, mode transitions, submission,
responses, geometry, focus/tab behavior, cancellation, replacement, service
replacement, controller detachment, and teardown must remain unchanged. No
widget is recreated and no behavior is added.

Match-case/grid relocation; distance/article rows; maximum-distance and
article-limit captions or bounds; spacing, margins, stretch, alignment,
mnemonic or translation-catalog policy; any third widget or broader layout
work; indexing lifecycle/UI; Preferences; adapters/index formats;
dependencies/builds; public/Core or composition-root changes; HTTP GET policy;
and unrelated parity are excluded and unranked. The legacy caption bounds
conflict with current migrated bounds, so neither caption is eligible without
a new explicit product/Core decision. No successor after P8-FT-49 is selected
or ranked.

Focused acceptance will extend only `full_text_query_composer_test`. It must
prove one unique two-item horizontal layout containing the existing unique
ignore-word-order control first and ignore-diacritics control second, while
preserving identity, parentage, object names, text, state, mode transitions,
and repeated `Compose()` results. Existing dialog tests retain submission,
generation-current completion, cancellation, replacement, service replacement,
controller detachment, response, geometry, focus, tab, and lifecycle
regressions. Add no executable or registered test. The focused Release command
remains `ctest --preset conan-release -R '^full_text_query_composer_test$'`.

The implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, Release install, packaged consumers,
unchanged standalone installed C and C++ consumers, and clean committed exact-
SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-49 adds no executable, test registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. Its future implementation is
limited to the private composer, its existing focused test, and these four
governing documents.

No production or test file changes in this documentation-only audit.
Implementation must stop on ref/worktree drift, legacy dirtiness, ambiguous
membership, order, or acceptance semantics, any third widget or second
behavior, caption-bound policy, broader layout work, mnemonic or translation-
catalog work, public/Core or composition-root expansion, dependency or
installed-surface change, an architectural decision requiring HTTP GET policy,
discovery of another required file, or scope expansion. This audit's gate is
exact four-file scope, cross-document consistency, Phase terminology,
successor neutrality, the exact Conan command, `git diff --check`, and clean
pinned refs and worktrees. Builds and tests are intentionally skipped for this
documentation-only audit.

### Phase 9 — Linux Integration And Release Quality

- Complete audio, clipboard and selection monitoring, global hotkeys, scan
  behavior, desktop files, icons, MIME integration, paths, process launching,
  translations, and resources.
- Document X11 and Wayland behavior separately where they differ.
- Finalize Conan profiles/locks, CI, install and uninstall behavior, CPack,
  formatter/static checks, dependency and license inventory, feature-parity
  matrix, known issues, and intentionally deferred features.

Packaging and clean-build checks run throughout the migration; this phase is
the final Linux acceptance gate, not the first time packaging is attempted.

Gate: clean clone/configure/build succeeds; the application launches;
representative dictionaries index and search; articles and embedded resources
render; settings persist; packages install and uninstall; automated tests
pass; and the build has no dependency on the legacy worktree.

### Phase 10 — Windows And macOS Restoration

After Linux acceptance:

- define platform Conan profiles and CI;
- implement the isolated platform adapters;
- solve Qt WebEngine deployment, native integration, audio, hotkeys, scanning,
  and installers;
- apply the same build, test, install, package, and workflow gates per
  platform.

This is a post-Linux roadmap and does not block the active Linux delivery.

The first formal Linux release is a full Linux behavior-parity milestone, not
a reduced-format or reduced-feature edition. In addition to every legacy
dictionary format, it must restore the pinned legacy baseline's major Linux
user workflows, including online dictionaries, audio, clipboard/selection
lookup, scan popup behavior, and global hotkeys. Earlier internal milestones
may be narrower only to make migration and verification incremental.

## Continuous Execution Rules

- Keep changes small and organized by one migration concern.
- Separate mechanical import, build wiring, Qt 6 API changes, and behavioral
  changes whenever practical.
- Run the smallest relevant tests after each increment and a clean build,
  install, and package gate at each phase boundary.
- Preserve source notices and update the dependency/license inventory whenever
  code or a dependency enters the migration tree.
- Report phase start, meaningful checkpoints, blockers or failed durable jobs,
  and phase completion. Long-running jobs must have a durable exit wake or an
  equivalent reconciliation mechanism.
- Do not modify legacy `master`, push, open a pull request, or publish artifacts
  without explicit authorization.

## Phase 2 Baseline

Phase 2 establishes the `GoldenDict` CMake project and `goldendict` Conan
package/application at version 1.6.0. This starts the Qt 6 and Tiger-based
GoldenDict version line. It provides a minimal Qt 6 application
window backed by Qt WebEngine. Product features from the legacy source are not
yet migrated.
