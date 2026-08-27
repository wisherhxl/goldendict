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
The next foundation increments add private table-driven transliteration
primitives. Russian transliteration preserves the pinned
longest-match, case-sensitive mapping and unmatched-Unicode behavior while
strictly bounding valid UTF-8 input and output. It adds no dictionary provider,
configuration field, installed API, locale work, or external dependency;
German transliteration reuses that bounded engine and preserves the pinned
12-entry case-sensitive table, including the intentionally disabled reverse
mappings. It likewise adds no provider or public surface. Other
transliterations and morphology remain separately gated. Greek transliteration
then reuses the same private bounded engine and preserves the pinned 645-entry
modern/classical table, including Beta Code, alternate diacritic order,
tonos/oxia conversion, and case-sensitive legacy spellings. It adds no
provider, configuration, installed API, UI, translation, or dependency.
Belarusian transliteration then adds the three inseparable pinned tables for
Latin/classic, Latin/school, and school/classic smoothing. It preserves their
334/333, 535/534, and 446/436 declaration/effective counts, first-retained
duplicates, longest matching, simple case folding, and table quirks behind the
same private bounded Core boundary. Romaji, Chinese conversion, provider
composition, and morphology remain separately gated. The next cohesive private
family adds only the pinned Hepburn Latin-to-Hiragana and Latin-to-Katakana
tables with exact 139/139 and 167/165 declaration/effective mapping counts. It
preserves ICU simple case folding, code-point longest matching through
four-character sources, first-retained `va`/`vo` Katakana duplicates,
unmatched Unicode, and the established strict bounds. Nihon-shiki,
Kunrei-shiki, Chinese conversion, provider composition, dictionary identities,
icons, configuration, UI, translations, and morphology remain later work.
The next independent morphology prerequisite adds private bounded discovery
of the pinned Hunspell `.aff`/`.AFF` plus `.dic`/`.DIC` companion pairs. It
preserves one-directory scanning, basename identities, and lower-case
companion preference while adding deterministic ordering, regular-file
validation, diagnostics, duplicate suppression, and a 4096-entry scan bound.
It adds no Hunspell dependency, content parser, provider, configuration,
installed API, UI, or transliteration behavior. Hunspell parsing and
suggestions remain separately gated.
The following private content-loading prerequisite preserves the original
affix and dictionary bytes and their `.aff`-then-`.dic` ordering while
validating the affix `SET` encoding and complete file contents through the
existing strict ICU boundary. It adds typed path-bearing failures, rejects
unsafe file pairs and symlinks, validates the dictionary entry header/count,
and enforces explicit file, aggregate, line, and entry limits. Generated UTF-8
and legacy single-byte fixtures cover valid boundaries and deterministic
format, encoding, resource, and filesystem failures. Hunspell engine/provider
composition then pins Conan Hunspell 1.7.2 privately, validates original pairs
through that loader before engine construction, and provides bounded,
serialized exact membership through the existing private backend contract.
Generated UTF-8, affix, and legacy single-byte fixtures preserve direct source
compatibility. Morphology suggestions, prefix enumeration, configuration,
installed API, and UI remain separately gated. The bounded single-word
morphology leaf then extends that same private provider through the existing
`SynonymBackend` only. It preserves legacy outer whitespace/punctuation
trimming and the 80-character limit, rejects whitespace-containing
expressions, uses the declared dictionary encoding, serializes `analyze()`,
and extracts decoded `st:` records in engine order without deduplication after
removing trailing `#` comments and simple-case-equivalent inputs. The next
private leaf restores pinned legacy `prefixMatch` as bounded whole-query
membership rather than enumeration: it applies the same outer trim and
whitespace rejection, uses the declared encoding, serializes `spell()`, and
returns the trimmed original UTF-8 headword on acceptance. Exact lookup,
synonym stems, and prefix suggestions remain unchanged; prefix enumeration
remains excluded. The next private morphology leaf restores the pinned
whitespace-triggered compound-expression path through the same
`SynonymBackend`. It tokenizes the trimmed expression into at most 21
alternating lexical and punctuation/Unicode-whitespace runs, preserves the
separator text exactly, and analyzes each lexical run through the declared
dictionary encoding and process-wide engine mutex. Bounded reconstruction uses
the original plus the first two accepted stems per lexical run in legacy branch
order, retains duplicates, removes the unchanged expression, observes request
cancellation/deadlines, and returns no more than the request result limit.
Spelling-suggestion articles, true prefix enumeration, configuration,
registration and enabled-dictionary policy, installed APIs, UI, translations,
and dependency changes remain excluded.
The dependent private spelling-suggestion article leaf then restores the
pinned `getArticle` behavior through the existing backend lookup contract.
After strict UTF-8 and query bounds, outer whitespace/punctuation trimming,
and phrase rejection, `spell()` and `suggest()` run under the process-wide
Hunspell mutex in the dictionary's declared encoding. Correct words and
suggestion-free misses return nothing; decoded suggestions preserve engine
order and duplicates, while a simple-case-equivalent suggestion suppresses
the whole article. The result is limited to 64 suggestions and 64 KiB of
escaped inert HTML whose `bword://` links enter the existing typed-link
sanitizer, with cancellation/deadline checkpoints throughout. Registration,
configuration and enabled policy, prefix enumeration, UI and translations,
installed APIs, dependency changes, and unrelated backends remain excluded.
The next narrowly bounded Hunspell leaf exposes true-prefix enumeration only
through a private Core capability. Proper UTF-8 prefixes are tested longest
first as complete Hunspell words using the established trim, declared
encoding, request bounds, and process-wide engine mutex; the complete input is
excluded and the request result limit is honored. Whole-query
`LookupPrefix()`, public `SuggestPrefix()`, installed contracts, UI, settings,
and spelling articles are unchanged.

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

### Phase 8 full-text ignore-options row parity (complete)

The implementation is based on clean migrated revision
`5203c9f5730961cf13afb0d57b8523bc011ebafb`, its identical upstream and
fresh live remote, and clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It completes exactly P8-FT-49:
restore the private horizontal row and legacy order of the two existing ignore
options.

Pinned legacy `fulltextsearch.ui:75-99` places exactly
`checkBoxIgnoreWordOrder` followed by `checkBoxIgnoreDiacritics` in one
`QHBoxLayout`. Before P8-FT-49,
`full_text_query_composer.cpp:84-93,137-141` already
owns the corresponding private `fullTextIgnoreWordOrder` and
`fullTextIgnoreDiacritics` controls and query mappings, but adds them as
separate vertical items in the opposite order. P8-FT-1 through P8-FT-48 supply
the composer, both controls, their exact text and state, mode behavior, query
composition, containing layout, focus/tab behavior, and request lifecycle.
The shared-library/GUI boundary keeps this presentation correction in private
Widgets and leaves Core, the composition root, public, and installed contracts
unchanged.

P8-FT-49 reuses the existing controls in one private horizontal layout,
with ignore-word-order first and ignore-diacritics second, and adds only that
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

Focused acceptance extends only `full_text_query_composer_test`. It must
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
dependency, CMake export, or Conan requirement. Its implementation is
limited to the private composer, its existing focused test, and these four
governing documents.

P8-FT-49 is complete. No later successor is selected or ranked.
Implementation was required to stop on ref/worktree drift, legacy dirtiness,
ambiguous membership, order, or acceptance semantics, any third widget or
second
behavior, caption-bound policy, broader layout work, mnemonic or translation-
catalog work, public/Core or composition-root expansion, dependency or
installed-surface change, an architectural decision requiring HTTP GET policy,
discovery of another required file, or scope expansion. The implementation
gate is exact six-file scope, cross-document consistency, Phase terminology,
successor neutrality, the exact Conan command, `git diff --check`, and clean
pinned refs and worktrees.

### Phase 8 full-text coupled search-options grid parity (selected)

The fresh documentation-only post-P8-FT-49 audit is based on clean migrated
revision `34f2b6903fba45e8a13650057cb7348e70f5be0f`, its identical local branch,
upstream, and fresh live remote, plus unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The clean pushed selection commit
`919e1ae017a2c57c17cba8ba46158b1a9828742b` authorized only P8-FT-50, now
complete: restore the pinned legacy coupled grid topology for the existing
full-text Search options while retaining migrated numeric contracts.

Pinned legacy `fulltextsearch.ui:28-99` establishes the exact topology. The
Search group's vertical order is the full-width query field, one options
`QGridLayout`, then the ignore-options horizontal row. Grid row 0 contains the
word-distance toggle at column 0, word-distance spin box at column 1, and a
two-item horizontal layout with `Mode:` then the mode selector at column 2.
Grid row 1 contains the article-limit toggle at column 0, article-limit spin
box at column 1, and `Match case` at column 2. Current private
The pre-P8-FT-50 private composer supplied every corresponding control and
behavior; its form and separate rows were the only topology difference.

The completed implementation reuses those unique controls. The composer's
top-level `QVBoxLayout` contains the existing full-width unlabeled query widget
first, the unique six-cell grid second, and the completed P8-FT-49
ignore-options row third. Only the grid's row-0/column-2 cell contains a nested
layout. All seven participating widgets remain direct child widgets of the
composer; the composer remains the sole direct child of the existing `Search`
group layout. No widget is recreated.

This leaf preserves the existing captions, `Mode:` label, four mode texts/data,
object names, identity, state and enablement, explicit focus/tab chain,
persistence, composition, submission, response, cancellation, replacement,
geometry, and every P8-FT-1 through P8-FT-49 behavior. Word distance retains
the validated Core-backed range `0..1000`, and articles per dictionary retains
`1..100000`; legacy Qt `0..99` defaults and synthesized range-bearing captions
are not restored. Indexing lifecycle/status UI, Preferences, adapters/index
formats, dependencies/builds, public/Core/config or composition-root contracts,
installed surfaces, layout styling policy, mnemonic/translation work, new
controls or behavior, HTTP GET policy, and unrelated parity are excluded and
unranked. No successor after P8-FT-50 is selected or ranked.

Focused acceptance changes only `full_text_query_composer_test` to prove
the unique six-item grid, exact coordinates and nested mode order, exact
query/grid/ignore top-level order and parentage, and unchanged widget identity,
parentage, names, captions, mode data, bounds, state transitions, and repeated
`Compose()` results. Existing `full_text_search_dialog_test` retains Search-
group hierarchy, explicit focus/tab order, submission, generation-current
completion, cancellation, replacement, service replacement, controller
detachment, response, geometry, and lifecycle ownership. Add no executable or
registration; the Release baseline remains exactly 109 tests.

The full implementation gate is focused Release composer testing, Linux Release
configure/build, exactly 109 registered tests, full Release CTest, Release
install, packaged consumers, unchanged standalone installed C and C++
consumers, and clean committed exact-SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-50 changed no installed interface, but install and consumer checks remain
the stronger full gate. Implementation must stop without commit or push on
ref/worktree drift, legacy dirtiness or movement, ambiguous topology,
parentage, caption or acceptance semantics, architectural conflict, failed
validation, discovery of another required file, or scope expansion.

### Phase 8 full-text word-distance caption policy closure

The resumed documentation-only post-P8-FT-50 audit verified clean migrated
HEAD, local branch, upstream, and live remote at
`90823b6ce600063642cc5780a0d4197e75605329`, plus the clean read-only legacy
checkout at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Its objective is only to
record GET's Option A decision: preserve the exact visible private label
`Maximum word distance`. Labels describe settings; numeric controls own and
expose their bounds, so `0..1000` remains the spin-box contract and is not
embedded in translatable caption text.

Pinned legacy `fulltextsearch.cc:203-211` supplies evidence for the historical
range-bearing caption. Completed P8-FT-1 through P8-FT-50, current
`full_text_query_composer.cpp:95-107`, and focused composer coverage establish
the prerequisites and show that production already satisfies Option A.
Acceptance is therefore documentation-only: all four governing documents state
the same policy, no source/test change is fabricated, the label and range stay
exact, and no successor is selected or ranked.

This closure preserves all existing full-text behavior, object identity,
composition and lifecycle semantics, public/Core/config and installed
contracts, dependencies, and the exactly 109-test Release baseline. Compiled
checks are skipped. The unchanged future implementation gate remains Linux
Release build and full CTest, Release install, packaged consumers, standalone
installed C and C++ consumers, and clean committed exact-SCM creation:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Indexing lifecycle/status UI, the articles-per-dictionary caption, other text
or accessibility policy, styling/layout, mnemonic/translation-catalog work,
Preferences, adapters/index formats, dependencies/builds, public/Core/config
or composition-root changes, HTTP GET policy, and unrelated parity are
excluded and unranked. Stop without commit or push on drift, ambiguity, design
conflict, another required file, failed validation, or scope expansion. No
successor is selected or ranked. The precise next boundary is a fresh bounded
post-policy full-text readiness audit across all remaining gaps and their then-
current prerequisites.

### Phase 8 full-text articles-per-dictionary caption policy prerequisite

The fresh independent bounded post-policy audit verified clean migrated HEAD,
local branch, upstream, and live remote at
`66b73596e6b9f2f296c0227933825fba100ba3b2`, and the unchanged clean read-only
legacy checkout at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It rechecked all remaining visible and private full-text gaps without advance
ranking and selects exactly P8-FT-51, the smallest independently
dependency-ready prerequisite: settle the articles-per-dictionary caption
policy. No successor after P8-FT-51 is selected or ranked.

Pinned legacy `fulltextsearch.cc:249-256` dynamically creates
`Max articles per dictionary (%1-%2):` and owns historical numeric bounds.
Completed current `full_text_query_composer.cpp:109-125` and focused composer
coverage instead preserve exact text `Maximum articles per dictionary`, with
the spin box solely owning and exposing `1..100000`. P8-FT-1 through P8-FT-50
supply the complete control, persistence, composition, request and presentation
prerequisites, but the word-distance policy closure does not decide this
separate caption.

GET must choose either the exact current label and sole spin-box bound
ownership, producing no caption implementation leaf, or a range-bearing
caption using current `1..100000` bounds, potentially authorizing a later
private Widgets leaf. Legacy Qt `0..99` defaults cannot be restored because
they conflict with the current bounded query contract. P8-FT-51 records only
this necessary product-policy prerequisite; it chooses neither outcome and
changes no production or test behavior.

Index lifecycle/readiness/status/progress UI and full-text Preferences
enablement, format exclusions and dictionary-size policy require separate Core
lifecycle or policy work and are not dependency-ready. Exact-document
navigation, match/excerpt presentation, ignore-diacritics consumption,
accessibility, styling/layout, translation-catalog work, adapters/index
formats, public/Core/config or composition-root changes, dependencies/builds,
installed surfaces, and unrelated parity remain excluded and unranked.

Acceptance is limited to consistent updates in these four governing documents,
the exact evidence and alternatives above, preserved boundaries, the unchanged
109-test Release baseline, and successor neutrality. Compiled checks are
omitted for this documentation-only audit. Stop without commit or push on
ref/worktree or legacy drift, ambiguity beyond the stated choice,
architectural conflict, another required file, failed validation, or scope
expansion.

### Phase 8 full-text articles-per-dictionary caption policy closure

The resumed documentation-only audit verified clean migrated HEAD, local
branch, upstream, and live remote at
`456f3e9c68bf9514bfc6ded225d2ddc96f6c9477`, plus the unchanged clean read-only
legacy checkout at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
GET selected P8-FT-51 Option A: retain exact visible private label
`Maximum articles per dictionary`. The associated spin box solely owns and
exposes `1..100000`; bounds are not embedded in translatable caption text. The
locked `Maximum word distance` / `0..1000` label-and-control policy remains
unchanged.

Pinned legacy `fulltextsearch.cc:249-256` supplies historical evidence for the
range-bearing article-limit caption. Current
`full_text_query_composer.cpp:95-125` and focused composer coverage establish
that production already satisfies both Option A policies, including exact
labels, numeric ranges, identity, enablement, state transitions, and repeated
composition. P8-FT-51 therefore closes without a caption implementation leaf
and without source or test changes.

All full-text behavior, public/Core/config and index-format boundaries,
installed contracts, dependencies, builds, and the exactly 109-test Release
baseline remain unchanged. Compiled checks are omitted. Stop without commit or
push on ref/worktree or legacy drift, ambiguity, design conflict, another
required file, failed validation, or scope expansion. No successor is selected
or ranked. The precise next boundary is a fresh independent bounded full-text
readiness audit across all remaining gaps and their then-current prerequisites.

### Phase 8 full-text dialog window-title translation (complete)

P8-FT-52 was implemented from clean synchronized migrated base
`b25cee8fd95381ecd16f733107f7d201d5068eeb` with the unchanged clean read-only
legacy checkout at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
The existing private dialog window title now resolves through its dialog
translation context without changing exact source or default visible text
`Full-text search`. No successor after P8-FT-52 is selected or ranked.

Pinned legacy `fulltextsearch.cc:219-223` sets that exact title through the
dialog's translation context. The implementation is limited to
`apps/goldendict/src/full_text_search_dialog.cpp` and its existing focused
dialog test: `FullTextSearchDialog::tr()` owns the title, and construction
coverage proves both exact-context replacement and the English fallback.

The shared-library/GUI boundary governs this leaf: it is presentation-owned
and requires no public, Core, configuration, index-format, dependency, build,
composition-root, or installed-interface change. It adds no translation
catalog, locale-loading infrastructure, executable, or test registration, and
the Release baseline remains exactly 109 tests. The locked
`Maximum word distance` / spin-box-owned `0..1000` and
`Maximum articles per dictionary` / spin-box-owned `1..100000` policies remain
unchanged.

Index readiness/status/progress/background lifecycle is not independently
ready because index state remains private per backend and index construction
occurs during dictionary loading. Full-text Preferences enablement, format
exclusions, and maximum-dictionary-size policy remain blocked on that separate
Core lifecycle/policy boundary despite their persisted fields. Other dialog
translation, accessibility and styling surfaces, exact-document navigation,
match/excerpt presentation, ignore-diacritics consumption, adapters/index
formats, and unrelated parity remain separate and unranked.

Acceptance requires the focused and full 109-test Linux Release suite, fresh
Release configure/build and install, standalone installed C/C++ consumers,
clean committed exact-SCM Conan creation with packaged consumers, exact
six-file scope, cross-document consistency, `git diff --check`, and clean
synchronized refs/worktrees.

### Phase 8 full-text Search group-box translation (complete)

P8-FT-53 was implemented from clean synchronized migrated base
`339d1dd6e8b3540923153628497af23b6fa7208b` with the unchanged clean read-only
legacy checkout at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
The private Search group-box title now resolves through the exact
`goldendict::app::FullTextSearchDialog` context without changing source or
default visible text `Search`. No successor after P8-FT-53 is selected or
ranked.

Pinned legacy `fulltextsearch.ui:23-27` marks the exact title as translatable.
`full_text_search_dialog.cpp` now constructs it with dialog-owned `tr()`;
completed P8-FT-52 supplies the dialog's private translation-context precedent.

The implementation is restricted to
`apps/goldendict/src/full_text_search_dialog.cpp` and
`apps/goldendict/tests/full_text_search_dialog_test.cpp`. It must use the
dialog-owned `tr("Search")`. Focused coverage proves the English fallback,
replacement through the exact context, sole direct-child identity/title, and
automatic removal of test-global translator state. The Shared-Library and GUI Boundary governs this private presentation
leaf. No catalog, locale loader, executable, test registration, public/Core/
configuration/index-format/dependency/build/composition-root, ABI, or
installed-interface change is authorized.

Exact `Maximum word distance` with spin-box-owned `0..1000` and exact
`Maximum articles per dictionary` with spin-box-owned `1..100000` remain
locked, and the Release baseline remains exactly 109 tests. Index readiness,
status, progress, background lifecycle, and full-text Preferences remain
blocked on a separate Core lifecycle/policy boundary. Translation catalogs and
other strings, accessibility, styling/layout, exact-document navigation,
match/excerpt presentation, ignore-diacritics consumption, adapters/index
formats, and unrelated parity remain separate and unranked.

Acceptance requires the focused and full 109-test Linux Release suite, fresh
Release dependency install/configure/build and install, standalone installed C
and C++ consumers, clean committed exact-SCM Conan creation with packaged
consumers, exact six-file scope, cross-document consistency, `git diff --check`,
and clean synchronized refs/worktrees.

### Phase 8 full-text partial-status translation (complete)

P8-FT-54 was implemented from clean synchronized migrated base
`8dcf3d87fe4b25a916c864da56c307f5c78de24b`, plus the unchanged clean
read-only legacy checkout at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
The existing status
`Results may be incomplete.` now resolves through the private
`goldendict::app::FullTextSearchDialog` context. No successor after P8-FT-54 is
selected or ranked.

Completed P8-FT-22 owns that exact visible text and its generation-current
partial-response visibility semantics. Current `full_text_search_dialog.cpp`
constructs the label with dialog-owned `tr()`; P8-FT-52 and P8-FT-53 establish
the dialog-owned translation context and scoped-translator test pattern. Pinned
legacy full-text sources contain no equivalent partial-response status, so no
legacy wording or translation context conflicts with this migrated-only status.

The implementation changes only `full_text_search_dialog.cpp` and
`full_text_search_dialog_test.cpp`, using dialog-owned
`tr("Results may be incomplete.")`. Focused acceptance covers exact-context
replacement, English fallback, stable label identity/text, all existing
P8-FT-22 visibility predicates, and automatic translator cleanup. The
Shared-Library and GUI Boundary remains governing; no catalog, locale loader,
executable, registration, public/Core/config/index-format/dependency/build/
composition-root, ABI, or installed contract changes.

Exact translated dialog title `Full-text search`, translated group title
`Search`, `Maximum word distance` with control-owned `0..1000`, and
`Maximum articles per dictionary` with control-owned `1..100000` remain locked.
Other response strings and catalog readiness, accessibility, styling/layout,
exact-document navigation, match/excerpt presentation, ignore-diacritics
consumption, adapters/index formats, and unrelated parity remain separate and
unranked. Index readiness/status/progress/background lifecycle and full-text
Preferences remain blocked on separate Core lifecycle/policy work.

Delivery requires the focused and full 109-test Linux Release suite, fresh
Release dependency install/configure/build and install, standalone installed C
and C++ consumers, clean committed exact-SCM Conan creation with packaged
consumers, exact six-file repository validation, and clean synchronized
refs/worktrees. Completion unlocks only a fresh independent bounded full-text
readiness audit; it does not select or rank its outcome.

### Phase 8 full-text empty-status translation (complete)

P8-FT-55 was implemented from clean synchronized migrated base
`175a92926b1046798b51b9757a28ed156555c0aa`, with the read-only legacy checkout
unchanged and clean at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The
existing private empty-response status `No matches` now resolves through the
`goldendict::app::FullTextSearchDialog` translation context.

Completed P8-FT-23 owns the exact text, unique widget identity, and
generation-current conclusive-empty visibility contract. The dialog now
constructs it with dialog-owned `tr("No matches")`;
P8-FT-52 through P8-FT-54 establish dialog-owned translation and focused scoped-
translator coverage. Pinned legacy full-text sources contain no equivalent
empty-response status, so no legacy wording or context conflicts with this
migrated-only presentation.

Implementation changed only `full_text_search_dialog.cpp` and
`full_text_search_dialog_test.cpp`. Focused coverage proves exact-context
replacement, English
fallback, stable widget identity/text, unchanged P8-FT-23 initial, replacement,
nonempty, partial, error, stale, cancelled, and detached behavior, plus scoped
translator cleanup. The Shared-Library and GUI Boundary remains governing; no
catalog, locale loader, executable, registration, public/Core/config/index-
format/dependency/build/composition-root, ABI, or installed contract changes.

Exact completed translations `Full-text search`, `Search`, and
`Results may be incomplete.` remain unchanged. Exact
`Maximum word distance` with control-owned `0..1000` and
`Maximum articles per dictionary` with control-owned `1..100000` remain locked.
Other response strings/catalog readiness, accessibility, styling/layout,
exact-document navigation, match/excerpt presentation, ignore-diacritics
consumption, adapters/index formats, and unrelated parity remain independent and
unranked. Index readiness/status/progress/rebuild/failure reporting/background
lifecycle and full-text Preferences remain blocked on separate Core lifecycle/
policy work.

The Release baseline remains exactly 109 tests. No successor after P8-FT-55 is
selected or ranked; completion unlocks only a fresh independent bounded full-
text readiness audit.

### Phase 8 full-text terminal-failure-status translation (complete)

P8-FT-56 is complete from clean synchronized migrated revision
`7bc3fbeee4af637e25dff8656ce7d22406d8ea2d` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The existing private terminal-
failure status now translates through
`goldendict::app::FullTextSearchDialog::tr()`.

Completed P8-FT-24 owns `fullTextFailureResponseStatus`, exact text
`Full-text search failed`, and its error-only generation-current visibility.
The label now uses dialog-owned `tr()`; P8-FT-52 through P8-FT-55 supply
the exact dialog context and scoped-translator test pattern. Pinned legacy has
no equivalent status or conflicting contract. The implementation changes
only `full_text_search_dialog.cpp` and `full_text_search_dialog_test.cpp`, with
focused coverage for exact-context replacement, English fallback, stable
identity/text, unchanged lifecycle predicates, stale/cancelled/detached safety,
and translator cleanup. It adds no executable or registered test.

The Shared-Library and GUI Boundary applies. Public/Core/configuration/index-
format/dependency/build/composition-root/ABI/installed interfaces, catalogs,
locale loading, and the exactly 109-test Release baseline remain unchanged.
Completed translations `Full-text search`, `Search`,
`Results may be incomplete.`, and `No matches` remain exact. The locked
captions and control-owned ranges remain `Maximum word distance` / `0..1000`
and `Maximum articles per dictionary` / `1..100000`.

All other visible and private full-text gaps remain independent and unranked.
Index readiness/status/progress/rebuild/failure reporting/background lifecycle
and full-text Preferences remain blocked on a separate fully evidenced Core
lifecycle/policy boundary. The completed leaf has the exact six-file scope and
preserves exactly 109 registered Release tests. P8-FT-56 completion unlocks
only a fresh independent bounded full-text readiness audit.

### Phase 8 full-text mixed-result-status translation (complete)

The fresh independent bounded post-P8-FT-56 audit was pinned to clean
synchronized migrated HEAD, local branch, upstream, and live remote at
`491d85500d27df280c19d4a62a2adc9e14d55a33`, plus unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It re-evaluated
every remaining visible and private full-text gap and selected exactly one
smallest independently evidence-ready leaf, P8-FT-57: translating the existing
private mixed-result status through the dialog's translation context.

Completed P8-FT-25 owns the unique `fullTextMixedResultResponseStatus`, exact
source and fallback text `Some dictionaries could not be searched`, and the
generation-current result-plus-error visibility contract without raw error
details. P8-FT-57 changes the label's former `QStringLiteral` construction to
dialog-owned `tr`; P8-FT-52 through P8-FT-56 supply the exact private
`goldendict::app::FullTextSearchDialog` context and scoped-translator test
pattern. Pinned legacy has no equivalent mixed-result status or conflicting
translation contract. No unresolved architecture or product choice remains.

The completed P8-FT-57 implementation changes exactly
`apps/goldendict/src/full_text_search_dialog.cpp` and
`apps/goldendict/tests/full_text_search_dialog_test.cpp`. It changes only the
label construction to dialog-owned
`tr("Some dictionaries could not be searched")` and extends focused coverage
for exact-context replacement, English fallback, stable identity/text,
unchanged P8-FT-25 visibility and partial-status coexistence, lifecycle safety,
and scoped translator cleanup. It adds no catalog, locale loader, executable,
registered test, public/Core/configuration/index-format/dependency/build/
composition-root/ABI/installed-interface change.

The Shared-Library and GUI Boundary governs. Completed translations
`Full-text search`, `Search`, `Results may be incomplete.`, `No matches`, and
`Full-text search failed` remain exact. Locked policies remain `Maximum word
distance` with control-owned `0..1000` and `Maximum articles per dictionary`
with control-owned `1..100000`. The Release baseline remains exactly 109
registered tests.

Index readiness/status/progress/rebuild/failure reporting/background lifecycle
and full-text Preferences remain blocked on a separate fully evidenced Core
lifecycle/policy boundary. Exact-document navigation, result/match/excerpt
presentation, ignore-diacritics consumption, adapters/index formats,
accessibility, styling/layout, catalogs, and unrelated parity remain independent
and unranked. Focused and full Release gates, install and consumer checks, and
clean exact-SCM package creation preserve the 109-test baseline. P8-FT-57
completion unlocks only a fresh independent bounded readiness audit; no
successor is selected, ranked, recommended, or named.

### Phase 8 full-text partial-empty-status translation (complete)

The fresh independent bounded post-P8-FT-57 audit is pinned to clean
synchronized migrated HEAD, upstream, and live remote at
`58612007652ac24f08fc0bd8e2a4fb2b59839366`, with the clean read-only legacy
checkout unchanged at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. P8-FT-58
translates the existing private partial-empty status through the dialog's
established context.

Completed P8-FT-26 owns `fullTextPartialEmptyResponseStatus`, exact source and
fallback text `No matches in searched dictionaries`, and its generation-current
zero-result, authoritative-partial visibility, partial-status coexistence, and
no-raw-detail behavior. The dialog now constructs that status with dialog-owned
`tr()`; P8-FT-52 through P8-FT-57 establish dialog-owned translation and focused
scoped-translator coverage. Pinned legacy `fulltextsearch.cc` and
`fulltextsearch.ui` contain no equivalent status or conflicting wording or
context contract. No unresolved architecture or product decision remains.

The completed implementation is restricted to `full_text_search_dialog.cpp` and
`full_text_search_dialog_test.cpp`; it uses dialog-owned
`tr("No matches in searched dictionaries")`, and focused coverage proves exact-context
replacement, English fallback, stable identity/text, unchanged P8-FT-26
predicates and lifecycle safety, and scoped translator cleanup. The
Shared-Library and GUI Boundary governs. No catalog, locale loader, executable,
registration, public/Core/configuration/index-format/dependency/build/
composition-root/ABI/installed-interface, or test-baseline change is
authorized.

Completed translations `Full-text search`, both `Search` uses,
`Results may be incomplete.`, `No matches`, `Full-text search failed`, and
`Some dictionaries could not be searched` remain exact. Locked policies remain
`Maximum word distance` with spin-box-owned `0..1000` and
`Maximum articles per dictionary` with spin-box-owned `1..100000`. Completed
P8-FT behavior, predicates, lifecycle, and raw-detail suppression remain
unchanged, as does the baseline of exactly 109 registered Release tests.

Index readiness/status/progress/rebuild/failure reporting/background lifecycle
and full-text Preferences remain blocked because no separately authoritative
Core lifecycle/policy resolution exists. Exact-document navigation,
result/match/excerpt presentation, ignore-diacritics consumption, adapters/
index formats, accessibility, styling/layout, catalogs, and unrelated parity
remain independent and unranked. Completion unlocks only a fresh independent
bounded readiness audit; no successor is selected, ranked, recommended, or
named.

### Phase 8 full-text error-count translation acceptance (complete)

P8-FT-59 is complete from synchronized migrated revision
`471ba2a7db8491aa486951506389101caa8cb255`, with the clean read-only legacy
checkout unchanged at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It accepts
focused translation of the existing private error-count status.

P8-FT-27 owns `fullTextErrorCountResponseStatus`, exact text `Errors: %1`, its
authoritative decimal substitution, visibility and coexistence predicates,
lifecycle safety, and no-raw-detail contract. Production retains dialog-owned
`tr("Errors: %1")`. The existing private dialog test proves exact dialog-context
and source replacement, authoritative single- and multi-digit decimal
interpolation, English fallback before installation and after scoped cleanup,
sole direct-child label identity, unchanged predicates and coexistence, stale/
cancelled/detached/replaced-service/teardown safety, and raw-detail suppression.
The Shared-Library and GUI Boundary governs. No production source,
catalog, locale loader, executable, registration, public/Core/configuration/
index-format/dependency/build/composition-root/ABI/installed-interface, or
registered-test change is authorized.

Completed translations `Full-text search`, both `Search` uses, `Results may be
incomplete.`, `No matches`, `Full-text search failed`, `Some dictionaries
could not be searched`, and `No matches in searched dictionaries` remain
exact. Locked policies remain `Maximum word distance` with spin-box-owned
`0..1000` and `Maximum articles per dictionary` with spin-box-owned
`1..100000`. All completed P8-FT behavior and the exactly 109-test Release
baseline remain unchanged.

Index readiness/status/progress/rebuild/failure reporting/background lifecycle
and full-text Preferences remain blocked without a separately authoritative
Core lifecycle/policy resolution. Other translation, accessibility, styling,
navigation, excerpt, diacritics, result presentation, adapters/index formats,
and unrelated parity remain independent and unranked. Completion unlocks only
a fresh independent bounded readiness audit; no successor is selected, ranked,
recommended, or named.

### Phase 8 P8-FT-60 exact-result navigation contract prerequisite (complete)

The bounded audit starts from synchronized migrated revision
`4cca1e81e1167222d067e475a4053088cf99ba38` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The approved
priority uniquely selects P8-FT-60: establish the smallest Core, facade, and
tab-navigation contract that can validate, resolve, persist, and replay an
accepted full-text result's stable dictionary ID and opaque `document_id`.
The contract is complete without direct dialog/MainWindow activation.

The identifier follows repository truth: completed durable numbering ends at
P8-FT-59 and P8-FT-60 is unused. A prior read-only recommendation for an
accepted-result-count translation test was never selected or persisted, so it
does not reserve the number. Translation-acceptance hardening is excluded.

Prerequisites already complete are the bounded structured `FullTextResult`,
private accepted-result/scope/context delivery, scoped current-tab navigation,
history/session persistence, and accepted-query article-search handoff.
Current `main_window.cpp:5996-6040` cannot safely finish the feature: it
discards dictionary and document identity, the service accepts only a
`LookupQuery`, and `article_tab_session.cc:38-66` forbids the existing
internal-link source/article fields for `kLookup`. Pinned legacy
`fulltextsearch.cc:596-609` and `mainwindow.cc:3001-3013` confirm only headword
plus dictionary-set targeting, not exact-document targeting.

P8-FT-60 implements a transport-neutral Core-owned exact target,
bounded validation and resolution, facade/tab identity and session replay, and
atomic failure for invalid, stale, missing-dictionary, or missing-document
targets. It preserves current-tab activation, selected group, immutable
accepted dictionary scope, history, main-query text/selection, and completed
article-search handoff semantics. Widgets must not interpret `document_id` or
perform backend lookup. Well-formed stale IDs map to missing-document failure
because no revision token is carried.

The Shared-Library and GUI Boundary governs the approved facade-only choice.
The installed C++ `DesktopFacade` and navigation DTO ABI intentionally change;
the headless `DictionaryService`, runtime-source contract, and C ABI remain
unchanged. Configuration adds a backward-compatible exact-target tail;
index-format, dependency, build, catalog/locale-loader, executable, and
registration boundaries remain unchanged. Completed translations
`Full-text search`, both `Search` uses, `Results may be incomplete.`, `No
matches`, `Full-text search failed`, `Some dictionaries could not be searched`,
`No matches in searched dictionaries`, and `Errors: %1` remain exact. Locked
policies remain `Maximum word distance` with spin-box-owned `0..1000` and
`Maximum articles per dictionary` with spin-box-owned `1..100000`. All
completed P8-FT identities, predicates, lifecycle, coexistence, and privacy
guarantees and exactly 109 registered Release tests remain unchanged.

The implementation changes the desktop Core/facade/navigation contract,
private resolution for twelve built-in full-text indexes, persistence, focused
existing tests, the packaged C++ consumer, and these documents. It excludes
direct activation, highlighting/excerpts, ignore-diacritics semantics,
translations, backend and index formats, unrelated configuration,
dependencies, build behavior, catalogs, and unrelated presentation. Completed
behavior and exactly
109 registered Release tests remain unchanged. No successor is selected or
ranked. Completion unlocks only its dependency boundary.

### Phase 8 P8-FT-61 exact-result activation connection (complete)

The implementation starts from synchronized migrated
HEAD, upstream, and live remote revision
`0394b031031c265c7799386996bcbda22e5b0a3b` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The approved
user-visible exact-result navigation and source-targeting priority selects
exactly P8-FT-61: wire accepted full-text result activation to the completed
P8-FT-60 exact-target facade/navigation contract. Repository truth durably
occupies P8-FT-60 with the completed foundation, making P8-FT-61 the next
stable identifier.

Prerequisites are the completed structured result projection and accepted
activation context, P8-FT-16 scoped current-tab activation, P8-FT-29 accepted-
query article-search handoff, and P8-FT-60 target validation/resolution,
navigation identity, atomic tab mutation, history, and session replay. Current
`full_text_search_dialog.h:24-35` and
`full_text_search_dialog.cpp:337-348` already deliver dictionary identity and
opaque `document_id` by value. Current `main_window.cpp:5996-6040` is the sole
activation owner but constructs headword-and-scope-only navigation. Current
`desktop_facade.h:35-85,139-190`, `desktop_facade.cc:156-205`, and
`application_service_test.cpp:2375-2438` provide the completed exact-target
contract and atomic failure evidence. Pinned legacy
`fulltextsearch.cc:596-609` and `mainwindow.cc:3001-3013` confirm only headword
and dictionary-set activation, so legacy supplies no competing exact-document
shape.

P8-FT-61 changes only private Widgets composition: MainWindow copies
`intent.result.dictionary.id` and `intent.result.document_id`, without parsing
either, into `TabNavigationState::exact_target`; preserves the existing lookup
kind, exact headword/title, selected group, and immutable accepted dictionary
scope; and submits the complete command once through
`DesktopFacade::OpenArticleTab`. Core/facade remains responsible for validation,
resolution, navigation identity, history, and persisted session replay under
the Shared-Library and GUI Boundary.

Acceptance requires successful activation to retain current-tab behavior,
group, accepted scope and ordering, main-query text/cursor/selection, lookup
dispatch, history/session behavior, and accepted-query article-search handoff,
while persisting the exact target. Invalid targets, unavailable dictionaries,
missing or stale documents, invalid navigation, tab-limit failure, and
navigation-limit failure must leave tabs, history/session, lookup requests, and
article-search state unchanged. Existing `Unable to update article state`
presentation suffices and must expose no target or backend detail.

Focused verification extends only the existing MainWindow/full-text dialog
smoke to prove exact target projection, success persistence, current-tab/group/
scope/query/article-search preservation, and atomic invalid/unresolved and tab-
operation failure. It adds no executable or registration; the Release baseline
remains exactly 109 tests. Release verification covers the focused smoke, full
suite, install, standalone consumers, and exact-SCM package consumers.

All completed P8-FT behavior, exact strings, captions, ranges, lifecycle and
privacy guarantees remain unchanged. Public/installed interfaces and ABI,
configuration, headless `DictionaryService`, `RuntimeDictionarySource`, C API,
dependencies, adapters/index formats, build, catalogs, locale loading,
highlighting/excerpts, ignore-diacritics semantics, translations, and unrelated
parity remain excluded and unranked. No successor after P8-FT-61 is selected,
ranked, recommended, or named. Completion unlocks only the dependency boundary
established by the activation connection.

### Phase 8 P8-FT-62 match-centered excerpt contract prerequisite (selected)

This fresh bounded documentation-only audit starts from synchronized migrated
HEAD, upstream, and live remote revision
`d8d25b50ddf7cd84f71e7b700cb28fa260ea6117` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
Repository truth durably completes P8-FT-60 as the exact-target facade,
navigation, history and session foundation and P8-FT-61 as the dependent
private activation connection. The next unused stable ordinal is P8-FT-62.
Under the approved highlighting and excerpt priority, the evidence uniquely
selects P8-FT-62: define the bounded match-centered excerpt contract required
before result-list presentation can safely consume backend matches.

Current `dictionary_service.h:164-190` defines `FullTextMatch` byte offset,
length and text and carries `FullTextResult::excerpt`, but does not define an
excerpt origin. Current `full_text_index.cc:314-410` validates and maps the
accepted match to original document `plain_text`; it then copies the first
`kMaximumFullTextExcerptBytes` bytes regardless of match position or UTF-8
boundary. Current `full_text_response_model.cpp:15-46` projects only headword
and dictionary tooltip while `full_text_search_dialog.cpp:300-348` preserves
the exact accepted DTO and generation context by value. Displaying the current
prefix as a match excerpt could therefore show no match or malformed UTF-8.

Current `main_window.cpp:5996-6040,7970-8110` preserves successful exact
activation, load, view, tab and generation checks before dispatching the
accepted query through literal WebEngine search. Literal search cannot preserve
wildcard, regular-expression, whole-word, word-order, word-distance,
normalization and exact backend-range semantics, and indexed plain-text byte
ranges cannot be applied directly to composed sanitized HTML. Pinned legacy
`fulltextsearch.cc:596-609`, `mainwindow.cc:3001-3013`, and
`articleview.cc:2569-2728` instead propagate a regular expression and rematch
rendered page text; they provide no safe competing excerpt DTO or DOM-range
mapping. Highlighting is not selected by this audit.

P8-FT-62 requires Core to keep each match offset and length as an authoritative
checked UTF-8 byte range in the original validated indexed `plain_text`, with
both endpoints on code-point boundaries and `text` exactly equal to that valid-
UTF-8 byte slice, including for pattern modes. `FullTextResult` gains
`std::size_t excerpt_byte_offset` as its final member, defaulting to zero, as
the explicit document-relative UTF-8 byte origin for `excerpt`. Shared index
result construction produces a deterministic valid-UTF-8 excerpt no larger than
`kMaximumFullTextExcerptBytes`, centered sufficiently to contain the first
match whenever the match fits the bound. Match ranges remain document-relative;
checked subtraction from the excerpt origin is the only allowed derivation of
an excerpt-relative range.

Precisely, Core chooses the longest code-point-aligned slice containing a
fitting first match, minimizes the difference between before-match and after-
match context bytes among equal-length candidates, and resolves a remaining
tie to the earlier origin. If the match exceeds the bound, the excerpt begins
at the match offset and is the longest code-point-aligned prefix within the
bound. The contract guarantees UTF-8 code-point integrity, not grapheme-cluster
segmentation.

P8-FT-62 is complete. The Shared-Library and GUI Boundary governs the change.
The excerpt is bounded
plain trusted DTO text. Widgets may convert and later present it with normal Qt
text-role escaping, but may not parse dictionary content, rebuild indexes,
invent match semantics, or expose raw backend data. The implementation
changes the installed C++ `FullTextResult` interface and shared index result
construction plus focused existing tests. Headless behavior, runtime-source
and C APIs, configuration, history/session format, index format, all accepted
adapter ingestion, dependencies, build, catalogs, locale loading, executables,
and test registrations remain unchanged. Appending the zero-defaulted final
member preserves existing shorter aggregate initializers at source level but
intentionally changes the installed C++ DTO layout/ABI, requiring rebuilt
consumers and a new exact-SCM package revision. All accepted built-ins retain
the shared index path, and `full-text-v1` serialization is unchanged.

Acceptance requires focused existing Core tests covering ASCII and multibyte
matches at document edges and interior, exact document range/text agreement,
deterministic origin, valid UTF-8 boundaries, the 4096-byte maximum, and the
defined over-bound match-prefix case. Existing model
and dialog tests cover exact accepted by-value preservation and suppression of
replacement, cancelled, stale, rejected, failed-activation and teardown state.
No new executable or registration is added; the Release baseline remains
exactly 109 tests.

All completed P8-FT strings, privacy and lifecycle contracts remain locked.
`Maximum word distance` remains spin-box-owned at `0..1000`, and `Maximum
articles per dictionary` remains spin-box-owned at `1..100000`. P8-FT-60
validation/resolution/navigation identity and P8-FT-61
activation/atomic failure/article-search handoff remain unchanged and
distinct. Ellipsis wording, typography, delegate layout, colors, multi-line
policy, result-list rendering, exact-article highlighting, index lifecycle and
Preferences, and all other full-text and unrelated parity remain excluded and
unranked. No successor after P8-FT-62 is selected, ranked,
recommended, or named. Completion unlocks only the dependency boundary
established by this match-centered excerpt/origin contract.

### Phase 8 P8-FT-63 accepted-query article-highlighting context prerequisite (complete)

The implementation starts from synchronized migrated HEAD, upstream, and live
remote revision `97f2269a0cee85ae96b6c634d1967116a476e7e9` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. GET resolves the
presentation policy in favor of strict pinned-Qt5 parity: keep the result list
headword-only, preserve the existing dictionary-name tooltip, add no excerpt
or row redesign, and pursue article-page highlighting after exact activation.
The implementation completes exactly P8-FT-63, the private accepted-query
highlighting-context prerequisite.

The private dialog activation context now retains the accepted query text,
mode, match-case, ignore-word-order, maximum-word-distance, and
`ignore_diacritics`. MainWindow retains that context through its existing
generation-gated exact navigation and page-load handoff. Literal first-match
selection, status, and forward/backward `findText` remain unchanged, but a
literal query cannot reproduce wildcard, regular-expression, whole-word,
ignore-word-order, maximum-word-distance, match-case, or normalization
semantics. Pinned legacy
`fulltextsearch.cc:596-609` and `articleview.cc:2569-2799` preserve the search
policy, inspect rendered page plain text, derive literal matched strings,
highlight occurrences, select the first match, and expose Previous/Next.

P8-FT-63 retains by value the exact accepted query text, mode, match-case,
ignore-word-order, maximum-word-distance, and existing ignore-diacritics values
through response acceptance, activation, and the article-load handoff.
Replacement submission, cancellation, stale or duplicate completion, rejected
or failed exact activation, tab/view replacement, and teardown cannot revive
or apply an older policy. Ignore-diacritics remains unconsumed, and current
literal article-search behavior does not change.

The Shared-Library and GUI Boundary governs. P8-FT-62 authoritative ranges are
indexed-document UTF-8 byte ranges, not rendered DOM coordinates. P8-FT-63
does not choose a DOM mapping or let Widgets rebuild query semantics. A later
fresh audit must define the Core-owned transport-neutral rendered-text match
plan and its WebEngine application before strict-parity highlighting can be
implemented. Rendered-page extraction, rematching, DOM/literal application,
highlight-all behavior, first-match selection, Previous/Next behavior, and
status wording remain excluded and unranked.

The completed prerequisite implementation is confined to private Widgets context
transport and focused existing tests. It changes no public/installed ABI, Core
DTO, headless service, runtime-source or C API, configuration, persistence,
index format, adapter, dependency, build, catalog, locale loader, generated
file, executable, translation, or test registration. The result model continues
to expose only the established headword/Edit roles and dictionary-name tooltip.
P8-FT-60 through P8-FT-62, locked strings/captions/ranges, all completed
privacy and lifecycle behavior, and exactly 109 registered Release tests remain
unchanged. No successor after P8-FT-63 is selected, ranked, recommended, or
named. Completion unlocks only the private accepted-query article-highlighting
context dependency boundary.

### Phase 8 P8-FT-64 rendered-page text extraction transport prerequisite (complete)

The fresh independent documentation-only audit starts from synchronized
migrated HEAD, upstream, and live remote revision
`6f473bf7ffc3d256342a585ab19313fe0b52a003` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. With P8-FT-60
through P8-FT-63 complete, strict pinned-Qt5 parity keeps full-text rows
headword-only, retains the dictionary-name tooltip, and continues toward
article-page highlighting, first-match selection, and Previous/Next navigation.
Ignore-diacritics behavior does not advance.

The audit selects exactly P8-FT-64: establish the private asynchronous transport
of the successfully loaded article page's rendered plain text. Current
`main_window.cpp:7976-8110` owns the generation- and view-gated exact-result load
and literal article-search handoff, and `main_window.cpp:7632-7644` demonstrates
`QWebEnginePage::toPlainText`. Pinned legacy `articleview.cc:2569-2799` reads
rendered plain text before rematching, highlighting, selection, and navigation.

The implementation accepts extraction only for the same accepted-query and
search generations, lookup presentation generation, tab ID, `ArticleView`,
page, and monotonic navigation identity that
started it. Replacement, cancellation, failed activation or load, newer lookup
or search work, tab/view replacement, navigation, and teardown invalidate the
callback. P8-FT-64 transports inert text only and adds no Core call, matching,
normalization, literal derivation, JavaScript or DOM mutation, highlighting,
selection, navigation, status wording, or ignore-diacritics consumption.

The Shared-Library and GUI Boundary remains controlling. P8-FT-62 offsets,
matches, and excerpts are coordinates in indexed UTF-8 document plain text, not
the rendered page or DOM. Widgets must not recreate wildcard, regex, whole-word,
word-order, word-distance, case, or normalization rules. A later audit must
resolve the Core-owned transport-neutral rendered-text matching-plan interface
and its installed desktop-orchestration API/ABI shape; P8-FT-64 does not choose
one or select any dependent presentation leaf.

The completed prerequisite changes only private Widgets transport and focused
existing GUI smoke coverage. It preserves public/installed interfaces,
the C API, Core DTOs, configuration, persistence, index format, adapters,
dependencies, build behavior, catalogs, locale loading, translations, generated
files, executables, locked strings/captions/ranges, completed P8-FT-60 through
P8-FT-63 behavior, and exactly 109 registered Release tests. No successor after
P8-FT-64 is selected, ranked, recommended, or named. Completion unlocks only
the generation-safe rendered-page text extraction dependency boundary.

### Phase 8 P8-FT-65 rendered-text matching-plan facade prerequisite (complete)

The implementation starts from clean migrated HEAD, branch, upstream and live
remote at `1dce706344bd33254bedeb1e13d9b6eb5fa8c8c4` and clean read-only legacy at
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. GET selects Option A and exactly
P8-FT-65: add the installed `DesktopFacade` desktop-orchestration entry point
for Core-owned rendered-text rematching and plan production. The stateless
matcher remains private and is shared with indexed full-text search;
`DictionaryService` and the C API do not change.
This follows current `desktop_facade.h:167-193`, private matcher evidence in
`full_text_index.cc:231-319,368-465`, P8-FT-64 transport at
`main_window.cpp:8312-8356`, and pinned legacy rendered-text behavior at
`articleview.cc:2569-2788`.

The transport-neutral request carries bounded valid rendered UTF-8, accepted
query text, `FullTextQueryMode`, match-case, ignore-word-order, optional bounded
maximum-word-distance and a positive timeout. The synchronous facade operation
accepts optional `CancellationToken`. It deliberately omits
`ignore_diacritics`. Existing 4096-byte query and 1000-word-distance bounds and
new public `kMaximumRenderedTextMatchPlanBytes` of 16 MiB, aligned with the
private document-text bound, govern validation. The installed DTOs are
`RenderedTextMatchPlanRequest`, `RenderedTextMatchRange`,
`RenderedTextMatchPlanError`, and `RenderedTextMatchPlanResult`; the const
synchronous operation is `DesktopFacade::BuildRenderedTextMatchPlan` with an
optional cancellation pointer. Error values distinguish none, invalid request,
malformed pattern, cancellation, deadline, resource limit and contained
internal failure; diagnostic messages are not UI text. Empty query, invalid UTF-8,
oversized or incompatible inputs are invalid-request failures; malformed
patterns, cancellation, deadline, resource exhaustion and contained internal
failures are distinct typed outcomes and produce no partial plan. Successful
no-match, including empty rendered text, returns an empty plan.

Each authoritative plan item contains a valid-boundary rendered-text UTF-8 byte
offset/length and the exact literal bytes at that range. Items are deterministic,
leftmost-first, non-overlapping and offset-ordered. Adjacent matches are valid,
identical literals at different ranges remain separate, zero-length matches are
discarded, and scanning continues from the accepted match's exclusive end.
This order is sufficient for later first/Previous/Next consumption without
placing UI state in Core.

Widgets retains P8-FT-63/P8-FT-64 generation, lookup, search, navigation, tab,
view and page gates and discards stale results. P8-FT-62 indexed-document byte
ranges, excerpts and match text are never treated as rendered-text or DOM
coordinates. P8-FT-65 is complete through the installed facade DTO/vtable, one
private shared Core matcher, focused existing-target tests, and the installed
C++ consumer. It adds no DOM/JavaScript application, highlighting, selection,
navigation, status wording or diacritics behavior and
changes no result-row/tooltip behavior, configuration, C API, index/adapters,
dependencies, catalogs, generated files, executables, strings/ranges,
translations or test registration. The Release baseline remains exactly 109.
No successor after P8-FT-65 is selected, ranked, recommended or named.
Completion unlocks only the P8-FT-65 `DesktopFacade` matching-plan dependency
boundary.

### Phase 8 P8-FT-66 private match-plan worker/controller prerequisite (complete)

The implementation starts from clean synchronized
migrated/local/upstream/live-remote revision
`7596259baab285526438af205df4172032401f62` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. P8-FT-66 supersedes P8-FT-65's
historical no-successor closure. GET's Option B selects only P8-FT-66. The
private Widgets worker/controller accepts a complete
`RenderedTextMatchPlanRequest` and monotonic work generation by value, calls
the synchronous P8-FT-65 facade operation off the GUI thread with an explicit
cancellation token, and queues the typed result to the GUI thread. The facade
is borrowed only while attached; running and pending work stop before facade
replacement or teardown, and replacement submission cancels superseded work.
The implementation shape follows current
`full_text_request_controller.cpp:21-188`; current
`main_window.cpp:8320-8364` supplies the identity-gated rendered-text transport,
while pinned legacy `articleview.cc:2569-2791` remains evidence only for later
presentation behavior.

Main-window delivery retains GUI-only identity and accepts completion only for
the same work generation, accepted-query generation and exact query policy,
lookup presentation generation, article-search generation, navigation
generation, tab, view and page. Replacement activation or article search,
lookup/navigation invalidation, tab/view/page replacement, tab close, facade
detachment and application teardown cancel or invalidate the work. Successful
empty and nonempty plans and typed errors remain inert private state; cancelled,
stale, duplicate, detached and teardown completions are silent.

Focused implementation acceptance belongs to existing targets and must cover
by-value isolation, worker-thread invocation, GUI-thread completion, running and
pending cancellation, empty/nonempty success and typed error delivery, every
identity rejection boundary, and the absence of presentation effects. No test
is registered. P8-FT-66 changes no installed interface, Core DTO, C API, index
format, configuration, dependency, catalog, translation or executable. It does
not call `findText`, execute DOM/JavaScript application, highlight, select,
navigate matches, change status wording or consume `ignore_diacritics`.
Headword-only rows, dictionary tooltips and exact activation remain unchanged.
No successor after P8-FT-66 is selected, ranked, recommended or named.
Completion unlocks only generation-safe private match-plan availability.

P8-FT-66 is complete. The private controller cancels and joins work across
replacement and teardown, and MainWindow accepts queued typed completion only
after the complete query, generation, tab, view and page identity remains
current. Empty/nonempty plans and typed failures are inert; no matching-plan
result is applied to presentation. Existing targets cover the leaf without
changing the exactly 109-test Release registration baseline.

### Phase 8 P8-FT-67 private CSS Custom Highlight plan application (completed)

The completed implementation starts from the selected clean synchronized
migrated/local/upstream/live-remote revision
`c8bfcd77e01a243e3b565ebc818151c2255a0a2c` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It supersedes P8-FT-66's
historical no-successor closure and selects only P8-FT-67, the smallest
user-visible application leaf now that a current ordered plan is available as
private GUI state. Current evidence is `main_window.cpp:8479-8695`,
`main_window.h:499-523`, `article_view.cpp:70-107` and `conanfile.py:51-58`;
pinned legacy evidence is `articleview.cc:2569-2791`.

Qt 6.11 `findText` has no Qt5 WebKit `HighlightAllOccurrences` flag, so repeated
single-literal calls cannot preserve the legacy simultaneous highlight-all
presentation. P8-FT-67 uses a private bounded ApplicationWorld script and CSS
Custom Highlight ranges instead. After the existing complete identity gate,
the script feature-probes the required CSS Highlight, Range, Selection and DOM
traversal capabilities, maps the ordered Core-authored rendered-text ranges to
the unchanged DOM, validates every literal and constructs the entire result
before replacing presentation. It registers all DOM occurrences of every
unique supplied literal under a private tab/page highlight using CSS system
mark colors, selects and scrolls the range mapped from the first ordered plan
item, and separately retains generation-bound ordered ranges at current
position zero. Literal grouping follows the retained match-case policy. It does
not insert wrappers, change text nodes, parse the query, rematch Core semantics
or consume `ignore_diacritics`.

Each transaction, callback and published application carries a generation-bound
owner token. Stale or failed work cleans only its own staging and cannot clear a
newer owner; explicit current lifecycle invalidation may force-clear that
owner. Empty success clears the private highlight and applied state. Unsupported APIs,
mapping/literal mismatch, script failure, stale callback and every existing
activation, search, lookup, navigation, tab, view, page, facade or teardown
invalidation atomically leave no private highlight, selection or applied state.
Focused acceptance belongs to existing GUI smoke targets and adds no registered
test. P8-FT-67 changes no installed/Core/C API or DTO, index, configuration,
dependency, catalog, translation or executable; headword-only rows, exact
dictionary tooltips and activation, ordinary find-in-page, status wording and
the exactly 109-test Release baseline remain unchanged. Previous/Next commands
remain outside the leaf. No successor after P8-FT-67 is selected, ranked,
recommended or named. Completion will unlock only generation-bound ordered
applied-range state.

### Phase 8 P8-FT-68 private ordered applied-range navigation command prerequisite (completed)

The completed implementation was selected by clean synchronized revision
`67dd57c26f1f7b7af1021ffb1041947ddb0c2f20` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It supersedes P8-FT-67's
historical no-successor closure and selects only P8-FT-68, the smallest
dependency-ready consumer of the private ordered ranges and position published
by current `article_view.cpp:90-256` and retained with accepted identity by
`main_window.h:499-523`. Pinned legacy `articleview.cc:2703-2704,2730-2788`
defines one-step, non-wrapping Previous/Next navigation, disabled boundary
directions and the state used for match-count presentation.

The leaf adds one private, asynchronous `ArticleView` navigation command with
an expected application token and direction. A current nonempty owner moves
exactly one range only when that direction is available; selection, scrolling
and zero-based position update atomically. A boundary-unavailable direction
returns the unchanged current snapshot and has no presentation effect. Missing,
empty, stale and token-mismatched state is rejected without effects. The typed
private result distinguishes current accepted state from rejection and returns
token, position, ordered count, `can_previous` and `can_next` equivalents for a
later identity-checked UI consumer.

P8-FT-68 adds no controls, button binding, status mutation, translated strings,
F3/shortcut behavior or `findText` use, and it does not consume
`ignore_diacritics`. P8-FT-67 highlighting and initial position zero, ordinary
find-in-page, result activation, headword-only rows and dictionary tooltips are
unchanged. No installed/Core/C contract or DTO, index, configuration,
dependency, catalog, translation, executable or test registration changes;
the Release baseline remains exactly 109 tests. No successor after P8-FT-68 is
selected, ranked, recommended or named. Completion unlocks only private
full-text navigation UI/status binding.

### Phase 8 P8-FT-69 private per-article full-text navigation row binding (completed)

The completed implementation was selected by clean synchronized revision
`35b2ffb94d3b819b7fe6242585fc5fa0729906b9` and unchanged clean pinned
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` supersedes P8-FT-68's
historical no-successor closure and completed only P8-FT-69. The implementation
recreates the pinned `articleview.ui:58-100` dedicated per-article
`ftsSearchFrame` below the web view, with exact source captions `&Previous` and
`&Next` and an independent status label. Pinned legacy
`articleview.cc:220-230,2688-2706,2730-2788` supplies the exact translated
`%1 of %2 matches` wording, one-based presentation, initial and boundary
enablement, and one-step non-wrapping behavior.

Each migrated `ArticleView` privately owns its row. It is visible only for a
current nonempty applied-range owner and is hidden and cleared on replacement,
page/load/view invalidation, tab close and teardown. Initial publication and
later commands bind status and button enablement only from an accepted P8-FT-68
typed snapshot after complete composition-root identity validation. Rejected,
stale, token-mismatched, detached and teardown callbacks make no UI change.
Accepted status uses `position + 1` and `ordered_count`; button state uses only
`can_previous` and `can_next`.

The row remains strictly separate from ordinary find-in-page controls, state,
status, shortcuts and `findText`. F3 remains exclusively the migrated
Dictionaries shortcut; P8-FT-69 adds no F3/Shift+F3 full-text binding. No
installed/Core/C interface or DTO, index, configuration, dependency, catalog,
activation, dictionary-name tooltip or headword-only result contract changes.
`ignore_diacritics` consumption remains separately unresolved and unranked.
Focused coverage changes only the existing GUI smoke target, registers no test,
and preserves the exactly 109-test Release baseline. No successor after
P8-FT-69 is selected, ranked, recommended or named.

### Phase 8 P8-FT-70 ICU normalized matching and origin-map prerequisite (complete)

P8-FT-70 was implemented from synchronized selected revision
`c91dfc628bea5382c2dc10182e848561c919305e` with clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It completes the selected Core
semantics and origin mapping required before `ignore_diacritics` can be
consumed by the rendered-text path.

The prior matcher used per-scalar mapping and made
diacritic removal ineffective when `match_case` is true. Current
`desktop_facade.h:169-177`, `desktop_facade.cc:130-164` and
`main_window.cpp:9040-9078` retain the separate Widgets value but intentionally
omit it from the rendered-text request and hard-code the matcher option false.
Pinned legacy `fulltextsearch.cc:596-609` and
`articleview.cc:133-190,2569-2648` establish independent case/diacritic policy
and original-position recovery. GET intentionally adopts ICU rather than Qt5's
custom diacritic tables and trailing `Mark_NonSpacing` rule; migrated behavior
uses ICU full folding, canonical equivalence and all `Mn`, `Mc` and `Me` marks.

The completed contract applies the same pipeline to query and source: NFD;
optional ICU full case fold only for case-insensitive matching; NFD again;
optional removal of all three Unicode Mark categories only when diacritics are
ignored; and NFC. The source path propagates original UTF-8 spans through
decomposition, folding expansion, canonical reordering, mark removal and NFC
contraction. A complete original cluster is a non-Mark scalar with its
immediately attached Marks; a leading or unattached Mark stays independent and
cannot attach across whitespace or punctuation.

Every normalized match maps to the minimal contiguous complete-original-
cluster range covering its touched normalized units, including a boundary
inside a multi-unit expansion. Repeated normalized units sharing one origin,
including `ß` folded to `ss`, yield at most one nonempty original occurrence.
Acceptance advances past all normalized units overlapping the accepted origin;
empty, backward, duplicate or overlapping mapped candidates are skipped with
continued cursor progress. Results remain leftmost-first and non-overlapping,
and their text is the exact complete UTF-8 original slice.

Focused existing Core targets cover the four policy combinations; precomposed,
decomposed and reordered canonical equivalents; case-fold expansion and
fold-emitted marks; contraction; repeated normalized units with one origin;
attached `Mn`/`Mc`/`Me`, standalone marks and supplementary scalars; all query
modes, every valid word-order/distance combination, repeated/adjacent matches,
cancellation, deadlines, malformed patterns, unchanged rejection of invalid
pattern-mode combinations and indexed/private-matcher agreement.
`full-text-v1` and the exactly 109-test Release registration baseline remain
unchanged.

Completed P8-FT-70 is Core-only. It corrects existing combined-flag
`FullTextQuery` behavior without changing public type shape, ABI, C API, DTOs,
facade request,
configuration, dependencies, catalogs, translations, activation, tooltips,
headword-only results, ordinary find or Widgets. Rendered request and Widgets
consumption remain unresolved. No successor is selected, ranked, recommended
or named; completion unlocks only the Core normalized matching/origin-map
dependency boundary.

### Phase 8 P8-FT-71 rendered-text ignore-diacritics consumption (completed)

P8-FT-71 was implemented from synchronized migrated
revision `14b78a90dd5a37dfa4a3381aeebf7a559eb9ae5d` and clean pinned legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Current
`desktop_facade.h:169-178`, `desktop_facade.cc:130-164`,
`main_window.h:455-522`, `main_window.cpp:9063-9344`,
`rendered_text_match_plan_controller.cpp:24-215` and
`article_view.cpp:88-205` prove that Widgets owns accepted presentation and
lifecycle identity, the controller copies work by value, Core owns matching,
and ArticleView/JavaScript only apply Core-authored exact original ranges.
Pinned legacy `fulltextsearch.cc:596-609` and
`articleview.cc:133-190,2569-2648` establish independent case/diacritic
semantics and mapping back to original text.

The implementation leaf adds default-false `ignore_diacritics` to the
transport-neutral installed `RenderedTextMatchPlanRequest`, populates it from
the accepted generation-bound value, and passes it to the existing Core
matcher. The request becomes the sole immutable policy snapshot: the duplicate
Widgets identity member is removed and existing completion/navigation stale
checks use the request member. This necessary public DTO source/layout change
does not change the facade method or vtable, C API, configuration, dependency,
index-format or `full-text-v1` serialization contracts.

P8-FT-70 ICU matching and exact original UTF-8 literals remain authoritative;
the Qt5 custom folding and trailing `Mark_NonSpacing` extension remain an
intentional divergence. ArticleView's payload and JavaScript gain no
diacritic-normalization responsibility. Exact DOM range application,
highlight-all, first selection, P8-FT-69 navigation/status, cancellation,
replacement, teardown and tab/view/page/generation isolation remain intact,
as do ordinary find-in-page, activation, translations, headword-only results
and dictionary tooltips. Existing targets gain focused coverage without a new
registration, preserving exactly 109 Release tests. No further successor is
selected, ranked, recommended or named.

### Phase 8 P8-FT-72 Core full-text index lifecycle contract prerequisite (completed)

P8-FT-72 was implemented from synchronized migrated revision
`0b84e6dc2ab627c613c483a931b995f6c0554191` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It resolves the former Core
lifecycle/policy blocker by assigning authoritative full-text index policy and
lifecycle coordination to Core. Widgets is limited to editing policy, issuing
rebuild/cancel intents and consuming immutable snapshots. Format adapters
report capability/source revision and perform bounded cancellable work without
owning policy or global coordination.

Current `application.h:268-278`, `dictionary_backend.h:30-40` and
`full_text_index.h:35-67` stop at persisted policy inputs and private adapter/
index state. Pinned legacy `config.hh:156-180`, `dictionary.hh:423-436`,
`fulltextsearch.cc:34-125` and
`mainwindow.cc:1381-1393,2100-2101,2158-2165,2288-2303` establish the policy,
bounded work and GUI-owned scheduling/cancellation responsibilities that the
migrated Core boundary must separate.

The completed private transport-neutral Core application/domain contract
contains the lifecycle policy inputs, generation- and dictionary-identified
rebuild/cancel intents, immutable snapshots, bounded work request/result values
and an abstract format-work port. The port reports capability and opaque source
revision, accepts existing Core cancellation/deadline types and contains work
failures. Focused contract tests use a deterministic fake port in the existing
`full_text_index_test` target and add no registration.

P8-FT-72 excludes coordinator execution, policy persistence application,
Preferences UI, facade/Widgets wiring, visible readiness/progress/status, real
adapter conversion, index serialization and the complete rebuild workflow. It
does not select progress, queue/concurrency, two-pass ordering, retry or failure
presentation policy. All installed/Core/C/DTO/configuration/dependency/
serialization contracts and completed P8-FT behavior remain locked, including
ordinary find-in-page, Dictionaries-only F3, translations and exactly 109
Release registrations. No successor is selected or ranked.

### Phase 8 P8-FT-73 private Core full-text index lifecycle coordinator (completed)

The post-P8-FT-72 readiness audit used synchronized migrated revision
`fbc50b18fb183f69c34b524db869140a3760da25` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Current
`full_text_index_lifecycle` provided the private contract and
exception-containing work boundary now used by the completed coordinator.
`dictionary_service.cc:1812-1912` composes backends into the private service and
`desktop_facade.cc:68-88` composes that service into the application, without
applying full-text lifecycle policy or producing lifecycle snapshots.

P8-FT-73 completes one private Core coordinator state machine. It owns one
current generation per registered dictionary ID, samples the
selected port's capability and opaque source revision for that exact identity,
and converts an explicit rebuild intent into requested state. Only a separately
submitted exact-identity bounded request publishes working and invokes the
port. Only an exact
current `(generation, dictionary_id)` result may publish current, cancelled or
failed. Unsupported capability publishes unavailable; supported dictionaries
without accepted work remain not-indexed. Replacement makes every older result,
failure or cancellation completion stale. Cancellation is scoped to the exact
identity, idempotent and propagated through the existing token; escaped adapter
exceptions remain contained as failed work results.

Pinned legacy `fulltextsearch.cc:31-125` owns GUI-side two-pass background
indexing, readiness checks, shared cancellation, joining and exception
containment. Pinned legacy
`mainwindow.cc:1381-1393,2100-2101,2164-2165,2180-2181,2302-2303` stops and
clears work before dictionary/preference replacement, reapplies per-dictionary
policy and restarts afterward. This establishes lifecycle responsibility and
stale-work exclusion, but the legacy two-pass ordering does not become policy
in the selected coordinator leaf.

P8-FT-73 excludes automatic startup/recomposition scheduling, policy
persistence/application, queue/concurrency and retry policy, two-pass ordering,
progress, real adapter conversion, facade/Widgets transport, visible readiness/
status/failure UI, serialization and the complete rebuild workflow. It changes
no installed/public C++, facade, C, DTO or configuration ABI, dependency,
`full-text-v1`, ordinary find-in-page, Dictionaries-only F3, translation,
completed full-text behavior or test registration. The Release baseline remains
exactly 109. The private coordinator is complete; no dependency beyond it is
selected or named.

### Phase 8 P8-FT-74 Core article-count lifecycle-policy ingress (completed)

The completed implementation used synchronized migrated base revision
`a5e013d8164550f4757c6d4a58949cb575624989` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Of persisted-policy application,
one real adapter bridge, composition ownership and facade/Widgets transport,
the implementation completes only the value-only persisted-policy ingress.
Current
`application.h:268-278`, `configuration.cc:141,189,219-220,746-750,1542,1557-1559`
and `legacy_configuration.cc:403,451,471-472` provide its complete input seam.
Pinned legacy `config.hh:156-181`, `preferences.ui:1371-1395`,
`preferences.cc:360-382,490-575` and representative `stardict.cc:202-206`
establish maximum dictionary size as an article count, accepted in
`0..10000000` with zero unlimited.

P8-FT-74 corrects the misleading C++ names to installed
`full_text_maximum_dictionary_articles` and private
`maximum_dictionary_articles` before consumption, while retaining the same
`std::uint32_t` position and layout. The installed C++ source member rename is
intentional and documented; other public/facade/C/DTO interfaces do not
change. For persistence compatibility, the current configuration continues to
read and canonically emit the frozen misnamed key
`full_text_maximum_dictionary_megabytes`, and legacy import retains
`fullTextSearch.maxDictionarySize`. No persisted-key or value migration is
required.

The private pure projection copies enabled, article-count limit and disabled-
format text unchanged into `FullTextIndexPolicy`. It does not parse format
tokens or apply per-dictionary eligibility. The lifecycle limit remains
separate from query `full_text_maximum_articles_per_dictionary`, which bounds
returned results. No byte-size limit or dual-field policy is introduced; a
future byte limit requires a new unambiguous field and key.

Focused existing tests cover default zero, both persistence paths, inclusive
`10000000`, rejected `10000001`, query-limit separation, corrected policy
equality and exact by-value projection without a new registration. Real port
bridges, coordinator composition, automatic apply/restart, facade/Widgets
transport, UI, progress, serialization and complete rebuild behavior remain
excluded. P8-FT-72/73 semantics and all other locked surfaces remain unchanged,
including exactly 109 Release registrations. P8-FT-74 is complete. No next
dependency is selected.

### Phase 8 P8-FT-75 private registration metadata and policy eligibility

P8-FT-75 completes the private Core prerequisite that defines immutable
registration metadata, a pure eligibility predicate and the separate
`kPolicyExcluded` lifecycle state. Its implementation is grounded in
synchronized migrated base revision
`18023f3aebae9ad610fa1b9afcb505dc946b7a1a` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.

Current `dictionary_service.cc:450-464,687-1036` owns stable dictionary-ID and
format-specific catalog construction, all twelve textual dictionaries copy
their reader article counts into authoritative identity, and
`full_text_index_lifecycle.h/.cc` owns policy, the narrow format-work port and
the explicit coordinator. Pinned legacy `stardict.cc:202-206` and the matching
checks in `aard.cc`, `bgl.cc`, `dictdfiles.cc`, `dsl.cc`, `epwing.cc`, `gls.cc`,
`mdx.cc`, `sdict.cc`, `slob.cc`, `xdxf.cc` and `zim.cc` establish the closed
format set and exact Qt5 rule.

The private copied metadata contains dictionary ID, authoritative
`std::size_t` article count and one exact case-sensitive ASCII format type from
`AARD`, `BGL`, `DICTD`, `DSL`, `MDICT`, `SDICT`, `SLOB`, `STARDICT`, `XDXF`,
`ZIM`, `EPWING` or `GLS`. Empty, unknown, differently cased, embedded-NUL and
non-ASCII values are rejected before duplicate lookup, port probing,
registration, snapshot, cancellation or generation mutation. The metadata ID
is the sole registration key. Composition/catalog owns eventual production of
metadata and a lifetime-safe port, but this leaf wires neither; the port stays
limited to capability, source revision and bounded cancellable work.

Core eligibility requires enabled policy, no occurrence of canonical format
type in raw disabled text under explicit length-aware ASCII case-insensitive
substring matching, and a zero limit or inclusive article count. Folding maps
only `A`-`Z` to `a`-`z`; other bytes, embedded NUL and non-ASCII data compare
unchanged. Partial substrings remain exclusions without locale dependence,
tokenization, trimming or delimiter normalization.

Accepted unsupported generations remain `kUnavailable`; supported but
ineligible generations become `kPolicyExcluded`, replace and cancel older
work, remain capability true, retain empty source revision and call neither
revision nor work. Supported eligible generations capture revision and become
requested, with revision exceptions contained as failed. Exact cancellation,
rejected excluded execution, stale-completion suppression and later eligible
recovery preserve P8-FT-73 identity and lifecycle rules. Generation-zero
snapshots keep the existing technical probe because no policy is yet accepted.

Focused acceptance extends only `full_text_index_test`: all twelve canonical
values, atomic invalid cases, copied metadata, ASCII substring and NUL/non-
ASCII edges, zero/equal/over article limits, state precedence, excluded no-call
behavior, cancellation, stale replacement and eligible recovery. No test is
registered, preserving exactly 109. Real adapters, composition wiring,
automatic apply/restart, facade/Widgets transport, UI, scheduling, progress,
retry, legacy two-pass ordering, serialization and complete rebuild remain
excluded, as do changes to every locked public, dependency, search, F3 and
translation surface. P8-FT-75 is complete. No successor is selected or named.

### Phase 8 P8-FT-76 private immutable full-text index publication contract (completed)

The completed implementation used synchronized migrated base revision
`c32aa1217bb9934b4b8ede1e1623a12c7a1777b3` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It implements only the private Core
snapshot-publication prerequisite needed before a real lifecycle adapter can be
composed. The twelve migrated textual adapters currently construct and retain
an unsynchronized `std::optional<FullTextIndex>`; none can safely replace that
value while full-text search or document resolution is in flight.

P8-FT-76 introduces a narrow private Core abstraction over an optional
immutable `std::shared_ptr<const FullTextIndex>` snapshot. The holder's sole
responsibility is acquisition and safe replace-on-success publication. Search
and resolution depend on this abstraction instead of atomic or mutex details;
building, lifecycle and scheduling remain separate. Index construction occurs
off-side through bounded incremental traversal. Only a complete successful
replacement is atomically published, and every search or resolution call keeps
its acquired snapshot alive for the complete operation. Old and new readers
may overlap safely, but no reader observes partial construction, in-place
mutation or destruction. Null, failed, cancelled, expired, over-budget and
stale publication attempts leave the current snapshot unchanged.

No further interface or pattern is introduced without concrete ownership,
testability or maintenance value.

The real adapter bridge and its exact generation-authorized publication
handoff remain outside this leaf. Core lifecycle/eligibility ownership,
composition/catalog ownership of immutable metadata plus port, and the narrow
capability/source-revision/bounded-work port remain unchanged. No persisted-
policy apply/restart, scheduler, progress, facade/Widgets transport, UI,
serialization or complete rebuild workflow is selected. Canonical validation,
`kPolicyExcluded`, the zero-unlimited `0..10000000` article-count rule,
intentional ICU divergence, dependencies and exactly 109 registrations remain
locked. P8-FT-76 is complete. No successor is selected or named.

### Phase 8 P8-FT-77 private bounded AARD full-text traversal prerequisite (completed)

The completed implementation is grounded at migrated base revision
`a4393dcb1fd3bf4cd13938dddc909ef96af26135` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The migrated base
`aard_reader.h:42-68`, `aard_reader.cc:483-499` and
`aard_dictionary.cc:55-92` showed AARD materializing all source articles before
materializing full-text documents. Pinned legacy `aard.cc:609-635`,
`ftshelpers.cc:298-366` and `fulltextsearch.cc:34-64` prove delegated per-
article cancellation and keep scheduling and two-pass ordering separate.

P8-FT-77 implements only a private synchronous AARD reader traversal. It invokes
the caller checkpoint before every record inspection, emits one article for
the first record owning each article, preserves current record order and exact
ordinal/headword/payload fields, and gives the visitor no ownership beyond the
call. Checkpoint and visitor exceptions propagate immediately with no later
emission. The construction-time AARD consumer changes to this seam while
retaining exact `aard-index:<record_ordinal>:<article_ordinal>` identity,
article assembly, generated-index behavior and error mapping.

The traversal does not own lifecycle request bounds: a later work consumer
must enforce document, per-document-byte and corpus-byte limits alongside
cancellation and deadline checkpoints. This leaf adds no cross-format
contract, real work port, composition/catalog registration, generation-
authorized publication, automatic policy application/restart, scheduling,
progress, facade/Widgets transport, UI or serialization. Core lifecycle and
eligibility ownership and complete immutable snapshot publication remain
locked. Existing AARD tests are extended without registering a test, keeping
the Release baseline at exactly 109. P8-FT-77 is complete. No successor is
selected or named.

### Phase 8 P8-FT-78 private generation-authorized immutable snapshot handoff prerequisite (completed)

The completed implementation is grounded at migrated base revision
`5c58b1ead60aece993bd41d49ce763ad67940a47` and clean legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. At that base,
`full_text_index_lifecycle.cc:241-287` performed bounded work outside the
coordinator lock and checked current generation only after return;
`full_text_index_snapshot.cc:10-24` atomically replaces complete immutable
snapshots. Publishing within a real AARD port could therefore have exposed a
stale result before Core rejected that generation. Legacy
`fulltextsearch.cc:34-70,100-111`,
`mainwindow.cc:1381-1393,2100-2101,2158-2165,2180-2181,2288-2303` and
`aard.cc:609-635` support cancellation and stop/apply/restart parity without
changing this migrated ownership requirement.

P8-FT-78 implements only a private result-to-publication handoff. Successful
bounded work returns an unpublished non-null immutable index candidate.
Composition/catalog registers the immutable metadata, lifetime-safe port and
the snapshot holder shared with dictionary readers. The Core coordinator
revalidates exact dictionary/generation identity and cancellation under its
existing synchronization boundary, atomically publishes the candidate, and
then records `kCurrent`. Every stale, cancelled, failed, exceptional, expired,
over-budget or mismatched outcome leaves the previous snapshot untouched;
completed work without a candidate is failure, not current.

No real adapter bridge or composition wiring is included. The port remains
capability/source-revision/bounded cancellable work only, AARD retains its
private callback traversal, and Core retains lifecycle/eligibility and
publication authorization. Automatic policy application/restart, scheduling,
progress, facade/Widgets transport, UI, serialization and later format work
remain excluded. All P8-FT-72 through P8-FT-77 behavior, canonical formats,
`kPolicyExcluded`, article limits, immutable reader retention, `full-text-v1`,
ICU divergence, public/installed boundaries, ordinary find/F3, translations
and exactly 109 Release registrations remain unchanged. P8-FT-78 is complete.
No successor is selected or named.

### Phase 8 P8-FT-79 private AARD full-text format-work bridge (complete)

The independent post-P8-FT-78 audit used synchronized migrated revision
`7a07e41b7ad0e9613a93129bd55c5cf598e06166` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Current AARD code already supplies
the sole-container source snapshot, P8-FT-77 bounded traversal, stable
first-record document construction and generated-index path. P8-FT-76/78
supply the immutable holder, complete candidate result and Core-authorized
publication. Pinned legacy AARD and shared FTS code confirm cancellable
article traversal and GUI-owned stop/apply/restart. Therefore a separate AARD
builder/result or source-revision leaf would add no independent ownership or
testability value.

P8-FT-79 implements the private AARD `FullTextIndexFormatWorkPort` bridge, its
authorization-safe prepared-artifact prerequisite and production registration.
The port reports capability only for a configured
index destination, derives its opaque revision deterministically from the sole
`.aar` source stamp, rejects request/revision mismatch, and converts bounded
P8-FT-77 visits into the existing assembled documents and stable AARD IDs. It
checks cancellation/deadline at every record checkpoint, applies nonzero
request document, per-document-byte and corpus-byte limits with overflow-safe
totals, builds entirely off-side and returns a non-null immutable candidate on
success without publication or canonical persistence. Only the coordinator
finalizes the prepared artifact after exact generation/cancellation
revalidation and before holder publication.

The AARD dictionary replaces direct optional-index ownership with one shared
snapshot holder seeded by construction-time create/reuse. Search, index state,
availability and document resolution retain one acquired snapshot per call.
The private catalog owns the dictionary, holder and port, constructs exact
immutable `{dictionary ID, "AARD", authoritative article count}` metadata and
registers the shared objects with the existing Core coordinator. Registration
is atomic for that entry and schedules no work.

Implementation stays in existing source and test targets and adds no test
registration. Acceptance covers exact metadata/revision, create/reuse/stale and
corrupt rebuild results, stable document identity and content, holder-backed
reads, catalog lifetime, every request bound, overflow, cancellation, deadline,
revision drift and contained traversal/assembly/index failures. Unsuccessful or
stale work leaves the published holder unchanged, and only the coordinator may
publish after its exact generation/cancellation check.

Automatic persisted-policy apply/restart, scheduler, progress, facade/Widgets
transport, UI, serialization changes and complete rebuild orchestration remain
excluded. All P8-FT-72 through P8-FT-78 contracts, canonical identifiers,
`kPolicyExcluded`, article/work bounds, ICU divergence, public/installed
surfaces, dependencies, ordinary find/F3, translations and exactly 109 Release
registrations remain unchanged. P8-FT-79 is complete. No successor beyond it
is selected or named.

### Phase 8 P8-FT-80 persisted full-text policy application (completed)

The independent post-P8-FT-79 audit used synchronized migrated revision
`d79180de54bc19076ff3eae4743cf5de40a40e18` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Current Core already projects
persisted preferences into `FullTextIndexPolicy`, owns canonical eligibility,
retains immutable registration metadata and replaces generations
monotonically. Composition already retains those preferences and owns the
completed AARD dictionary, holder and port registration. Pinned legacy applies
full-text parameters to dictionaries before starting or restarting its indexer.
The audit therefore selects only P8-FT-80: apply persisted policy to registered
dictionaries before any reconciliation or scheduling boundary.

The completed leaf adds one private coordinator-wide application operation. It
assigns a strictly newer generation to every entry present for that
application,
re-evaluates capability and the existing eligibility predicate, captures the
eligible source revision, cancels the superseded generation and transitions to
exactly `kUnavailable`, `kPolicyExcluded`, `kFailed` or `kWorkRequested`.
Applying to no registrations succeeds without effect; applying repeatedly is
monotonic. Composition calls the operation once after discovery with the
policy projected from the already-loaded application preferences. Only AARD is
affected in production because no other real format-work port is registered.

Acceptance extends only existing lifecycle and application-service tests. It
proves enabled and disabled policy, disabled format matching, article-count
thresholds, capable and incapable ports, source-revision failure, multiple and
zero registrations, repeated application, strictly newer per-entry generations
and cancellation of superseded requested or working work. Cancellation is
cooperative; an excluded, unavailable, failed, stale or cancelled completion
cannot finalize an artifact, mutate the canonical index, publish a holder
snapshot or reach `kCurrent`.

P8-FT-80 does not schedule or execute work and adds no startup/restart
reconciliation, progress/status surface, real port, facade/Widgets transport,
UI, serialization, public/installed API, dependency or test registration.
P8-FT-72 through P8-FT-79, `full-text-v1`, canonical IDs,
`kPolicyExcluded`, article/work bounds, ICU divergence, ordinary find/F3,
UI/translations and exactly 109 registrations remain locked. Reconciliation,
scheduling/submission, visibility, other formats and UI transport remain
unselected and unranked. No successor beyond P8-FT-80 is selected or named.

### Phase 8 P8-FT-81 private startup full-text artifact reconciliation (complete)

The independent post-P8-FT-80 audit used synchronized migrated revision
`73f9e6cd976379e5f16a4c7eb5deb4e1f965ad80` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Current AARD construction opens or
rebuilds the canonical full-text artifact and publishes its immutable snapshot
before composition registers the holder and port. Persisted-policy application
then creates an eligible `kWorkRequested` generation without consulting that
startup artifact. Pinned legacy checks `haveFTSIndex()` before each bounded
indexing pass. The audit therefore selects only P8-FT-81: reconcile exact
requested generations with already validated startup artifacts before any work
submission or scheduling boundary.

The completed leaf adds a private format-to-Core evidence seam and one private
reconciliation operation. Immutable evidence binds the startup snapshot to the
captured source revision and exact current identity. If capability, eligibility,
identity, revision, holder snapshot and uncancelled state still match, Core
changes only that generation from `kWorkRequested` to `kCurrent`. It performs no
work-port call, preparation, finalization, holder publication, canonical write
or generation allocation.

Absent, stale, corrupt or otherwise unverifiable evidence, a startup build that
published no snapshot, revision-mismatched, cancelled, replaced, excluded,
unavailable, failed and non-requested entries remain unchanged. A snapshot
successfully rebuilt from a stale or corrupt on-disk artifact is valid evidence;
reconciliation performs no additional canonical write. Repeated reconciliation
is idempotent; zero and multiple entries are deterministic; stale evidence
cannot authorize persistence, publication or a state transition. Production
scope is only the completed AARD registration.

Acceptance extends only the existing `full_text_index_test`,
`aard_dictionary_test` and `application_service_test` registrations. It proves
exact identity/revision/snapshot binding, retained snapshot identity, no
port call or additional artifact rewrite, idempotence, zero/multiple entries,
successfully rebuilt startup snapshots and every rejection above. P8-FT-81 adds
no scheduler, thread, queue, retry,
progress/status surface, additional format bridge, facade/Widgets transport,
UI, serialization, public/installed API, dependency or test registration.
P8-FT-72 through P8-FT-80, `full-text-v1`, canonical IDs, `kPolicyExcluded`,
article/work bounds, ICU divergence, ordinary find/F3, UI/translations and
exactly 109 registrations remain locked. No successor beyond P8-FT-81 is
selected or named.

### Phase 8 P8-FT-82 private bounded full-text work-request projection (complete)

The fresh post-P8-FT-81 audit used synchronized migrated revision
`333bdbbca0812c8289bdc3194d66cd17300ecbee` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. At that baseline, the private
lifecycle request carried document, per-document byte, corpus byte and deadline
bounds, but the execution operation accepted those values from its caller.
AARD rejected zero bounds, and pinned legacy checked index readiness before
launching its background pass without defining transport-neutral byte or
deadline limits. The audit selected only P8-FT-82: make Core project the
validated bounded request for the exact eligible startup-unreconciled
`kWorkRequested` generation before any work-submission ownership is introduced.

The completed leaf defines an immutable private execution-bounds value and a
side-effect-free coordinator projection. Resource bounds must all be nonzero,
the absolute deadline must be in the future, and validation must be
overflow-safe; the corpus limit cannot mathematically exceed the
document/per-document product, which need not be representable. The
coordinator requires the exact current
uncancelled requested identity, rechecks capability and policy eligibility, and
supplies identity, policy, captured source revision and cancellation from
authoritative generation state. Success returns a request without invoking its
port or moving it to `kWorking`.

Zero, expired or mathematically incoherent bounds and unknown, stale, replaced,
cancelled, unavailable, excluded, failed, working, current or otherwise
non-requested identities return no request. They cannot mutate lifecycle state,
prepare or finalize an update, write an artifact, publish a holder snapshot or
allocate a generation. Repetition is deterministic and side-effect-free, and
multiple registrations remain isolated.

Acceptance extends only the existing `full_text_index_test` registration. It
proves exact bounds and deadline projection, coordinator-authoritative lifecycle
fields, every rejection above, no format-port call, zero/multiple-entry
behavior, and compatibility with the completed bounded execution and
persistence-safe handoff contracts. P8-FT-82 adds no code registration,
submission, scheduler, dispatcher/executor ownership, thread, queue,
shutdown/join, retry, progress/status, additional format bridge, facade/UI
transport, public/installed API or dependency. P8-FT-72 through P8-FT-81,
`full-text-v1`, canonical IDs, `kPolicyExcluded`, article/work bounds, ICU
divergence, ordinary find/F3, UI/translations, stale/artifact/snapshot safety
and exactly 109 registrations remain locked. Delivery changes exactly the
private lifecycle header and implementation, the existing lifecycle test and
these four governing documents. P8-FT-82 is complete, and no successor is
selected or named.

### Phase 8 P8-FT-83 private deterministic full-text work discovery (complete)

The implementation used synchronized migrated revision
`93590fa656b06d31bdd3c92bc477f72fdbb5256f` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The coordinator owns accepted
generations in an ordered private registry, creates eligible `kWorkRequested`
generations, discovers their identities and retains exact-identity projection
as the authoritative bounded safety gate. Pinned legacy
`fulltextsearch.cc:34-125` discovers work while scanning dictionaries before
its background owner executes it; the migrated implementation separates
discovery from executor ownership.

P8-FT-83 adds one side-effect-free coordinator query,
`std::vector<FullTextIndexWorkIdentity> DiscoverRequestedWork() const`. It
returns snapshots of only accepted current `kWorkRequested` generations that
remain format-capable, policy-eligible, uncancelled and backed by a cancellation
token. Results use canonical dictionary-ID order from the existing registry;
this is deterministic discovery, not scheduler priority or legacy two-pass
policy. Zero registrations or no actionable entries return an empty vector.

Discovery claims no generation, constructs no bounds, invokes no format port,
reads or writes no artifact, prepares or finalizes no update, publishes no
snapshot, requests no cancellation and allocates no generation. Returned
identities are observations that may become stale; P8-FT-82 must revalidate
each exact identity and its bounds before later submission. Repetition is
deterministic while lifecycle state is unchanged, and registrations remain
isolated.

Acceptance extends only the existing `full_text_index_test` registration. It
proves empty, single and multiple-entry discovery; canonical dictionary-ID
order; inclusion of only the latest actionable generation; exclusion of
unavailable, not-indexed, policy-excluded, working, current, cancelled and
failed entries; policy/capability/cancellation revalidation; no observable
mutation or port call; and successful projection of a discovered identity plus
safe rejection of a subsequently stale identity. P8-FT-83 adds no executor,
dispatcher, submission, thread, queue, concurrency limit, shutdown/join,
retry, progress/status, two-pass ordering, format bridge, facade/UI transport,
public/installed API, dependency or registration. P8-FT-72 through P8-FT-82,
`full-text-v1`, canonical IDs, `kPolicyExcluded`, bounds, ICU, find/F3,
UI/translations, stale/artifact/snapshot safety and exactly 109 registrations
remain locked. Delivery uses the exact lifecycle header, implementation,
existing lifecycle test and four-document allowlist. P8-FT-83 is complete. No
successor beyond P8-FT-83 is selected or named.

### Phase 8 P8-FT-84 private serial full-text work executor (complete)

The implementation at synchronized migrated base revision
`148f33510cf5dbbfa5527dafed4525f99219217a` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8` completes one smallest scheduling
leaf. Lifecycle code separates P8-FT-83 deterministic
discovery, P8-FT-82 bounded projection and the sole execution-time claim.
Pinned legacy `fulltextsearch.cc:34-112` establishes a single background
runnable with cooperative cancellation and wait-on-stop, while
`mainwindow.cc:1381-1393,2100-2101,2158-2165,2180-2181,2302-2303` establishes
stop-before-replace/restart lifetime ordering.

P8-FT-84 introduces only `FullTextIndexWorkExecutor`, a private Core serial
executor: exactly one owned
worker, a coalesced pending sweep, copied immutable bounds and explicit
shutdown. Each sweep uses one canonical discovery snapshot, projects every
identity through P8-FT-82 and lets `ExecuteBoundedWork` perform the only claim.
Concurrent submissions do not create duplicate queues, and newly requested
work requires a later explicit submission. Shutdown rejects new submission,
discards unclaimed scheduling state, cancels the exact active identity and
joins before referenced coordinator or port destruction. No failure,
cancellation, expiry or stale rejection is retried.

Acceptance extends only `full_text_index_test` and adds no registration. Gated
fake ports prove serial canonical order, exact bounds, coalescing, sole claim,
stale/replacement safety, isolated terminal failure, active cancellation,
shutdown rejection and deterministic join without sleeps. Composition ingress
at `dictionary_service.cc:1090-1104,1775-1783`, automatic startup or policy-
change scheduling, multiple workers, configurable concurrency, priority,
legacy two-pass ordering, progress/status, facade/Widgets transport,
Preferences, additional formats and serialization remain excluded.
Public/installed APIs, dependencies, `full-text-v1`, UI/find/F3, P8-FT-72
through P8-FT-83, snapshot/persistence safety and exactly 109 registrations
remain locked. Delivery changes the two private executor files, Core source
list, existing test and exactly four governing sections. P8-FT-84 is complete.
At its completion, no successor beyond it was selected or named.

### Phase 8 P8-FT-85 overflow-safe execution-bounds coherence (complete)

The fresh post-P8-FT-84 audit uses synchronized migrated revision
`20b684eb2ad09ab48c22b7df81afa65d46a1d26f` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Tiger's architecture detection
explicitly supports x86 and 32-bit compilation modes, so production policy
cannot assume a 64-bit `size_t`. The P8-FT-82 multiplication-representability
condition can reject valid individual bounds solely because their mathematical
aggregate exceeds `SIZE_MAX`; it must be corrected before selecting a
production bounds provider.

P8-FT-85 changes only the private coherence test. With positive `D`, `B` and
`C`, accept exactly when mathematical `C <= D * B`, evaluated without
multiplication: reject `D < C / B`, and reject equality when `C % B` is
nonzero. Retain the future-deadline requirement, every lifecycle revalidation
and exact unchanged forwarding. This supports coherent products beyond
`SIZE_MAX` on both 32- and 64-bit targets without weakening the corpus bound.

Acceptance extends the existing `full_text_index_test` registration with
representable equality, quotient/remainder boundaries, representable adjacent
cases, coherent overflowing products including `D == B == C == SIZE_MAX`, and
host-independent synthetic 32-/64-bit arithmetic cases. Zero, expired,
incoherent, stale and non-requested cases remain rejected without observable
mutation or port work.

No provider, composition/executor wiring, startup or replacement submission,
new scheduling policy, artifact/snapshot/persistence change, format bridge,
UI, serialization, dependency, public API or registration is selected.
P8-FT-72 through P8-FT-84 and exactly 109 registrations remain locked. This
correction is complete; no successor beyond P8-FT-85 is selected or named.

### Phase 8 P8-FT-86 parity-preserving private execution-bounds provider (complete)

The fresh post-P8-FT-85 audit uses synchronized migrated revision
`ab38dd0d76a8cb8843a9eb4089148104570c444b` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Current configuration and policy
permit `0..10000000` dictionary articles with zero unlimited. Pinned legacy
`fulltextsearch.cc:34-112` runs eligible indexing serially in the background
without equivalent document, byte or wall-clock quotas, while
`mainwindow.cc:1381-1393,2100-2101,2158-2165,2180-2181,2302-2303` stops work
before replacement and restarts it afterward. After P8-FT-85 removed the
machine-product restriction, the smallest dependency-ready composition
prerequisite is a private no-practical-quota production bounds provider.

P8-FT-86 provides only the private argument-free
`DefaultFullTextIndexExecutionBounds()` factory. Its maximum-document,
maximum-document-byte and maximum-corpus-byte fields each equal `SIZE_MAX`, and
its deadline equals `steady_clock::time_point::max()`. P8-FT-82 remains the
sole validation and projection authority. The factory does not inspect a
clock, discover or submit work, invoke an executor or port, or mutate lifecycle
state. A future configured provider may replace these defaults without
changing coordinator/executor responsibilities.

Completed acceptance extends the existing `full_text_index_test` registration with exact
field checks, successful unchanged P8-FT-82 projection for a current requested
identity, and proof of no lifecycle or port effects. No arbitrary expiry test
is added. The exact implementation allowlist is
`full_text_index_lifecycle.h`, `full_text_index_lifecycle.cc`,
`full_text_index_test.cpp` and the four governing documents.

Executor ownership/lifetime, `ServiceState` wiring, startup/recomposition
submission, configured quotas, legacy two-pass priority, progress/status,
Preferences, additional format bridges, artifact/snapshot/persistence changes,
UI, serialization, dependencies, public/installed APIs and registrations remain
excluded. P8-FT-72 through P8-FT-85, Core authority, serial/coalesced/no-retry
execution, `full-text-v1`, find/F3, translations and exactly 109 registrations
remain locked. No successor beyond P8-FT-86 is selected or named.

### P8-FT-87 Private ServiceState Executor Ownership Prerequisite (complete)

The independent post-P8-FT-86 audit is grounded at synchronized migrated
revision `e8f2d9f49076def8186785ece6a712d5a2ea90ed` and clean pinned legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Current `ServiceState`
composition registers AARD, applies persisted lifecycle policy and reconciles
startup evidence but does not own the completed serial executor. The executor
contract requires its referenced coordinator and ports to outlive it and joins
its worker during shutdown. Pinned legacy owns one indexing operation and
orders it after dictionary storage so it is destroyed first. The selected
smallest dependency-ready leaf is P8-FT-87: private executor ownership and
lifetime composition, not submission.

The completed implementation adds exactly one concretely owned
`FullTextIndexWorkExecutor` to `ServiceState`. It is created only after all
discovery, registration, policy application and startup reconciliation succeed.
Member declaration and destruction order must stop and join the executor before
destroying any registered port or the coordinator it references. It starts
idle, receives no bounds and performs no submission, discovery, claim or work.
Executor-construction failure follows existing service-construction failure;
shutdown stays idempotent and private.

Completed acceptance extends only the existing `application_service_test` and
`full_text_index_test` registrations. It proves one owned executor per
successful service state, safe idle construction for zero and multiple
registrations, no composition-time lifecycle or port effect, and joined
shutdown before AARD port/coordinator teardown using deterministic lifetime
sentinels rather than sleeps. The lifetime aggregate mirrors the production
dependency order with its coordinator first, registered port/holder owners
next and an executor-owner probe last; that probe resets and joins its optional
executor before recording completion, followed by port/holder and coordinator
destruction records. Application-service coverage observes only idle
construction, lifecycle and artifact stability, while the existing active-work
executor case retains direct shutdown/join coverage. No executable or test
registration is added.

Startup/recomposition submission, configurable bounds, lifecycle transitions,
retry, extra queues or workers, progress/status, Preferences, other format
bridges, artifact/snapshot/persistence changes, UI, serialization, dependencies
and public/installed APIs remain excluded. P8-FT-72 through P8-FT-86, private
Core authority, no-quota defaults, 32/64-bit coherence,
serial/coalesced/no-retry execution, `full-text-v1`, find/F3, translations and
exactly 109 registrations remain locked. Completion unlocks only a later audit
of startup submission with `DefaultFullTextIndexExecutionBounds()`; no
successor beyond P8-FT-87 is selected or named. Next dependency: none selected.

### P8-FT-88 Private Replacement-Safe Activation Ownership Prerequisite (complete)

The fresh post-P8-FT-87 audit uses synchronized migrated revision
`7c764ca79774a6c8be3db9f0b18f53310130a974` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Current
`dictionary_service.cc:677-682,1089-1105,1776-1788` leaves each newly composed
`ServiceState` executor idle. The three replacement flows at
`main.cpp:715-769,807-838,854-907` run synchronously on the Qt main thread and
fully validate a candidate before publication, but their owner sees only
installed `DesktopFacade` objects. It cannot stop or activate the private old
and candidate executors. Because requests retain shared old service state,
constructor-time submission could overlap old and new writers against the
same canonical artifacts. Pinned legacy prevents this by stopping and joining
indexing before dictionary or policy replacement and restarting afterward at
`mainwindow.cc:1381-1393,2100-2101,2158-2165,2180-2181,2290-2303`.

P8-FT-88 defines one private Core
application-composition authority for replacement-safe activation. It owns the
single serialized, non-reentrant transition for the published state. Candidate
construction, registration, persisted-policy application and startup-artifact
reconciliation finish while its executor is idle. Any failure before handoff
destroys only the candidate and leaves the old publication and indexing
activity untouched.

An accepted future handoff has exact ordering: shut down and join the old
executor if present; submit the candidate executor exactly once with
`DefaultFullTextIndexExecutionBounds()`; then publish/swap the candidate through
the existing serialized replacement mechanism. Initial startup follows the
same activation path without an old state. Old externally held snapshots stay
valid for reads, but shutdown is permanent and they cannot resume indexing.
Duplicate or reentrant activation is rejected before either state changes. If
candidate submission unexpectedly rejects after the old join, publication
fails and the candidate is discarded; the old state remains readable with
indexing stopped, with no rollback restart or retry. A later execution failure
remains a lifecycle failure and does not invalidate the readable service.

The completed owner pairs each concrete `DesktopFacadeImpl` with a move-only
handle to its exact `DictionaryServiceImpl` shared `ServiceState` executor.
Candidate construction runs outside the owner mutex; installation revalidates
open state and uses first-successful-install ownership. Activation moves the old
handle out under lock, unlocks for shutdown/join and candidate submission, then
relocks to publish or abort. Owner and handle destructors stop and join
idempotently, so externally retained facade snapshots remain readable but
cannot retain indexing activity.

P8-FT-88 implements the prerequisite authority only. It adds no startup
submission or replacement wiring, process-global artifact registry,
policy-change trigger,
configured bound, progress/status, Preferences, legacy two-pass priority,
additional format bridge, artifact/snapshot/persistence change, UI,
serialization, dependency, installed/public API or registration. P8-FT-72
through P8-FT-87, private Core authority, no-quota defaults,
serial/coalesced/no-retry behavior, persistence/snapshot safety,
`full-text-v1`, find/F3, translations and exactly 109 registrations remain
locked. No successor beyond P8-FT-88 is selected or named. Next dependency:
none selected.

### Phase 8 prepared Core facade activation (complete)

The leaf grounded at
`25dc64dd2dac735e63183b1f6d018ce90a31dff4` converts the P8-FT-88 owner into
the prepared Core facade activation required by durable transactions. A
complete facade, service state, runtime-source composition, coordinator, and
idle allocated executor are constructed before activation. The move-only
candidate is owner/generation-bound and one-shot; preparation, rejection, and
abandonment leave the active facade and executor usable.

Activation performs only validation, bounded publication, and a generation
advance at the irreversible point. Old-executor stop/join and exactly-once new
submission follow as forward-only handoff. No post-publication failure becomes
pre-publication rejection or online rollback. Widgets rebinding, transaction
coordination, recovery, notification, Network, persistence, and source-specific
recomposition remain later work.

### Phase 8 production source-editor transaction invocation (complete)

The production source editor now routes its shared local-directory,
online-source, and external-program apply callback through the durable
configuration-reload coordinator. The complete candidate preserves the current
article session, records unchanged history intent, prepares the unchanged
Network cache policy, and publishes Network, Core, and Widgets only after the
configuration decision is durable. Pre-decision rejection leaves the active
owners and persisted configuration unchanged; successful publication refreshes
the source projections and removes the pending record durably.

The existing source smoke injects every pre-decision rejection boundary and
pins both successful local and online/external publication order. History,
Favorites, translations, wire formats, cache layout, dependencies, and public
APIs remain unchanged. Dictionary-group editing is the next independent
production compatibility caller and is not part of this leaf.

### Phase 8 production dictionary-group transaction invocation (complete)

Dictionary-group editing now routes the final production runtime-
reconstruction callback through the durable configuration-reload coordinator.
The desired configuration changes only the accepted group fields, records
unchanged history intent without a desired-history payload, prepares unchanged
Network policy and complete Core/Widgets candidates, and publishes authoritative
configuration and facade mirrors once.

The production dictionary-group smoke rejects every pre-decision boundary and
pins exact configuration bytes, owner identity, visible group rollback, retry,
and zero history/Favorites I/O. It also pins successful publication order and a
representative forward-only failure with restart-visible evidence. Startup
recovery, public APIs, translations, dependencies, source ordering, cache
layout, and unrelated application behavior remain unchanged. This completes
the production configuration-reconstruction caller migration.

### Phase 8 private initial command-line lookup ingress

The application accepts at most one ordinary startup lookup operand. A
source-private parser accepts a nonempty plain word or the pinned legacy,
case-sensitive `goldendict://[/]...` and `dict://[/]...` forms. Legacy URI
normalization removes the optional third slash, removes one trailing slash
only from a payload longer than one character, and strictly percent-decodes
UTF-8. Both input and normalized text use the existing 4096-byte lookup bound.
Empty, malformed, unsupported-scheme, option-bearing, and multiple-operand
command lines produce no lookup.

The move-only parsed value is consumed exactly once after the configured
article session is restored, the complete Core facade candidate is activated,
and Widgets is bound to that published facade. `MainWindow` then uses its
existing current-tab `DesktopFacade::OpenArticleTab` navigation path and normal
lookup handoff. Prepared, rejected, abandoned, or retired facade candidates
cannot receive the request. No-operand startup is unchanged.

This leaf follows the Shared-Library And GUI Boundary: parsing and presentation
handoff stay private to `apps/goldendict`, while validation, tab/session
mutation, history signaling, and dictionary requests retain their established
facade/Core ownership. That Phase 8 leaf adds no installed API, persistence,
dependency, single-instance forwarding, arbitrary URL handling, desktop
integration, scan, hotkey, tray, X11, Wayland, Windows, shell, network, or
process-launch behavior.

The first Phase 9 Linux leaf adds private single-instance lookup forwarding.
A per-user, configuration-profile-derived runtime lock elects the only
application writer; only its owner may recover the local-server endpoint.
Secondary processes reuse the accepted Phase 8 parser and forward only its
normalized bounded value in a private, versioned, user-only local frame.
Missing or rejected operands exit as no-ops, and failed forwarding never
creates another full application instance.

The primary queues accepted values in a bounded FIFO until its exact Core and
Widgets facade publication completes. The composition root then installs one
consumer and drains through the existing `MainWindow::SubmitInitialLookup`
current-tab path. Desktop activation, MIME/icon metadata, arbitrary URL,
process and network behavior, Windows/macOS support, public Core/C ABI, and
additional lookup writers remain excluded.

This bounded Linux leaf restores activation for those accepted forwarded
lookups. The sole consumer first asks the published `MainWindow` to show or
restore itself, raise, and request activation, then performs the unchanged
current-tab lookup handoff. Hidden and minimized behavior is pinned in the
existing GUI smoke, maximized state is preserved, and the Release baseline
remains exactly 114 registered tests.

This leaf does not add an activation-only wire message: a secondary invocation
without a lookup remains a no-op. Desktop metadata, broader command forwarding,
Windows/macOS behavior, Debug verification, public or installed Core changes,
persistence, network behavior, and unrelated refactoring remain excluded. A
later activation-only message is the next dependency unlocked.

The next bounded Linux leaf restores the pinned legacy bare `bringToFront`
behavior. An exactly argument-free secondary invocation sends a distinct
application-private activation message with no lookup payload. The existing
sole transport owner queues and acknowledges it, and the post-publication
consumer invokes only the established Widgets reveal, restore, raise, and
activation operation. Normalized lookup framing and ingress remain unchanged;
malformed, unsupported, option-bearing, and ambiguous input remains a no-op,
and activation never changes the query or starts navigation. Focused coverage
stays in the existing command-line test registration and the Release baseline
remains exactly 114 tests.

Desktop metadata, broader commands, Windows/macOS behavior, Debug verification,
public or installed Core changes, persistence, network behavior, packaging,
and unrelated refactoring remain excluded. No successor is selected.

The first post-activation Phase 9 Linux leaf restores the pinned legacy
XWayland fallback. Before `QApplication` construction, one source-private
application helper compares `XDG_SESSION_TYPE` with `wayland`
case-insensitively and selects Qt's `xcb` platform, replacing any existing
`QT_QPA_PLATFORM` value. Absent, empty, X11, and unknown session values leave
the platform environment unchanged. Focused coverage remains in the existing
command-line test registration and the Release baseline remains exactly 114
tests.

This leaf adds no native Wayland promise, desktop metadata, audio, clipboard,
hotkey, scan-popup, tray, translation, dependency, persistence, network,
Windows/macOS, Debug, public or installed Core API, or unrelated refactoring.
No successor is selected.

The next bounded Phase 9 Linux leaf restores the pinned legacy main-window
middle-click lookup. A middle-button press delivered to the main window reads
Qt's X11 primary selection and submits it through the existing published-facade
current-tab lookup path. Empty selection text remains a no-op, other buttons
retain normal main-window handling, and the existing tab-bar middle-click close
contract remains unchanged. Focused coverage stays in the existing article-tab
GUI smoke and the Release baseline remains exactly 114 tests.

Native Wayland selection, continuous clipboard monitoring, scan popup,
modifiers, hotkeys, tray, audio, desktop metadata, Windows/macOS, Debug, public
or installed Core changes, dependencies, persistence, and unrelated refactoring
remain excluded. No successor is selected.

The next bounded Phase 9 Linux leaf installs the pinned legacy desktop launcher
and 256-pixel application icon. The launcher passes one URI with `%u` into the
existing private command-line parser and declares only the already-supported
`goldendict` and `dict` scheme handlers. Installation is Linux-only and uses
the standard applications and hicolor icon directories.

Runtime registration commands, metainfo, MIME databases, tray behavior,
Windows/macOS, Debug, public or installed Core changes, dependencies, and
unrelated packaging expansion remain excluded. The existing installed-runtime
smoke verifies the files and exact handler contract, so the Release baseline
remains exactly 114 tests. No successor is selected.

The next bounded Phase 9 Linux leaf installs the pinned legacy AppStream
metainfo beside the existing launcher and icon. The application install remains
the sole metadata owner, and the metainfo launchable references the already
installed `org.goldendict.GoldenDict.desktop` ID.

Runtime registration commands, MIME databases, metadata modernization,
Windows/macOS, Debug, public or installed Core changes, dependencies, and
unrelated packaging expansion remain excluded. The conditional installed-runtime
smoke verifies the metainfo contract when runtime dependency installation is
enabled; the normal Linux Release baseline remains exactly 114 tests and does
not include that conditional smoke. No successor is selected.

The next bounded Phase 9 Linux leaf installs the pinned legacy English and
Russian Qt help collections under `share/goldendict/help`. The application
install remains the sole resource owner, and the existing conditional
installed-runtime smoke verifies both paths and exact pinned hashes.

Help presentation, locale selection, configuration, Qt Help linkage,
translations, Windows/macOS, Debug, public or installed Core changes,
dependencies, and unrelated packaging expansion remain excluded. A fresh
normal/default library-mode Release configuration still registers exactly 114
tests and must pass 114/114; it does not register the conditional smoke. A
separate fresh supported `install_mode=runtime` Release configuration registers
and explicitly runs `goldendict_installed_runtime_smoke`. No later successor is
selected.

The next bounded Phase 9 Linux leaf privately presents those installed help
collections through Qt Help and Widgets. The existing help-language,
interface-language, and system-locale precedence is retained, but selection is
bounded to Russian or the English fallback. The application composition root
owns one lazy help window and restores the pinned `GoldenDict reference`/F1
entry without adding a Core or facade operation.

Help settings UI, translation generation, help geometry and zoom persistence,
full-text help-intent wiring, Windows/macOS, Debug, public or installed Core
changes, and unrelated refactoring remain excluded. The Linux Release library
baseline is 115 tests after the focused private help test; runtime mode adds the
existing conditional installed-runtime smoke. The next dependency-ready leaf
is private full-text help-intent routing to the installed collection.

That dependency-ready successor now routes the existing private full-text Help
intent to the pinned `Full-text search` identifier in the selected installed
English or Russian collection. The published dialog connects to the existing
single lazy Linux presenter; prepared Widgets candidates remain inert.

The leaf adds no test registration, Core/facade/installed API, help settings or
persistence, collection changes, translation generation, Windows/macOS
behavior, Debug gate, dependency, or unrelated refactoring. The Linux Release
library baseline remains exactly 115 tests, and runtime mode retains its
conditional installed-runtime smoke. The next dependency unlocked is a fresh
bounded help-parity audit; no successor is selected or ranked.

That bounded audit identifies one independently dependency-ready successor:
the migrated dictionary browser restores its pinned legacy `Help` button and
dialog-scoped F1 action, then routes their single private intent through the
same lazy Linux presenter to `Dictionary headwords`. Legacy Preferences and
Manage Dictionaries help remain separate because their dialog-owned help
windows require a composition and ownership decision.

The leaf adds no test registration, Core/facade/installed API, help settings or
persistence, collection changes, translation generation, Windows/macOS
behavior, Debug gate, dependency, or unrelated refactoring. The Linux Release
library baseline remains exactly 115 tests. The next dependency unlocked is a
fresh bounded audit of the remaining dialog-owned help routes; no ownership
choice or successor is selected.

The next bounded Phase 9 Linux leaf restores the pinned legacy Manage
Dictionaries `Help` button and dialog-scoped F1 action, then routes their
single private intent through the composition-root-owned lazy presenter to the
installed `Manage dictionaries` identifier. The migrated source-directories
dialog remains presentation-only and does not own a help window.

The leaf adds no test registration, Core/facade/installed API, help settings or
persistence, collection or installed-runtime changes, translation generation,
Windows/macOS behavior, Debug gate, dependency, or unrelated refactoring. The
Linux Release library baseline remains exactly 115 tests. The next dependency
unlocked is a fresh bounded audit of the remaining help routes and settings;
no successor is selected or ranked.

That bounded audit selects one dependency-ready Linux settings leaf. The
Preferences General page now exposes the installed help collections as
`Default`, `English`, and `Russian`, using the already persisted
`help_language` preference. Default retains the existing help-language,
interface-language, then system-locale selection path. Accepting a changed
choice invalidates the composition-root-owned lazy presenter; the next help
request recreates it with the selected collection.

The leaf adds no configuration field, Core/facade/installed API, dialog-owned
presenter, collection or installed-runtime change, translation generation,
Windows/macOS behavior, Debug gate, dependency, or unrelated refactoring. The
Linux Release library baseline remains exactly 115 tests. Help geometry and
zoom persistence require separate configuration-backed leaves and remain
unselected.

The next bounded Phase 9 Linux leaf restores initial English/Russian interface
translation under the application-private composition boundary. English uses
source strings. Russian uses the application catalog ported from pinned legacy
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8` plus Conan Qt's Russian catalogs,
all staged and installed under `share/goldendict/locale`. The existing
`interface_language` preference has precedence over the system locale;
unsupported selections fall back to English. Linux Preferences exposes
Default, English, and Russian, with accepted changes taking effect only after
restart. The independent `help_language` behavior is unchanged.

The leaf adds no locale beyond Russian, live retranslation, Help geometry or
zoom, Windows/macOS behavior, Debug gate, public/Core API, or unrelated
translation sweep. The Linux Release library baseline is exactly 116 tests;
runtime mode retains its conditional installed-runtime smoke. The next
dependency unlocked is a fresh bounded audit of remaining Phase 9 Linux
integration and release-quality work; no successor is selected.

The following source-only Phase 9 leaf imports the complete 45-file pinned
legacy GoldenDict Qt Linguist catalog inventory. Forty-four byte-identical
sources remain explicitly disabled; they are neither compiled nor installed.
The previously accepted derived Russian source remains the sole enabled
application catalog, so English/Russian runtime behavior is unchanged. The
inventory records its GPL-3.0-or-later provenance and requires a separate
Qt 6 context review and focused runtime smoke before enabling any locale.

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
