# Architecture

This document describes GoldenDict's Tiger-based repository structure, project
layout rules, and design rationale. `AGENTS.md` stays short and links here for
details.

## Project Purpose

GoldenDict is a cross-platform dictionary lookup application. The migration
uses Tiger's reusable CMake and Conan 2 structure on Linux, Windows, and macOS.

Tiger owns module and application conventions, Conan dependency integration,
installation layout, and generated project configuration files. GoldenDict
owns product identity and behavior. See [migration.md](migration.md).

## Supported Platforms And Toolchains

Supported operating systems:

- Linux.
- Windows.
- macOS.

Supported compiler families:

- MSVC 2019 or newer.
- GCC 9.1 or newer.
- Clang/LLVM 8.0 or newer.

For now, treat macOS compiler support under the Clang/LLVM policy instead of
maintaining a separate AppleClang minimum version.

## Repository Structure

- `apps/`: application targets.
- `apps/<app_name>/resources/`: optional application resource directory for
  runtime files such as configs, images, and other assets. `TigerApp.cmake`
  copies its contents to the app binary directory after build when resource
  copying is enabled.
- `modules/`: reusable project modules.
- `cmake/`: shared CMake logic, templates, checks, and packaging helpers.
- `cmake_finders/`: project CMake finder modules for Conan dependencies.
- `conan/recipes/`: narrowly scoped local recipes for dependencies unavailable
  from configured Conan remotes.
- `profiles/`: project Conan profiles for dependency build requirements.
- `test_package/`: Conan package verification project.
- `CMakeLists.txt`: root CMake entry point.
- `conanfile.py`: Conan recipe for configuring, building, and packaging.
- `VERSION`: project version source.

See [coding-style.md](coding-style.md) for concrete module, application,
proto, and generated-file layout rules.

## Module Design

Tiger separates public shared-library modules from runnable applications.
GoldenDict initially uses one product library, `goldendict_core`, while
`apps/goldendict` contains presentation and the composition root only.

The planned dependency spine is:

```text
apps/goldendict GUI ----------> desktop application facade --+
                                                             |
future dictionary service ---> headless dictionary API ------+-> goldendict_core
                                                             |
future transport adapter -----> transport-neutral DTOs ------+

goldendict_core contains private application/domain, dictionary-format,
article, configuration, and infrastructure components.
```

The internal components preserve dependency inversion and focused tests
without creating a public ABI for each layer or dictionary format. The
headless API supports discovery, indexing, lookup, article/resource retrieval,
cancellation, and lifecycle without Qt Widgets, Qt Gui, or Qt WebEngine. It
does not choose HTTP, gRPC, JSON, or another future service transport.

Consumers inject a `CoreConfiguration` containing dictionary roots and the
generated-index directory. `LoadConfiguration` treats a missing file as a
clean profile, while `SaveConfiguration` persists the same bounded,
versioned schema. The current schema also stores ordered dictionary groups as
transport-neutral IDs, names, optional icon references, and ordered stable
dictionary IDs. Optional v1 metadata records add favorites folders, shortcut
strings, separately bounded muted-ID collections, and canonical Base64 icon
metadata without changing the original group record; files written before the
extension therefore remain readable with empty metadata defaults. Group
counts, membership counts, value sizes, and duplicate IDs are validated in the
core before an atomic replacement. Bounded legacy XML migration preserves
group and dictionary order and retains unknown nonempty dictionary IDs without
catalog resolution.
Local source edits use one complete `CoreConfiguration` candidate. The public
core API owns source-list bounds and validation; the composition root restores
a replacement facade, atomically saves the candidate, and only then rebinds
Widgets. Presentation code never persists or discovers sources, and failure
leaves the active facade and unrelated application state unchanged.
The P5b source presentation extends the same dialog and complete-candidate
command to MediaWiki, website, Forvo, and DICT records. Stable source IDs are
hidden from editing, enabled and ordered intent is staged in Widgets, and core
validation failures are returned to the open dialog. The composition root
continues to own construction, session restoration, atomic persistence, and
the final facade swap; presentation code neither persists configuration nor
handles Forvo credentials.
P5c extends this presentation boundary with external-program fields and an
ordered argument-template editor. Stable IDs remain hidden and every argument
remains a distinct DTO value; executable and working-directory pickers only
collect absolute paths. Widgets do not test-run programs, compose command
lines, or add environment policy. The existing core validation and
composition-root transaction continue to own acceptance, shell-free runtime
construction, session restoration, persistence, and facade replacement.
The Phase 8 Edit-menu leaf exposes that same source-configuration workflow
through one canonical Widgets-owned `dictionaries` action shared with the
existing source button. Widgets owns modal and busy presentation; validation,
session restoration, atomic persistence, and facade replacement remain in the
existing application/core path. Its dependent Preferences leaf adds the
canonical action only with a bounded legacy-shaped General/Tabs surface for
the two tab-opening preferences the current application already applies.
Widgets constructs a complete preferences candidate; the composition root
validates, recomposes, restores the complete session, atomically persists, and
then replaces runtime state. Runtime-unbacked legacy preference groups are not
shown as inert controls.
The following General/History leaf exposes the existing `store_history` and
`maximum_history_entries` fields with the pinned defaults and `0..99999`
range. The composition root owns recording, bounded replacement imports,
newest-first trimming, persistence, session restoration, and facade
replacement; the dialog only edits a complete candidate. Disabling recording
preserves existing entries, while lowering the maximum persists the trimmed
history before live state is swapped.
The following General/Favorites leaf exposes only the existing
`confirm_favorites_deletion` field, defaulting on. Widgets confirms the
canonical selected-item removal before dispatch; rejection leaves the tree
and all application state untouched, while acceptance continues through the
existing atomic favorites-save path. The legacy save-interval control remains
absent because current favorites mutations are persisted immediately and no
compatible delayed-save runtime contract exists.
The following General/Articles leaf passes the existing collapse flag and
validated symbol limit into private core article-composition policy. Core wraps
only qualifying multi-dictionary sections in trusted, script-free collapsible
markup after dictionary payload sanitization; single-result pages remain
expanded and print media exposes complete content. The public facade and
configuration format remain unchanged. Successful preference recomposition
restores the complete facade session and replays current pages, while Widgets
retains presentation-only scroll and in-page search state by stable tab ID.
The following General/Input phrase length leaf exposes the existing enable flag
and validated `1..1000000` symbol limit. A private configured application policy
counts Unicode scalar values in the submitted UTF-8 phrase and rejects, rather
than truncates, over-limit lookup, suggestion, navigation, and restored-session
input before backend or tab mutation. Widgets only present the controls and
non-modal rejection status; group, history, tab/session, result, cancellation,
deadline, and stale-response behavior remains owned by the existing paths. This
transport-neutral rule intentionally differs from the pinned Qt 5 implementation,
which counted UTF-16 code units and exposed a `9999999` widget maximum.
The Phase 8 P4a current-persistence foundation extends that complete candidate
with ordered, bounded transport-neutral records for MediaWiki, website, Forvo,
and DICT sources. Stable IDs, names, enabled intent, adapter-compatible URLs,
templates, languages, hosts, ports, databases, and strategies are persisted;
credentials, legacy website encodings, and iframe flags are excluded. The P5a.1
runtime increment adds a transport-neutral core extension contract and composes
enabled MediaWiki and website records through the optional network module in
persisted family order after the unchanged local catalog. The existing adapters
perform transport work while core retains lookup orchestration and article
sanitization. P5a.2 adds ordered Forvo-language children and DICT sources to the
same internal composer. Forvo credentials enter only through an in-memory map
keyed by configured source ID; missing values produce source-ID-only diagnostics
without changing persisted enabled intent. Child identities are deterministic
and retain the configured source ID as provenance. P5a.3 appends configured
external programs after DICT through the existing shell-free process adapter.
The internal composer returns only a completely constructed desktop facade and
recoverable source-ID-only diagnostics. Startup and configuration replacement
therefore share one local-plus-runtime composition path, and Widgets are rebound
only after candidate construction, session restoration, and atomic persistence
succeed.
The core factory is intentionally a generic extension seam: injected identities
need not correspond one-to-one with persisted records, allowing deterministic
derived identities such as future per-language Forvo sources. It atomically
rejects null sources, empty IDs, and collisions with local or other runtime
sources. Configuration-derived composers instead validate the complete DTO and
emit only enabled records. Network wrappers check cancellation and absolute
deadlines before every operation, include both conditions in adapter cancellation
polling, and recheck before returning; the adapters' existing bounded transport
or process timeout remains the outer fallback between polling opportunities.
External plain-text and HTML results pass through core's normal untrusted article
policy, prefix-match stdout is interpreted as bounded nonempty Unicode lines,
and external sources do not expose resources.
Exact lookup now carries an explicit diacritic policy through the public
runtime request options. A runtime identity must advertise support before core
can request diacritic-insensitive matching, and supporting sources must apply
the policy before their result bound. The configured network and external
adapters do not advertise that capability, so core reports a typed per-source
lookup error instead of silently accepting provider-dependent behavior.
The Phase 8 P4b current-persistence increment adds ordered, bounded
external-program records with stable identity, enabled intent, plain-text,
HTML, or prefix-match output, an absolute executable, ordered argument
templates, and an optional absolute working directory. Canonical parent and
argument counts preserve explicit empty collections and reject orphaned or
reordered arguments. The model remains shell-free and excludes audio programs,
icons, environment and process policy, runtime composition, and execution.
An explicit bounded Forvo collection-count record distinguishes an older
current file with no P4a fields from a current file whose Forvo list was
intentionally emptied; canonical saves always write the count.
The v1 store also carries a transport-neutral current preferences DTO as
independently optional named records. Missing records use fixed clean-profile
defaults, while canonical booleans, enums, finite zoom values, numeric bounds,
modifier masks, and bounded UTF-8 strings are validated before atomic
replacement.
The private bounded legacy XML importer maps the portable, non-secret subset
of those preferences into the same DTO. Recognized scalar values use strict
boolean, enum, numeric, and string conversions; missing values retain current
defaults, while malformed recognized values abort atomic migration. Proxy
credentials, general layout state, source definitions, and Widgets behavior
remain excluded. A narrow extension maps only the exact legacy tab-opening
booleans and opaque `mainWindowGeometry`. Core validates and canonically
persists these transport-neutral values, bounding decoded geometry at 64 KiB;
Widgets alone captures and restores it with Qt geometry APIs. Window/dock
state remains excluded. The pinned legacy application has no persisted article
session: its configuration contains no tab or navigation records, startup
always creates a fresh welcome tab, and per-view history and scroll data exist
only in memory. Consequently there is no legacy article-session format or
migration input; current-format session persistence remains unchanged.
The current configuration also carries a separately bounded opaque Qt 6 main-
window state. Core owns its 64 KiB persistence limit without interpreting Qt
bytes. Widgets captures and transactionally restores a current-only version,
checks restored floating docks and toolbars against available screens, and
rolls back on malformed, incompatible, or unusable state. Legacy version-1
state is intentionally excluded: the pinned legacy repository contains no
authentic opaque-state artifact and does not pin an exact Qt runtime whose
binary compatibility with Qt 6 could be established. Windows-only legacy
normal/maximized rectangles and their explicit window-mode behavior are also
excluded; only the separately bounded legacy `mainWindowGeometry` is migrated.
Current Qt 6 state versions 2 through 7 remain transactionally supported with
rollback and semantic defaults for shell objects absent from an older version.
The first legacy-compatible shell increment gives the existing, fully backed
favorites and history docks their pinned `favoritesPane` and `historyPane`
identities and deterministic visible right-side vertical defaults. A private
Qt state-version transition accepts the immediately preceding current Qt 6
dock identities and rewrites them on the next save; it does not accept or
interpret legacy version-1 state. Search, results, navigation, and dictionary
shell objects were prerequisites for the completed opaque-state acceptance
audit, not evidence that version-1 bytes could be migrated safely.
The next shell increment recomposes the existing group selector, query editor,
lookup split-button, and facade-backed Back/Forward actions into a visible
legacy-compatible `navToolbar`. Widgets own its top-area default, top/bottom
movement, control order, focus chain, and query-focus shortcuts; lookup,
selection, tab targeting, and navigation semantics remain unchanged. The
private Qt state version advances and transactionally accepts both the
immediately preceding Qt 6 layout and the older dock-identity transition,
without accepting legacy version-1 bytes. The article toolbar, dictionary bar,
results pane, menus, and unsupported navigation actions remain independent
shell leaves.
The results-navigation shell increment adds the legacy-compatible `dictsPane`
without extending the facade: Widgets retain an ordered per-tab projection of
each completed bounded `LookupResponse`, render only real dictionary
identities, and navigate by the matching composed-section order. Replacement,
cancellation, failure, tab closure, and facade rebinding clear presentation
state before it can become stale. Prefix suggestions, full-text results, and
dictionary browsing remain independent workflows.
The prefix-suggestion shell similarly stays private to Widgets. One serial
worker retains at most a running request and the latest replacement, passes
cancellation through the existing transport-neutral contract, and returns
tagged completions to per-tab generation caches. Core remains authoritative for
bounded ordering, Unicode folding, group resolution, errors, and
lookup/tab/history behavior; the pane does not invoke enumeration, full-text,
wildcard, regular-expression, or compound search.
The dictionary-participation prerequisite keeps the toolbar boundary equally
narrow. Lookup and suggestion requests carry an explicit filter-active bit so
an empty dictionary-ID collection can mean no participating dictionaries;
without that bit, existing empty collections remain unfiltered. Core still
owns validation, group intersection, catalog ordering, unavailable-identity
errors, bounds, and cancellation. A later Widgets `dictionaryBar` may therefore
own only ephemeral presentation state and must not rewrite persisted group
membership, group muting metadata, or source enabled intent.
The resulting Widgets toolbar keeps that state per group for one main-window
lifetime, reconciles it against catalog and membership refreshes, and projects
it into both request DTOs only while visible. A change cancels and replaces
only active-tab work; background requests keep their immutable submitted
filter. Qt layout persistence records the toolbar hierarchy and visibility at
private version 7, never its participation set.
The first menu leaf exposes the completed shell through a legacy-compatible
Widgets-owned View menu. It reuses the four dock and two toolbar toggle actions
directly, so visibility, checked state, shortcuts, and Qt ownership cannot
diverge. Menus are not part of `QMainWindow::saveState`; private state version 7
and all core/application interfaces remain unchanged.
The History branch similarly reuses its pane toggle and existing transfer and
clear actions. The File branch reuses the already-backed tab, print, preview,
and atomic HTML-save actions across the menu and existing controls, and routes
quit through the application's orderly shutdown path. Widgets continues to own
dialogs and action availability; facade-owned session mutation and the
composition root's persistence remain unchanged. Unsupported legacy page
setup, rescan, and tray commands are not represented by placeholders.
The Search branch follows the same canonical-action rule: its sole supported
entry is the existing in-article find action. Widgets keeps query, match
status, focus, and asynchronous completion generations per article tab, while
each WebEngine view retains its own match state. Full-text, compound, scan, and
global search remain absent rather than crossing the GUI boundary with an
unbacked command.
The Favorites branch likewise reuses the pane toggle plus the existing XML
transfer and active-query Add actions. Widgets owns menu identity, dialogs,
availability, and reentrant-command exclusion; the composition root and core
continue to own selected-folder targeting, validation, atomic persistence and
transfer, and failure preservation. Selected-tree removal remains on its
existing canonical action, while the unsupported legacy plain-list export is
omitted instead of being recreated in Widgets.
The Help branch remains entirely in Widgets. It exposes the fixed HTTPS
project homepage, the composition root's resolved current configuration
directory, and a modal About dialog populated from build/runtime product,
version, Qt, and shipped license metadata. A private validated desktop-service
dispatcher prevents arbitrary or credential-bearing navigation and permits
deterministic tests. Offline reference help and the legacy forum entry remain
absent because the current distribution ships no help collection and has no
verified HTTPS forum target.
The application composition layer also owns a private, non-Widgets adapter for
legacy configuration locations. It converts Qt's runtime paths and platform
into an injected value object, then applies the pinned portable, Linux/Unix,
Windows, or macOS directory rule without scanning profile roots. The selected
exact `config` path is passed to the unchanged core migration transaction.
Current state is checked first; final symlinks and non-regular selected sources
are rejected, and no lower-priority profile is tried after selection. This
keeps platform APIs and environment lookup outside the reusable core while the
bounded parser, validation, and persistence remain core-owned.
The same private adapter derives current history and favorites beside the
resolved current configuration and discovers their exact lowercase legacy
companions from that single profile. Linux history alone retains the pinned
profile-local-then-XDG-data rule. History and favorites precedence, validation,
and atomic migration are independent per destination; startup only composes
the resolved paths and reports each failure separately.
`CreateDictionaryService` is the headless composition entry
point and `CreateDesktopFacade` is the presentation-facing entry point. Both
compose private format adapters inside `goldendict_core`; neither exposes a
StarDict type. Synchronous lookup is available for simple headless consumers,
and `StartLookup` returns an owned request with explicit completion and
cancellation whose lifetime does not depend on the service object.

The desktop facade also owns the in-memory article-tab session. Its public,
transport-neutral snapshot carries ordered stable tab IDs, the active tab, and
bounded per-tab navigation state for queries, groups, titles, and internal-link
context. A session always contains at least one tab; closing the active tab
selects its next neighbor or the previous final neighbor, and closing the last
tab creates a fresh untitled tab. Navigation is capped at 100 entries per tab,
new navigation after going back discards the forward suffix, and the session is
capped at 32 tabs. These desktop semantics do not change the headless lookup
contract. The Qt Widgets presentation mirrors that ordered state with a
visible tab bar and one retained WebEngine view per open tab, routes tab and
back/forward commands through the facade, and keeps query/group controls bound
to the active tab. The composition root restores the configured facade session
before presentation synchronization, rebuilds each view from only its current
navigation entry, and atomically saves facade exports after successful
mutations and on orderly shutdown. New-tab placement is an explicit core
append/after-active policy; Widgets derives it and default activation from
core preferences while explicit foreground/background commands and modifier
overrides retain fixed meanings. The pinned legacy application did not persist
sessions, so no legacy-session migration layer follows this current-format
contract.

The core persistence foundation also exposes a complete transport-neutral
session DTO containing each tab's ordered navigation history and cursor. It
validates and restores that DTO atomically, derives the next stable tab ID
without collisions, and stores an optional canonical session block in the
current configuration. Older current configurations retain the single-empty-
tab default. Application startup/save wiring and restart presentation use this
contract without adding GUI-owned history. Bounded geometry and tab-opening
preferences use the same atomic configuration path without adding GUI types.

Built-in local formats are composed behind the same private backend contract.
StarDict owns its generated index and typed resource adapter. Dictd consumes
the original `.index` plus `.dict` or `.dict.dz` files directly, including the
optional original-headword column and `00databaseshort` title metadata. Dictd
articles enter the common inert article assembler as untrusted plain text;
legacy presentation-specific phonetic and cross-reference markup remains an
article-phase concern rather than format discovery behavior.

SDict consumes original `.dct` containers directly. Its private adapter
validates the packed little-endian header, full-index chain, headword UTF-8,
and article ranges before exposing entries. Plain, zlib, and bzip2 fields are
decompressed under explicit output limits; legacy structural tags and word
references are converted to the common sanitized-HTML and typed-link path.

XDXF consumes original `.xdxf` XML and gzip-compatible `.xdxf.dz` files
directly through a bounded Expat stream parser. The private adapter preserves
dictionary and language metadata, maps logical XDXF markup and word references
into the common sanitized-HTML path, and resolves bounded resources from safe
relative paths beside the dictionary or below its `.files` directory. Resource
ZIP archives and full-text indexes remain later parity increments.

GLS consumes original `.gls` and gzip-compatible `.gls.dz` glossaries. The
private adapter strictly decodes UTF-8, UTF-16LE, and UTF-16BE input under
explicit size limits, preserves title/language metadata and alternate
headwords, and sends glossary HTML through the common sanitizer. Resource
reads are confined to safe relative paths beside the glossary or below its
`.files` directory. Resource ZIP archives and full-text indexes remain later
parity increments.

ABBYY Lingvo DSL consumes original `.dsl` and gzip-compatible `.dsl.dz` files;
companion `_abrv` files are not exposed as standalone dictionaries. The
private adapter strictly decodes BOM-marked UTF-8/UTF-16 and declared
Windows-1250/1251/1252 input, preserves name and language directives, expands
bounded optional headword parts and tildes, and converts common DSL markup,
word links, and image references into the common sanitized article path.
Resource reads are confined to safe relative paths beside the dictionary or
under either applicable `.files` directory. Abbreviation expansion, resource
ZIP archives, nested cards, and full-text indexes remain later parity work.

Babylon BGL consumes original `.bgl` containers. The private adapter strictly
validates the Babylon signature, embedded gzip stream, variable-width block
lengths, entry ranges, and aggregate output limits before exposing data. It
preserves title and language metadata, decodes UTF-8 and the common Babylon
legacy code pages through the shared encoding primitive, exposes alternate
headwords through folded lookup and suggestions, and keeps embedded resources
behind the typed resource API. Definitions enter the common sanitizer as
untrusted HTML. Advanced transcription/control records, historical Hebrew
repairs, and full-text indexes remain later parity work.

MDict consumes 2.x `.mdx` dictionaries and their `.mdd`, `.1.mdd`, and later
resource volumes. The private adapter validates UTF-16 header XML and checksums,
bounds key/record tables and all decompression, strictly decodes declared text,
applies embedded stylesheet markers, resolves bounded article redirects, and
serves normalized MDD resource names through the typed resource API. Plain and
zlib blocks are supported in this slice; encrypted dictionaries, LZO blocks,
older 1.x containers, local-file resource fallback, and full-text indexes
remain explicit parity work.

Aard consumes `.aar` archive volumes through a private bounded adapter. It
validates the fixed header and 32/64-bit index layouts, decompresses metadata
and articles with bzip2 or zlib (while accepting legacy raw article payloads),
strictly validates UTF-8 headwords and JSON strings, exposes folded lookup and
suggestions, and rewrites Aard word/redirect links into the common typed lookup
path before sanitization. Multi-volume aggregation, icon sidecars, and
full-text indexes remain explicit parity work.

ZIM consumes single `.zim` archives and consecutive split volumes beginning at
`.zimaa` through a private bounded adapter. It validates the fixed header,
MIME, URL, directory, cluster, and 32/64-bit blob-offset tables; resolves
bounded redirects; exposes metadata, folded article lookup and suggestions;
and serves namespaced binary resources through the typed resource API. Raw,
zlib, and bzip2 clusters are supported in this slice. LZMA2/Zstd clusters,
complex relative-link rewriting, icon conversion, and full-text indexes remain
explicit parity work.

SLOB consumes `.slob` containers through a private bounded adapter. It
validates the magic, declared encoding, metadata tags, content types,
reference/item/bin offset tables, and decompressed lengths; supports aliases,
folded lookup and suggestions, and serves non-article bins through the typed
resource API. Raw, zlib, and bzip2 item stores are supported in this slice.
LZMA2, advanced content conversion, icons, and full-text indexes remain
explicit parity work.

EPWING consumes recursively discovered `CATALOGS` books through a private,
bounded adapter rather than exposing the legacy libeb ABI. The first slice
validates catalog/subbook/index page ranges, decodes declared Latin-1 or the
default JIS X 0208 text, supports simple word-index entries with folded exact,
prefix, and suggestion queries, rewrites resolved article references, and
confines resources to catalog-declared subbooks. Compressed HONMON streams,
mixed JIS/GB2312 text, grouped indexes, gaiji and multimedia rendering, and
full-text indexing remain explicit parity work.

LSA consumes recursively discovered `.lsa` and `.dat` audio archives through
a private bounded adapter. It validates the UTF-16 entry table and all sample
ranges, exposes folded exact/prefix/suggestion queries, and emits sanitized
HTML5 audio references. Ogg Vorbis inspection and sample-range decoding live
behind a separate private audio component, which returns bounded PCM WAV data
through the typed resource API; codec types do not cross the dictionary or
public API boundaries. Archive icons and streaming very large recordings
remain explicit parity work.

ZIP sound packs consume recursively discovered `.zips` archives through a
private bounded ZIP adapter. It validates classic single-volume central and
local headers, member ranges, names, sizes, and CRC-32 values; supports stored
and deflated audio members; exposes folded exact/prefix/suggestion queries;
and serves typed audio resources through sanitized HTML5 playback references.
ZIP64, encrypted/split archives, archive icons, and streaming very large audio
members remain explicit parity work.

Sound directories remain explicit configuration rather than being inferred
from ordinary dictionary roots. Each configured path and display name becomes
one private backend that recursively indexes regular audio files by filename
without its extension, exposes folded exact/prefix/suggestion queries, and
serves bounded resources through sanitized HTML5 playback references. Symlink
traversal is excluded and resource paths are re-confined to the configured
canonical root. Per-directory icon configuration and streaming very large
files remain explicit parity work.

Lookup normalization is a private foundation concern. Backends compare a
canonical Unicode form that applies compatibility normalization, full case
folding, diacritic removal, and whitespace/punctuation folding. Public results
retain the dictionary's original headword and expose the canonical form only as
transport-neutral match metadata, so GUI and future AI-service adapters share
identical lookup semantics.

The headless lookup request supports bounded prefix matching through
`MatchMode::kPrefix`. A separate `Suggest` operation returns lightweight
headwords and match metadata without reading or assembling article bodies.

Complete headword enumeration is a separate per-dictionary capability. The
public core request accepts one stable dictionary identity, a bounded page
size, an opaque cursor, a deadline, and cancellation. Cursors carry a checked
version, service-snapshot identity, dictionary digest, and unique ordinal, so
malformed or stale continuation cannot silently restart. Supported backends
return exact-unique source strings in the pinned legacy case-sensitive UTF-16
code-unit order. StarDict, the article-oriented Aard, BGL, Dictd, DSL, EPWING,
GLS, MDict, SDict, SLOB, XDXF, and ZIM backends, and the LSA, sound-directory,
and ZIP-sound backends share a lazily published four-byte ordinal index over
their immutable natural records.
Construction is bounded by each validated source index and checks
cancellation/deadline, while subsequent pages are direct ranges. Format-owned
aliases, redirect keys, and validated derived audio names are included;
metadata, resource identifiers, resource bytes, and synthesized article text
are not. Audio enumeration never rewalks directories, reads resource files, or
decompresses archive members. Runtime sources and local formats without an
explicit enumeration adapter report unsupported without performing source
work. The desktop facade streams complete single-dictionary exports through
that bounded API without accumulating the list. Exports use UTF-8 with a BOM,
one sanitized headword per LF-terminated line, preserve enumeration order and
exact-duplicate semantics, and atomically replace the destination only after
every page is written and flushed. Unsuccessful exports remove the sibling
temporary file and leave an existing destination unchanged.
Prefix ranking is core behavior: canonical exact matches come first, followed
by shorter canonical candidates with deterministic scores. Neither the GUI nor
a future AI transport reimplements folding, ranking, limits, cancellation, or
dictionary traversal.

Legacy text encoding is also a private foundation concern. Format adapters use
one bounded, strict UTF-8 conversion primitive rather than Qt GUI-era codec
objects or format-local conversion loops. The primitive supports both decoding
dictionary payloads and encoding query text, rejects substitution on malformed
or unrepresentable data, and keeps ICU types out of public headers.

`main.cpp` may wire a future optional integration module into the core
extension contracts; other GUI code must not include adapter headers.

Another product DLL is justified only by a real deployment boundary such as
optional dependencies, platform isolation, or plugin loading. Network, audio,
and desktop integration are evaluated against that rule when their phases
begin instead of being pre-split speculatively.

Phase 2 retains Tiger's base module as build infrastructure and the minimal
GoldenDict application shell. Phase 4 introduces `goldendict_core`.

## AI Dictionary Service Compatibility

The future service is an AI retrieval surface, not a remote copy of the GUI.
Its adapter may use MCP, HTTP, gRPC, or another protocol, but those transports
map onto the same headless core contract.

The contract is designed around:

- dictionary catalog and capability discovery;
- exact, suggested, and later full-text lookup with explicit dictionary and
  language filters;
- bounded single or batch requests with cancellation, deadlines, pagination,
  and result limits;
- structured results containing the requested and normalized headword, match
  kind or score, stable dictionary identity, dictionary metadata, language
  metadata, article text, optional sanitized HTML, and typed resource
  references;
- provenance sufficient for an AI client to identify and cite the dictionary
  source and edition; and
- deterministic errors and partial-result reporting without hidden desktop
  state.

The installed API publishes hard limits for query text, filter counts, filter
size, and result count. The core rejects malformed UTF-8 and embedded NUL
bytes at the service boundary before dispatching work to a format adapter;
requested result counts above the published maximum are deterministically
clamped.

Dictionary content is untrusted data. The core and service adapter must not
execute embedded active content or interpret article text as instructions.
Transport adapters apply authentication, authorization, rate limits, request
budgets, logging, and protocol serialization outside `goldendict_core`.

The General synonym-search preference remains part of the core-owned atomic
preferences candidate. For exact lookup only, core may ask the private
StarDict, Babylon BGL, and GLS adapters to resolve a synonym to its primary
headword and then applies the normal group/dictionary filters, ordering,
bounds, cancellation, and deadline while searching that primary across the
participating dictionaries. Runtime sources and other formats are unchanged;
suggestions, prefix/pattern lookup, enumeration, and the original query stored
in matches, tabs, history, and sessions are not rewritten. StarDict `.syn`
records are validated as source data and cached in a private versioned index;
older implementation-generated caches rebuild automatically.

The General optional-parts preference extends the same complete candidate and
desktop recomposition path without changing installed request or result types.
Private DSL rendering marks `[*]` zones for the common sanitizer, which emits
only fixed inert semantic markup. Desktop composition keeps those zones hidden
behind one script-free article control by default or exposes them when the
default-off preference is enabled. Hidden optional text does not count toward
the existing large-article collapse threshold; structured plain text and
headless visible rendering retain the complete article.

The Phase 8 Preferences completeness audit preserves this ownership rule for
all remaining leaves. Widgets may expose only a backed control and submit a
complete preferences candidate; core owns transport-neutral defaults,
validation, current persistence, and legacy migration; the composition root
must construct and restore replacement runtime state before persistence and a
live swap. Presentation-only tab/window state stays in Widgets and must not
change core tab ordering. Network behavior remains in the optional network
module, and Phase 9 desktop, translation, help, audio, hotkey, scan, and
credential policy is not pulled into Phase 8 through inert controls.

Bounded per-dictionary article-context navigation is accepted as a Phase 7
Widgets leaf, not as a Preferences leaf. The active tab retains a private
presentation snapshot derived from ordered lookup-result dictionary identities
and shares each dictionary's first composed-result index with the results pane.
The context menu shows each dictionary once in first-result order, uses its
catalog display name with an ID fallback, and caps the list at the pinned
default of 20. The legacy `.........` overflow action reveals the existing
results pane. Originating-tab, view, presentation-generation, and WebEngine
document-generation checks make replaced or closed origins harmless. No core
DTO, persistence schema, dictionary backend, or article security boundary
changes.

The next-leaf audit returns to the earliest incomplete foundation and selects
the Phase 5 bounded full-text indexing and query contract. It belongs in
`goldendict_core`: index lifecycle and query semantics remain browser- and
transport-neutral, results preserve stable dictionary/headword provenance and
bounded match metadata, and completion, cancellation, deadlines, corruption,
and resource limits follow the existing headless ownership rules. A generated
reference corpus validates the contract without importing format-specific
indexing. Concrete Phase 6 adapters, the Phase 8 workflow and controls, and
presentation highlighting remain downstream consumers rather than part of the
foundation leaf.

The completed foundation exposes `SearchFullText` through the installed
headless service with bounded query, provenance, match, excerpt, and typed-error
DTOs. Its private versioned reference index uses the existing atomic
generated-index envelope and distinguishes create, reuse, stale rebuild, and
corrupt rebuild. Migrated formats do not advertise full-text support until
Phase 6, and the Phase 8 workflow remains downstream. Adding the virtual
operation intentionally changes the core ABI; the exact SCM/package revision
identifies compatible consumers.

The first Phase 6 full-text adapter leaf is StarDict. It is the smallest
representative article backend because it already combines validated primary
and synonym records, generated-index lifecycle, plain and HTML article
assembly, stable provenance, and installed-consumer fixtures. P6-FT-1 adds a
private full-text capability implemented by StarDict and dispatched by the
application service; it does not extend installed headers or runtime-source
interfaces. Each validated primary `.idx` record contributes one document,
while `.syn` aliases do not duplicate the referenced article. Searchable text
comes from the existing inert article-assembly policy, and a distinct private
artifact under the configured index directory covers `.ifo`, `.idx`,
`.dict`/`.dict.dz`, and optional `.syn` source revision. This private
ingestion, lifecycle, and service-aggregation pattern is the prerequisite for
later per-format adapters. The leaf is complete: the service deterministically
merges bounded StarDict results with typed unsupported and unavailable errors,
and the installed consumer exercises the unchanged `SearchFullText` contract.

The next bounded per-format leaf is P6-FT-2, the private SDict adapter. The
completed StarDict capability and service-dispatch pattern is its prerequisite;
SDict is the smallest remaining dependency-ready representative because one
validated `.dct` source already provides complete full-index enumeration,
plain/zlib/bzip2 article decoding, safe HTML and word-link conversion, stable
record/article offsets, inert article assembly, and generated fixtures. One
document is created for each distinct validated article offset. The first
full-index record that references the offset supplies the canonical headword
and stable record-derived provenance; later records sharing that offset are
aliases and do not duplicate the article. Searchable text is only the bounded
plain text produced by the existing article assembler, excluding metadata,
link targets, resources, and raw markup. A distinct private artifact under the
configured index directory tracks the sole `.dct` source revision. StarDict
and SDict results are merged through the existing private dispatch, while
non-adapted formats remain typed unsupported results. No installed API,
runtime-source interface, public capability, or configuration surface changes.
The leaf is complete with generated distinct-offset, compressed-article,
inert-assembly, lifecycle, contained-failure, mixed-service, and installed-
consumer coverage.

P6-FT-3 creates one private document per validated XDXF article ordinal. The
first validated key belonging to the article supplies the canonical headword;
later keys are aliases and do not duplicate the article. Stable provenance is
derived from that first key's record ordinal and the article ordinal.
Searchable text is only the bounded plain text obtained by passing the existing
sanitized `text/html` article through the inert article assembler. Metadata,
resource contents and paths, link targets, image references, raw XML and
markup, and alias text absent from the assembled article are excluded. A
distinct private `.gdfts` artifact under the configured index directory uses
the discovered `.xdxf` or `.xdxf.dz` file as its complete source revision.
StarDict, SDict, and XDXF reuse the accepted private capability and service
dispatch; all other formats retain typed unsupported behavior. Installed
`SearchFullText` APIs and DTOs, runtime-source interfaces, public capability
flags, preferences, dependencies, and the private index format remain
unchanged.
The leaf is complete with generated source-order, alias-deduplication,
inert-assembly, lifecycle, contained-failure, mixed-service, and installed-
consumer coverage.

The fresh post-P6-FT-3 readiness audit selects P6-FT-4, the private Dictd
adapter, rather than following migration order mechanically. Direct inspection
of pinned legacy `dictdfiles.cc` at `3d93dd6` supersedes the earlier exclusion:
the backend owns `_FTS` creation and search, initializes `can_FTS`, and applies
the `DICTD` full-text preference gate. The migrated reader already retains
every validated source-order index record and optional original-headword
alias, validates article byte ranges, loads plain or dictzip data, returns
`text/plain` articles through the inert assembler, owns the complete two-file
source set, and has generated fixtures. GLS, DSL, BGL, MDict, Aard, ZIM, SLOB,
and EPWING remain later leaves because their encoding/synonym,
expanded-headword, redirect-chain, resource, multi-volume, or specialized-
container rules require larger format-specific contracts.

The audit compares the accepted migrated reader models, not their original
migration sequence:

| Format | Complete records and dedup model | Assembly and provenance | Source/index ownership and readiness |
| --- | --- | --- | --- |
| Dictd | All index records and optional original-headword aliases retained; repeated byte ranges can be deduplicated explicitly | Direct plain-text assembly; first owning record ordinal plus validated offset and size is stable | `.index` plus selected `.dict`/`.dict.dz`, generated fixtures, and direct pinned legacy full-text support; selected |
| GLS | All headword records retained; aliases share an article and primary headword | Safe HTML is reusable, but canonical synonym behavior spans UTF-8/UTF-16 decoding | One plain/compressed source and fixtures with legacy support; later |
| DSL | Expanded optional and tilde headwords share an article | Safe HTML is reusable, but expanded-headword canonical provenance needs its own rule | Plain/compressed source and fixtures with legacy support; later |
| BGL | Primary and alias records share decoded articles | Safe HTML is reusable, but alias provenance crosses binary control records and code pages | One gzip/block source plus embedded resources and fixtures with legacy support; later |
| MDict | All key records retained; redirects resolve by folded target | Safe HTML is reusable, but redirect-chain canonical ownership needs a dedicated rule | MDX plus companion MDD source set and fixtures with legacy support; later |
| Aard | Index records map to decoded article objects, including redirects | Safe article output is reusable, but redirect target dedup/provenance remains format-specific | One archive today, with multi-volume parity outstanding; fixtures and legacy support exist; later |
| ZIM | Directory records resolve redirect chains to cluster/blob articles | Safe article output is reusable, but redirect/resource namespaces and target ownership interact | Consecutive split-file source set, fixtures, and legacy support; later |
| SLOB | Multiple references can share item/bin-backed articles | Safe article output is reusable, but reference-to-item dedup and provenance need a dedicated rule | One container today, with store/conversion variants and fixtures plus legacy support; later |
| EPWING | Word-index records currently retain article text per record | Safe output is reusable, but duplicate/grouped-index ownership is not yet explicit | `CATALOGS` plus subbook tree, fixtures, and legacy support; later |
| LSA / ZIP sounds / sound directories | Complete audio-name records exist, but not textual definition articles | Generated playback HTML contains resource references rather than eligible searchable article text | Resource-oriented backends; excluded from per-format textual full-text ingestion |

P6-FT-4 creates one private document per distinct validated non-metadata Dictd
`(article_offset, article_size)` byte range. Source-order records named
`00databaseshort`, `00-database-short`, `00databaseinfo`, or
`00-database-info` remain dictionary metadata and are not searchable
documents. The first remaining record owning a range supplies the canonical
headword; its optional original headword and later records sharing that range
are aliases and do not duplicate the article. Stable provenance is
`dictd-index:<record-ordinal>:<article-offset>:<article-size>`. Searchable text
is only the bounded plain text obtained by passing the existing `text/plain`
article through the inert assembler; metadata, alias-only text, resource data,
and generated link or markup interpretation are excluded.

The distinct private `.gdfts` artifact under the configured index directory
uses the `.index` file and the actually selected `.dict` or `.dict.dz`
companion as its complete source revision. Changes to either source or a switch
of the selected companion force a stale rebuild. StarDict, SDict, XDXF, and
Dictd reuse the accepted private capability and service dispatch. With no
configured index directory Dictd remains typed unsupported; all other
non-adapted local and runtime formats retain typed unsupported behavior, and
missing requested IDs remain typed unavailable. Installed `SearchFullText`
APIs and DTOs, runtime-source interfaces, public capability flags,
preferences, dependencies, and the private index format remain unchanged.

P6-FT-4 is complete. Dictd now contributes range-deduplicated private
full-text documents from validated non-metadata source records, tracks the
selected two-file source revision, and participates in the accepted mixed
service dispatch without changing installed or runtime-source surfaces.

The fresh post-P6-FT-4 readiness audit selects P6-FT-5, the private GLS
adapter, as the smallest dependency-ready remaining leaf. Pinned legacy
`gls.cc` at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` creates and searches
`_FTS`, initializes `can_FTS`, and
applies the `GLS` full-text preference gate. The migrated reader already
retains every source-order headword record, makes pipe-separated aliases share
one article and primary headword, bounds UTF-8/UTF-16 and plain/gzip decoding,
returns sanitized `text/html`, owns one discovered source, and has generated
reader, backend, and installed-consumer fixtures. The audit finds no other
migrated textual article backend outside the remaining formats below; runtime
sources have no per-format ingestion contract.

| Remaining format | Complete records and dedup model | Assembly/provenance and readiness |
| --- | --- | --- |
| GLS | Source-order records explicitly share one materialized article and primary headword | Single plain/compressed source, safe HTML assembly, fixtures, and pinned legacy support; selected |
| DSL | Optional and tilde-expanded headwords share articles | Safe HTML is reusable, but expanded-headword canonical ownership needs a dedicated rule; later |
| BGL | Primary and alias records share decoded articles | Binary control records and code pages complicate alias provenance; later |
| MDict | Key records resolve folded redirect chains | Redirect target ownership spans MDX and optional MDD resources; later |
| Aard | Index records map to article objects and redirects | Redirect ownership and outstanding multi-volume behavior require a larger contract; later |
| ZIM | Directory records and redirects resolve cluster/blob articles | Redirect/resource namespaces and split-volume ownership interact; later |
| SLOB | References can share item/bin-backed articles | Reference-to-item deduplication and store variants need a dedicated rule; later |
| EPWING | Word-index records retain article text | Duplicate/grouped-index ownership and the specialized source tree are not explicit; later |
| LSA / ZIP sounds / sound directories | Records identify audio resources, not textual definitions | Playback HTML and resource bytes are ineligible for textual ingestion; excluded |

P6-FT-5 creates one private document per validated GLS article ordinal. The
first source-order record referencing the article supplies the canonical
primary headword; later pipe-separated headwords sharing it are aliases and do
not duplicate the document. Stable provenance is
`gls-index:<first-record-ordinal>:<article-ordinal>`. Searchable text is only
the bounded plain text produced by passing the existing sanitized `text/html`
article through the inert assembler. Alias-only headword text, glossary
metadata, resource paths and bytes, image and link targets, raw markup,
`.files` contents, and future resource ZIP contents are excluded.

The distinct private `.gdfts` artifact under the configured index directory
uses the discovered `.gls` or `.gls.dz` file as its complete source revision.
Source mutation or switching the discovered plain/compressed file forces a
stale rebuild; external resource changes do not. With no configured index
directory GLS remains typed unsupported. StarDict, Dictd, SDict, XDXF, and GLS
reuse the accepted private capability and service dispatch in existing backend
order under the global result bound. Adapted no-match dictionaries add no
error, requested non-adapted local or runtime sources remain typed unsupported,
and missing requested IDs remain typed unavailable. Cancellation, deadlines,
corruption, resource limits, and storage failures remain contained per
dictionary.

P6-FT-5 changes no installed `SearchFullText` API or DTO, runtime-source
interface, public capability flag, configuration, preference, dependency, or
private index format. Other adapters, legacy `_FTS` compatibility, metadata or
resource indexing, highlighting, the Phase 8 workflow, and unrelated
refactors are excluded. No leaf after P6-FT-5 is selected.

P6-FT-5 is complete. GLS now contributes one private document per validated
article ordinal with first-record ownership, inert assembled text,
single-source `.gdfts` lifecycle, and deterministic mixed-service dispatch.
No subsequent leaf is selected.

The fresh post-P6-FT-5 audit covers every remaining migrated textual article
format. At pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, `dsl.cc:255-257`,
`bgl.cc:257-259`, `mdx.cc:271-273`, `aard.cc:285-287`, `zim.cc:728-730`,
`slob.cc:637-639`, and `epwing.cc:144-147` each gate an explicitly supported
full-text backend; their constructors at `dsl.cc:310`, `bgl.cc:308`,
`mdx.cc:333`, `aard.cc:332`, `zim.cc:790`, `slob.cc:708`, and `epwing.cc:235`
initialize that capability. No other migrated textual article format remains:
LSA, ZIP sounds, and sound directories materialize resource playback rather
than eligible definition text, and runtime sources have no private per-format
ingestion contract.

| Candidate | Migrated ownership, materialization, and source readiness | Audit decision |
| --- | --- | --- |
| DSL | Optional and tilde-expanded headwords can share articles; bounded plain/compressed decoding, safe HTML, external resource directories, and generated fixtures exist | Resource snapshot and expanded-headword ownership make the leaf larger; unselected |
| BGL | Primary and alternate records share decoded articles; bounded gzip/block parsing, code pages, safe HTML, embedded resources, and generated fixtures exist | Binary control-record and alias ownership remain larger; unselected |
| MDict | Key records and folded redirect chains materialize safe HTML; MDX/MDD discovery and generated fixtures exist | Redirect-chain ownership and the multi-file revision are larger; unselected |
| Aard | Source-order records map directly to deduplicated article ordinals, including redirect-only articles; bounded 32/64-bit indexes, raw/zlib/bzip2 decoding, safe HTML links, one `.aar` source, and generated fixtures exist | Smallest dependency-ready leaf; P6-FT-6 selected |
| ZIM | Directory records and redirects resolve cluster/blob articles; split-file discovery, bounded decoding, resources, and generated fixtures exist | Redirect/resource namespaces and a split-file revision are larger; unselected |
| SLOB | References can share item/bin articles; declared encoding, raw/zlib/bzip2 stores, resources, and generated fixtures exist | Reference/item ownership and content-type filtering are larger; unselected |
| EPWING | Word-index records retain materialized text; bounded decoding, internal references, subbook resources, and generated fixtures exist | Duplicate/grouped-index ownership and the source tree are larger; unselected |

P6-FT-6 therefore adds only the private Aard adapter. Every unique validated
article ordinal contributes one document, including a redirect-only article
already materialized as a safe internal link. The first source-order record
referencing that ordinal supplies the canonical headword; later records are
aliases and do not duplicate it. Stable provenance is
`aard-index:<first-record-ordinal>:<article-ordinal>`. Ingestion passes the
existing sanitized `text/html` article through the inert assembler and indexes
only bounded plain text. Alias-only headword text, metadata, link targets, raw
markup, and future multi-volume or icon data are excluded.

The distinct private `.gdfts` artifact uses the sole discovered `.aar` file as
its complete source revision; mutation or replacement forces a stale rebuild.
Without a configured index directory Aard remains typed unsupported. Adapted
results merge in stable dictionary-ID order under the global bound; adapted
no-match dictionaries add no error, requested non-adapted local or runtime
sources remain typed unsupported, and missing IDs remain typed unavailable.
Cancellation, deadlines, corruption, resource limits, and storage failures
remain contained per dictionary. Installed APIs and DTOs, runtime interfaces,
public capability flags, preferences, dependencies, GUI/Phase 8 behavior, and
the private index format stay unchanged. Legacy `_FTS` compatibility,
metadata/resource indexing, highlighting, other adapters, and unrelated
refactors are excluded. P6-FT-6 is complete with unique-article ingestion,
first-record provenance, inert assembled text, single-source lifecycle, and
deterministic mixed-service dispatch. No leaf after P6-FT-6 is selected.

The fresh post-P6-FT-6 audit revalidates every remaining migrated textual
article format against pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Legacy `dsl.cc:255-257`,
`bgl.cc:257-259`, `mdx.cc:271-273`, `zim.cc:728-730`, `slob.cc:637-639`, and
`epwing.cc:144-147` apply format-specific full-text preference gates, while
their constructors at `dsl.cc:310`, `bgl.cc:308`, `mdx.cc:333`, `zim.cc:790`,
`slob.cc:708`, and `epwing.cc:235` initialize `can_FTS`. DSL additionally owns
legacy `_FTS` creation and traversal at `dsl.cc:1337` and `dsl.cc:1368`, then
dispatches search through `FtsHelpers::FTSResultsRequest` at `dsl.cc:2094`.
Registration inspection confirms that these six formats,
and no unreviewed migrated textual backend, remain after the accepted
StarDict, SDict, XDXF, Dictd, GLS, and Aard adapters.

| Candidate | Complete migrated traversal and ownership | Materialization, sources, and readiness | Audit decision |
| --- | --- | --- | --- |
| DSL | Every source article receives one ordinal; optional and tilde expansions plus alternate headword lines retain source order and share that article | Bounded UTF-8/UTF-16, plain/gzip decoding, sanitized HTML, one selected `.dsl`/`.dsl.dz`, and generated fixtures are complete; annotations and resource directories do not contribute article text | Smallest dependency-ready leaf; P6-FT-7 selected |
| BGL | Primary and alternate binary records share article ordinals, but control-record ownership still spans code-page decoding | One `.bgl` also owns embedded resources; fixtures exist, but alias/control provenance needs a separate contract | Unselected |
| MDict | Source-order keys can redirect through folded chains to record data | MDX plus optional MDD volumes and redirect-target ownership make the complete revision and dedup contract larger | Unselected |
| ZIM | Directory records and redirect chains resolve cluster/blob objects across article and resource namespaces | Consecutive split volumes and namespace-sensitive target ownership require a larger snapshot contract | Unselected |
| SLOB | Source-order references can share item/bin-backed objects | Declared encodings, content-type filtering, stores, and reference/item ownership require a dedicated contract | Unselected |
| EPWING | Subbook word-index records retain materialized article text | Duplicate/grouped-index ownership and the complete `CATALOGS`/subbook source tree remain unsettled | Unselected |

P6-FT-7 adds only the private DSL adapter. Each validated DSL article ordinal
contributes one document. The first expanded record produced from the first
source-order headword line supplies the canonical headword; its remaining
optional expansions, tilde expansions, and later alternate headword lines are
aliases and do not duplicate the document. Stable provenance is
`dsl-index:<first-record-ordinal>:<article-ordinal>`. Searchable content is only
bounded plain text produced by passing the existing sanitized `text/html`
article through the inert assembler. Alias-only text, directives, annotations,
resource paths and bytes, link and image targets, raw DSL or HTML markup,
unsupported abbreviation dictionaries, future resource ZIPs, and unsupported
nested-card behavior are excluded.

The distinct private `.gdfts` artifact uses the selected `.dsl` or `.dsl.dz`
file as its complete source revision; content mutation, replacement, or a
switch between the discovered plain and compressed source forces a stale
rebuild. `.ann` files and `.files` trees affect metadata or resource retrieval
only and are outside the revision. Without a configured index directory DSL
remains typed unsupported. The seven adapted formats merge in stable
dictionary-ID order under the global bound; adapted no-match dictionaries add
no error, requested non-adapted local or runtime sources remain typed
unsupported, and missing requested IDs remain typed unavailable. Cancellation,
deadlines, corruption, resource limits, and storage failures remain contained
per dictionary.

P6-FT-7 changes no installed `SearchFullText` API or DTO, runtime-source
interface, public capability flag, configuration, preference, dependency,
GUI/Phase 8 behavior, or private `.gdfts` serialization. Legacy `_FTS`
compatibility, metadata/resource indexing, highlighting, other adapters, and
unrelated refactors are excluded. No leaf after P6-FT-7 is selected.

P6-FT-7 is complete. DSL now contributes one private full-text document per
validated article ordinal with first-expanded-record ownership, inert assembled
text, a selected-source `.gdfts` lifecycle, and deterministic seven-format
service dispatch. No successor is selected.

The fresh post-P6-FT-7 audit revalidates the complete remaining migrated
textual-format registry as BGL, MDict, ZIM, SLOB, and EPWING. At pinned legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, BGL declares `_FTS`
construction at `bgl.cc:253`, applies its full-text preference gate at
`bgl.cc:255-259`, initializes `can_FTS` and the source-backed `_FTS` lifecycle
at `bgl.cc:308-314`, and builds through the common full-text helper at
`bgl.cc:475-499`. LSA, ZIP sounds, and sound directories remain resource
backends rather than textual-definition candidates; runtime sources have no
private per-format ingestion contract.

| Candidate | Complete traversal and ownership | Materialization and complete revision | Audit decision |
| --- | --- | --- | --- |
| BGL | Supported entry blocks produce source-order article ordinals; every retained primary or alternate record references its owning ordinal | Bounded gzip/block and code-page decoding, sanitized HTML, one `.bgl`, embedded resources, and generated fixtures are complete | Smallest dependency-ready leaf; P6-FT-8 selected |
| MDict | Source-order keys resolve folded redirect chains to record data | MDX plus optional MDD volumes leave redirect-target ownership and the multi-file revision unsettled | Unselected |
| ZIM | Directory records and redirect chains resolve cluster/blob objects across article and resource namespaces | Consecutive split volumes leave namespace-sensitive ownership and the complete revision unsettled | Unselected |
| SLOB | Source-order references can share item/bin-backed objects | Content-type filtering, stores, and reference/item ownership require a dedicated contract | Unselected |
| EPWING | Subbook word-index records retain materialized text | Duplicate/grouped-index ownership and the complete `CATALOGS`/subbook tree remain unsettled | Unselected |

P6-FT-8 adds only the private BGL adapter. Each distinct article ordinal
referenced by at least one retained record contributes one document; an
unreferenced ordinal contributes none. The first retained source-order record
for the ordinal supplies the canonical headword, including when an empty
nominal primary was discarded, and all later primary or alternate records are
aliases that do not duplicate the document. Stable provenance is
`bgl-index:<first-record-ordinal>:<article-ordinal>`.

Searchable content is only bounded plain text produced by passing the existing
sanitized `text/html` article through the inert assembler. Metadata,
alias-only text, embedded resource names and bytes, image and link targets,
raw BGL blocks or markup, and code-page control data are excluded. The
distinct private `.gdfts` artifact snapshots the sole discovered `.bgl` as
its complete source revision. Any container mutation or replacement,
including an embedded-resource-only change, forces a stale rebuild because
article and resource blocks share that physical source, while resource bytes
remain unindexed.

Without a configured index directory BGL remains typed unsupported. The eight
adapted formats retain stable dictionary-ID aggregation, filtering, and the
global result bound; adapted no-match dictionaries add no error, requested
non-adapted local or runtime sources remain typed unsupported, missing IDs are
typed unavailable, and per-dictionary failures remain contained. P6-FT-8
changes no installed `SearchFullText` API or DTO, runtime interface, public
capability flag, configuration, preference, dependency, GUI/Phase 8 behavior,
or private `.gdfts` serialization. Legacy `_FTS` compatibility,
metadata/resource indexing, highlighting, other adapters, and unrelated
refactors are excluded. No leaf after P6-FT-8 is selected or ranked.

P6-FT-8 is complete. BGL now contributes one private full-text document per
referenced nonempty article ordinal with first-retained-record ownership,
inert assembled text, a sole-container `.gdfts` lifecycle, and deterministic
eight-format service dispatch. No successor is selected.

The fresh post-P6-FT-8 audit revalidates the complete remaining migrated
textual-format registry as MDict, ZIM, SLOB, and EPWING. At pinned legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, SLOB declares and gates
full-text support at `slob.cc:633-643`, initializes `can_FTS` and its
source-backed `_FTS` lifecycle at `slob.cc:667-714`, and traverses source-order
references while deduplicating the item/bin identity at
`slob.cc:1148-1263`. The migrated reader parses the reference table in source
order at `modules/core/src/formats/slob/slob_reader.cc:223-248`, validates and
decodes raw, zlib, or bzip2 item stores at `slob_reader.cc:249-299`, and retains
only `text/html*` or `text/plain*` references while deduplicating their
`(item, bin)` payloads at `slob_reader.cc:300-331`. Generated discovery,
reader, dictionary, and installed-consumer fixtures already cover the sole
`.slob` container and its private dependencies.

| Candidate | Complete traversal and ownership | Materialization and complete revision | Audit decision |
| --- | --- | --- | --- |
| MDict | Source-order keys resolve folded redirect chains to record data | MDX plus optional MDD volumes leave redirect-target ownership and the multi-file revision unsettled | Unselected |
| ZIM | Directory records and redirect chains resolve cluster/blob objects across article and resource namespaces | Consecutive split volumes leave namespace-sensitive ownership and the complete revision unsettled | Unselected |
| SLOB | Source-order textual references resolve item/bin-backed articles; later references can alias the same physical pair | Bounded declared-encoding conversion, raw/zlib/bzip2 stores, sanitized HTML/plain articles, one `.slob`, and generated fixtures are complete | Smallest decision-complete leaf; P6-FT-9 selected |
| EPWING | Subbook word-index records retain materialized text | Duplicate/grouped-index ownership and the complete `CATALOGS`/subbook tree remain unsettled | Unselected |

P6-FT-9 adds only the private SLOB adapter. Traversal retains source-order
references whose resolved, lowercased content type begins with `text/html` or
`text/plain`. Retained textual references receive zero-based
`record_ordinal` values in retained-reference order; excluded non-text
references do not consume an ordinal. Each reference carries unsigned
zero-based `item_index` and `bin_index` values from the container. The first
retained encounter of each distinct `(item_index, bin_index)` pair receives a
zero-based `article_ordinal` in first-encounter order and creates one document;
later references to the pair are aliases. The first retained reference owns
the canonical headword and `first_record_ordinal`. Stable provenance is
`slob-index:<first-record-ordinal>:<article-ordinal>:<item-index>:<bin-index>`;
all four values use canonical unsigned base-10 without signs or padding.

Searchable content is only bounded plain text produced by passing the existing
sanitized `text/html` or escaped, `<pre>`-wrapped `text/plain` article through
the inert assembler. Metadata tags, alias headwords as independent content,
non-text bins, resource names and bytes, icons, raw markup, and advanced
conversion are excluded. The distinct private `.gdfts` artifact snapshots the
sole discovered `.slob` as its complete source revision, so any container
mutation or replacement stales the artifact even when only an excluded
resource changes.

Without a configured index directory SLOB remains typed unsupported. The
nine-format mixed service retains stable dictionary-ID ordering, filtering,
the global result bound, adapted no-match behavior, typed unsupported for
requested non-adapted local or runtime sources, typed unavailable for missing
IDs, and contained per-dictionary failures. P6-FT-9 changes no installed
`SearchFullText` API or DTO, runtime interface, public capability flag,
configuration, preference, dependency, GUI/Phase 8 behavior, or private
`.gdfts` serialization. Implementation, legacy `_FTS` compatibility,
metadata/resource indexing, highlighting, other adapters, and unrelated
refactors are excluded. No leaf after P6-FT-9 is selected or ranked.

P6-FT-9 is complete. SLOB now contributes one private full-text document per
distinct retained textual `(item_index, bin_index)` pair, with source-order
retained-reference ordinals, first-reference ownership, exact four-component
provenance, inert assembled text, a sole-container source revision, and
deterministic nine-format service dispatch. No successor is selected.

The fresh post-P6-FT-9 audit covers the complete remaining migrated textual
registry: MDict, ZIM, and EPWING. At pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, MDict gates full-text support at
`mdx.cc:264-274` and uses the complete dictionary filename set for its generic
full-text lifecycle at `mdx.cc:489-517`; ZIM gates support at
`zim.cc:724-733`, owns the complete filename-set lifecycle at
`zim.cc:752-796`, and collects and deduplicates linked articles at
`zim.cc:1094-1195`; EPWING gates support at `epwing.cc:138-148` and uses its
complete dictionary filename set at `epwing.cc:372-397`.

| Candidate | Migrated traversal and ownership | Materialization, revision, and audit decision |
| --- | --- | --- |
| MDict | Source-order keys receive separate article slots before folded `@@@LINK=` resolution | Bounded decoding, styles, safe HTML, MDX/MDD discovery, and fixtures exist, but redirect-target ownership and the exact MDX/MDD revision contract require a prerequisite; unselected |
| ZIM | Directory-table order, bounded redirect resolution, namespace/MIME eligibility, and terminal-entry deduplication are explicit | Bounded UTF-8 HTML/plain text, raw/zlib/bzip2 clusters, consecutive split discovery, private dependencies, and generated fixtures are complete; smallest decision-complete leaf, P6-FT-10 selected |
| EPWING | `CATALOGS` order and supported word-index records are traversed, but current deduplication includes the headword | Safe rendered text and generated fixtures exist, but grouped-index ownership and the complete mutable subbook-tree revision require a prerequisite; unselected |

P6-FT-10 adds only the private ZIM adapter. It traverses every directory entry
in directory-table order and resolves redirects through the existing bounded
cycle and invalid-target checks. A source entry is retained only when it is in
namespace `A`, or in namespace `C` for ZIM 6.1 and later, and its terminal
target MIME begins with `text/html` or `text/plain`. Retained eligible sources
receive zero-based `record_ordinal` values in retained-source order; metadata,
resources, namespace `X`, non-text targets, and all other excluded entries do
not consume an ordinal.

The zero-based directory-table index of the terminal target is the
deduplication identity. Its first retained source creates one document,
supplies the canonical nonempty title-or-URL headword and
`first_record_ordinal`, and assigns a zero-based `article_ordinal` in
first-encounter order. Later retained sources resolving to the same terminal
entry are aliases. Stable provenance is
`zim-index:<first-record-ordinal>:<article-ordinal>:<target-entry-index>:<cluster-index>:<blob-index>`.
All five components use canonical unsigned base-10 without signs or padding;
the format prefix, first logical owner, deduplicated ordinal, terminal
directory identity, and physical cluster/blob identity make the string stable
and collision-safe within the dictionary revision.

Searchable content is only bounded plain text produced by the inert assembler
from the existing validated UTF-8 HTML or escaped, `<pre>`-wrapped plain text.
Metadata, alias headwords as independent content, resource names and bytes,
non-text blobs, icons, raw markup, unsupported compression/conversion, and new
link rewriting are excluded. The distinct private `.gdfts` artifact snapshots
the complete ordered discovered source set: the sole `.zim`, or every
consecutive split part from `.zimaa` through the last part before the first
missing suffix. Mutation, replacement, addition, removal, or order change of
any included part stales the artifact, including a resource-only change.

Without a configured index directory ZIM remains typed unsupported. The
ten-format mixed service preserves stable dictionary-ID ordering, filtering,
the global bound, adapted no-match behavior, typed unsupported for requested
non-adapted local or runtime sources, typed unavailable for missing IDs, and
contained per-dictionary failures. P6-FT-10 changes no installed
`SearchFullText` API or DTO, runtime interface, public capability flag,
configuration, preference, dependency, GUI/Phase 8 behavior, or private
`.gdfts` serialization. Legacy `_FTS` compatibility, metadata/resource
indexing, highlighting, other adapters, and unrelated
refactors are excluded. No leaf after P6-FT-10 is selected or ranked.

P6-FT-10 is complete. ZIM now contributes one private full-text document per
distinct eligible terminal directory entry, with retained-source ordinals,
first-source ownership, exact five-component provenance, inert assembled text,
complete ordered split-volume lifecycle, and deterministic ten-format service
dispatch. No successor is selected.

The fresh post-P6-FT-10 audit covers the complete remaining migrated textual
registry: MDict and EPWING. At pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, MDict gates full-text support at
`mdx.cc:264-274`, uses its complete dictionary filename set for lifecycle
rebuilds at `mdx.cc:489-517`, and extracts article text at `mdx.cc:519-533`;
EPWING gates support at `epwing.cc:138-148`, uses its complete dictionary
filename set at `epwing.cc:372-397`, and extracts the physical page/offset at
`epwing.cc:400-420`.

| Candidate | Migrated traversal and ownership | Materialization, revision, and audit decision |
| --- | --- | --- |
| MDict | MDX keys retain source order and the private immutable view resolves bounded folded `@@@LINK=` chains to terminal physical identities with first-source ownership | Bounded decoded/styled HTML, complete ordered MDX/consecutive-MDD revision, and generated ownership fixtures are accepted by P6-FT-11; no adapter is selected or ranked |
| EPWING | `CATALOGS` order, subbooks, and supported `0x90`/`0x91`/`0x92` word indexes are traversed, but deduplication includes the headword in `(text-file, headword, page, offset)` | Bounded safe rendering and generated fixtures exist. Headword-independent physical ownership and the complete mutable `CATALOGS`/subbook-tree revision are not exposed; unselected and unranked |

P6-FT-11 is complete as a private MDict reader-ownership prerequisite, not an
adapter. Its immutable ingestion view assigns a zero-based
`record_ordinal` to every accepted nonempty MDX key in source order before
redirect resolution. Missing-target and cyclic redirects still consume their
source ordinal; MDD entries never consume one. Bounded folded `@@@LINK=`
resolution reports terminal, missing-target, and cycle outcomes explicitly.
Each terminal exposes its zero-based MDX key ordinal plus exact decoded record
offset and size as physical identity. Deduplication uses only that terminal
identity, never a headword, folded spelling, or equal content.

The first source-order key that resolves to a terminal owns its canonical
headword and `first_record_ordinal`; later resolving keys are aliases. A
zero-based `article_ordinal` follows first-terminal encounter order. This
prerequisite locks the following adapter audit to exact provenance
`mdict-index:<first-record-ordinal>:<article-ordinal>:<terminal-key-ordinal>:<record-offset>:<record-size>`.
Every component is canonical unsigned base-10 without signs or padding. The
logical first owner, deduplicated ordinal, terminal key, and exact record range
make the string collision-safe within the complete dictionary revision.

The ingestion view supplies only the resolved terminal article's existing
bounded decoded/styled HTML for later inert plain-text assembly. Missing
targets, cycles, unresolved redirect payloads, empty inert output, metadata,
MDD resource names or bytes, and alias headwords as independent content are
excluded. Its complete ordered source revision is the discovered MDX followed
by the base MDD and every consecutive `.1.mdd`, `.2.mdd`, and later companion.
Mutation or replacement of any member, or addition or removal of a companion,
changes the revision. MDD companions affect revision only and never own text
documents.

Generated ownership and lifecycle fixtures pin the redirect, identity,
ownership, materialization, consecutive-companion, mutation, checkpoint, and
cancellation rules without publishing partial state.

Until a later audit accepts an adapter, MDict and EPWING remain typed
unsupported without changing mixed-service ordering, dictionary filtering,
the global bound, unavailable errors, or contained per-dictionary failures.
P6-FT-11 changes no installed `SearchFullText` API or DTO, runtime interface,
public capability, configuration, preference, dependency, GUI/Phase 8
behavior, or private `.gdfts` serialization. This prerequisite excludes an
adapter implementation, legacy `_FTS`, metadata/resource indexing,
highlighting, other formats, dependencies, and unrelated refactors. No adapter
or leaf after the prerequisite is selected or ranked.

The fresh post-P6-FT-11 audit revalidates the complete remaining migrated
textual registry as exactly MDict and EPWING. At pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, MDict gates full-text support at
`mdx.cc:264-274`, owns rebuilds through the complete dictionary filename set at
`mdx.cc:489-517`, and extracts article text at `mdx.cc:519-533`; EPWING gates
support at `epwing.cc:138-148`, owns rebuilds through its dictionary filename
set at `epwing.cc:372-397`, and exposes physical article page/offset at
`epwing.cc:400-420`.

| Candidate | Readiness evidence | Audit decision |
| --- | --- | --- |
| MDict | P6-FT-11 supplies source ordinals, explicit terminal/missing/cycle outcomes, folded redirect resolution, exact terminal key/range identity, first-source/article ownership, aliases, checkpoints, safe bounded materialization, and the complete ordered MDX/consecutive-MDD snapshot | Decision-complete; select only P6-FT-12, the private MDict adapter |
| EPWING | Current migrated deduplication still includes the headword in `(text-file, headword, page, offset)`, and neither `CATALOGS` nor the complete mutable subbook tree is exposed as a revision | Not decision-complete; unselected and unranked |

P6-FT-12 is complete as the private MDict full-text adapter. It traverses the
immutable P6-FT-11 ingestion view in `record_ordinal` order and creates one
document for each first encounter of a distinct terminal
`(terminal-key-ordinal, record-offset, record-size)` identity. The first
resolving source owns the canonical headword and `first_record_ordinal`; later
resolving keys remain aliases, and `article_ordinal` follows zero-based
first-terminal encounter order. Each document uses the locked collision-safe
provenance
`mdict-index:<first-record-ordinal>:<article-ordinal>:<terminal-key-ordinal>:<record-offset>:<record-size>`,
with every component canonical unsigned base-10 without signs or padding.

Materialization converts only the resolved terminal's existing bounded
decoded/styled HTML into inert plain text through the established private
assembly path. Missing targets, cycles, unresolved redirect payloads, empty
inert output, metadata, MDD names or bytes, aliases as independent documents,
and active markup are excluded. The source revision is the discovered MDX,
then base MDD, then every consecutive numbered MDD; mutation, replacement,
addition, removal, or order change of any member stales the artifact, including
resource-only MDD changes. MDD companions own no text documents.

The eleven-format mixed service preserves dictionary-ID ordering and filtering,
the global result bound, adapted no-match behavior, typed unsupported for
EPWING and requested non-adapted local or runtime sources, typed unavailable
for missing IDs, and contained per-dictionary failures. P6-FT-12 changes no
installed `SearchFullText` API or DTO, runtime interface, public capability,
configuration, preference, dependency, GUI/Phase 8 behavior, or private
`.gdfts` serialization. Legacy `_FTS`, metadata/resource indexing,
highlighting, other formats, dependencies, and
unrelated refactors. The accepted implementation adds MDict as the eleventh
adapted format with generated ownership, materialization, lifecycle,
mixed-service, and installed-consumer coverage. EPWING remains typed
unsupported and unselected, and no adapter or leaf after P6-FT-12 is selected
or ranked.

The dependent Phase 8 Preferences leaf reuses the installed transport-neutral
`maximum_dictionary_references` field without changing the DTO layout. Core
owns its legacy-compatible `0..9999` validation, current persistence, and
strict `maxDictionaryRefsInContextMenu` migration. Widgets applies the value
to every private per-tab snapshot only after the complete candidate succeeds;
snapshot revisions invalidate actions captured before a live change. Zero
lists no dictionaries and retains the `.........` handoff whenever results are
represented. The results pane remains complete and independently bounded.

P8-PREF-5 exposes only disabled or credential-free manual HTTP CONNECT
proxying with a validated host and port. The optional
network composer injects one private candidate into every configured online
source: Qt HTTP adapters use their request-local network manager, while DICT
uses a bounded, cancellable CONNECT handshake before its raw TCP protocol.
Neither path mutates the application proxy or WebEngine profile. Private DICT
transport tests cover optional CONNECT authentication, but credentials never
enter preferences, persistence, runtime composition, or diagnostics.

The hide-single-tab leaf deliberately evolves the installed transport-neutral
`ApplicationPreferences` DTO with a default-off boolean. Source consumers must
recompile and binaries built against the previous structure layout are not ABI
compatible; Conan's SCM recipe revision and resulting package revision identify
the rebuilt package without changing the in-progress `1.6.0` product version.
Core retains unified validation, current persistence, and strict legacy
`hideSingleTab` migration. Widgets alone applies Qt tab-bar auto-hide, which
changes visibility at one versus multiple tabs without changing tab identity,
ordering, activation, session data, or saved layout.
The following MRU-tab-order leaf deliberately evolves that installed DTO again
with another default-off boolean and the same source-consumer rebuild
requirement. Core owns canonical current persistence and strict legacy
`mruTabOrder` migration, while Widgets owns a runtime-only stable-tab-ID list
bounded by the existing 32-tab session capacity. Ctrl+Tab and Ctrl+Shift+Tab
traverse one chord-stable MRU sequence in opposite directions. This corrects
the pinned legacy reverse inconsistency without reordering or extending the
core or persisted tab collection; facade/session replacement reconstructs the
runtime list from the active identity and persisted positional order.
The following ESC-hiding leaf reuses the existing default-off
`escape_hides_main_window` preference without changing the installed DTO or
configuration schema. Widgets exposes the pinned control and handles plain ESC
only at the main-window key fallback, after the focused child declines it.
Enabled handling hides rather than toggles the window; modal dialogs retain
their own ESC behavior. Tray, close-to-tray, scan-popup, and global-hotkey
behavior remain outside this leaf.
The following article-click leaf likewise reuses the existing default-on
double-click-translation and default-off single-click-selection fields. A
private Widgets boundary handles eligible unmodified left-button events with a
fixed application-owned DOM query in WebEngine's isolated application world.
It returns only a nonempty selection shorter than 60 UTF-16 units, excludes
links, form controls, and editable content, and invalidates stale document,
pointer, or preference callbacks. Article content receives no script, object,
listener, or command capability; CSP, sanitization, navigation policy, and the
existing bounded selection-lookup command remain unchanged.

The Phase 7 network-cache ownership audit assigns the persisted maximum-size
and clear-on-exit fields exclusively to the Qt Network adapter used for
GoldenDict-managed HTTP/HTTPS source traffic. This preserves the pinned
behavior: legacy GoldenDict attached one `QNetworkDiskCache` to its article
network manager, changed that cache's maximum size, and cleared that same
cache during coordinated shutdown. It did not configure an independent
browser cache. Core continues to own the transport-neutral persisted fields;
the network module provides one application-lifetime owner in place of the
former ephemeral per-request manager. Widgets
and `QWebEngineProfile` remain outside the contract.

The composition root injects a dedicated `qt-network-http` path below
`QStandardPaths::CacheLocation`; tests inject an isolated root. A zero limit
means no Qt Network disk cache and evicts the previously owned directory;
a positive limit is converted from MiB to bytes and applied exactly. Startup
validates the bound and prepares the owned path before publishing the network
runtime. Path or cache setup failure produces a redacted diagnostic and
degrades online sources to uncached traffic without blocking local
dictionaries. When clear-on-exit is false, entries may persist across clean
restarts. When it is true, coordinated shutdown first prevents new requests,
cancels or joins outstanding requests, and then clears only the owned Qt
Network cache. Crashes and forced termination provide no cleanup guarantee;
cleanup failure is non-fatal and may be retried on a later clean shutdown.
Diagnostics must not expose URLs, cache keys, credentials, or response data.

Preparation and activation are separate network-runtime operations so the
later Preferences control can validate and prepare a candidate, persist it,
and only then publish it. Preparation never attaches a second disk-cache
instance to the active directory. Activation of a reduced or zero limit may
irreversibly evict disposable bytes; abandoning a prepared candidate leaves
the active owner unchanged.

Preference apply validates and prepares a complete candidate before
persistence and activation. Any validation, preparation, persistence, or
activation failure preserves the previous persisted policy and active owner.
Reducing the limit or disabling caching may evict entries immediately;
rollback restores policy and ownership but cannot reconstruct evicted cache
bytes. This bounded loss of disposable cache content is the only rollback
exception and must remain explicit in the later Preferences leaf.

The Phase 8 General/Network cache Preferences leaf exposes only the pinned
maximum-size and clear-on-exit controls on the Network page. The size editor is
bounded to the legacy 0--2000 MiB range; zero disables disk caching, and the
clear-on-exit control is disabled while zero is selected. Widgets receive the
owned directory only for the legacy explanatory tooltip and do not discover,
own, or reinterpret cache policy. Apply reuses the complete
validate/prepare/persist/activate transaction above, including facade and
article-session preservation and the disposable-byte rollback exception.

The final post-P6-FT-12 textual-format readiness audit covers only EPWING. At
pinned legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`,
`/home/log/Workspace/GoldenDict/epwing.cc:138-148` gates full-text support,
`/home/log/Workspace/GoldenDict/epwing.cc:372-397` rebuilds the legacy
full-text index from the dictionary filename set,
`/home/log/Workspace/GoldenDict/epwing.cc:400-423` extracts an article by
physical page/offset, and
`/home/log/Workspace/GoldenDict/epwing.cc:954-1023` constructs that set from
`CATALOGS` and the selected subbook files. In the migrated tree,
`modules/core/src/formats/epwing/epwing_reader.cc:468-607` traverses `CATALOGS`
subbooks and supported `0x90`/`0x91`/`0x92` indexes, but
`epwing_reader.cc:645-655` deduplicates by
`(text-file, headword, page, offset)`. `epwing_reader.h:34-81` exposes neither
headword-independent physical ownership nor a complete source snapshot.
Therefore the private adapter is not decision-complete, and only P6-FT-13, the
private EPWING reader ownership/revision prerequisite, is selected.

P6-FT-13 is complete with one private immutable ingestion view. Its source
records follow `CATALOGS` subbook order, supported index-table order,
index-page order, and entry order. Every record exposes a zero-based
`record_ordinal`, decoded
headword, and physical `(text_file_ordinal, page, offset)` identity independent
of the headword. The first record that encounters a physical identity owns its
canonical headword and zero-based `article_ordinal`; later records for the same
identity are aliases. Each retained article exposes those ownership fields,
its physical identity, bounded rendered HTML, and traversal checkpoints. Equal
bytes at different physical identities remain different articles.

The prerequisite locks exact future provenance as
`epwing-index:<first-record-ordinal>:<article-ordinal>:<text-file-ordinal>:<page>:<offset>`,
with canonical unsigned base-10 components without signs or padding. Internal
EPWING references remain inert article links: they do not redirect physical
ownership or create documents, and a referenced target may own a future
document only through its own supported word-index record. Future
materialization may derive only inert plain text from the existing bounded
rendering. Empty output, copyright/metadata, resource names and bytes, aliases
as documents, unindexed reference targets, active markup, scripts, media, and
gaiji payloads are excluded.

The ingestion view exposes a `dictionary::SourceSnapshot` captured from
the complete ordered revision: `CATALOGS`; optional decoding-affecting
`LANGUAGE`; then every regular, non-symlink file in every catalog-selected
subbook/content tree, with subbooks in catalog order and files in deterministic
relative-path byte order. Mutation, replacement, addition, removal, path/order
change, or selected-tree topology change stales the future artifact, including
resource-only changes. Unselected sibling trees and generated/cache files are
outside the revision. Snapshot capture and traversal must honor limits,
checkpoints, cancellation, and deadlines and must publish no partial view on
failure.

The accepted P6-FT-13 implementation changes no installed API/DTO, runtime
interface, capability, configuration/Preferences, dependency, GUI/Phase 8
behavior, or private `.gdfts` serialization. It excludes an adapter, legacy `_FTS`,
metadata/resource indexing, highlighting, other adapters, implementation
outside the private EPWING reader/ownership/revision boundary, and unrelated
refactors. EPWING remains typed unsupported until a later adapter audit. No
adapter, successor, or leaf after P6-FT-13 is selected or ranked.

The post-prerequisite readiness audit confirms that P6-FT-13 removed both
EPWING blockers. At current revision
`1935cc2c7f11efa64f597621bc375ca71402dd56`, the private ingestion view now
provides the complete ordered source revision and headword-independent
physical ownership required by the pinned legacy behavior above. The adapter
is decision-complete, so the audit selects exactly P6-FT-14, the private
EPWING full-text adapter. It selects and ranks no successor.

P6-FT-14 is complete and retains exactly one document for each ingestion
article with non-empty assembled text, in retained physical-article order. The
first record
for `(text_file_ordinal, page, offset)` supplies the canonical headword,
`first_record_ordinal`, and `article_ordinal`; later records for that identity
remain aliases and never become documents. Equal rendered bytes at different
physical identities remain separate documents. The exact collision-safe
provenance is
`epwing-index:<first-record-ordinal>:<article-ordinal>:<text-file-ordinal>:<page>:<offset>`,
using canonical unsigned base-10 components without signs or padding.

Materialization passes each retained article's bounded rendered HTML through
the established article assembler and indexes only its non-empty inert plain
text. Internal EPWING references remain inert links and neither redirect
physical ownership nor create documents. Empty output, aliases as documents,
copyright and metadata, resource names or bytes, unindexed reference targets,
active markup, scripts, media, and gaiji payloads are excluded.

The adapter opens or rebuilds the private generated full-text artifact against
the ingestion view's complete ordered `SourceSnapshot`: `CATALOGS`, optional
decoding-affecting `LANGUAGE`, then every regular non-symlink file in each
catalog-selected subbook tree in catalog and relative-path byte order.
Mutation, replacement, addition, removal, path or order change, selected-tree
topology change, and resource-only change make the artifact stale; unselected
sibling trees and generated/cache files do not. Reuse, stale and corrupt
rebuilds, limits, reader/index/storage errors, checkpoints, cancellation, and
deadlines remain typed and publish no partial adapter state.

Service composition supplies the generated-index path and dispatches EPWING
as the twelfth adapted format. Stable dictionary-ID ordering and filtering,
the global result bound, adapted no-match behavior, typed unavailable IDs, and
contained per-dictionary failures remain unchanged. The installed C++
consumer must observe an exact-provenance EPWING result; the installed C
consumer remains unchanged.

P6-FT-14 preserves every installed API and DTO, runtime interface, capability,
configuration and Preferences field, dependency, GUI/Phase 8 behavior,
private `.gdfts` serialization, and the eleven completed adapter behaviors.
Legacy `_FTS` compatibility, metadata/resource indexing, highlighting, other
adapters, compressed or otherwise unsupported EPWING parity, and unrelated
refactors are excluded.

No successor after P6-FT-14 is selected or ranked.

## Phase 8 Full-Text Workflow Readiness

The post-adapter audit is based on migrated revision
`f5547edfc3d5464d2182d5196df669b63765b568` and pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Phase 6 per-format full-text
support is complete: `dictionary_service.h:28-32,47-63,149-172,256-277`
defines the installed bounded query, modes, match and typed-error contract, and
`dictionary_service.cc:1078-1157` validates and aggregates it across the
private backends with deterministic dictionary order, one global result bound,
filtering, deadlines, cancellation, unavailable IDs, contained errors and
partial-response state. P6-FT-1 through P6-FT-14 supply the twelve textual
format adapters without exposing their ingestion or private `.gdfts`
serialization.

The migrated application has no full-text dialog or result presentation.
Its nearest asynchronous precedent is private Widgets code:
`suggestion_worker.cpp:8-102` owns a replaceable pending request, an atomic
cancellation token, worker-thread execution, exception containment and a
terminal callback; `main_window.cpp:715-724,5988-5997,6285-6289` delivers the
response on the GUI thread and stops the worker around facade replacement and
shutdown. The installed full-text operation remains synchronous at
`dictionary_service.h:270-272`; this is sufficient for a private worker and
does not justify another installed request interface.

The pinned legacy application keeps the user concerns distinct even though
one dialog composes them. `mainwindow.cc:4754-4791` owns the modeless dialog
and translation handoff. `fulltextsearch.cc:232-285,408-420,438-516` owns query
modes/options and launches asynchronous per-dictionary requests;
`fulltextsearch.cc:356-368,518-580` owns cancellation, aggregation and busy
state; `fulltextsearch.cc:594-610` activates a result and constructs the
highlight handoff; and `fulltextsearch.cc:613-659` derives dictionaries from
the selected group, full-text eligibility and muting. Index availability and
lifecycle are separate: `fulltextsearch.cc:370-405` displays ready/to-index
counts and the current index name, while `mainwindow.cc:754,1381-1393,4850-4855`
owns background indexing and status. Persistent controls are separate again:
`preferences.cc:360-382,490-575` and `preferences.ui:1253-1429` own global
enablement, the twelve format gates and the maximum dictionary-size policy.

The candidate comparison therefore has explicit dependencies:

| Candidate | Readiness and dependency | Audit decision |
| --- | --- | --- |
| Request/controller ownership and asynchronous execution | The installed operation, cancellation token and private worker precedent are complete; no product choice or public contract is required | Smallest independent prerequisite; P8-FT-1 selected |
| Dialog shell and query modes/options | Requires a request controller and product decisions about which current persisted defaults are dialog state | Deferred and not ranked |
| Dictionary selection | Requires dialog composition and an explicit choice between legacy group/muting semantics and the migrated dictionary-bar/group model | Deferred and not ranked |
| Result presentation and activation | Requires completed requests plus a decision about grouping, duplicate headwords, partial errors and lookup handoff | Deferred and not ranked |
| Highlighting | Requires activation semantics and a bounded WebEngine highlighting contract not present in current DTOs | Deferred and not ranked |
| Persistent Preferences controls | Persistence exists, but applying enablement, format exclusions and size limits affects index composition/lifecycle policy | Deferred and not ranked |
| Index availability/lifecycle visibility | Current installed identity and response DTOs expose neither capability nor lifecycle state; background build ownership is a separate prerequisite | Deferred and not ranked |

P8-FT-1 is complete as a private cancellable asynchronous full-text request
controller in Widgets. It accepts an immutable `FullTextQuery`, a borrowed
`DictionaryService` whose lifetime is guarded by its owner, and a monotonically
increasing request generation. One private worker owns at most one pending and
one running request and a cancellation token for each accepted request. A new
submission cancels and replaces older pending/running work. The worker invokes
the unchanged `SearchFullText` operation off the GUI thread and posts exactly
one terminal `FullTextResponse` with its generation back to the GUI thread.
The controller discards stale generations and completions received after its
consumer is detached.

Core continues to own query validation, dictionary filtering and ordering,
query modes, limits, deadlines, result and match DTOs, per-dictionary failures
and partial-response semantics. The controller owns only request lifetime,
threading, cancellation and safe completion delivery. Explicit cancel,
consumer destruction, facade replacement and application shutdown cancel
pending/running work and join the worker before the borrowed service can be
replaced or destroyed. These operations are idempotent. Exceptions crossing
the service call boundary become one terminal `kInternal` error with an empty
dictionary ID; normal core partial results and typed cancellation, deadline,
unavailable and backend errors pass through unchanged. P8-FT-1 exposes only a
binary running/finished lifecycle to its future consumer and does not invent
per-dictionary progress.

P8-FT-1 does not add the dialog, action/menu wiring, query widgets, dictionary
or group selection, result model, activation or article lookup, highlighting,
Preferences UI or application of persisted full-text policy, index readiness
or progress, or background index lifecycle. It changes no installed API or
DTO, runtime-source interface, dependency, configuration representation,
private `.gdfts` serialization or adapter. Legacy `_FTS` compatibility,
metadata/resource indexing, platform integration, unrelated UI parity and
refactors remain excluded.

The focused private QTest covers off-GUI-thread execution, GUI-thread terminal
delivery, replacement and explicit cancellation, unchanged partial and typed
errors, exception containment and redaction, stale and detached completion
rejection, service replacement, destruction, and shutdown join. No successor
after P8-FT-1 is selected or ranked.

### Phase 8 full-text query-mode persistence prerequisite

The documentation-only post-P8-FT-1 readiness audit is pinned to migrated
revision `66c53d263ebfb735f898b950786bc43f9081821b` and legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks exactly the modeless
dialog shell/query controls, dictionary/group/muting selection, result
model/presentation/activation, article-highlighting handoff, persistent
Preferences controls, and index availability/status/background lifecycle.
Selection still requires a group/muting policy; presentation and highlighting
require activation and WebEngine contracts; Preferences requires index policy;
and index visibility requires lifecycle APIs. The dialog/query leaf is nearest
but exposes one smaller independent persistence prerequisite.

Pinned legacy `fulltextsearch.cc:232-285,387-398` and `config.hh:156-181`
persist four modes in UI order: whole words `0`, plain text `1`, wildcards `2`,
and regular expression `3`. Migrated `ApplicationPreferences` currently
persists only whole words `0`, wildcard `1`, and regular expression `2`, while
the installed `FullTextQueryMode` already supports all four. Reusing legacy
ordinals directly would reinterpret existing migrated configuration.

P8-FT-2 completes only the nonvisual four-mode persistence prerequisite. The
implementation adds the authorized installed/public
`FullTextSearchMode::kPlainText = 3` enumerator while preserving the enum's
underlying type, the existing `ApplicationPreferences` field, DTO layout, and
scalar configuration wire shape. Migrated meanings remain `0` whole words,
`1` wildcard, `2` regular expression, and become `3` plain text. The legacy
XML importer translates its distinct historical ordinals explicitly: `0`
whole words, `1` plain text, `2` wildcard, and `3` regular expression.

Configuration loading, validation, and serialization own this mapping. The
input is persisted query-mode state; the output is the typed application
preference; migrated serialization emits the migrated ordinal. Unknown
current or legacy values remain atomic configuration errors. Existing
match-case, word-distance, article-limit, word-order, diacritic and all other
full-text fields retain their current validation and wire behavior.

P8-FT-2 starts no request, owns no service or controller, and has no
asynchronous lifetime, cancellation, visible error, action, dialog, query
widget, or query-construction behavior. It excludes dictionary/group/muting
selection, results, activation and highlighting, Preferences widgets or policy
application, index availability/status/background lifecycle, adapter or
`.gdfts` changes, legacy `_FTS` binary compatibility, metadata/resource
indexing, dependencies, platform work, and unrelated Phase 8/9/10 behavior.
No successor after P8-FT-2 is selected or ranked.

### Phase 8 full-text per-dictionary limit prerequisite

The fresh documentation-only post-P8-FT-2 readiness audit is pinned to
migrated revision `fcc1eec921a5e564b9b49cefdbf00f4846d71e21` and the
unchanged legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It rechecks the modeless dialog/query controls, dictionary/group/muting
selection, result presentation/activation, highlighting, Preferences
integration, and index visibility/status/background lifecycle against the
completed private request controller and four-mode persistence.

P8-FT-3 completes the one narrow prerequisite before the dialog/query-controls
leaf. The installed `FullTextQuery::result_limit`
remains the global hard safety and output cap with its existing default,
validation and source meaning. A distinct installed
`maximum_articles_per_dictionary` field is added with default `100` and valid
range `1..100000`, matching the persisted legacy preference one-to-one. The
two fields are validated independently and neither is derived from or
multiplied by the other.

For each selected dictionary, the application-service aggregator requests and
accepts at most `min(maximum_articles_per_dictionary, remaining global
result_limit)` articles. It stops when the unchanged global `result_limit` is
reached. The new default preserves existing callers because the global cap is
already at most `100`. Dictionary order and filtering, cancellation,
deadlines, unavailable and unsupported dictionaries, contained errors and
partial-response behavior remain unchanged.

Core owns both installed query limits, their validation, and aggregation.
Future Widgets dialog composition maps the legacy "maximum articles per
dictionary" control only to `maximum_articles_per_dictionary`; the global cap
remains an internal or advanced safety contract and is not relabeled as that
legacy control. The additive DTO member is an authorized public/source-
compatible expansion that requires consumer rebuild plus install and
exact-SCM package verification.

The completed P8-FT-3 adds no action, dialog, query widget, selection policy,
result model, activation, highlighting, Preferences widget, index visibility,
progress or background lifecycle. It preserves the completed controller,
four-mode persistence and legacy translation, all twelve adapters, private `.gdfts`
serialization and dependencies. Legacy `_FTS` compatibility,
metadata/resource indexing, platform work and unrelated migration behavior
remain excluded. Dictionary/group selection still requires an explicit
migrated group/muting policy; presentation and highlighting retain activation
and WebEngine dependencies; Preferences and index visibility retain index
policy and lifecycle prerequisites. No successor after P8-FT-3 is selected or
ranked.

### Phase 8 full-text optional per-dictionary limit prerequisite

The fresh documentation-only post-P8-FT-3 readiness audit is pinned to
migrated revision `bbad53ecebee93419caa3acf9c560bea128c3a83` and the
unchanged read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks the modeless
dialog/query controls, dictionary/group/muting selection, result
presentation/activation, highlighting, Preferences integration, and index
visibility/status/background lifecycle against the completed asynchronous
controller, four-mode persistence, and global/per-dictionary query limits.
Migrated evidence is `dictionary_service.h:28,151-161` for the current global
bound and DTO defaults, `dictionary_service.cc:633-655` for validation, and
`dictionary_service.cc:1136-1153` for remaining-capacity aggregation. Pinned
legacy evidence is `config.hh:156-176` for the separate value and enablement
fields and `fulltextsearch.cc:249-256,387-394,438-446` for checkbox loading,
persistence, and unchecked `-1` request behavior.

P8-FT-4 completes the sole dependency-ready leaf selected by the audit: the
installed DTO, validation, and aggregator prerequisite needed to express the
legacy per-dictionary-limit toggle without a sentinel. Core changes
`FullTextQuery::maximum_articles_per_dictionary` to
`std::optional<std::size_t>`, engaged by default at `100` so existing P8-FT-3
callers retain their behavior. Engaged values validate in `1..100000`;
disengaged means that no per-dictionary truncation is applied, while the
independent global cap still bounds the request and response.

`FullTextQuery::result_limit` remains the global hard safety and output cap.
Its caller default remains `20`; only its accepted range widens from
`1..100` to `1..1000000`. For an engaged per-dictionary limit, the aggregator
requests at most the minimum of that value and the remaining global capacity.
For a disengaged limit, it requests at most the remaining global capacity. It
always stops at `result_limit`; neither limit is derived from or multiplied by
the other limit or the selected dictionary count.

The later Widgets dialog composer, which P8-FT-4 does not select, maps a
checked legacy control to the engaged persisted value and an unchecked control
to `std::nullopt`. It uses the fixed application global cap `100000`,
independent of the selected dictionary count and per-dictionary value. Core
owns the optional installed field, both bounds, validation, and aggregation;
Widgets will own only that later UI-to-query mapping.

P8-FT-4 is an authorized installed/public ABI expansion verified through a
consumer rebuild, install verification, and exact-SCM package verification. It
changes no controller, persistence mapping, adapter, private `.gdfts` format,
or dependency and adds no visible UI. Dialog composition,
dictionary/group/muting policy, results/activation, highlighting, Preferences
widgets and index policy, index visibility/status/background lifecycle, legacy
`_FTS`, metadata/resource indexing, platform work, and unrelated migration
behavior remain excluded. The completed P8-FT-4 selects or ranks no successor.

The fresh documentation-only post-P8-FT-4 readiness audit is pinned to
`e594de3fc6c0b6e912387d78f873eb9dd3e5a749` and the unchanged read-only legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes the remaining
Phase 8 full-text workflow into private query composition, modeless dialog and
action integration, dictionary/group/muting projection, result presentation
and activation, highlighting, Preferences/index policy, and index
visibility/status/background lifecycle. Only P8-FT-5, the private Widgets
query composer, is dependency-ready and selected. It is a non-integrated
prerequisite rather than inert application UI.

P8-FT-5 is complete as a private, non-integrated Widgets query composer. It
deterministically converts private controls initialized from persisted defaults
to one `FullTextQuery`. UTF-8 text is copied unchanged. Persisted whole
words, plain text, wildcard, and regular-expression modes map explicitly to
`kWholeWords`, `kPlainText`, `kWildcard`, and `kRegularExpression`; match case,
ignore diacritics, and ignore word order map directly. A checked word-distance
control maps its `0..1000` value to `maximum_word_distance`, while unchecked
maps to `std::nullopt`. A checked per-dictionary control maps its persisted
`1..100000` value to `maximum_articles_per_dictionary`, while unchecked maps
to `std::nullopt`. The independent application global `result_limit` is always
`100000`; the existing default timeout is retained. Until the separate
selection leaf, dictionary IDs remain empty and `dictionary_filter_active`
remains false.

Whole-word and plain-text modes enable word-order and optional-distance
controls. Wildcard and regular-expression modes disable their effective query
mapping by composing `ignore_word_order = false` and
`maximum_word_distance = std::nullopt`, without destroying retained control
values. Construction and repeated composition neither mutate persisted
configuration nor start backend work.
Core remains authoritative for bounds, validation, search semantics, ordering,
filtering, typed errors, cancellation, deadlines, and aggregation. The
installed API, persistence schema, facade, controller, adapters, dependencies,
and private index format remain unchanged.

Migrated evidence is `dictionary_service.h:28-32,47-52,149-160` for query
fields and bounds, `application.h:193-198,268-277` for persisted defaults, and
`full_text_request_controller.h/.cpp` plus
`full_text_request_controller_test.cpp` for the accepted downstream submission
boundary. Pinned legacy evidence is `fulltextsearch.ui` and
`fulltextsearch.cc:232-315,387-446` for control identities, mode-dependent
enablement, retained values, and optional-limit behavior. Modeless dialog/main-
window action integration, selection/muting, request completion UI,
results/activation, highlighting, Preferences enablement and index policy,
index visibility/background lifecycle, `.gdfts`, legacy `_FTS`, and unrelated
behavior are excluded. No successor after P8-FT-5 is selected or ranked.

The focused offscreen Widgets QTest covers all four mode mappings, direct
booleans, exact optional bounds and unchecked states, retained values through
mode transitions, the fixed independent global cap, unchanged timeout and
unfiltered dictionary state, UTF-8 text, repeated deterministic composition,
and persisted-preference immutability. The private component has no controller
or service dependency and cannot submit work. Its registration intentionally
raises the suite baseline from 104 to 105 tests. P8-FT-5 changes no installed
interface or persistence schema and selects or ranks no successor.

### Phase 8 full-text capability projection

The completed P8-FT-6 implementation started from clean pushed migrated
revision `ac8a01c6b6212d6313364e2107a3bcb8b13df535`
and the unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It independently evaluates the
remaining modeless dialog/MainWindow action integration,
dictionary/group/muting projection, request submission/completion states,
results/activation, highlighting, Preferences/index policy, and index
visibility/background lifecycle. It selects exactly one smallest prerequisite,
P8-FT-6: expose full-text search capability through the installed dictionary
catalog. No later workflow leaf is selected or ranked.

P8-FT-6 is complete because pinned legacy selection includes only dictionaries
whose `canFTS()` is true before applying group muting, while the migrated
Widgets boundary can currently observe catalog identity and group membership
but not full-text eligibility. Eligibility is discovered only inside the
private Core search implementation by testing for `FullTextBackend`; allowing
Widgets to reproduce that test would violate the shared-library/GUI boundary.
The installed, transport-neutral `DictionaryIdentity` therefore gains one
additive boolean, `supports_full_text_search`, defaulting to `false`.

Core is the sole owner of the mapping. `GetCatalog()` reports `true` exactly
when the composed backend implements the private `FullTextBackend` contract;
the same public identity carried by every `FullTextResult` reports the same
value. The twelve completed textual adapters—StarDict, Dictd, SDict, XDXF,
GLS, DSL, BGL, MDict, Aard, ZIM, SLOB, and EPWING—map to `true`. LSA, ZIP
sounds, sound directories, online sources, external programs, and any current
or future backend without that private contract map to `false`. Catalog order,
identity fields, source redaction, search behavior, index construction, and
error behavior are unchanged; no caller infers readiness from file existence
or attempts a search to discover capability.

The field describes whether the current composed dictionary supports the
installed full-text operation, not whether its private index is presently
ready, stale, building, or failed. It is recomputed whenever the application
replaces the dictionary service and has no independent persistence or mutable
lifecycle. Default `false` preserves source compatibility for aggregate
initialization, while the installed-struct expansion is an intentional ABI
change requiring exact-SCM consumer/package verification.

Acceptance is covered by focused Core tests proving exact true/false mapping,
catalog/result consistency, stable ordering, and replacement recomputation;
the installed C++ consumer must read both a supported and unsupported catalog
entry, while the installed C consumer remains unchanged because this C++ DTO
is not in its interface. The registered suite remains 105 tests. Full Release
tests, install verification, standalone installed-consumer verification, and
clean committed exact-SCM `conan create` with packaged consumers are the
completed implementation gates.

P8-FT-6 affects only the installed `DictionaryIdentity` declaration, private
Core identity projection/catalog composition, focused Core and installed C++
consumer tests, and build metadata only if needed to register those tests. It
does not add a dialog or action, apply group membership or muting, submit or
complete requests, present or activate results, highlight articles, expose
index readiness/status, start background indexing, apply Preferences policy,
change persistence, adapters, `.gdfts`, legacy `_FTS`, dependencies, or
platform behavior. Decisive migrated evidence is
`dictionary_service.h:85-96,149-160,182-195,256-272`,
`dictionary_service.cc:468-481,1075-1119`,
`main_window.cpp:5535-5770`, and the completed P8-FT-1/P8-FT-5 components.
Pinned legacy evidence is `fulltextsearch.cc:613-659`. No successor after
P8-FT-6 is selected or ranked.

### Phase 8 full-text dictionary participation projection (complete)

The fresh documentation-only post-P8-FT-6 readiness audit is pinned to clean
pushed migrated revision `9801ebdb99e09600efc0fad32405bee02dd4971e` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It independently decomposes the
remaining modeless dialog/MainWindow action integration, dictionary/group and
muting projection, request submission/completion states, results presentation
and activation, highlighting, Preferences/index policy, and index
visibility/background lifecycle. It selects exactly one smallest dependency-
ready leaf, P8-FT-7: the private Widgets dictionary-participation projection.
No successor after P8-FT-7 is selected or ranked.

P8-FT-7 is complete and combines only already-owned facts. Core remains the
sole owner of `DictionaryIdentity::supports_full_text_search`. MainWindow
remains the owner of the selected group, configured ordered membership and
normal-search muting, and the per-group ephemeral `dictionaryBar` checks. A private Widgets seam
projects those facts into the P8-FT-5 composer result; it does not inspect a
private backend or infer index readiness.

Projection starts in catalog order for All Dictionaries and configured member
order for a named group, drops unresolved IDs and every identity whose public
full-text capability is false, and drops the named group's configured
`muted_dictionary_ids`. When `dictionaryBar` is visible, only its currently
checked IDs remain, preserving the applicable catalog/group order. When it is
hidden, its ephemeral checks are not an active filter and the capability-
filtered baseline applies. The projection always writes the ordered IDs to
`FullTextQuery::dictionary_ids` and sets `dictionary_filter_active = true`,
including when the result is empty; an empty selection therefore cannot widen
into an unfiltered service request. Catalog replacement, group changes, muting
changes, and bar changes are recomputed from current state and never persisted
by this seam.

Focused private Widgets tests cover All Dictionaries and named-group order,
capability filtering, configured muting, unresolved members, visible checked
and unchecked bar state, hidden-bar baseline behavior, an active empty filter,
catalog/group replacement, and preservation of every other composed query
field. They also prove no controller call and no preference/configuration
mutation. A focused offscreen MainWindow smoke proves that the real group
selector and dictionary bar feed the projection without opening a dialog or
submitting a request. The implementation gate is the focused tests,
Linux Release configure/build and full `ctest --preset conan-release` with
only an intentional registered-test delta, followed by clean committed exact-
SCM `conan create` with the Release Qt WebEngine host profile and packaged
consumers. The added smoke raises the registered suite from 105 to 106 tests.
No install or standalone installed-consumer check is required because no
installed surface changes.

P8-FT-7 affects only the private Widgets projection seam, its focused tests,
the MainWindow smoke, and test registration if needed. Modeless dialog/action
integration, request submission/completion UI, results and activation,
highlighting, Preferences/index policy, index visibility/status/background
lifecycle, public APIs, persistence, adapters, `.gdfts`, legacy `_FTS`,
dependencies, and unrelated behavior remain excluded. Decisive migrated
evidence is `full_text_query_composer.h/.cpp`,
`dictionary_service.h:85-96,149-160`, `main_window.cpp:5535-5770`, and the
P8-FT-5/P8-FT-6 focused tests. Pinned legacy evidence is
`fulltextsearch.cc:613-659`. No successor after P8-FT-7 is selected or ranked.

The fresh documentation-only post-P8-FT-7 readiness audit is pinned to clean
pushed migrated revision `1eeea0a73ff29b832f68012bd2b99b7f6208cf87` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It decomposes the remaining
modeless dialog and MainWindow/Search action integration, request submission,
cancellation, replacement and completion states, results presentation and
activation, highlighting, Preferences/index policy, and index
visibility/status/background lifecycle. Exactly one smallest dependency-ready
leaf is selected: P8-FT-8, the private modeless full-text dialog shell and
MainWindow/Search-action integration. No successor after P8-FT-8 is selected
or ranked.

P8-FT-8 is complete and owns only the Widgets host and its MainWindow
lifetime. It adds
`fullTextSearchAction` after `searchInPageAction` in `menuSearch`, with legacy
text `Full-text search`, `Ctrl+Shift+F`, `WidgetWithChildrenShortcut`, and
`TextHeuristicRole`. The action is enabled exactly when a usable desktop facade
is attached; persisted full-text enablement and index readiness do not govern
it in this leaf. Triggering creates at most one non-modal MainWindow-owned
dialog, or shows, raises, and activates the existing instance. The dialog title
is `Full-text search`, the window-context-help button is absent, and the
completed P8-FT-5 composer is its query-control body. On first creation it
copies the current main lookup text into the composer and selects the entire
query. Each show recomputes the completed P8-FT-7 projection from the current
catalog, selected group, configured muting, and visible dictionary-bar checks.

Closing the dialog destroys that one instance and clears the MainWindow
reference. Dialog close, MainWindow destruction, and facade replacement first
detach and stop the P8-FT-1 controller, so its borrowed service cannot outlive
the facade even though P8-FT-8 never submits work. Re-triggering after close
creates a fresh shell from current preferences and MainWindow participation
state. Geometry and control edits are not persisted by this leaf.

Focused private Widgets tests verify the action identity/order/mappings,
facade-dependent enablement, singleton create/show/raise lifecycle, initial
query copy and selection, current-state reprojection, close/reopen freshness,
and safe close/facade-replacement/MainWindow teardown without a service call.
An offscreen MainWindow smoke exercises the real menu, lookup field, group
selector, and dictionary bar. The full implementation gate is the focused
tests, Linux Release configure/build, full `ctest --preset conan-release` with
only the intentional registered-test delta, then clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers. No install or standalone installed-consumer check is required
because installed interfaces remain unchanged.

P8-FT-8 affects only the private dialog shell, MainWindow action/composition
and lifetime wiring, focused tests, smoke, and test registration. Request
submission/cancellation/replacement/completion UI, results projection and
presentation, activation, highlighting, Preferences/index policy, index
visibility/status/background lifecycle, public APIs, configuration
persistence, adapters, `.gdfts`, legacy `_FTS`, dependencies, and unrelated
behavior are excluded. Decisive migrated evidence is
`main_window.cpp:425-433,6064-6086`, `full_text_query_composer.h/.cpp`,
`full_text_dictionary_projection.h/.cpp`, and
`full_text_request_controller.cpp:143-197`; pinned legacy evidence is
`mainwindow.ui:614-627`, `mainwindow.cc:4754-4791`, and
`fulltextsearch.cc:195-340`. No successor after P8-FT-8 is selected or ranked.

P8-FT-9 is complete as the private dialog request submission, cancellation,
replacement and terminal-state integration leaf selected by the post-P8-FT-8
audit. Its implementation is based on clean migrated revision
`750177f4f8a2d9fd709a2c27efe3e254505308d6` and the unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. No successor after
P8-FT-9 is selected or ranked.

P8-FT-9 consumes the completed P8-FT-1 request controller, P8-FT-5 query
composer, P8-FT-7 authoritative dictionary projection and P8-FT-8 dialog
lifetime. Search composes the current controls, reapplies the latest projected
dictionary IDs and active-filter flag, assigns a monotonically increasing
dialog generation, clears the prior private terminal response, and submits
through the controller. A newer submission replaces running or pending work;
the private submission entry point remains callable for replacement while the
visible Search button is disabled, and only the current generation may change
dialog state. Cancel is idempotent,
cancels the controller, invalidates the active generation and returns the
dialog to idle without accepting a cancelled or stale completion.

While a request is active, the dialog shows an indeterminate progress
indicator and exposes cancellation. Current-generation completion stores the
unchanged `FullTextResponse` privately for a later presentation leaf, hides
progress and restores idle Search state. Close, facade replacement and
MainWindow teardown retain P8-FT-8 detach/stop ordering. This leaf does not
project, merge, sort, display, select or activate results; display counts,
beeps, user-facing validation/error/partial-response policy, highlighting,
persistence and indexing UI remain unselected.

Focused private Widgets tests and an offscreen dialog/MainWindow smoke verify
exact current composition plus projection, increasing generations, running
state, replacement cancellation, explicit idempotent cancellation, stale and
cancelled completion suppression, unchanged current response retention,
terminal idle restoration and safe teardown. P8-FT-9 extends existing test
registrations, so the registered Release suite remains 108 tests. The
implementation gate is the
focused tests, Linux Release configure/build, full
`ctest --preset conan-release` with only the intentional registered-test delta,
then clean committed exact-SCM `conan create` with the Release Qt WebEngine
host profile and packaged consumers. Installed interfaces remain unchanged,
so install and standalone installed-consumer checks are not required.

Result model/presentation, ordering/metadata, selection/article activation,
highlighting handoff/article-view behavior, Preferences enablement and
full-text/index policy, index readiness/visibility/status/background
lifecycle, public APIs, persistence, adapters, `.gdfts`, legacy `_FTS`,
dependencies and unrelated behavior are excluded. Those presentation surfaces
depend on the completed response path; highlighting additionally depends on
activation and a reviewed WebEngine handoff. Preferences and indexing remain
blocked on product policy and a Core lifecycle contract rather than being
folded into this leaf. Decisive migrated evidence is
`full_text_request_controller.h/.cpp`, `full_text_query_composer.h/.cpp`,
`full_text_dictionary_projection.h/.cpp` and
`full_text_search_dialog.h/.cpp`; pinned legacy evidence is
`fulltextsearch.cc:338-570` and `fulltextsearch.ui:99-238`. No successor after
P8-FT-9 is selected or ranked.

### Phase 8 full-text per-document response projection prerequisite (complete)

The fresh documentation-only post-P8-FT-9 readiness audit is pinned to clean
migrated revision `ee632c2e470b1f24e73d311dd0eec8727d5b5c15` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks result response
projection/model, ordering/metadata/error and partial-response presentation,
result UI and selection states, article activation/lookup handoff,
highlighting/WebEngine handoff, Preferences enablement/index policy, and index
readiness/visibility/status/background lifecycle without ranking them in
advance.

The audit selected exactly one smallest independently dependency-ready
prerequisite, P8-FT-10: a private, non-integrated Qt item model that projects
one immutable row for every `FullTextResult` in the completed terminal
`FullTextResponse`. Rows preserve Core response order and remain distinct even
when headwords compare equal. `Qt::DisplayRole` exposes the UTF-8 headword as a
`QString`; private typed access returns the unchanged dictionary identity,
headword, document ID, match metadata, excerpt, and byte-match vector needed by
later presentation and activation leaves. Resetting from a response replaces
the complete snapshot atomically; an empty response produces zero rows.

Core remains authoritative for validation, dictionary ordering, global and
per-dictionary bounds, match metadata, contained errors, and partial-response
state. Widgets owns only the lossless row projection and snapshot lifetime.
The model neither merges legacy case-insensitive duplicate headwords nor
sorts, filters, localizes, truncates, or interprets result metadata. It retains
no error or partial state because their visible presentation policy remains a
separate decision; the dialog continues to retain the terminal response
unchanged until a later integration leaf.

P8-FT-10 depends only on the installed result DTO and completed P8-FT-9
terminal-response path. Its implementation is limited to the private
model, focused offscreen Widgets QTest, and existing private test registration.
Acceptance requires exact row count and order, distinct duplicate headwords,
UTF-8 display conversion, field-for-field typed metadata preservation,
empty/reset/replacement behavior, source-response independence, and no dialog,
controller, service, or persistence effects. The focused gate is that model
QTest; the full gate is Linux Release configure/build, full
`ctest --preset conan-release` with only its intentional registered-test delta,
then clean committed exact-SCM `conan create` with the Release Qt WebEngine
host profile and packaged consumers. Installed interfaces are unchanged, so
install and standalone installed-consumer checks are not required.

Result-view layout, counts, selection, empty/error/partial states, article
activation and lookup handoff, highlighting and WebEngine behavior,
Preferences enablement/index policy, index readiness/visibility/status and
background indexing lifecycle, public APIs, persistence, adapters, `.gdfts`,
legacy `_FTS`, dependencies, and unrelated behavior are excluded. Decisive
migrated evidence is
`modules/core/include/goldendict/core/dictionary_service.h:164-196`,
`modules/core/src/application/dictionary_service.cc:1078-1157`, and
`apps/goldendict/src/full_text_search_dialog.h/.cpp`; pinned legacy evidence is
`fulltextsearch.hh:41-65,135-156`,
`fulltextsearch.cc:129-186,518-610,685-750`, and
`fulltextsearch.ui:99-238`. No successor after P8-FT-10 is selected or ranked.

P8-FT-10 is complete. The private model owns an ordered immutable result
snapshot, exposes only the UTF-8 headword for `Qt::DisplayRole` and unchanged
typed result access, and retains no response-level error or partial state. It
remains non-integrated, and no successor is selected or ranked.

### Phase 8 full-text dialog response-model integration prerequisite (complete)

The fresh documentation-only post-P8-FT-10 readiness audit is pinned to clean
migrated revision `f177cb2915a1c0e4618b44713e40c4eb1cf4c600` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks without advance
ranking the dialog's retained response and private model, visible result
presentation and states, activation/navigation/lookup, highlighting and
WebEngine handoff, Preferences enablement/index policy, and index
readiness/visibility/status/background lifecycle.

The audit selects exactly one smallest independently decision-complete leaf,
P8-FT-11: private synchronization of the completed P8-FT-9 dialog response
ownership with the completed P8-FT-10 projection model. The dialog owns one
child `FullTextResponseModel`. Starting a replacement submission clears the
retained response and model rows together. Only a generation-current terminal
completion is retained unchanged and copied into the model's independent
ordered result snapshot; response errors and `partial` remain solely in the
unchanged retained response. Stale or cancelled completions update neither.
Cancellation, service replacement, controller detachment, and teardown retain
their P8-FT-9 semantics and safe ordering.

P8-FT-11 depends only on P8-FT-9 terminal-response retention and the P8-FT-10
private model. Acceptance requires initial and replacement emptiness, exact
current-response projection for success, empty, contained-error, and partial
responses, atomic row replacement, unchanged retained response-level state,
stale/cancelled completion suppression, and absence of controller, service,
configuration, or persistence effects beyond completed contracts. Ownership
remains private to Widgets; Core continues to own ordering, bounds, errors,
partial-response meaning, and index behavior.

No list, table, or tree widget; columns or additional roles; counts; selection;
empty/error/partial presentation; article activation/navigation or lookup
handoff; highlighting/WebEngine behavior; Preferences enablement/index policy;
index readiness/visibility/status/background lifecycle; public API;
persistence; adapter; `.gdfts`; legacy `_FTS`; dependency; or unrelated work
is included. Decisive migrated evidence is
`apps/goldendict/src/full_text_search_dialog.h/.cpp`,
`apps/goldendict/src/full_text_response_model.h/.cpp`, and their focused tests;
pinned legacy evidence is `fulltextsearch.hh:135-156`,
`fulltextsearch.cc:518-610,685-750`, and `fulltextsearch.ui:99-238`.
No successor after P8-FT-11 is selected or ranked.

P8-FT-11 is complete. `FullTextSearchDialog` owns one private child response
model, clears its retained response and projected rows on replacement
submission, and updates both only for a generation-current accepted
completion. The retained response continues to own complete error and partial
state while the model owns its independent ordered result snapshot. No visible
result view or successor is selected or ranked.

### Phase 8 full-text visible result-list attachment (complete)

The fresh documentation-only post-P8-FT-11 readiness audit is pinned to clean
migrated revision `649e5001712a50c719b681fba11565f2bcef4c71` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking: visible result presentation
and decoration; selection; counts and empty/error/partial states;
activation/navigation/lookup; highlighting and WebEngine handoff; Preferences
enablement/index policy; and index readiness, visibility, status, and background
lifecycle.

The audit selects exactly one smallest dependency-ready and independently
decision-complete leaf, P8-FT-12: private attachment of one visible `QListView`
to the completed dialog-owned `FullTextResponseModel`. The existing model's
ordered UTF-8 display rows become visible without changing their data or
ownership. Replacement submission immediately presents zero rows through the
P8-FT-11 reset; only a generation-current completion presents its synchronized
rows. Empty and error-only responses remain zero-row lists, while successful
and partial responses show every projected result in Core order, including
duplicate headwords.

P8-FT-12 depends only on P8-FT-10 projection and P8-FT-11 synchronization.
Widgets owns the list and model attachment; Core remains authoritative for
ordering, bounds, metadata, errors, partial-state meaning, and index behavior.
Acceptance requires exactly one dialog-owned visible result list using the
existing child model; initial, replacement-pending, empty, and error-only
zero-row behavior; exact ordered UTF-8 rows for current successful and partial
responses; atomic repeated replacement; stale/cancelled suppression; and no
new controller, service, configuration, or persistence effect.

Result decoration such as dictionary tooltips, columns, additional roles, and
richer delegates remains separate. Initial/current selection, keyboard focus,
and selection retention remain separate. Counts and empty/error/partial
presentation remain separate. Click, double-click, Return/Enter activation,
article lookup construction, dictionary scoping, and MainWindow navigation
remain separate. Highlighting remains downstream of activation and a reviewed
WebEngine handoff. Preferences enablement, format exclusions, size policy, and
persistence remain behind a product-policy decision; index readiness,
visibility, status, progress, and background lifecycle remain behind a Core
lifecycle contract. Public APIs/DTOs, adapters, `.gdfts`, legacy `_FTS`,
dependencies, build-system changes, and unrelated behavior are excluded.

Decisive migrated evidence is
`apps/goldendict/src/full_text_search_dialog.h/.cpp`,
`apps/goldendict/src/full_text_response_model.h/.cpp`, their focused tests, and
the completed P8-FT-10/P8-FT-11 contracts. Pinned legacy evidence is
`fulltextsearch.hh:135-156`, `fulltextsearch.cc:518-610,685-750`, and
`fulltextsearch.ui:99-238`: it establishes a `QListView` while leaving
activation and status behavior separable.
No successor after P8-FT-12 is selected or ranked.

P8-FT-12 is complete. `FullTextSearchDialog` owns one private visible
`QListView` attached directly to its existing child `FullTextResponseModel`.
The view exposes the model's exact ordered UTF-8 display rows, including
duplicates, while initial, replacement-pending, empty, and error-only states
remain zero-row lists. Existing generation checks and atomic model resets keep
stale and cancelled completions invisible. Widgets owns only the view/model
attachment; Core remains authoritative for result semantics and index behavior.
No successor is selected or ranked.

### Phase 8 full-text result activation intent (complete)

The documentation-only post-P8-FT-12 readiness audit is pinned to clean
migrated revision `32b1fba41ee4b7b8e145acf41256e7c393b2764e` and the unchanged
clean read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It selects exactly one smallest dependency-ready and independently
decision-complete leaf, P8-FT-13: a private Widgets-owned activation-intent
boundary for the visible result list. A valid row activates exactly once on a
single primary-button click or Return/Enter and yields the corresponding
existing `FullTextResult`; invalid, empty, stale, or reset indexes yield no
intent. A double click follows the single-click contract and must not cause a
second activation.

P8-FT-13 depends only on completed P8-FT-10 result projection and P8-FT-12
list attachment. `FullTextSearchDialog` owns event interpretation and uses
`FullTextResponseModel::ResultAt()` rather than reconstructing metadata from
display text. Core continues to own result identity and metadata; no public
API or DTO changes. The dialog exposes only a private application-facing
activation callback/signal carrying a safe value snapshot so model reset,
replacement, cancellation, service replacement, or teardown cannot invalidate
the delivered intent.

MainWindow article lookup, exact dictionary scoping, tab/history/navigation
mutation, highlighting, and WebEngine handoff remain a separate downstream
surface because current tab navigation does not preserve a per-result
dictionary filter. Initial/current selection, focus acquisition, and retention
remain separate: Return/Enter activates only an already-current valid index.
Dictionary decoration and extra roles remain separate. Counts and
empty/error/partial presentation remain separate. Preferences enablement/index
policy and index readiness/visibility/status/background lifecycle remain
separate policy/lifecycle surfaces. Persistence, adapters, `.gdfts`, legacy
`_FTS`, dependencies, build-system changes, and unrelated behavior are
excluded.

Evidence is migrated `full_text_search_dialog.h/.cpp`,
`full_text_response_model.h/.cpp`, and their focused tests, plus pinned legacy
`fulltextsearch.cc:292-293,594-610,664-673` and
`fulltextsearch.hh:227,232-233`. No successor after P8-FT-13 is selected or
ranked.

P8-FT-13 is complete. The dialog emits one private by-value
`FullTextResult` activation intent for a valid primary single click or
current-row Return/Enter. The result is resolved through `ResultAt()` without
reinterpreting Core metadata; invalid, reset, stale, and cancelled rows emit
nothing, and double-click adds no duplicate. No production consumer is
connected, so lookup construction, dictionary scoping, and navigation remain
unchanged. No successor is selected or ranked.

### Phase 8 full-text scoped navigation prerequisite (complete)

The independent post-P8-FT-13 documentation audit was pinned to clean migrated
revision `6ac1912965a60f6d8c9b5614752fa97f3426d80f` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It selected exactly one smallest dependency-ready leaf, P8-FT-14: extend the
Core-owned article navigation contract so a lookup navigation can retain an
authoritative ordered dictionary scope. This prerequisite is necessary before
the existing activation intent can produce useful MainWindow behavior without
silently widening the lookup during history replay, tab restore, or a
dictionary-participation refresh.

`TabNavigationState` owns the optional scope because Core already owns tab
history, validation, and session round trips. The scope consists of an explicit
active flag plus the ordered dictionary IDs from the submitted full-text query;
an active empty scope remains authoritative and yields no dictionary lookup.
MainWindow copies that state unchanged into `LookupQuery`, while ordinary
lookups continue to derive participation from the current dictionary bar when
the navigation has no retained scope. Validation applies the existing bounded
string/vector policy and rejects an invalid scoped navigation atomically.
The scope accepts at most `kMaximumLookupDictionaryFilters` IDs, and every ID
must be nonempty valid UTF-8 without NUL and no longer than
`kMaximumLookupFilterBytes`. Session persistence appends the active flag,
dictionary count, and encoded IDs to each `article_tab_navigation` record;
the loader accepts both the existing ten-field record as unscoped and the new
count-checked form.

The result headword, not `document_id`, remains the future lookup query. The
approved legacy-parity target is the complete dictionary scope that produced
the full-text response, not only `result.dictionary.id`. P8-FT-14 does not yet
connect `ResultActivationRequested`, choose a tab disposition, mutate the
query edit, record history, select an exact article/document, or hand off
highlighting. Those remain separately decomposed downstream surfaces.
Selection/focus/retention; dictionary decoration; counts and response-state
presentation; Preferences policy; index readiness/status/background lifecycle;
adapters, index formats including legacy `_FTS`, dependencies, build-system
work, and unrelated behavior are also excluded.

Evidence is migrated `main_window.cpp` (`StartLookupInTab`,
`StartNavigationLookup`, and `ApplyDictionaryFilter`), Core
`desktop_facade.h` and its tab/session tests, the P8-FT-7 dictionary projection
contract, and pinned legacy `fulltextsearch.cc:594-610` plus
`mainwindow.cc:3002-3014`, which passes the clicked headword and its complete
matching dictionary-ID list into article lookup. P8-FT-14 is complete. Its
only public contract change is the additive `dictionary_filter_active` and
`dictionary_ids` fields on `TabNavigationState`; the C ABI is unchanged. Core
validates and round-trips the presence distinction and ordered IDs, while
MainWindow copies an explicit scope unchanged into `LookupQuery` and retains
ordinary dictionary-bar projection for unscoped navigation.
No successor after P8-FT-14 is selected or ranked.

### Phase 8 full-text accepted-response activation context (complete)

The independent post-P8-FT-14 documentation audit is pinned to clean migrated
revision `e3f2ac70814ea8166af747ffee2e3718b5323ac6` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It rechecks every remaining full-text workflow surface without advance ranking
and selects exactly one smallest dependency-ready leaf, P8-FT-15: retain the
private dictionary-scope context of the generation-current accepted response
and deliver it with result activation intent.

A direct P8-FT-13-to-MainWindow connection is not yet dependency-ready.
`ResultActivationRequested` carries only `FullTextResult`, while
`ProjectedQuery()` is mutable independently of the retained response and its
rows. Reopening the modeless dialog after group or dictionary participation
changes can therefore replace the projected scope without replacing the
visible response. Reading that state during activation could silently widen,
narrow, or otherwise misidentify the scope that produced the result.

P8-FT-15 now snapshots `dictionary_filter_active` and the ordered
`dictionary_ids` when a search is submitted. Only its generation-current
accepted completion associates that snapshot with the retained response and
model rows. Replacement submission clears response, rows, and activation
context together; stale or cancelled completion, service replacement,
controller detachment, and teardown cannot restore or emit old context. A
valid activation continues to carry the exact by-value `FullTextResult` and
additionally carries the associated immutable submitted scope. An active empty
scope remains authoritative. The boundary is private to Widgets and adds no
public Core API or DTO.

The downstream activation-to-navigation contract is fixed but not selected.
It uses the activated result's exact headword, not the full-text query,
excerpt, `document_id`, normalized match text, or display reconstruction. It
opens and activates the current article tab, leaves the main query edit
unchanged, and constructs a `kLookup` `TabNavigationState` whose query and
title are the headword, whose group is MainWindow's active group at activation,
and whose dictionary scope is copied unchanged from the accepted response.
After successful `OpenArticleTab`, MainWindow follows its existing convention:
sync tabs, emit `ArticleTabSessionMutated`, and call
`StartNavigationLookup(..., true)` so tab history, session identity, ordinary
lookup-history emission, replay, and restoration use that same navigation.
Authoritative-empty is a successful scoped navigation with no dictionary work
and never falls back to current dictionary-bar participation. Missing facade
or activation context, invalid activation or navigation, and tab-limit failure
start no lookup and cause no history/session mutation; existing MainWindow
failure status behavior remains authoritative.

Exact `document_id` selection, source-dictionary targeting, match/excerpt
metadata, highlighting, ignore-diacritics handoff, and WebEngine behavior remain
deferred. Selection/focus/retention; result decoration; counts and
empty/error/partial presentation; the activation-to-MainWindow connection;
Preferences/index policy; index readiness, visibility, status, progress, and
background lifecycle; adapters and index formats including legacy `_FTS`;
persistence beyond the existing navigation state; dependencies, build-system
work, and unrelated parity are separate surfaces.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
P8-FT-7 dictionary projection, P8-FT-13 activation intent, P8-FT-14 scoped
navigation, and `main_window.cpp` current-tab lookup/history conventions.
Pinned legacy `fulltextsearch.cc:594-610` and `mainwindow.cc:3002-3014` pass
the clicked headword and complete matching dictionary-ID list to the current
article view without synchronizing the main query edit.
No successor after P8-FT-15 is selected or ranked.

### Phase 8 full-text scoped result navigation connection (complete)

The independent post-P8-FT-15 documentation audit is pinned to clean migrated
revision `bb7298ab1fd04c302fb74d2903d3fa92a8c63bc6` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It rechecks every remaining full-text workflow surface without advance ranking
and selects exactly one smallest dependency-ready leaf, P8-FT-16: connect the
accepted full-text result activation intent to MainWindow's existing scoped
current-tab navigation path.

P8-FT-13 supplies exact by-value result activation, P8-FT-14 preserves an
optional authoritative dictionary scope through navigation history, session
state, restoration, replay, and participation refresh, and P8-FT-15 associates
the immutable submitted scope only with its generation-current accepted
response. The earlier scope-identity gap is therefore closed. MainWindow owns
the private presentation-event connection, navigation construction, current-
tab mutation, existing failure status, and lookup handoff; Core continues to
own navigation validation, bounded history/session mutation, and scoped replay.
No public API, persistence contract, dependency, adapter, or index-format
change is required.

P8-FT-16 uses the activated result's exact headword as both navigation query
and title. It targets and activates the current article tab, never opens a new
or background tab, leaves the main query edit unchanged, records MainWindow's
active group at activation, and copies the accepted response's dictionary
scope unchanged, including authoritative empty. After successful
`OpenArticleTab`, MainWindow follows its established order: synchronize tabs,
emit `ArticleTabSessionMutated`, and call `StartNavigationLookup(..., true)`.
That single navigation identity drives Core tab history, session restoration
and replay, ordinary lookup-history emission, and the lookup request.

Missing facade, dialog, or accepted activation context; invalid activation or
navigation; and tab-limit failure start no lookup and cause no lookup-history
or session mutation. Existing MainWindow failure-status behavior remains
authoritative. Focused acceptance covers exact headword/query/title, current-
tab activation without tab creation, unchanged main query text, activation-
time group capture, ordered nonempty and authoritative-empty scope, successful
mutation/lookup sequencing, history/session identity and replay without scope
widening, and every failure no-op path.

Exact `document_id` selection, source-dictionary targeting, match/excerpt
metadata, highlighting, ignore-diacritics handoff, and WebEngine behavior remain
deferred. Initial/current selection, focus, and retention; result decoration;
counts and empty/error/partial presentation; Preferences enablement and index
policy; index readiness, visibility, status, progress, and background
lifecycle; adapters and index formats including legacy `_FTS`; persistence
beyond existing navigation; dependencies and build-system work; and unrelated
parity remain separate surfaces. These surfaces are decomposed only; none is
selected or ranked.

Evidence is migrated `full_text_search_dialog.h/.cpp` and focused tests,
P8-FT-7 dictionary projection, P8-FT-13 activation intent, P8-FT-14 scoped
navigation and Core tab/session coverage, P8-FT-15 accepted-response context,
and `main_window.cpp` current-tab `OpenArticleTab`, synchronization, session-
mutation, failure-status, and `StartNavigationLookup` conventions. Pinned
legacy `fulltextsearch.cc:594-610` and `mainwindow.cc:3002-3014` pass the exact
clicked headword and complete matching dictionary-ID list to the current
article view without synchronizing the main query edit.
P8-FT-16 is complete. MainWindow owns one lifetime-safe private connection per
dialog instance and translates accepted activation intent through the existing
scoped current-tab navigation sequence. Focused application smoke coverage
pins successful identity and sequencing, replay, dialog replacement, and
failure no-op behavior without adding a public surface or test executable.
No successor after P8-FT-16 is selected or ranked.

### Phase 8 full-text accepted-result count presentation (complete)

The independent post-P8-FT-16 documentation audit is pinned to clean migrated
revision `b7cfd864b85df4a7ee36d4e08e36287c4fabfd7b` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It rechecks every remaining full-text workflow surface without advance ranking
and selects exactly one smallest dependency-ready leaf, P8-FT-17: present the
count of results retained by the generation-current accepted response.

P8-FT-17 is dependency-ready because P8-FT-11 retains the complete accepted
response, P8-FT-12 atomically attaches its ordered results to the visible list,
and replacement submission already clears the response and model together.
The dialog owns the private count label and derives it from the accepted model
row count after reset; Core continues to own result ordering, duplicates,
errors, and `partial` meaning. No public API, DTO, persistence, dependency,
adapter, index-format, or build-system change is required.

The label reads `Articles found: N`. It starts at zero and returns to zero when
a replacement search is submitted. A generation-current accepted success or
partial response shows the complete number of retained projected rows,
including duplicates. Accepted empty and error-only responses show zero. A
partial response's number describes retained results only and does not claim
that the search was complete. Stale or cancelled completions, service
replacement, controller detachment, and teardown cannot overwrite the current
count.

Focused acceptance covers initial zero, replacement reset, nonempty success,
duplicate rows, empty and contained-error responses, partial responses with
and without retained rows, repeated accepted responses, and stale/cancelled or
detached completion safety. The existing private dialog test and application
smoke are the focused implementation gate; the established Linux
Release build, full tests, package, install, and installed-consumer workflow
remains the full gate.

Exact `document_id` lookup and source-dictionary targeting; initial/current
selection, keyboard focus, and retention; dictionary/result decoration,
tooltips, and metadata; empty/error/partial messaging beyond the numeric
retained-result count; match/excerpt presentation; highlighting,
ignore-diacritics transfer, and WebEngine handoff; Preferences enablement,
format exclusions, size/index policy, and persistence; index readiness,
visibility, status, progress, background lifecycle, rebuild, and failure UI;
adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies, builds, and
unrelated parity remain separate surfaces. They are decomposed only; none is
selected or ranked.

Evidence is migrated `full_text_search_dialog.cpp`,
`full_text_response_model.cpp`, and their focused tests, plus pinned legacy
`fulltextsearch.cc:290,448-449,570-571` and
`fulltextsearch.ui:99-129`, which initialize and update the visible
`articlesFoundLabel` from retained result count.
P8-FT-17 is complete through the private dialog-owned label and the existing
accepted-response/model synchronization contract. It adds no test executable
or public/installed interface, and the registered Release baseline remains 109
tests.
No successor after P8-FT-17 is selected or ranked.

### Phase 8 full-text result dictionary-name tooltip (complete)

The independent post-P8-FT-17 documentation audit is pinned to clean migrated
revision `7c8fc16e55844b2712f3257a78b7f6b8e6cc3b5b` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It rechecks every remaining full-text workflow surface without advance ranking
and selects exactly one smallest dependency-ready leaf, P8-FT-18: project the
result dictionary name as the visible row's tooltip.

P8-FT-18 is dependency-ready because P8-FT-10 gives the private response model
an immutable snapshot of every complete `FullTextResult`, P8-FT-11 synchronizes
that model only with the generation-current accepted response, and P8-FT-12
attaches it directly to the visible result list. The private Widgets model owns
the tooltip projection. Core continues to own dictionary identity and result
semantics. No public API, DTO, persistence, dependency, adapter, index-format,
or build-system change is required.

For a valid result row, `Qt::ToolTipRole` returns the exact UTF-8
`FullTextResult::dictionary.name` decoded for Qt display. Duplicate headwords
from different result dictionaries retain independent row tooltips. An empty
dictionary name produces no visible tooltip; the model does not substitute the
dictionary ID, source, edition, or other metadata. Invalid, foreign,
out-of-range, or nonzero-column indexes and unsupported roles return no value.
The existing display text, ordering, duplicates, metadata snapshot, activation,
accepted-generation synchronization, and retained-result count are unchanged.

Focused acceptance covers exact Unicode dictionary names, distinct tooltip
values for duplicate-headword rows from different dictionaries, empty-name
suppression, copied and moved response lifetime, deterministic reset
replacement, invalid and foreign indexes, unsupported roles, and unchanged
display and result metadata. The existing private response-model test is the
focused implementation gate; the established Linux Release build, full tests,
package, install, and installed-consumer workflow is the full gate.

Exact `document_id` navigation and source-dictionary targeting; initial/current
selection, keyboard focus, and selection retention; non-tooltip decoration,
columns, delegates, icons, and additional metadata roles; empty/error/partial
messaging beyond the numeric retained-result count; match ranges and excerpt
presentation; highlighting, ignore-diacritics transfer, and WebEngine handoff;
Preferences enablement, format exclusions, size/index policy, and persistence;
index readiness, visibility, status, progress, background lifecycle, rebuild,
and failure UI; adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies,
builds, and unrelated parity remain independent surfaces. They are decomposed
only; none is selected or ranked.

Evidence is migrated `full_text_response_model.h/.cpp`, its focused tests, and
the P8-FT-11/P8-FT-12 dialog synchronization and attachment, plus pinned legacy
`fulltextsearch.cc:690-721`, where `HeadwordsListModel::data()` independently
projects contributing dictionary names through `Qt::ToolTipRole`.
P8-FT-18 is complete through the private response-model tooltip projection and
focused tests. It adds no test executable or public/installed interface, and
the registered Release baseline remains 109 tests.
No successor after P8-FT-18 is selected or ranked.

### Phase 8 full-text result edit-role projection (complete)

The independent post-P8-FT-18 documentation audit is pinned to clean migrated
revision `d7d2f76a397f5adf0a546ef3885216b35f82753c` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It rechecks every remaining full-text workflow surface without advance ranking
and selects exactly one smallest dependency-ready leaf, P8-FT-19: project the
result headword through the private response model's edit role.

P8-FT-19 is dependency-ready because P8-FT-10 gives the private response model
an immutable snapshot of every complete `FullTextResult`, P8-FT-11 synchronizes
that model only with the generation-current accepted response, and P8-FT-12
attaches it directly to the visible result list. The private Widgets model owns
the role projection. Core continues to own result identity and semantics. No
public API, DTO, persistence, dependency, adapter, index-format, or build-system
change is required.

For a valid result row, `Qt::EditRole` returns the exact UTF-8
`FullTextResult::headword` decoded identically to `Qt::DisplayRole`. Duplicate
rows retain their independent headwords. Invalid, foreign, out-of-range, or
nonzero-column indexes and unsupported roles return no value. The existing
display and tooltip values, ordering, duplicates, metadata snapshot,
activation, accepted-generation synchronization, retained-result count,
selection, focus, and retention behavior are unchanged.

Focused acceptance covers exact Unicode edit-role headwords, duplicate
rows, equality with the display role, copied and moved response lifetime,
deterministic reset replacement, invalid and foreign indexes, unsupported
roles, and unchanged tooltip and result metadata. The existing private
response-model test is the focused implementation gate, with
`ctest --preset conan-release -R '^full_text_response_model_test$'` after the
Release target built. The established Linux Release build, full tests, package,
install, and installed-consumer workflow is the full gate.

Exact `document_id` navigation and source-dictionary targeting; initial/current
selection, keyboard focus, and selection retention; non-edit-role decoration,
columns, delegates, icons, and additional metadata roles; empty/error/partial
messaging beyond the numeric retained-result count; match ranges and excerpt
presentation; highlighting, ignore-diacritics transfer, and WebEngine handoff;
Preferences enablement, format exclusions, size/index policy, and persistence;
index readiness, visibility, status, progress, background lifecycle, rebuild,
and failure UI; adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies,
builds, and unrelated parity remain independent surfaces. They are decomposed
only; none is selected or ranked.

Evidence is migrated `full_text_response_model.h/.cpp`, its focused tests, and
the P8-FT-10/P8-FT-11/P8-FT-12 model ownership, synchronization, and attachment,
plus pinned legacy `fulltextsearch.cc:690-721`, where
`HeadwordsListModel::data()` returns the exact headword for both
`Qt::DisplayRole` and `Qt::EditRole`.
P8-FT-19 is complete through the private response-model edit-role projection
and focused tests. It adds no test executable or public/installed interface,
and the registered Release baseline remains 109 tests.
No successor after P8-FT-19 is selected or ranked.

### Phase 8 full-text result-list selection contract (complete)

The independent post-P8-FT-19 documentation audit is pinned to clean migrated
revision `a60f258e9226ebc7e1ee2115055d2ee531dc097a` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It rechecks every remaining full-text workflow surface without advance ranking
and selects exactly one smallest dependency-ready leaf, P8-FT-20: make the
private result list's current index, single-row selection, and reset focus
behavior explicit and deterministic.

P8-FT-20 is complete. P8-FT-10 owns the immutable response
snapshot, P8-FT-11 synchronizes only accepted responses, P8-FT-12 attaches the
model to one visible `QListView`, and P8-FT-13 requires a valid current row for
activation. The private Widgets dialog owns its list and selection model. The
response model continues to own only ordered result data, and Core continues to
own result identity and navigation semantics. No public API, DTO, persistence,
dependency, adapter, index-format, or build-system change is required.

The list has at most one current and selected row. Initial, empty, and
error-only states have neither. A generation-current successful or partial
response does not select a row or steal keyboard focus. Ordinary user
interaction may establish one current and selected row. Starting a replacement
clears rows, current index, and selection atomically; the accepted replacement
remains unselected even when it contains the same headword or row position.
Stale or cancelled completions cannot restore selection, current index, or
focus. Reset preserves whether the list or another widget already owns keyboard
focus. Existing click, Return, and Enter activation remains unchanged and still
requires a valid current row.

Focused acceptance extends the private dialog test for initial,
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
formats, dependencies, builds, and unrelated parity remain independent
surfaces. They are decomposed only; none is selected or ranked. No public API,
DTO, persistence, Core, adapter, index, dependency, or build surface belongs to
P8-FT-20.

Evidence is migrated `full_text_search_dialog.h/.cpp`,
`full_text_response_model.h/.cpp`, their focused tests, and completed
P8-FT-10 through P8-FT-13, plus pinned legacy
`fulltextsearch.cc:287-315,448-449,567-579,662-673` and
`fulltextsearch.ui:99`. The legacy dialog attaches one list model and delegate,
clears the model before replacement, adds accepted rows without programmatic
selection or focus transfer, and activates only a clicked or current valid row.
No successor after P8-FT-20 is selected or ranked.

### Phase 8 full-text bidirectional result rendering (complete)

The independent post-P8-FT-20 documentation audit is pinned to clean migrated
revision `53281651f9f882cfb9364a55a908f7104d760456` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It rechecks every remaining full-text workflow surface without advance ranking
and selected exactly one smallest dependency-ready leaf, P8-FT-21: give the
private result list legacy-compatible per-row bidirectional painting and
elision. P8-FT-21 is complete.

P8-FT-21 is dependency-ready because P8-FT-12 owns one visible `QListView`,
P8-FT-18 and P8-FT-19 expose the exact dictionary name and headword roles, and
P8-FT-20 fixes selection, current-index, reset, and focus behavior. A private
Widgets delegate owns only presentation of the existing display text. The
response model continues to own ordered result data, and Core continues to own
result identity and navigation semantics. No public API, DTO, persistence,
dependency, adapter, index-format, or build-system change is required.

For each painted result row, the delegate derives direction independently from
the exact displayed headword. Right-to-left text paints with
`Qt::RightToLeft`; when elision is enabled it elides at the left with
`Qt::ElideLeft`. All other text paints with `Qt::LeftToRight`; when elision is
enabled it elides at the right with `Qt::ElideRight`. An existing
`Qt::ElideNone` setting remains unchanged. Mixed Unicode text follows Qt's
per-string direction result. The delegate changes no model role or value, row
order, tooltip, retained-result count, selection, focus, activation, response
ownership, or accepted-generation synchronization.

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
formats, dependencies, builds, and unrelated parity remain independent
surfaces. They are decomposed only; none is selected or ranked. No public API,
DTO, persistence, Core, adapter, index, dependency, or build surface belongs to
P8-FT-21.

Evidence is migrated `full_text_search_dialog.h/.cpp`,
`full_text_response_model.h/.cpp`, their focused tests, and completed
P8-FT-12/P8-FT-18/P8-FT-19/P8-FT-20. Pinned legacy evidence is
`fulltextsearch.cc:287-315`, `delegate.hh`, `delegate.cc:5-31`, and
`fulltextsearch.ui:99`: the dialog installs a private word-list delegate whose
paint path derives direction per displayed string, chooses left or right
elision accordingly, and preserves an explicit no-elision setting.
No successor after P8-FT-21 is selected or ranked.

### Phase 8 full-text partial-response status (complete)

The independent post-P8-FT-21 documentation audit is pinned to clean migrated
revision `b47e96630f2b4f9bb702442b9f563dc0719eec04` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It rechecks every remaining full-text workflow surface without advance ranking
and selects exactly one smallest dependency-ready leaf, P8-FT-22: present the
generation-current accepted response's authoritative partial state.

P8-FT-22 is dependency-ready because P8-FT-9 accepts only a
generation-current terminal response, P8-FT-11 retains that complete response,
and P8-FT-17 presents its retained-result count. One private Widgets-owned
status label consumes only the existing transport-neutral
`FullTextResponse::partial` fact. Core continues to own response semantics,
errors, results, and ordering. No public API, DTO, persistence, dependency,
adapter, index-format, or build-system change is required.

The status reads `Results may be incomplete.` exactly when the accepted
response has `partial == true`. Initial state and replacement submission hide
it. An accepted complete response keeps it hidden even when it has no results
or contains errors. An accepted partial response shows it with zero or nonzero
retained rows and with or without errors. Widgets does not infer partiality from
the result count, error collection, cancellation, or individual error codes and
does not expose dictionary IDs, backend messages, or error details. Stale or
cancelled completions, controller detachment, service replacement, and teardown
cannot introduce or overwrite the current status.

Focused acceptance covers the initial and replacement-reset states;
complete and partial responses with zero and nonzero retained rows; partial
responses with and without errors; complete responses containing errors;
repeated accepted responses; and stale, cancelled, detached, replaced-service,
and teardown completion safety. Existing result projection, count, selection,
focus, activation, response ownership, and accepted-generation coverage remains
green. The focused command is
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
builds, and unrelated parity remain independent surfaces. They are decomposed
only; none is selected or ranked. No public API, DTO, persistence, Core,
adapter, index, dependency, or build surface belongs to P8-FT-22.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, and completed P8-FT-9/P8-FT-11/P8-FT-17.
Pinned legacy `fulltextsearch.cc:448-449,499-586` silently contains individual
dictionary failures while updating retained rows and count; it supplies no
safe structured error-detail presentation contract. P8-FT-22 therefore exposes
only the migrated response's bounded partial fact. P8-FT-22 is complete. No
successor after P8-FT-22 is selected or ranked.

### Phase 8 full-text empty-result status (complete)

The independent documentation-only post-P8-FT-22 readiness audit is pinned to
clean migrated revision `8a79669095166821e6361f24bf02a27d8bb6a2fb` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-23: present the unambiguous empty result
of a generation-current accepted response.

P8-FT-23 is dependency-ready because P8-FT-9 accepts only the current terminal
generation, P8-FT-11 retains the complete response, P8-FT-17 presents its
retained-result count, and P8-FT-22 independently presents authoritative
partiality. One private Widgets-owned status consumes only the existing
response's result count, `partial` flag, and error collection. Core remains
authoritative for all three facts. No public API, DTO, persistence, dependency,
adapter, index-format, or build-system change is required.

The status reads `No matches` exactly when a generation-current accepted
response has zero retained results, `partial == false`, and no errors. Initial
state and replacement submission hide it. Nonempty, partial, and
error-containing responses hide it, so Widgets neither claims a conclusive
empty search after incomplete work nor preempts a later error-presentation
contract. The existing partial status remains independent. Stale or cancelled
completions, controller detachment, service replacement, and teardown cannot
introduce or overwrite the current status. Result rows, count, selection,
focus, activation, response ownership, and accepted-generation synchronization
remain unchanged.

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
unrelated parity remain independent surfaces. They are decomposed only; none is
selected or ranked. No public API, DTO, persistence, Core, adapter, index,
dependency, or build surface belongs to P8-FT-23.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, and completed
P8-FT-9/P8-FT-11/P8-FT-17/P8-FT-22. Pinned legacy
`fulltextsearch.cc:448-449,499-586` clears retained rows before a replacement,
silently contains individual dictionary failures, and leaves the accepted
zero count as its only empty-result presentation. The migrated application
already uses exact text `No matches` for a conclusive zero-match article search
at `main_window.cpp:5122-5136,7706-7723`. P8-FT-23 is complete. No successor
after P8-FT-23 is selected or ranked.

### Phase 8 full-text terminal failure status (complete)

The independent documentation-only post-P8-FT-23 readiness audit is pinned to
clean migrated revision `3b67ce9413cba3555115779ddd48d70e927a7fd4` and the
unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-24: present an unambiguous terminal
failure for a generation-current accepted response.

P8-FT-24 is dependency-ready because P8-FT-9 accepts only the current terminal
generation, P8-FT-11 retains the complete response, P8-FT-17 presents its
retained-result count, P8-FT-22 presents authoritative partiality, and
P8-FT-23 distinguishes a conclusive empty response. One private Widgets-owned
status consumes only the existing response's result count, `partial` flag, and
error collection. Core remains authoritative for all three facts. No public
API, DTO, persistence, dependency, adapter, index-format, or build-system
change is required.

The status reads `Full-text search failed` exactly when a generation-current
accepted response has zero retained results, `partial == false`, and one or
more errors. Initial state and replacement submission hide it. Conclusive
empty, nonempty, and partial responses hide it, so Widgets neither labels
retained or incomplete work as a terminal failure nor exposes dictionary IDs,
error codes, backend messages, or raw details. The result count, partial
status, and empty status remain independent. Stale or cancelled completions,
controller detachment, service replacement, and teardown cannot introduce or
overwrite the current status. Result rows, selection, focus, activation,
response ownership, and accepted-generation synchronization remain unchanged.

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
builds, and unrelated parity remain independent surfaces. They are decomposed
only; none is selected or ranked. No public API, DTO, persistence, Core,
adapter, index, dependency, or build surface belongs to P8-FT-24.

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
teardown paths. The existing private dialog test is the focused implementation
gate. The established Linux Release build, full test, package, install, and
installed-consumer workflow remains the full gate; the leaf adds no test
executable or installed surface, so the registered Release baseline remains
109 tests.

Ignore-diacritics semantics and legacy regular-expression equivalence; match-
range or excerpt rendering; exact `document_id` navigation and source-
dictionary targeting; columns, icons, additional metadata roles, and other
decoration; Preferences enablement, format exclusions, size/index policy, and
persistence; index readiness, visibility, status, progress, background
lifecycle, rebuild, and failure UI; adapters, `.gdfts`, legacy `_FTS`, index
formats, dependencies, builds, and unrelated parity remain independent
surfaces. They are decomposed only; none is selected or ranked.

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
safety. The existing private dialog test and MainWindow full-text smoke are the
focused implementation gate. The established Linux Release build, full test,
package, install, and installed-consumer workflow remains the full gate; the
leaf adds no test executable or installed surface, so the registered Release
baseline remains 109 tests.

Ignore-diacritics semantics and legacy regular-expression equivalence; match-
range or excerpt rendering; exact `document_id` navigation and source-
dictionary targeting; columns, icons, additional metadata roles, and other
decoration; Preferences enablement, format exclusions, size/index policy, and
persistence; index readiness, visibility, status, progress, background
lifecycle, rebuild, and failure UI; adapters, `.gdfts`, legacy `_FTS`, index
formats, dependencies, builds, and unrelated parity remain independent
surfaces. They are decomposed only; none is selected or ranked.

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
smallest dependency-ready leaf, P8-FT-32: persist one bounded opaque full-text
dialog geometry value without yet connecting it to Widgets.

P8-FT-32 is dependency-ready because the current `CoreConfiguration` already
persists independently optional opaque main-window geometry, validates its
decoded size at 64 KiB, and imports the pinned legacy geometry atomically.
Pinned legacy `config.hh:156-181`, `config.cc:1008-1044,1990-2027`, and
`fulltextsearch.cc:195-221,387-399` establish that full-text dialog geometry is
another opaque Qt byte sequence stored under the full-text preferences. The
transport-neutral configuration boundary can retain those bytes without
interpreting Qt geometry or adding a dependency.

The completed implementation adds one independently optional full-text dialog
geometry value to `CoreConfiguration`. Missing current or legacy data yields
an empty value. Canonical current saves use the existing binary-value encoding
pattern. Legacy migration maps exactly the recognized
`preferences/fullTextSearch/dialogGeometry` value. Decoded data is bounded at
64 KiB; duplicate, malformed, or oversized recognized input rejects the
complete load or migration atomically. Core validates and persists the opaque
bytes but never calls Qt geometry APIs.

The `CoreConfiguration` field is an authorized installed/public ABI expansion
identified by Conan's exact SCM and package revisions. It adds no runtime
interface, dependency, adapter, index-format, or build-system surface.

P8-FT-32 is complete and does not capture, restore, apply, or save geometry in
Widgets. Dialog creation, initialization, idle dismissal, window-manager close,
active-request cancellation, service replacement, teardown, requests, results,
selection, activation, navigation, article search, and notification remain
unchanged.
The later Widgets connection may consume the persisted value only through a
separately reviewed leaf; it is decomposed but remains unselected and unranked.

Focused acceptance covers missing/default current and legacy data; current
round-trip and canonical save; valid legacy migration; duplicate, malformed,
and oversized current and legacy values; the exact 64 KiB boundary; atomic
failure without source modification or partial current output; and installed
C and C++ consumer access to the expanded configuration DTO. The focused
command is
`ctest --preset conan-release -R '^application_service_test$'` after the
Release target has been built. The full implementation gate is Linux Release
configure/build, full `ctest --preset conan-release` without an unintended
registration delta, clean exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers, Release install, and standalone
installed C and C++ consumers. P8-FT-32 adds no test executable or dependency,
so the registered Release baseline remains 109 tests.

Widgets geometry capture/restoration; ignore-diacritics semantics and legacy
regular-expression equivalence; match-range or excerpt rendering; exact
`document_id` navigation and source-dictionary targeting; columns, icons,
additional metadata roles, and other decoration; Preferences enablement,
format exclusions, size/index policy, and persistence; index readiness,
visibility, status, progress, background lifecycle, rebuild, and failure UI;
adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies, builds, and
unrelated parity remain independent surfaces. They are decomposed only; none
is selected or ranked. No successor after P8-FT-32 is selected or ranked.

### Phase 8 full-text dialog geometry Widgets connection (complete)

The independent documentation-only post-P8-FT-32 audit is pinned to clean
migrated revision `6ff78e84f2b5f0395283a00f0632174672828625` and unchanged
clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-33: connect P8-FT-32's bounded opaque
dialog geometry to the existing modeless Widgets dialog. P8-FT-33 is complete.

The prerequisite chain is complete: `CoreConfiguration` owns bounded atomic
persistence and exact legacy migration, `FullTextSearchDialog` owns the Qt
window, and the application composition root owns configuration saves. Pinned
legacy `fulltextsearch.cc:195-225,387-399` restores nonempty geometry at
construction and captures it when the dialog finishes. Under the shared-
library/GUI rule, Core retains uninterpreted bytes, Widgets alone calls Qt
geometry APIs, and composition alone joins captured bytes to configuration.

On new-dialog creation, Widgets now attempts one restore only for a nonempty
value. Absence preserves default geometry. Qt rejection also preserves the
default without rewriting the stored value or adding screen, topology,
placement, or fallback policy. Idle Cancel and window-manager close capture
the exact `saveGeometry()` bytes before destruction for the next established
atomic save. Active Cancel remains cancellation-only: it neither closes the
dialog nor captures or saves geometry. Completion, activation, replacement,
service replacement, controller detachment, and teardown do not independently
persist it.

P8-FT-33 changes no Core contract, installed header, public DTO, dependency,
adapter, index format, or build surface. Focused acceptance extends the
existing dialog test and full-text application smoke with absent, valid, and
Qt-invalid restore; exact capture on both idle dismissal paths; reconstruction
round-trip; active-cancellation exclusion; and regression coverage for the
existing workflow and lifecycle. The focused Release command is
`ctest --preset conan-release -R '^(full_text_search_dialog_test|goldendict_full_text_dialog_smoke)$'`
after the Release target has been built.

The full implementation gate is Linux Release configure/build, exactly 109
registered tests and full `ctest --preset conan-release`, clean committed
exact-SCM `conan create` with the Release Qt WebEngine host profile and
packaged consumers, Release install, and standalone installed C and C++
consumers. Those consumers remain source-compatible and unchanged because
P8-FT-33 adds no installed interface; they remain the stronger package gate
and continue to cover P8-FT-32's installed configuration field.

Screen/topology normalization, placement fallback, other dialog state, query
semantics, excerpts, exact document/source targeting, decoration,
Preferences/index policy, index lifecycle, adapters/index formats,
dependencies, builds, and unrelated parity remain independently decomposed,
unselected, and unranked. No successor after P8-FT-33 is selected or ranked.
The implementation remains bounded to the private Widgets/application
connection, its existing tests, and the four governing documents.

### Phase 8 full-text dialog minimum-size contract (complete)

The independent documentation-only post-P8-FT-33 audit is pinned to clean
migrated revision `eb4911bd8fd82402f0dc5b861da65aecb0927633` and unchanged
clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selects exactly one
smallest dependency-ready leaf, P8-FT-34: preserve the legacy full-text
dialog's private Widgets minimum size.

The prerequisite chain is complete: P8-FT-7 owns the modeless dialog and its
layout, and P8-FT-33 owns bounded geometry restoration without moving Qt
geometry policy into Core. Pinned legacy `fulltextsearch.ui:13-18` supplies the
exact independent contract: the dialog minimum is 430 logical pixels wide and
450 logical pixels high. Under the shared-library/GUI rule, Widgets alone owns
and enforces those bounds; Core, the composition root, and installed consumers
do not interpret them.

Every newly constructed full-text dialog must report `minimumWidth() == 430`
and `minimumHeight() == 450`. Direct resize and P8-FT-33 restoration cannot
leave either dimension below its minimum. A valid restored geometry already
at or above both bounds remains governed by P8-FT-33 without a new placement
or normalization policy. Absence or Qt rejection still preserves the default
layout, and geometry capture, idle dismissal, active cancellation, completion,
activation, replacement, service replacement, detachment, and teardown remain
unchanged.

P8-FT-34 changes no Core contract, installed header, public DTO, dependency,
adapter, index format, persistence, or build surface. Focused implementation
acceptance extends `full_text_search_dialog_test` with the exact minimum,
undersized direct-resize clamping, undersized restored-geometry clamping,
larger valid geometry preservation, and regression coverage for P8-FT-33
restoration and lifecycle behavior. The focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built.

The full implementation gate is Linux Release configure/build, exactly 109
registered tests and full `ctest --preset conan-release`, clean committed
exact-SCM `conan create` with the Release Qt WebEngine host profile and
packaged consumers, Release install, and standalone installed C and C++
consumers. Those consumers remain unchanged and source-compatible because
P8-FT-34 adds no installed interface; they remain the stronger package gate.

Default or initial size, maximum size, aspect ratio, screen/topology
normalization, placement fallback, DPI policy beyond Qt logical sizing, other
dialog state, query semantics, excerpts, exact document/source targeting,
decoration, Preferences/index policy, index lifecycle, adapters/index formats,
dependencies, builds, and unrelated parity remain independently decomposed,
unselected, and unranked. No successor after P8-FT-34 is selected or ranked.
The completed implementation remains bounded to the private Widgets dialog,
its existing focused test, and the four governing documents. The constructor
sets the exact minimum before the existing one-time geometry restore, leaving
Qt to enforce the private logical-size constraint without adding placement or
normalization policy.

### Phase 8 full-text dialog initial-size contract (complete)

The independent documentation-only post-P8-FT-34 audit was pinned to clean
migrated revision `7ff83942ab6b71dda1a0a798eb0dcbe8ef1ccd24` and unchanged
clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks every remaining
full-text workflow surface without advance ranking and selected exactly one
smallest dependency-ready leaf, P8-FT-35: preserve the legacy full-text
dialog's private Widgets initial logical size.

P8-FT-7 owns the modeless dialog and layout, P8-FT-33 owns one-time geometry
restoration, and P8-FT-34 owns the 430-by-450 minimum. Pinned legacy
`fulltextsearch.ui:5-12` supplies the exact independent contract: absent stored
geometry initializes the dialog to 492 logical pixels wide and 593 logical
pixels high. Under the shared-library/GUI rule, Widgets alone owns this size;
Core, the composition root, and installed consumers do not interpret it.

Every newly constructed dialog now has `size() == QSize(492, 593)` after
layout construction when stored geometry is absent or rejected by Qt. Widgets
establishes that size after the P8-FT-34 minimum and before P8-FT-33's restore.
Valid restored geometry continues to win, undersized restored geometry remains
clamped, and later direct resizing and all persistence and lifecycle behavior
remain unchanged.

P8-FT-35 changed no Core contract, installed header, public DTO, dependency,
adapter, index format, persistence, or build surface. Focused implementation
acceptance extends `full_text_search_dialog_test` with exact absent and
Qt-rejected initialization, valid larger restoration, undersized restoration
clamping, later direct resizing, and P8-FT-33/P8-FT-34 regressions. The focused
Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built.

The full implementation gate is Linux Release configure/build, exactly 109
registered tests and full `ctest --preset conan-release`, clean committed
exact-SCM `conan create` with the Release Qt WebEngine host profile and
packaged consumers, Release install, and standalone installed C and C++
consumers. They remain unchanged and source-compatible because P8-FT-35 adds
no installed interface, and remain the stronger package gate.

Help integration; maximum size and aspect ratio; screen/topology normalization,
placement fallback, and DPI policy beyond Qt logical sizing; other dialog
state; query semantics; excerpts; exact document/source targeting; decoration;
Preferences/index policy; index lifecycle; adapters/index formats;
dependencies; builds; and unrelated parity remain independently decomposed,
unselected, and unranked. No successor after P8-FT-35 is selected or ranked.

### Phase 8 full-text Help activation intent (complete)

The independent documentation-only post-P8-FT-35 audit is pinned to clean
migrated revision `89019d32113e4c68985756aa7a04e158951e1893`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It rechecks the four governing
migration documents and relevant current and pinned legacy code without
advance ranking and selects exactly one smallest dependency-ready leaf,
P8-FT-36: restore the private full-text Help activation intent.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:250-255` supplies one `Help` button, and
`fulltextsearch.cc:300-309,676-680` connects that button and an F1 action with
`Qt::WidgetWithChildrenShortcut` to the same dialog help request. The
pre-P8-FT-36 `full_text_search_dialog.cpp:47-155` had neither control nor
action. The current application also has no equivalent dependency-ready
full-text help destination: its Help menu intentionally omits the legacy reference action and
F1 shortcut. Widgets therefore owns only intent production; Core and installed
consumers do not acquire a GUI or help-system contract.

P8-FT-36 adds one private `fullTextHelpButton` with text `Help` to the existing
bottom control row and one private `fullTextHelpAction` whose exact shortcut is
F1 and whose context is `Qt::WidgetWithChildrenShortcut`. Activating either
emits exactly one argument-free private `HelpRequested()` signal. The dialog
remains open, and activation does not submit or cancel a search, alter the query,
accepted response, result model, or selection, impose focus behavior beyond
normal Qt button/shortcut activation, capture geometry, or mutate
configuration. Repeated independent activations remain deterministic.

Help-content lookup, a help window or engine, URL selection or dispatch,
composition-root consumption, Help-menu changes, embedded documentation, and
HTTP GET policy are excluded. Query semantics, excerpts, exact document/source
targeting, decoration, Preferences/index policy, index lifecycle,
adapters/index formats, dependencies, builds, remaining dialog layout/state,
and unrelated parity also remain independently decomposed, unselected, and
unranked.

The completed focused implementation acceptance extends
`full_text_search_dialog_test` to prove the button identity and text, the F1
shortcut and context, exactly one signal for each activation, deterministic
repetition, and unchanged idle and active-search state, response, lifecycle,
and geometry behavior. The focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built.

The full implementation gate is Linux Release configure/build,
exactly 109 registered tests and full `ctest --preset conan-release`, clean
committed exact-SCM `conan create` with the Release Qt WebEngine host profile
and packaged consumers, Release install, and standalone installed C and C++
consumers. P8-FT-36 adds no test executable, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement, so both consumers must remain
unchanged and source-compatible. No successor after P8-FT-36 is selected or
ranked. Implementation must stop on ref/worktree drift, legacy dirtiness,
ambiguous help evidence or acceptance, any choice of help destination or
transport, a public/Core or composition-root contract, dependency or installed
surface change, architectural decision requiring HTTP GET policy, or scope
expansion.

### Phase 8 full-text Search button default policy (complete)

The independent documentation-only post-P8-FT-36 audit is pinned to clean
migrated revision `ca6f206f1913a7941d8dcf3599704353e27e3c4e`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing migration documents and relevant current and pinned legacy code
without advance ranking, it selects exactly one smallest dependency-ready
leaf, P8-FT-37: preserve the private full-text Search button's default policy.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:204-214` makes `OKButton` the explicit default button while
setting `autoDefault` false. The pre-P8-FT-37
`full_text_search_dialog.cpp:133-136` made `fullTextSearchButton` the explicit
default but left Qt's auto-default property enabled, and the focused dialog
test pinned neither property. Widgets therefore owns the complete correction; Core, the
composition root, and installed consumers acquire no button or focus policy.

P8-FT-37 keeps `fullTextSearchButton` at `isDefault() == true` and makes it
report `autoDefault() == false` after construction and throughout idle,
submission, completion, and active-cancellation transitions. The property
correction does not independently submit or cancel work, close the dialog,
move focus, alter the query, accepted response, result model, selection,
geometry, or configuration, or change Help activation.

Tab order, initial or transferred focus, Return/Enter dispatch beyond Qt's
established explicit-default behavior, Cancel and Help button default policy,
help consumption, query semantics, results and navigation, index lifecycle and
presentation, Preferences/index policy, adapters/index formats, dependencies,
builds, and unrelated parity remain independently decomposed, unselected, and
unranked. No successor after P8-FT-37 is selected or ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` to prove
the exact button identity and both properties across the stated transitions,
plus regressions for Search, Cancel, and Help behavior. The focused Release
command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The full implementation gate remains Linux
Release configure/build, exactly 109 registered tests and full Release CTest,
clean committed exact-SCM `conan create` with the Release Qt WebEngine host
profile and packaged consumers, Release install, and standalone installed C
and C++ consumers. P8-FT-37 changes no installed header, DTO, ABI, dependency,
CMake export, Conan requirement, executable, or test registration, so both
consumers remain unchanged and source-compatible.

Implementation must stop on ref/worktree drift, legacy dirtiness, ambiguous
default-button evidence or acceptance semantics, any broader focus, tab-order,
or keyboard-policy choice, public/Core or composition-root expansion,
dependency or installed-surface change, an architectural decision requiring
HTTP GET policy, or scope expansion.

### Phase 8 full-text dialog tab sequence (complete)

The independent documentation-only post-P8-FT-37 audit is pinned to clean
migrated revision `9be9e1d8928c25f312b075fdda1674bb44d96013`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing migration documents and relevant current and pinned legacy code
without advance ranking, it selects exactly one smallest dependency-ready
leaf, P8-FT-38: restore the private full-text dialog's explicit keyboard tab
sequence.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:274-285` supplies the exact ordered chain, while current
`full_text_search_dialog.cpp:65-169` composes the corresponding controls across
the dialog and nested query composer without a dialog-level tab order. P8-FT-7
owns the private modeless dialog, P8-FT-31 owns Cancel lifecycle behavior, and
P8-FT-37 owns the Search button's default policy. Widgets therefore owns the
complete correction; Core, the composition root, and installed consumers do
not acquire a focus or keyboard contract.

The completed P8-FT-38 implementation establishes this consecutive forward
chain:
`fullTextQueryText`, `fullTextSearchResults`,
`fullTextUseMaximumWordDistance`, `fullTextMaximumWordDistance`,
`fullTextQueryMode`, `fullTextUseMaximumArticles`,
`fullTextMaximumArticlesPerDictionary`, `fullTextMatchCase`,
`fullTextSearchButton`, and `fullTextCancelButton`. The chain is stable after
construction and through idle, submission, completion, and active cancellation;
temporarily disabled controls do not cause Widgets to rewrite it.

Initial or transferred focus, focus policies, traversal after Cancel or before
the query field, and the relative placement of omitted controls including
Ignore Diacritics, Ignore Word Order, and Help are excluded. Return/Enter
dispatch, shortcuts, button default policies, search and response behavior,
index lifecycle and presentation, Preferences/index policy, adapters/index
formats, dependencies, builds, and unrelated parity also remain independently
decomposed, unselected, and unranked. No successor after P8-FT-38 is selected
or ranked.

Completed focused acceptance extends only
`full_text_search_dialog_test` to inspect the exact named forward chain across
the stated transitions and retain regressions for initial query focus, Search
default policy, Cancel behavior, Help activation, and request lifecycle. The
focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
implementation gate remains Linux Release configure/build, exactly 109
registered tests and full Release CTest, clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers, Release install, and standalone installed C and C++ consumers.
P8-FT-38 adds no executable, registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement, so both consumers remain
unchanged and source-compatible.

No successor after P8-FT-38 is selected or ranked. Any future work requiring
focus-policy changes, ordering omitted controls, traversal endpoints or
wraparound, initial/transferred focus, Return/Enter or shortcut behavior,
public/Core or composition-root contracts, dependencies, or installed surfaces
remains separately reviewed and unranked.

### Phase 8 full-text result-count minimum height (completed)

The independent documentation-only post-P8-FT-38 audit is pinned to clean
migrated revision `55777c188049b6de1d84a85db2b2db2b3d71a1e8`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing migration documents and relevant current and pinned legacy code
without advance ranking, it selects exactly one smallest dependency-ready
leaf, P8-FT-39: preserve the private full-text result-count label's explicit
minimum height.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:103-115` gives `articlesFoundLabel` a minimum height of 21
logical pixels, while current `full_text_search_dialog.cpp:82-86` creates the
mapped private `fullTextArticlesFoundLabel` without an equivalent minimum.
P8-FT-17 already owns the accepted result-count presentation and
P8-FT-34/P8-FT-35 own the dialog minimum and initial geometry. Widgets
therefore owns the complete correction; Core, the composition root, and
installed consumers acquire no sizing or presentation contract.

The completed P8-FT-39 implementation makes
`fullTextArticlesFoundLabel` retain
`minimumHeight() == 21` after construction and through idle, submission,
generation-current accepted completion, active cancellation, replacement, and
service/controller lifecycle transitions. Existing count text and visibility,
response statuses, progress behavior, layout ownership, sizing above the
minimum, and P8-FT-34/P8-FT-35 geometry behavior remain unchanged.

Result-count wording or localization, width policy, fixed or maximum height,
font/style/DPI policy, rearranging the count and progress widgets, response-
status layout, progress behavior or alignment, dialog geometry, focus/tab/key
behavior, search/result semantics, index lifecycle/UI, Preferences, adapters
and index formats, dependencies/builds, and unrelated parity are excluded and
remain unranked. No successor after P8-FT-39 is selected or ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` to
prove the exact minimum after construction and across the stated transitions
while retaining the listed regressions. The focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
implementation gate remains Linux Release configure/build, exactly 109
registered tests and full Release CTest, clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers, Release install, and standalone installed C and C++ consumers.
P8-FT-39 adds no executable, registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement, so both consumers remain
unchanged and source-compatible.

The implementation changes only private dialog construction, its existing
focused test, and the four governing documents. The Release registration
baseline remains exactly 109 tests; no successor after P8-FT-39 is selected or
ranked.

### Phase 8 full-text progress-bar alignment (complete)

The independent documentation-only post-P8-FT-39 audit is pinned to clean
migrated revision `6dc40e40038395ecb8f1c912aab720ada078ee93`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing migration documents and relevant current and pinned legacy code
without advance ranking, it selects exactly one smallest dependency-ready
leaf, P8-FT-40: preserve the private full-text search progress bar's explicit
centered alignment.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:117-128` gives `searchProgressBar` the explicit alignment
`Qt::AlignCenter`; `full_text_search_dialog.cpp` now creates the mapped private
`fullTextSearchProgress`, preserves that alignment, and establishes its
indeterminate range. The completed private dialog and
request workflow already own the progress widget and all lifecycle behavior.
Widgets therefore owns the complete correction; Core, the composition root,
and installed consumers acquire no progress or presentation contract.

P8-FT-40 preserves `fullTextSearchProgress->alignment() == Qt::AlignCenter`
after construction and through idle, submission, generation-current accepted
completion, active cancellation, replacement, and service/controller
lifecycle transitions. Its existing indeterminate range, visibility, request
lifecycle, result/count/status presentation, layout ownership, dialog
geometry, search semantics, and cancellation behavior remain unchanged.

Progress text and format, text visibility, value/range policy, orientation,
inversion, style and animation, size policy, layout rearrangement, indexing
progress UI and lifecycle, platform-specific styling, public/Core contracts,
dependencies and builds, and unrelated parity are excluded and remain
unranked. No successor after P8-FT-40 is selected or ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` to
prove the exact alignment after construction and across the stated transitions
while retaining progress range/visibility and surrounding workflow regressions. The
focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
implementation gate remains Linux Release configure/build, exactly 109
registered tests and full Release CTest, clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers, Release install, and standalone installed C and C++ consumers.
P8-FT-40 adds no executable, registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement, so both consumers remain
unchanged and source-compatible.

The implementation stops on ref/worktree drift, legacy dirtiness, ambiguous
alignment evidence or acceptance semantics, any broader progress, layout, or
style choice, public/Core or composition-root expansion, dependency or
installed-surface change, an architectural decision requiring HTTP GET policy,
or scope expansion. The implementation gate is exact production and focused
test scope, exactly four governing documentation updates, cross-document
consistency, Phase terminology, successor language, and the full Release,
install, consumer, and exact-SCM package verification described above.

### Phase 8 full-text result-count/progress row (complete)

The independent documentation-only post-P8-FT-40 audit is pinned to clean
migrated revision `0b6a081699947168959884a940870d8a741c1d74`, its identical
upstream and live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing migration documents and relevant current and pinned legacy code
without advance ranking, it selects exactly one smallest dependency-ready
leaf, P8-FT-41: restore the private result-count/progress horizontal row.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:102-129` places `articlesFoundLabel` first and
`searchProgressBar` second in one `QHBoxLayout`. Current
`full_text_search_dialog.cpp` instead adds the mapped private
`fullTextArticlesFoundLabel` and `fullTextSearchProgress` as separate items in
the dialog's vertical layout, with response-status widgets between them.
P8-FT-17, P8-FT-39, and P8-FT-40 already own count presentation, minimum
height, and progress alignment. Widgets therefore owns the complete layout
correction; Core, the composition root, and installed consumers acquire no
layout or presentation contract.

P8-FT-41 is complete with one private horizontal layout in the enclosing
vertical dialog layout, containing the unique direct dialog children
`fullTextArticlesFoundLabel` first and `fullTextSearchProgress` second. That
relationship remains stable after construction and through idle, submission,
generation-current accepted completion, active cancellation, replacement,
service replacement, and controller detachment. Existing count text and
minimum height, progress alignment/range/visibility, response-status order and
behavior, request lifecycle, dialog geometry, focus chain, search semantics,
and cancellation behavior remain unchanged.

Spacing, margins, stretch factors, size policies, widths, status-widget
rearrangement, broader layout redesign, progress behavior or style, geometry,
keyboard behavior, indexing lifecycle/UI, Preferences, adapters and index
formats, dependencies/builds, public/Core or composition-root changes, and
unrelated parity are excluded and remain unranked. No successor after
P8-FT-41 is selected or ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` to
prove the unique horizontal row, exact label-then-progress order, enclosing-
layout attachment, unchanged widget properties, and the stated lifecycle regressions.
The focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
implementation gate remains Linux Release configure/build, exactly 109
registered tests and full Release CTest, clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers, Release install, and standalone installed C and C++ consumers.
P8-FT-41 adds no executable, registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement, so both consumers remain
unchanged and source-compatible.

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
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing migration documents and relevant current and pinned legacy code
without advance ranking, it selects exactly one smallest dependency-ready
leaf, P8-FT-42: restore the private button-row spacer sequence.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:190-270` places expanding horizontal spacers before Search,
between Search and Cancel, between Cancel and Help, and after Help in one
`QHBoxLayout`. Current `full_text_search_dialog.cpp` preserves the three mapped
private buttons in that order but adds stretches only before Search and after
Help. P8-FT-36 through P8-FT-38 already own Help intent, Search default policy,
and the exact tab sequence. Widgets therefore owns the complete layout
correction; Core, the composition root, and installed consumers acquire no
layout or presentation contract.

P8-FT-42 restores one private horizontal button layout whose exact item
sequence is expanding horizontal spacer, unique direct dialog child
`fullTextSearchButton`, expanding horizontal spacer, unique direct dialog child
`fullTextCancelButton`, expanding horizontal spacer, unique direct dialog child
`fullTextHelpButton`, and expanding horizontal spacer. That relationship
remains stable after construction and through idle, submission,
generation-current accepted completion, active cancellation, replacement,
service replacement, and controller detachment. Existing button identity,
text, order and parentage, Search default policy, tab chain, Help intent,
Cancel lifecycle, request/response behavior, and geometry remain unchanged.

Exact spacer size hints, stretch factors, margins, layout spacing, button sizes
or size policies, button reordering, broader layout or style work, indexing
lifecycle/UI, Preferences, adapters and index formats, dependencies/builds,
public/Core or composition-root changes, HTTP GET policy, and unrelated parity
are excluded and remain unranked. No successor after P8-FT-42 is selected or
ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` to
prove the unique button row, exact seven-item spacer/button sequence,
horizontal expansion of all four spacers, enclosing-layout attachment,
unchanged button contracts, and the stated lifecycle regressions. The focused
Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
implementation gate remains Linux Release configure/build, exactly 109
registered tests and full Release CTest, clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers, Release install, and standalone installed C and C++ consumers.
P8-FT-42 is complete. It adds no executable, registration, installed header,
DTO, ABI, dependency, CMake export, or Conan requirement, so both consumers
remain unchanged and source-compatible.

Implementation must stop on ref/worktree drift, legacy dirtiness, ambiguous
layout evidence or acceptance semantics, any exact spacer-size, stretch-factor,
margin, spacing, style, or broader layout choice, public/Core or composition-
root expansion, dependency or installed-surface change, an architectural
decision requiring HTTP GET policy, discovery of another required file, or
scope expansion. This selection audit changes documentation only, so compiled
verification is intentionally skipped; exact four-file scope, cross-document
consistency, Phase terminology, successor neutrality, and `git diff --check`
are its verification gate.

### Phase 8 full-text Search group-box boundary (complete)

The independent documentation-only post-P8-FT-42 audit is pinned to clean
migrated revision `a491c72f7d1e2c2fa4b37151addccabdbf1a9b9d`, its identical
upstream and fresh live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing migration documents and relevant current and pinned legacy code
without advance ranking, it selects exactly one smallest dependency-ready
leaf, P8-FT-43: restore the private full-text Search group-box boundary.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:23-96` places the query field and search-option controls in
one `QGroupBox` titled exactly `Search`. Current
`full_text_search_dialog.cpp:65-70` instead adds the mapped private
`FullTextQueryComposer` directly to the dialog's enclosing vertical layout.
P8-FT-1 through P8-FT-42 already own query composition and control behavior,
the exact tab sequence, result presentation, surrounding layout, geometry, and
request lifecycle. Widgets therefore owns the complete presentation-only
correction; Core, the composition root, and installed consumers acquire no
grouping or presentation contract.

P8-FT-43 restores exactly one private `QGroupBox` titled `Search` between the
enclosing dialog layout and the unique existing `FullTextQueryComposer`. The
composer is directly contained by that group box, and the relationship remains
stable after construction and through idle, submission, generation-current
accepted completion, active cancellation, replacement, service replacement,
and controller detachment. Existing query values, labels, control ordering and
enablement, composition semantics, focus and tab chain, submission and
response behavior, result-count/progress row, statuses, button row, geometry,
and lifecycle behavior remain unchanged.

Group-box margins, spacing, size policy, alignment, styling, checkability,
flatness, mnemonic policy, broader composer-layout parity, indexing lifecycle
or UI, Preferences, adapters and index formats, dependencies/builds,
public/Core or composition-root changes, HTTP GET policy, and unrelated parity
are excluded and remain unranked. No successor after P8-FT-43 is selected or
ranked.

Completed focused acceptance extends only `full_text_search_dialog_test` to prove
the unique group box, exact title, unique directly contained composer,
attachment to the enclosing dialog layout, unchanged query-control contracts,
and the stated lifecycle regressions. The focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'`. The full
implementation gate remains Linux Release configure/build, exactly 109
registered tests and full Release CTest, Release install, packaged consumers,
standalone installed C and C++ consumers, and clean committed exact-SCM
creation with:

```sh
conan create . --build=missing \
  -pr:h=profiles/qt-webengine -pr:b=default \
  -s:h build_type=Release
```

P8-FT-43 adds no executable, registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement, so both consumers remain
unchanged and source-compatible.

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
upstream and fresh live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing migration documents and relevant current and pinned legacy code
without advance ranking, it selects exactly one smallest dependency-ready
leaf, P8-FT-44: restore the private full-text ignore-words-order label.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:83-89` gives the existing checkbox the exact translatable
text `Ignore words order`; before P8-FT-44,
`full_text_query_composer.cpp:89-92` used `Ignore word order` for the mapped
unique private
`fullTextIgnoreWordOrder` checkbox. P8-FT-1 through P8-FT-43 already own its
query semantics, persisted value, control behavior, tab sequence, containing
layout, and request lifecycle. Widgets therefore owns the complete text-only
correction; Core, the composition root, and installed consumers acquire no
label or presentation contract.

P8-FT-44 changes only that checkbox's text to exactly `Ignore words order`.
Its identity, parentage, checked and enabled state, ordering, focus and tab
behavior, mode-dependent behavior, query composition, submission, response,
geometry, and lifecycle behavior remain unchanged. The exact text remains
stable through construction, mode changes, option toggles, composition,
submission, generation-current accepted completion, active cancellation,
replacement, service replacement, and controller detachment.

Layout, mnemonic policy, translation-catalog work, other labels, grammar
modernization, indexing lifecycle or UI, Preferences, adapters and index
formats, dependencies/builds, public/Core or composition-root changes, HTTP
GET policy, and unrelated parity are excluded and remain unranked. No
successor after P8-FT-44 is selected or ranked.

Completed focused acceptance extends only `full_text_query_composer_test` to
prove the unique checkbox's exact text and preserve its identity, state,
enablement, composition semantics, and relevant control-transition regressions.
The focused Release command is
`ctest --preset conan-release -R '^full_text_query_composer_test$'`. The full
implementation gate remains Linux Release configure/build, exactly 109
registered tests and full Release CTest, Release install, packaged consumers,
standalone installed C and C++ consumers, and clean committed exact-SCM
creation with:

```sh
conan create . --build=missing \
  -pr:h=profiles/qt-webengine -pr:b=default \
  -s:h build_type=Release
```

P8-FT-44 adds no executable, registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement, so both consumers remain
unchanged and source-compatible. The implementation changes only the
private composer, its existing focused test, and these four governing
documents.

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

The independent documentation-only post-P8-FT-44 audit is pinned to clean
migrated revision `c771e6a47bf8fda61d57dd241d751c6ead8ce454`, its identical
upstream and fresh live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing migration documents and relevant current and pinned legacy code
without advance ranking, it selects exactly one smallest dependency-ready
leaf, P8-FT-45: restore the private full-text query-mode label.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:41-53` gives the label associated with the unique search-mode
selector the exact translatable text `Mode:`. Before P8-FT-45,
`full_text_query_composer.cpp:60-74,131-134` maps that selector to the unique
private `fullTextQueryMode` combo box but gave its `QFormLayout` label the text
`Mode`. P8-FT-1 through P8-FT-44 already own the selector's persistence, four
modes, query composition, control behavior, containing layout, focus and tab
behavior, and request lifecycle. Widgets therefore owns the complete text-only
correction; Core, the composition root, and installed consumers acquire no
label or presentation contract.

P8-FT-45 changed only the `QFormLayout` label associated with the unique
`fullTextQueryMode` selector to exactly `Mode:`. The label-field association,
selector identity, values, ordering, current and enabled state, focus and tab
behavior, persistence, query composition, submission, responses, geometry,
and lifecycle behavior remain unchanged. The exact label and association
remain stable through construction, mode and option transitions, repeated
composition, submission, generation-current accepted completion, active
cancellation, replacement, service replacement, and controller detachment.

Layout restructuring, any other label, mnemonic policy, translation-catalog
work, grammar modernization, indexing lifecycle or UI, Preferences, adapters
and index formats, dependencies/builds, public/Core or composition-root
changes, HTTP GET policy, and unrelated parity are excluded and remain
unranked. No successor after P8-FT-45 is selected or ranked.

Completed focused acceptance extends only `full_text_query_composer_test` to use
`QFormLayout::labelForField()` to prove the unique selector has exactly one
associated label with exact text `Mode:` and to preserve selector identity,
values, state, mode transitions, and composition semantics. Existing dialog
tests retain submission, generation-current completion, cancellation,
replacement, service replacement, controller detachment, response, geometry,
and lifecycle regressions. The focused Release command is
`ctest --preset conan-release -R '^full_text_query_composer_test$'`. The full
implementation gate remains Linux Release configure/build, exactly 109
registered tests and full Release CTest, Release install, packaged consumers,
standalone installed C and C++ consumers, and clean committed exact-SCM
creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-45 adds no executable, registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement, so both consumers remain
unchanged and source-compatible. The implementation changes only the private
composer, its existing focused test, and these four governing documents.

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

The independent documentation-only post-P8-FT-45 audit is pinned to clean
migrated revision `05e2f6b7ca3a657d1c0fe57bea7e47e691762054`, its identical
upstream and fresh live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing full-text documents and only the relevant current and pinned legacy
Widgets evidence, it selects exactly one smallest dependency-ready leaf,
P8-FT-46: remove the private full-text query field's unmatched label.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:28-31` places the unique query `QLineEdit` directly in the
Search group without a label. Before P8-FT-46,
`full_text_query_composer.cpp:57-58,131-133` mapped that field to the unique
private `fullTextQueryText` line edit but added a
`QFormLayout` label with text `Query`. P8-FT-1 through P8-FT-45 already own the
field, composition, persistence, containing layout, focus and tab behavior,
request lifecycle, and adjacent mode label. Widgets therefore owns the
complete one-label correction; Core, the composition root, and installed
consumers acquire no presentation contract.

The completed implementation changes only the query field's form row from
labeled to unlabeled full-width placement while retaining the existing
`fullTextQueryText`. Field identity, parentage, value, ordering, focus and tab
behavior, query composition, submission, responses, geometry, and lifecycle
behavior remain unchanged. The absent association and unique field remain
stable through construction, text mutation, mode and option transitions,
repeated composition, submission, generation-current accepted completion,
active cancellation, replacement, service replacement, and controller
detachment.

Any second label or behavior, broader layout restructuring, spacing, margins,
mnemonic policy, translation-catalog work, indexing lifecycle or UI,
Preferences, adapters and index formats, dependencies/builds, public/Core or
composition-root changes, HTTP GET policy, and unrelated parity are excluded
and remain unranked. No successor after P8-FT-46 is selected or ranked.

Completed focused acceptance extends only `full_text_query_composer_test` to
use `QFormLayout::labelForField()` to prove that the unique query field has no
associated label while preserving its identity, parentage, value, text
mutation, mode and option transitions, and repeated composition. Existing
dialog tests retain submission, completion, cancellation, replacement,
service replacement, controller detachment, response, geometry, focus, tab,
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

P8-FT-46 adds no executable, registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. Its implementation is
limited to the private composer, its existing focused test, and these four
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

The independent documentation-only post-P8-FT-46 audit is pinned to clean
migrated revision `08837e18ecef39bb97ffddc5842fe0560e2d326c`, its identical
upstream and fresh live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing full-text documents and only the relevant current and pinned legacy
Widgets evidence, it selects exactly one smallest dependency-ready leaf,
P8-FT-47: restore the private wildcard query-mode item text.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.cc:232-236` adds the third item of the unique search-mode
selector with exact translatable text `Wildcards` and maps it to the legacy
wildcard mode. Before P8-FT-47, `full_text_query_composer.cpp:60-74` mapped the same third
item of the unique private `fullTextQueryMode` selector to
`FullTextSearchMode::kWildcard` but displayed `Wildcard`. P8-FT-1 through
P8-FT-46 already own the selector, its four-item order and values, persistence,
query composition, containing layout, focus and tab behavior, and request
lifecycle. Widgets therefore owns the complete one-item correction; Core, the
composition root, and installed consumers acquire no presentation contract.

The completed implementation changes only the third selector item's displayed
text to exactly `Wildcards`. Selector identity, parentage, four-item count and
order, enum data, selected index, persisted mode, query composition, option
enablement, focus and tab behavior, submission, responses, geometry, and
lifecycle behavior remain unchanged. The exact text, unique selector, and
wildcard data mapping remain stable through construction, every mode and
option transition, repeated composition, submission, generation-current
accepted completion, active cancellation, replacement, service replacement,
and controller detachment.

The regular-expression item, any other label or behavior, maximum-distance or
article-limit captions and bounds, broader layout restructuring, spacing,
margins, mnemonic policy, translation-catalog work, indexing lifecycle or UI,
Preferences, adapters and index formats, dependencies/builds, public/Core or
composition-root changes, HTTP GET policy, and unrelated parity are excluded
and remain unranked. No successor after P8-FT-47 is selected or ranked.

Completed focused acceptance extends only `full_text_query_composer_test` to prove
that the unique selector's third item has exact text `Wildcards` and retains
its `FullTextSearchMode::kWildcard` data through construction, mode and option
transitions, and repeated composition. Existing dialog tests retain
submission, completion, cancellation, replacement, service replacement,
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

P8-FT-47 adds no executable, registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. Its implementation is
limited to the private composer, its existing focused test, and these four
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

The independent documentation-only post-P8-FT-47 audit is pinned to clean
migrated revision `f4bf4d3fc40de94913563d49f1fa824261c3c1d4`, its identical
upstream and fresh live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing full-text documents and only the relevant current and pinned legacy
Widgets evidence, it selects exactly one smallest dependency-ready leaf,
P8-FT-48: restore the private regular-expression query-mode item text.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.cc:232-236` adds the fourth item of the unique search-mode
selector with exact translatable text `RegExp` and maps it to the legacy
regular-expression mode. Before P8-FT-48,
`full_text_query_composer.cpp:60-75` mapped the same fourth item of the unique
private `fullTextQueryMode` selector to
`FullTextSearchMode::kRegularExpression` but displayed `Regular expression`.
P8-FT-1 through P8-FT-47 already own the selector, its four-item order and
values, persistence, query composition, containing layout, focus and tab
behavior, and request lifecycle. Widgets therefore owns the complete one-item
correction; Core, the composition root, and installed consumers acquire no
presentation contract.

The completed implementation changes only the fourth selector item's displayed
text to exactly `RegExp`. Selector identity, parentage, four-item count and
order, enum data,
selected index, persisted mode, query composition, option enablement, focus
and tab behavior, submission, responses, geometry, and lifecycle behavior
remain unchanged. The exact text, unique selector, and regular-expression data
mapping must remain stable through construction, every mode and option
transition, repeated composition, submission, generation-current accepted
completion, active cancellation, replacement, service replacement, and
controller detachment.

The wildcard item, any other label or behavior, maximum-distance or
article-limit captions and bounds, broader layout restructuring, spacing,
margins, mnemonic policy, translation-catalog work, indexing lifecycle or UI,
Preferences, adapters and index formats, dependencies/builds, public/Core or
composition-root changes, HTTP GET policy, and unrelated parity are excluded
and remain unranked. No successor after P8-FT-48 is selected or ranked.

Completed focused acceptance extends only `full_text_query_composer_test` to
prove that the unique selector's fourth item has exact text `RegExp` and
retains its
`FullTextSearchMode::kRegularExpression` data through construction, mode and
option transitions, and repeated composition. Existing dialog tests retain
submission, completion, cancellation, replacement, service replacement,
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

P8-FT-48 adds no executable, registration, installed header, DTO, ABI,
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
`5203c9f5730961cf13afb0d57b8523bc011ebafb`, its identical
upstream and fresh live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After rechecking the four
governing full-text documents and only the relevant current and pinned legacy
Widgets evidence, it completes exactly one smallest dependency-ready leaf,
P8-FT-49: restore the private horizontal row and legacy order of the two
existing ignore options.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:75-99` places exactly `checkBoxIgnoreWordOrder` followed by
`checkBoxIgnoreDiacritics` in one `QHBoxLayout`. Before P8-FT-49,
`full_text_query_composer.cpp:84-93,137-141` already owns the corresponding
private controls and query mappings, but adds them as separate vertical items
in the opposite order. P8-FT-1 through P8-FT-48 already own the composer, both
controls, their exact text and state, mode behavior, query composition,
containing layout, focus/tab behavior, and request lifecycle. Widgets owns the
complete presentation correction; Core, the composition root, and installed
consumers acquire no presentation contract.

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

Focused acceptance extends only `full_text_query_composer_test` to prove
one unique two-item horizontal layout containing the existing unique ignore-
word-order control first and ignore-diacritics control second, while preserving
identity, parentage, object names, text, state, mode transitions, and repeated
`Compose()` results. Existing dialog tests retain submission, completion,
cancellation, replacement, service replacement, controller detachment,
response, geometry, focus, tab, and lifecycle regressions. Add no executable
or registered test. The focused Release command remains
`ctest --preset conan-release -R '^full_text_query_composer_test$'`.

The implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, Release install, packaged consumers,
unchanged standalone installed C and C++ consumers, and clean committed exact-
SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-49 adds no executable, registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. Its implementation is
limited to the private composer, its existing focused test, and these four
governing documents.

P8-FT-49 is complete. No successor is selected or ranked. Implementation was
required to stop on ref/worktree drift, legacy dirtiness, ambiguous
membership, order, or acceptance semantics, any third widget or second
behavior, caption-bound policy, broader layout work, mnemonic or translation-
catalog work, public/Core or composition-root expansion, dependency or
installed-surface change, an architectural decision requiring HTTP GET policy,
discovery of another required file, or scope expansion. The implementation
gate is exact six-file scope, cross-document consistency, Phase terminology,
successor neutrality, the exact Conan command, `git diff --check`, and clean
pinned refs and worktrees.

### Phase 8 full-text coupled search-options grid parity (selected)

The documentation-only post-P8-FT-49 audit is based on clean migrated revision
`34f2b6903fba45e8a13650057cb7348e70f5be0f`, its identical local branch,
upstream, and fresh live remote, and unchanged clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. GET's product decision selects
exactly P8-FT-50: restore the pinned legacy coupled grid topology for the
existing search-option controls without rolling back migrated semantics.

The shared-library/GUI boundary governs this leaf. Pinned legacy
`fulltextsearch.ui:28-99` places the full-width query field before one
`QGridLayout`: row 0 contains the word-distance toggle at column 0, its spin
box at column 1, and a two-item horizontal layout containing `Mode:` followed
by the selector at column 2; row 1 contains the article-limit toggle at column
0, its spin box at column 1, and `Match case` at column 2. The existing
ignore-word-order/ignore-diacritics horizontal row follows the grid. The
pre-P8-FT-50 composer already owned all controls, state, query composition, and
validated bounds, but separated these controls across a form, two horizontal
rows, and a vertical match-case item.

P8-FT-50 is complete and reuses the unique existing controls. The composer's
top-level `QVBoxLayout` contains the full-width unlabeled query widget first,
the unique six-cell options grid second, and the completed P8-FT-49
ignore-options row third. The grid contains the exact coordinates above; only
its row-0/column-2 cell nests the unique two-item mode-label/selector horizontal
layout. All seven participating widgets remain direct child widgets of the
composer; the composer remains the sole direct child of the existing `Search`
group layout, and no widget is recreated.

The existing captions, `Mode:` label, four mode texts/data, object names,
identity, checked/enabled state, explicit focus/tab chain, persistence,
composition, submission, response, cancellation, replacement, geometry, and
P8-FT-1 through P8-FT-49 behavior remain unchanged. The word-distance spin box
retains `0..1000` and the articles-per-dictionary spin box retains
`1..100000`; legacy Qt `0..99` defaults and synthesized range-bearing captions
are not restored. Indexing lifecycle/status UI, Preferences, adapters/index
formats, public/Core/config or composition-root contracts, dependencies,
builds, installed surfaces, spacing/margins/stretch/alignment policy,
mnemonics, translation-catalog work, additional controls or behavior, HTTP GET
policy, and unrelated parity are excluded and unranked. No successor after
P8-FT-50 is selected or ranked.

Focused acceptance belongs only to `full_text_query_composer_test`: it proves
one unique six-item grid, exact coordinates and nested mode-layout
order, exact query/grid/ignore top-level order and layout parentage, and
unchanged widget parentage, identity, names, captions, mode texts/data, ranges,
state transitions, and repeated `Compose()` results. Existing dialog tests
retain Search-group, focus/tab, request, response, geometry, and lifecycle
ownership. Add no executable or registration; the Release baseline remains
exactly 109 tests. The full future implementation gate is focused Release
composer testing, Linux Release configure/build, exactly 109 registered tests,
full Release CTest, Release install, packaged consumers, unchanged standalone
installed C and C++ consumers, and clean committed exact-SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-50 changed no installed interface, but install and consumer checks remain
the stronger full gate. Implementation must stop on ref/worktree drift, legacy
dirtiness or movement, ambiguous topology, parentage, caption or acceptance
semantics, architectural conflict, failed validation, discovery of another
required file, or scope expansion.

### Phase 8 full-text word-distance caption policy closure

The documentation-only post-P8-FT-50 audit is pinned to clean migrated
revision `90823b6ce600063642cc5780a0d4197e75605329` and clean read-only legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. GET selected Option A: setting
labels describe the setting, while the associated numeric control owns and
exposes its bounds. Therefore the existing private visible label remains
exactly `Maximum word distance`, the spin box retains `0..1000`, and bounds
must not be synthesized into translatable caption text.

This decision intentionally yields no caption implementation leaf: production
code and focused tests already satisfy it. It changes no Widgets behavior,
public/Core/config contract, installed surface, dependency, build, test
registration, or Release baseline of exactly 109 tests. Pinned legacy
`fulltextsearch.cc:203-211` remains evidence for the historical range-bearing
caption, not authority to restore its obsolete bounds or text policy.

No compiled check is required for this four-document-only closure. Future
implementation still retains Linux Release build and CTest, Release install,
packaged and standalone installed C/C++ consumer gates, and exact-SCM creation:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Indexing lifecycle/status UI, the articles-per-dictionary caption, other text,
accessibility, styling/layout, translation-catalog work, public/Core/config or
composition-root changes, and unrelated parity remain excluded and unranked.
Stop on ref/worktree drift, ambiguity, architectural conflict, another required
file, failed validation, or scope expansion. No successor is selected or
ranked; the next boundary is a fresh post-policy full-text readiness audit.

### Phase 8 full-text articles-per-dictionary caption policy prerequisite (selected)

The fresh bounded post-policy audit is pinned to clean migrated HEAD, local
branch, upstream, and live remote at
`66b73596e6b9f2f296c0227933825fba100ba3b2`, plus the unchanged clean read-only
legacy checkout at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
After rechecking every remaining visible and private full-text gap against
current architecture and pinned legacy evidence, it selects exactly one
smallest independently dependency-ready prerequisite, P8-FT-51: decide the
articles-per-dictionary caption policy. No successor after P8-FT-51 is
selected or ranked.

Pinned legacy `fulltextsearch.cc:249-256` synthesizes a range-bearing
`Max articles per dictionary (%1-%2):` caption and assigns the numeric control
legacy bounds. Current `full_text_query_composer.cpp:109-125` instead displays
exactly `Maximum articles per dictionary`, while the associated spin box
solely owns and exposes `1..100000`; focused composer coverage preserves that
label, range, identity, enablement, and repeated composition. The completed
word-distance policy does not authorize extending its outcome to this separate
caption without an explicit product decision.

P8-FT-51 requires GET to choose between retaining the exact current label with
sole spin-box ownership of `1..100000`, which would create no implementation
leaf, and adopting a range-bearing caption using the current `1..100000`
bounds, which would authorize a later private Widgets caption leaf. Restoring
legacy Qt `0..99` defaults is not an option because it would conflict with the
current bounded query contract. This audit records the prerequisite only; it
does not choose an outcome or authorize source or test changes.

Index readiness, visibility, status, progress and background lifecycle UI, and
full-text Preferences enablement, type exclusions and dictionary-size policy
remain blocked on separate Core lifecycle or policy work. Exact-document
navigation, match/excerpt presentation, ignore-diacritics consumption,
accessibility, styling/layout, translation-catalog work, adapters/index
formats, public/Core/config or composition-root changes, dependencies, builds,
installed surfaces, and unrelated parity remain excluded and unranked.

Acceptance is four-document consistency, exact evidence and alternatives,
preserved public/Core/config/index-format/dependency boundaries, the unchanged
109-test Release baseline, and explicit no-successor language. Compiled checks
are omitted. Stop on ref/worktree or legacy drift, ambiguity beyond the stated
product choice, architectural conflict, another required file, failed
validation, or scope expansion.

### Phase 8 full-text articles-per-dictionary caption policy closure

The resumed documentation-only P8-FT-51 audit is pinned to clean migrated
HEAD, local branch, upstream, and live remote at
`456f3e9c68bf9514bfc6ded225d2ddc96f6c9477`, plus the unchanged clean read-only
legacy checkout at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
GET selected P8-FT-51 Option A: the exact private visible label remains
`Maximum articles per dictionary`; the associated spin box solely owns and
exposes `1..100000`; bounds must not be embedded in translatable caption text.
The already locked `Maximum word distance` label and sole spin-box ownership
of `0..1000` remain unchanged.

This decision intentionally creates no caption implementation leaf. Current
`full_text_query_composer.cpp:95-125` and focused composer coverage already
prove both exact labels, both ranges, identity, enablement, state transitions,
and repeated composition. Pinned legacy `fulltextsearch.cc:249-256` remains
historical evidence for the article-limit range-bearing caption, not authority
to restore its obsolete bounds or caption policy.

The closure changes no Widgets behavior, public/Core/config contract,
index format, installed surface, dependency, build, test registration, or
Release baseline of exactly 109 tests. Compiled checks are omitted for this
four-document-only closure. Stop on ref/worktree or legacy drift, ambiguity,
architectural conflict, another required file, failed validation, or scope
expansion. No successor is selected or ranked; the precise next boundary is a
fresh independent bounded full-text readiness audit.

### Phase 8 full-text dialog window-title translation (complete)

P8-FT-52 was implemented from clean synchronized migrated revision
`b25cee8fd95381ecd16f733107f7d201d5068eeb` with the unchanged clean read-only
legacy checkout pinned at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
The existing private full-text dialog title now resolves through its Widgets
translation context while retaining exact source text `Full-text search`.

Pinned legacy `fulltextsearch.cc:219-223` establishes the exact title and its
translation ownership. `full_text_search_dialog.cpp` now uses the dialog's
existing private `tr()` context. Focused construction coverage proves both an
exact-context test translator replacement and the unchanged English fallback
without a translator.

This implementation reinforces the shared-library/GUI boundary: the title remains
private presentation state and creates no public, Core, configuration,
index-format, composition-root, dependency, build, or installed-interface
contract. It adds no catalog or locale-loading policy, executable, or test
registration and preserves the exactly 109-test Release baseline. The locked
word-distance and articles-per-dictionary labels remain exact, with only their
spin boxes exposing `0..1000` and `1..100000`, respectively.

Per-backend index state and synchronous dictionary-load index construction do
not yet provide a dependency-ready readiness/status/progress/background
lifecycle boundary. Full-text Preferences enablement, type exclusions and
maximum-dictionary-size policy remain blocked on that separate Core decision.
Other translation, accessibility and styling surfaces, exact-document
navigation, match/excerpt presentation, ignore-diacritics consumption,
adapters/index formats, and unrelated parity remain separate and unranked.

The delivery gate is the focused and full 109-test Linux Release suite, fresh
Release configure/build and install, standalone installed C/C++ consumers,
clean committed exact-SCM Conan creation with packaged consumers, repository
validation, and clean synchronized refs/worktrees. No successor after P8-FT-52
is selected or ranked.

### Phase 8 full-text Search group-box translation (complete)

P8-FT-53 was implemented from clean synchronized migrated base
`339d1dd6e8b3540923153628497af23b6fa7208b` with the unchanged clean read-only
legacy checkout pinned at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
The existing private Search group-box title now resolves through the exact
`goldendict::app::FullTextSearchDialog` translation context while retaining
exact source and default visible text `Search`.

Pinned legacy `fulltextsearch.ui:23-27` marks that exact group-box title as
translatable. `full_text_search_dialog.cpp` now constructs the same visible
title with dialog-owned `tr()`, while completed P8-FT-52 supplies the private
translation-context precedent.

The implementation is limited to
`apps/goldendict/src/full_text_search_dialog.cpp` and
`apps/goldendict/tests/full_text_search_dialog_test.cpp`: construct the title
with the dialog's `tr("Search")`, retaining the exact English fallback.
Focused construction coverage proves exact-context replacement, sole
direct-child group identity/title, and automatic removal of global test
translator state. No catalog, locale loader, executable, test registration, public/Core/
configuration/index-format/dependency/build/composition-root, ABI, or
installed-interface change is authorized. The Shared-Library and GUI Boundary
continues to govern this private Widgets presentation leaf.

The exact `Maximum word distance` label with spin-box-owned `0..1000` and
exact `Maximum articles per dictionary` label with spin-box-owned `1..100000`
remain locked. The Release baseline remains exactly 109 tests. Index readiness,
status, progress, and background lifecycle plus full-text Preferences remain
blocked on a separate Core lifecycle/policy boundary. Translation catalogs and
other strings, accessibility, styling/layout, exact-document navigation,
match/excerpt presentation, ignore-diacritics consumption, adapters/index
formats, and unrelated parity remain separate and unranked.

Delivery requires the focused and full 109-test Linux Release suite, fresh
Release dependency install/configure/build and install, standalone installed C
and C++ consumers, clean committed exact-SCM Conan creation with packaged
consumers, exact six-file repository validation, and clean synchronized
refs/worktrees. No successor after P8-FT-53 is selected or ranked.

### Phase 8 full-text partial-status translation (complete)

P8-FT-54 was implemented from clean synchronized migrated base
`8dcf3d87fe4b25a916c864da56c307f5c78de24b`, plus the unchanged clean
read-only legacy checkout at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
The existing partial-response status
`Results may be incomplete.` now resolves through the private
`goldendict::app::FullTextSearchDialog` context. No successor after P8-FT-54 is
selected or ranked.

P8-FT-22 already owns the exact visible text and its generation-current
authoritative-partial visibility contract. Current
`full_text_search_dialog.cpp` constructs that label with dialog-owned `tr()`,
while P8-FT-52 and P8-FT-53 establish the same context and
the focused scoped-translator precedent. Pinned legacy full-text UI and code
have no equivalent partial-response status, so they supply no conflicting
wording or translation-context contract.

The implementation is restricted to `full_text_search_dialog.cpp` and
`full_text_search_dialog_test.cpp`: construct the existing label with
dialog-owned `tr("Results may be incomplete.")` and prove exact-context
replacement, English fallback, stable widget identity and text, unchanged
P8-FT-22 visibility behavior, and scoped translator cleanup. The Shared-Library
and GUI Boundary governs this private Widgets leaf. No catalog, locale loader,
executable, registration, public/Core/configuration/index-format/dependency/
build/composition-root, ABI, or installed-interface change is authorized.

The exact translated `Full-text search` dialog title and `Search` group title
remain unchanged. Exact `Maximum word distance` with control-owned `0..1000`
and exact `Maximum articles per dictionary` with control-owned `1..100000`
remain locked. Other response-string and catalog readiness, accessibility,
styling/layout, exact-document navigation, match/excerpt presentation,
ignore-diacritics consumption, adapters/index formats, and unrelated parity
remain separate and unranked. Index readiness/status/progress/background
lifecycle and full-text Preferences remain blocked on a separate Core
lifecycle/policy boundary.

Delivery requires the focused and full 109-test Linux Release suite, fresh
Release dependency install/configure/build and install, standalone installed C
and C++ consumers, clean committed exact-SCM Conan creation with packaged
consumers, exact six-file repository validation, and clean synchronized
refs/worktrees. Completion unlocks only a fresh independent bounded full-text
readiness audit; it does not select or rank its outcome.

### Phase 8 full-text empty-status translation (complete)

P8-FT-55 was implemented from clean synchronized migrated base
`175a92926b1046798b51b9757a28ed156555c0aa`, plus the unchanged clean read-only
legacy checkout at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The existing
private empty-response status `No matches` now resolves through the
`goldendict::app::FullTextSearchDialog` translation context.

P8-FT-23 already owns the exact text, unique `fullTextEmptyResponseStatus`
identity, and generation-current conclusive-empty visibility contract. The
dialog constructs the label with dialog-owned `tr("No matches")`; P8-FT-52
through P8-FT-54 establish the exact private dialog context and scoped-
translator test pattern. Pinned legacy full-text UI
and code have no equivalent empty-response status, so they supply no conflicting
wording or translation-context contract.

The implementation is restricted to `full_text_search_dialog.cpp` and
`full_text_search_dialog_test.cpp`. Focused coverage proves exact-context
replacement, English fallback, stable widget identity and text, unchanged
P8-FT-23 visibility predicates, stale/cancelled/detached safety, and scoped
translator cleanup. The
Shared-Library and GUI Boundary governs this private Widgets leaf. It authorizes
no catalog, locale loader, executable, registration, public/Core/configuration/
index-format/dependency/build/composition-root, ABI, or installed-interface
change.

The completed translations `Full-text search`, `Search`, and
`Results may be incomplete.` remain exact. `Maximum word distance` with
control-owned `0..1000` and `Maximum articles per dictionary` with
control-owned `1..100000` remain locked. Other response strings and catalog
readiness, accessibility, styling/layout, exact-document navigation,
match/excerpt presentation, ignore-diacritics consumption, adapters/index
formats, and unrelated parity remain independent and unranked. Index readiness,
status, progress, rebuild/failure reporting, background lifecycle, and full-text
Preferences remain blocked on a separate Core lifecycle/policy boundary.

The Release baseline remains exactly 109 tests. No successor after P8-FT-55 is
selected or ranked; completion unlocks only a fresh independent bounded full-
text readiness audit.

### Phase 8 full-text terminal-failure-status translation (complete)

P8-FT-56 is complete from clean synchronized migrated revision
`7bc3fbeee4af637e25dff8656ce7d22406d8ea2d`
and the unchanged clean read-only legacy checkout at
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Re-evaluation of the remaining
visible and private full-text gaps selected exactly the existing private
terminal-failure status for the dialog's translation context.

P8-FT-24 already owns the unique `fullTextFailureResponseStatus`, exact source
and fallback text `Full-text search failed`, and its generation-current
error-only visibility contract. The dialog now constructs that label with
dialog-owned `tr()`, while focused scoped-translator coverage proves the exact
private context and source. Pinned legacy has no
equivalent terminal-failure status, so it supplies no conflicting wording or
translation-context contract. No architecture or product choice remains.

The implementation is limited to `full_text_search_dialog.cpp` and its existing
focused test. It uses dialog-owned `tr("Full-text search failed")`; coverage
proves exact-context replacement, English fallback, stable identity and text,
unchanged P8-FT-24 visibility predicates, stale/cancelled/detached safety, and
scoped translator cleanup. The
Shared-Library and GUI Boundary governs this private Widgets leaf; no catalog,
locale loader, executable, registration, public/Core/configuration/index-
format/dependency/build/composition-root, ABI, or installed-interface change is
authorized.

The completed translations `Full-text search`, `Search`,
`Results may be incomplete.`, and `No matches` remain exact. The locked
`Maximum word distance` caption with control-owned `0..1000` and
`Maximum articles per dictionary` caption with control-owned `1..100000`
remain unchanged. All other visible and private full-text gaps remain
independent and unranked. Index readiness/status/progress/rebuild/failure
reporting/background lifecycle and full-text Preferences remain blocked on a
separate fully evidenced Core lifecycle/policy boundary.

The completed leaf changes only the private dialog, its focused test, and the
four governing documents. The Release baseline remains exactly 109 tests.
P8-FT-56 completion unlocks only a fresh independent bounded full-text
readiness audit.

### Phase 8 full-text mixed-result-status translation (complete)

The fresh independent bounded post-P8-FT-56 audit was pinned to clean
synchronized migrated HEAD, local branch, upstream, and live remote at
`491d85500d27df280c19d4a62a2adc9e14d55a33`, plus the unchanged clean
read-only legacy checkout at
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Re-evaluation of every remaining
visible and private full-text gap selected exactly one smallest independently
evidence-ready leaf, P8-FT-57: translating the existing private mixed-result
status through the dialog's translation context.

P8-FT-25 already owns the unique `fullTextMixedResultResponseStatus`, exact
source and fallback text `Some dictionaries could not be searched`, and its
generation-current mixed-result visibility contract. P8-FT-57 changes that
label's former `QStringLiteral` construction to dialog-owned `tr`, while
P8-FT-52 through P8-FT-56 establish the exact private
`goldendict::app::FullTextSearchDialog` translation context and focused scoped-
translator pattern. Pinned legacy has no equivalent mixed-result status, so it
supplies no conflicting wording or translation-context contract. No
architecture or product choice remains.

The completed P8-FT-57 implementation changes exactly
`apps/goldendict/src/full_text_search_dialog.cpp` and
`apps/goldendict/tests/full_text_search_dialog_test.cpp`: use dialog-owned
`tr("Some dictionaries could not be searched")` and prove exact-context
replacement, English fallback, stable identity and text, unchanged P8-FT-25
visibility and coexistence predicates, stale/cancelled/detached safety, and
scoped translator cleanup. The Shared-Library and GUI Boundary governs this
private Widgets leaf. No catalog, locale loader, executable, registration,
public/Core/configuration/index-format/dependency/build/composition-root, ABI,
or installed-interface change is authorized.

The completed translations `Full-text search`, `Search`,
`Results may be incomplete.`, `No matches`, and `Full-text search failed`
remain exact. The locked `Maximum word distance` caption with control-owned
`0..1000` and `Maximum articles per dictionary` caption with control-owned
`1..100000` remain unchanged. Index readiness/status/progress/rebuild/failure
reporting/background lifecycle and full-text Preferences remain blocked on a
separate fully evidenced Core lifecycle/policy boundary. Exact-document
navigation, result/match/excerpt presentation, ignore-diacritics consumption,
adapters/index formats, accessibility, styling/layout, catalogs, and unrelated
parity remain independent and unranked.

The completed leaf changes only the private dialog, its focused test, and the
four governing documents. Focused and full Release gates, install and consumer
checks, and clean exact-SCM package creation preserve exactly 109 registered
Release tests. P8-FT-57 completion unlocks only a fresh independent bounded
readiness audit; no successor is selected, ranked, recommended, or named.

### Phase 8 full-text partial-empty-status translation (complete)

The fresh independent bounded post-P8-FT-57 audit is pinned to clean
synchronized migrated HEAD, local branch, upstream, and live remote at
`58612007652ac24f08fc0bd8e2a4fb2b59839366`, plus the unchanged clean
read-only legacy checkout at
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. P8-FT-58 translates the existing
private partial-empty status through the dialog's translation context.

P8-FT-26 already owns the unique `fullTextPartialEmptyResponseStatus`, exact
source and fallback text `No matches in searched dictionaries`, and its
generation-current zero-result, authoritative-partial visibility and
partial-status coexistence contract without raw error details. The dialog now
constructs the status with dialog-owned `tr()`. P8-FT-52 through P8-FT-57 establish the exact
private `goldendict::app::FullTextSearchDialog` context and focused scoped-
translator pattern. Pinned legacy `fulltextsearch.cc` and `fulltextsearch.ui`
contain no equivalent partial-empty status or conflicting translation
contract. No architecture or product choice remains.

The completed implementation is bounded to the private dialog source and its
existing focused test and uses dialog-owned
`tr("No matches in searched dictionaries")`; focused coverage proves exact-context
replacement, English fallback, stable identity and text, unchanged P8-FT-26
visibility and partial-status coexistence, stale/cancelled/detached/replaced-
service/teardown safety, and scoped translator cleanup. The Shared-Library and
GUI Boundary governs. No catalog, locale loader, executable, registration,
public/Core/configuration/index-format/dependency/build/composition-root, ABI,
installed-interface, or test-baseline change is authorized.

Completed translations `Full-text search`, both `Search` uses,
`Results may be incomplete.`, `No matches`, `Full-text search failed`, and
`Some dictionaries could not be searched` remain exact. Locked policies remain
`Maximum word distance` with spin-box-owned `0..1000` and
`Maximum articles per dictionary` with spin-box-owned `1..100000`. All
completed P8-FT predicates, lifecycle behavior, and raw-detail suppression
remain unchanged. The Release baseline remains exactly 109 registered tests.

Index readiness/status/progress/rebuild/failure reporting/background lifecycle
and full-text Preferences remain blocked because repository evidence supplies
no separately authoritative resolution of the Core lifecycle/policy boundary.
Exact-document navigation, result/match/excerpt presentation, ignore-diacritics
consumption, adapters/index formats, accessibility, styling/layout, catalogs,
and unrelated parity remain independent and unranked. Completion unlocks only
a fresh independent bounded readiness audit; no successor is selected, ranked,
recommended, or named.

### Phase 8 full-text error-count translation acceptance (complete)

P8-FT-59 is complete from synchronized migrated revision
`471ba2a7db8491aa486951506389101caa8cb255` and the unchanged clean read-only
legacy checkout at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It accepts
focused translation of the existing private error-count status without a
production-source change.

P8-FT-27 already owns the unique `fullTextErrorCountResponseStatus`, exact
source and fallback text `Errors: %1`, authoritative decimal error count,
visibility for one or more accepted errors, coexistence with the other
response statuses, generation and lifecycle safety, and raw-detail
suppression. Production retains dialog-owned `tr("Errors: %1")`. The existing
private dialog test proves the exact `goldendict::app::FullTextSearchDialog`
context and source, authoritative single- and multi-digit decimal
interpolation, English fallback before installation and after scoped cleanup,
sole direct-child label identity, and unchanged P8-FT-27 predicates,
coexistence, stale/cancelled/detached/replaced-service/teardown safety, and
raw-detail suppression. The Shared-Library and GUI Boundary governs. No source
behavior, catalog, locale loader, executable, registration, public/Core/
configuration/index-format/dependency/build/composition-root/ABI/installed
interface, or registered-test-count change is authorized.

Completed translations `Full-text search`, both `Search` uses,
`Results may be incomplete.`, `No matches`, `Full-text search failed`, `Some
dictionaries could not be searched`, and `No matches in searched dictionaries`
remain exact. Locked policies remain `Maximum word distance` with spin-box-
owned `0..1000` and `Maximum articles per dictionary` with spin-box-owned
`1..100000`. All completed P8-FT behavior remains unchanged, and the Release
baseline remains exactly 109 registered tests.

Index readiness/status/progress/rebuild/failure reporting/background lifecycle
and full-text Preferences remain blocked because no separately authoritative
Core lifecycle/policy resolution exists. Other translation, accessibility,
styling, navigation, excerpt, diacritics, result presentation, adapters/index
formats, and unrelated parity remain independent and unranked. Completion
unlocks only a fresh independent bounded readiness audit; no successor is
selected, ranked, recommended, or named.

### Phase 8 exact-result navigation contract prerequisite (complete)

The fresh bounded audit is pinned to clean synchronized migrated HEAD, local
branch, upstream, and live remote at
`4cca1e81e1167222d067e475a4053088cf99ba38`, plus the unchanged clean
read-only legacy checkout at
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The user-approved priority makes
exact-result navigation and source targeting authoritative over the remaining
independent presentation gaps. The audit therefore selects exactly P8-FT-60:
define the narrow Core, facade, and navigation contract required to consume an
accepted full-text result's stable dictionary identity and opaque
`document_id` without implementing result activation yet.

P8-FT-60 is the next stable persisted ordinal because repository truth ends at
completed P8-FT-59 and contains no persisted P8-FT-60. A prior read-only
accepted-result-count translation-test recommendation was never selected,
committed, or recorded and owns no identifier; translation-acceptance
hardening remains deferred.

Current `dictionary_service.h:183-190` preserves `dictionary`, `headword`,
opaque `document_id`, `excerpt`, and ordered `matches` in `FullTextResult`.
`full_text_search_dialog.h:29-35` and
`full_text_search_dialog.cpp:337-348` show that the private dialog delivers
the exact accepted result, immutable submitted dictionary scope, query text,
and `ignore_diacritics` value by value. However,
`MainWindow::NavigateToFullTextResult` intentionally creates a headword-only
`kLookup` navigation and ignores the result's dictionary ID and `document_id`;
`main_window.cpp:5996-6040` performs that projection, while its focused smoke
check at `main_window.cpp:6394-6431` pins the source and article fields empty.
`LookupQuery` has no document selector, and
`article_tab_session.cc:38-66` requires those fields to remain empty for
`kLookup`. The similarly named source/target article fields belong to
`kInternalLink` semantics and cannot be repurposed implicitly.

Pinned legacy `fulltextsearch.cc:596-609` emits only the selected headword,
aggregated dictionary IDs, highlight expression, and ignore-diacritics value;
legacy `mainwindow.cc:3001-3013` forwards those values to headword definition
lookup. Legacy therefore supplies source scoping and later highlighting
evidence, but no exact-document contract.

The Shared-Library and GUI Boundary governs P8-FT-60. Core validates and
resolves the transport-neutral dictionary/document target behind
`DesktopFacade`; Widgets may only coordinate later commands and presentation.
The prerequisite preserves
current-tab activation, selected group and authoritative accepted dictionary
scope, tab history and persisted session replay, main-query text and selection,
and the completed accepted-query article-search handoff. Invalid, stale,
missing-dictionary, and missing-document targets must fail without mutating tab
navigation or history. Well-formed stale or removed IDs are document-not-found
because the accepted target has no source-revision token. The approved change
intentionally extends the installed C++ `DesktopFacade` vtable and navigation
DTO ABI; `DictionaryService`, `RuntimeDictionarySource`, and the C ABI remain
unchanged.

Completed translations `Full-text search`, both `Search` uses, `Results may be
incomplete.`, `No matches`, `Full-text search failed`, `Some dictionaries
could not be searched`, `No matches in searched dictionaries`, and
`Errors: %1` remain exact. Locked policies remain `Maximum word distance` with
spin-box-owned `0..1000` and `Maximum articles per dictionary` with spin-box-
owned `1..100000`. All completed P8-FT identities, predicates, lifecycle,
coexistence, and privacy guarantees remain unchanged. Configuration adds a
backward-compatible exact-target session tail; index-format, dependency, build,
catalog/locale-loader, executable, and registration boundaries remain
unchanged. The Release baseline remains exactly 109 registered tests.

The implementation changes the desktop Core contract, private resolution for
the twelve accepted built-ins, session persistence, focused existing tests,
the packaged C++ consumer, and these documents. Direct exact-result activation,
highlighting
and excerpts, ignore-diacritics consumption, translation acceptance, index and
adapter formats, unrelated configuration, dependencies, build behavior,
catalogs, locale loading, and unrelated parity are excluded and unranked.
P8-FT-60 selects no
successor. Completion unlocks only its dependency boundary.

### Phase 8 exact-result activation connection (complete)

The implementation starts from synchronized migrated
revision `0394b031031c265c7799386996bcbda22e5b0a3b` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
The approved visible exact-result navigation and source-targeting priority
selects exactly P8-FT-61: connect an accepted full-text result activation to
the completed P8-FT-60 exact-target navigation contract. P8-FT-61 is the next
stable ordinal after the durably completed P8-FT-60 foundation.

Current `full_text_search_dialog.h:24-35` and
`full_text_search_dialog.cpp:337-348` already deliver by value the accepted
result's authoritative dictionary identity and opaque `document_id`, together
with immutable accepted scope and query context. Current
`main_window.cpp:5996-6040` owns conversion of that private Widgets intent into
the current-tab `TabNavigationState`, but still omits `exact_target`. Current
`desktop_facade.h:35-85,139-190`, `desktop_facade.cc:156-205`, and the focused
Core coverage in `application_service_test.cpp:2375-2438` establish that
P8-FT-60 atomically validates and resolves `ExactArticleTarget`, includes it in
lookup navigation identity and persisted history/session, and rejects invalid,
unavailable-dictionary, and missing-document targets before mutation. Pinned
legacy `fulltextsearch.cc:596-609` and `mainwindow.cc:3001-3013` preserve
headword and aggregated dictionary targeting only and provide no exact-document
contract to reproduce.

The Shared-Library and GUI Boundary governs the connection. MainWindow owns
the application-command projection: it copies `intent.result.dictionary.id`
and `intent.result.document_id` without inspection into
`TabNavigationState::exact_target`, retains the existing `kLookup` headword,
title, selected group, and exact accepted dictionary scope, and submits the
complete navigation once through `DesktopFacade::OpenArticleTab`. Widgets must
not parse the opaque ID, resolve a backend, or bypass the facade. Core remains
the sole owner of target validation/resolution, navigation identity, history,
and session replay.

Facade rejection, including invalid navigation or target, unavailable
dictionary, missing or stale document, tab limit, and navigation-history
limit, must remain atomic: no tab, history/session, lookup request, or article-
search presentation mutation occurs. The established private status `Unable
to update article state` is sufficient and exposes no identifier or backend
detail. Success preserves current-tab activation, selected group, immutable
accepted scope and ordering, main-query text/cursor/selection, lookup dispatch,
history/session behavior, and the completed accepted-query article-search
handoff.

P8-FT-61 changes only private GUI composition and focused existing GUI smoke
coverage. `MainWindow` attaches the accepted IDs before the single facade call;
the existing smoke captures that command and covers successful persistence plus
atomic rejection for every facade error category. It adds no public API, ABI,
configuration, test
registration, dependency, build, catalog, locale-loader, installed/service,
headless `DictionaryService`, `RuntimeDictionarySource`, C API, adapter, or
index-format change. Highlighting/excerpts, ignore-diacritics consumption,
translations, and all unrelated parity remain excluded and unranked. All
completed P8-FT strings, captions, ranges, lifecycle and privacy behavior, and
the exactly 109-test Release registration baseline remain unchanged. No
successor after P8-FT-61 is selected, ranked, recommended, or named. Completion
unlocks only the dependency boundary established by this connection.

### Phase 8 match-centered excerpt contract prerequisite (selected)

The fresh bounded documentation-only audit starts from synchronized migrated
HEAD, upstream, and live remote revision
`d8d25b50ddf7cd84f71e7b700cb28fa260ea6117` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
After the completed P8-FT-60 exact-target facade/navigation foundation and
P8-FT-61 exact-result activation connection, the approved highlighting and
excerpt priority selects exactly P8-FT-62: establish the smallest
Shared-Library-owned match-centered excerpt contract required before Widgets
can safely present an accepted result excerpt. P8-FT-62 is the next stable
ordinal after the durably completed P8-FT-61 connection.

Current `dictionary_service.h:164-190` defines document-relative UTF-8 byte
ranges in `FullTextMatch` and carries an otherwise unspecified `excerpt` in
`FullTextResult`. Current `full_text_index.cc:314-410` maps the accepted match
back to the validated original `plain_text`, but sets `excerpt` to its first
4096 bytes. That prefix can end inside a UTF-8 code point and can omit a match
later in the document; no result field identifies the excerpt's document
origin. Current `full_text_response_model.cpp:15-46` and
`full_text_search_dialog.cpp:300-348` retain the DTO by value but expose only
the established headword, dictionary tooltip, and activation behavior.
Therefore direct result-list excerpt presentation is not safely ready.

Exact-article highlighting is not the smaller safe leaf. Current
`main_window.cpp:5996-6040,7970-8110` dispatches the accepted query only after
successful generation-current exact navigation and page load, but Qt
WebEngine's literal `findText` does not reproduce wildcard, regular-expression,
whole-word, word-order, word-distance, normalization, or backend range
semantics. Backend byte ranges refer to indexed plain text, not positions in
the composed sanitized HTML DOM. Pinned legacy `fulltextsearch.cc:596-609`,
`mainwindow.cc:3001-3013`, and `articleview.cc:2569-2728` instead pass a search
expression and reconstruct matching text from the rendered page; they provide
no competing excerpt DTO or DOM-range contract to adopt.

The Shared-Library and GUI Boundary governs P8-FT-62. Core must continue to
own match semantics and define each `FullTextMatch::byte_offset` and
`byte_length` as a checked range in the original validated UTF-8 document
`plain_text`; `FullTextMatch::text` must equal that exact byte slice. The result
range start and end must be UTF-8 code-point boundaries, so `text` is valid
UTF-8 even for pattern modes. The result
contract appends `FullTextResult::excerpt_byte_offset` as the final DTO member,
a `std::size_t` document-relative UTF-8 byte origin for `excerpt` defaulting to
zero. Core
constructs a deterministic valid-UTF-8 excerpt no larger than
`kMaximumFullTextExcerptBytes`, positioned to contain the first authoritative
match whenever that match itself fits the bound. Existing match ranges remain
document-relative; a consumer may derive an excerpt-relative range only by
checked subtraction from the authoritative origin.

The selection rule is byte-based and decision-complete. When the first match
fits the bound, choose the longest code-point-aligned document slice containing
it; among equal-length candidates minimize the difference between before-match
and after-match context bytes, then choose the earlier origin. When the match
itself exceeds the bound, start at its byte offset and return the longest
code-point-aligned prefix not exceeding the bound. UTF-8 code-point integrity
is guaranteed; grapheme-cluster shaping remains Qt presentation behavior and
does not redefine Core byte ranges.

The excerpt remains bounded plain trusted DTO text, not markup. Widgets may
convert and present it through normal Qt text roles, with Qt owning display
escaping, but must not parse dictionaries, rebuild indexes, reinterpret match
semantics, or expose raw backend details. Ellipsis wording, typography,
delegate layout, highlighting colors, multi-line policy, result-list
presentation, and DOM highlighting remain outside this prerequisite because
repository and pinned-legacy evidence provide no single authoritative product
shape.

P8-FT-62 intentionally changes the installed C++ `FullTextResult` DTO contract
and shared full-text index result construction. It changes no headless service
behavior, runtime-source or C API, configuration, persisted session, index
format, adapter ingestion, dependency, build, catalog, locale-loader,
executable, or test-registration boundary. P8-FT-60 validation, navigation,
history and session identity and P8-FT-61 activation, atomic failure and
article-search handoff remain distinct and unchanged. All completed P8-FT
strings, privacy and lifecycle guarantees remain unchanged. In particular,
`Maximum word distance` remains spin-box-owned at `0..1000`, and `Maximum
articles per dictionary` remains spin-box-owned at `1..100000`. The Release
registration baseline remains exactly 109 tests.

P8-FT-62 is complete. The installed C++ DTO appends the zero-defaulted
`excerpt_byte_offset`; existing shorter aggregate initializers remain source-
compatible, while consumers must rebuild for the intentional DTO layout/ABI
change. Shared index result construction maps matches to complete original
UTF-8 code points and applies the documented longest-slice, balanced-context,
earlier-origin and over-bound-prefix rules. No adapter implements excerpt or
match semantics, and serialized `full-text-v1` indexes remain unchanged.

Acceptance requires focused existing Core cases for ASCII and multibyte
matches near document start, middle, and end; exact document-range/text
correspondence; deterministic excerpt origin; valid UTF-8 boundaries; the
4096-byte bound; and the defined over-bound match-prefix policy. Existing
model/dialog cases must prove accepted by-value preservation
and that replacement, cancellation, stale completion, rejection, failed exact
activation, and teardown cannot present or revive stale data. Implementation
verification includes focused and full Release tests, install, standalone and
packaged C/C++ consumers, and exact-SCM Conan creation. Result-list rendering,
index lifecycle and Preferences, and all other full-text and unrelated parity
remain excluded and unranked. No successor after
P8-FT-62 is selected, ranked, recommended, or named. Completion will unlock
only the dependency boundary established by this match-centered excerpt/origin
contract.

### Phase 8 accepted-query article-highlighting context prerequisite (complete)

The implementation starts from synchronized migrated HEAD, upstream, and live
remote revision `97f2269a0cee85ae96b6c634d1967116a476e7e9` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The approved strict-
parity policy keeps the full-text result list headword-only, preserves its
existing dictionary-name tooltip, and excludes excerpts and row redesign.
Article-page match highlighting after successful exact activation is the only
approved presentation direction. P8-FT-63 completes the smallest prerequisite:
retain the accepted submission's complete highlighting-relevant policy through
private activation and article-load handoff.

The private dialog activation context binds query text, mode, match-case,
ignore-word-order, maximum-word-distance, and `ignore_diacritics` to the
accepted response. MainWindow retains the same values in its private
generation- and view-gated handoff. The exact-result page load still dispatches
only that text through Qt WebEngine's literal `findText`; the
existing article-search controls already select the first literal match and
support forward/backward navigation. That path cannot reconstruct wildcard,
regular-expression, whole-word, word-order, maximum-distance, case, or
normalization policy. Pinned legacy `fulltextsearch.cc:596-609` and
`articleview.cc:2569-2799` retain that policy, read rendered page plain text,
derive matched literal strings, highlight occurrences, select the first match,
and provide Previous/Next navigation.

P8-FT-63 extends only the private accepted activation context and article-load
handoff to preserve by
value the submitted query text, mode, match-case flag, ignore-word-order flag,
maximum-word-distance value, and existing ignore-diacritics flag. Replacement,
cancellation, stale or duplicate completion, rejected or failed activation,
tab/view replacement, and teardown cannot revive or apply older context. This
prerequisite does not consume ignore-diacritics, rematch rendered text, execute
JavaScript, or change current literal article search.

The Shared-Library and GUI Boundary remains governing. P8-FT-62 byte ranges
refer to indexed UTF-8 plain text and are not rendered DOM positions. A later
audit must define a Core-owned transport-neutral rendered-text matching plan
before Widgets can apply literal matches through WebEngine; Widgets may not
recreate matching semantics. Rendered-page extraction, match-plan interfaces,
DOM or literal application, highlight-all behavior, first-match selection, and
Previous/Next parity remain outside P8-FT-63 and unranked.

P8-FT-63 changes no installed API or ABI, Core DTO, C API, configuration,
persistence, index format, adapter, dependency, build, catalog, locale loader,
generated file, executable, or test registration. P8-FT-60 through P8-FT-62,
all completed strings, privacy and lifecycle contracts, the locked spin-box
ranges, and exactly 109 registered Release tests remain unchanged. No
successor after P8-FT-63 is selected, ranked, recommended, or named.
Completion unlocks only the private accepted-query article-highlighting
context dependency boundary.

### Phase 8 rendered-page text extraction transport prerequisite (complete)

The fresh independent bounded documentation-only audit starts from synchronized
migrated HEAD, upstream, and live remote revision
`6f473bf7ffc3d256342a585ab19313fe0b52a003` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. After completed
P8-FT-60 through P8-FT-63, strict pinned-Qt5 parity still keeps the full-text
result list headword-only, preserves the dictionary-name tooltip, and targets
article-page highlighting, first-match selection, and Previous/Next navigation
without advancing ignore-diacritics behavior.

P8-FT-64 completes the smallest independently implementable
prerequisite: privately transport the successfully loaded article page's plain
rendered text through Qt WebEngine's asynchronous extraction boundary. Current
`main_window.cpp:7976-8110` generation- and view-gates exact-result article
loading but sends only the accepted query text to literal `findText`; current
`main_window.cpp:7632-7644` demonstrates asynchronous
`QWebEnginePage::toPlainText` transport. Pinned legacy
`articleview.cc:2569-2799` synchronously reads the rendered frame's plain text,
rematches it, derives literal matches, highlights occurrences, selects the
first match, and navigates Previous/Next.

P8-FT-64 carries rendered plain text only after a successful, current exact-
result page load and binds the callback to the accepted search generation,
lookup presentation generation, tab ID, and `ArticleView`. Replacement,
cancellation, failed activation or load, a newer lookup or search generation,
tab/view replacement, navigation, and teardown must discard the callback. The
text is an inert private Widgets transport value: this leaf does not store it
in Core, rematch it, normalize it, execute a pattern, derive literals or ranges,
mutate the DOM, highlight, select a match, navigate, change status wording, or
consume ignore-diacritics.

The Shared-Library and GUI Boundary governs the selection. P8-FT-62 document
byte offsets, match text, and excerpts describe indexed validated UTF-8
`plain_text`; they are not rendered-page or DOM coordinates and must not be
applied to the extracted text. Widgets must not recreate wildcard, regular-
expression, whole-word, word-order, word-distance, case, or normalization
semantics. The later Core-owned transport-neutral rendered-text matching-plan
interface remains unresolved because placing it on an installed desktop-
orchestration boundary and choosing its synchronous/asynchronous DTO and ABI
shape is a separate architecture decision. P8-FT-64 selects no such interface
and no matching, DOM/literal application, highlight-all, first-selection, or
Previous/Next implementation.

P8-FT-64 changes only private Widgets transport and focused existing GUI smoke
coverage. The accepted query generation is carried by value, and successful
current exact-result loads retain rendered text only while tab, view, page,
lookup/search generations, and navigation identity remain current. It changes
no public or installed API/ABI, Core DTO, C API,
configuration, persistence, index format, adapter, dependency, build, catalog,
locale loader, translation, generated file, executable, or test registration.
All completed P8-FT behavior, locked strings/captions/ranges, and exactly 109
registered Release tests remain unchanged. No successor after P8-FT-64 is
selected, ranked, recommended, or named. Completion unlocks only the
generation-safe rendered-page text extraction dependency boundary.

### Phase 8 P8-FT-65 rendered-text matching-plan facade prerequisite (complete)

The implementation starts from clean synchronized migrated HEAD, local branch,
upstream, and live remote at
`1dce706344bd33254bedeb1e13d9b6eb5fa8c8c4`, plus the unchanged clean
read-only legacy checkout at
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. GET selects exactly P8-FT-65:
expose the smallest Core-owned rendered-text rematching and matching-plan
operation through the installed `DesktopFacade` desktop-orchestration
interface. The stateless implementation remains private to `goldendict_core`.

Current `dictionary_service.h:150-160,259-281` owns the accepted full-text
query vocabulary and headless service operations; `desktop_facade.h:167-193`
owns the installed desktop orchestration vtable. Current private matching and
UTF-8 origin mapping are concentrated in
`full_text_index.cc:231-319,368-465`, while
`main_window.h:454-484`, `main_window.cpp:6021-6076` and
`main_window.cpp:8312-8356` preserve the accepted context and generation-safe
rendered-text transport. Pinned legacy `articleview.cc:2569-2788` globally
rematches rendered plain text, retains literal occurrences in match order,
selects the first and drives Previous/Next; it does not provide a reusable
Core or installed API shape.

The Shared-Library and GUI Boundary governs this choice. `DictionaryService`
remains the headless dictionary/index operation interface; a free installed
matching function and a new module would create competing public contracts.
P8-FT-65 instead intentionally extends the installed C++ `DesktopFacade`
vtable and adds transport-neutral request/result DTOs. Widgets passes the
accepted query text, `FullTextQueryMode`, match-case, ignore-word-order and
optional maximum-word-distance values, plus rendered-page plain text converted
to valid UTF-8. The request also carries a positive bounded timeout; the facade
operation accepts an optional `CancellationToken`. `ignore_diacritics` is not a
request field and remains retained but unconsumed in private Widgets context.

The installed additions are
`kMaximumRenderedTextMatchPlanBytes` (16 MiB),
`RenderedTextMatchPlanRequest`, `RenderedTextMatchRange`,
`RenderedTextMatchPlanError`, and `RenderedTextMatchPlanResult`, plus
`DesktopFacade::BuildRenderedTextMatchPlan(const
RenderedTextMatchPlanRequest&, const CancellationToken*) const`. The request
contains `rendered_text`, `query_text`, `mode`, `match_case`,
`ignore_word_order`, `maximum_word_distance`, and `timeout`. A range contains
`byte_offset`, `byte_length`, and `literal`; the result contains the ordered
range vector, one typed error value, and a diagnostic message. The default
cancellation pointer is null and the default timeout matches the existing
five-second full-text query default. Request defaults otherwise match
`FullTextQuery`: whole words, case-insensitive, ordered words and no distance.
The error values are `kNone`, `kInvalidRequest`, `kMalformedPattern`,
`kCancelled`, `kDeadlineExceeded`, `kResourceLimit`, and `kInternal`;
diagnostic messages are for containment/diagnosis and are not UI status text.

The new rendered-text bound matches the existing private 16 MiB full-text
document limit, the query is bounded by `kMaximumFullTextQueryBytes`, and
distance by
`kMaximumFullTextWordDistance`. An empty query, invalid UTF-8 text or query,
oversized input, a non-positive timeout, an out-of-range distance, or word-order/distance
constraints on wildcard or regular-expression modes is an invalid-request
failure. A malformed wildcard/regular expression is a typed malformed-pattern
failure. Cancellation, deadline expiry, resource exhaustion and contained
internal failure remain distinct typed outcomes. Empty rendered text is valid
and, like any other no-match case, succeeds with an empty plan, not an error or
UI status.

Core must extract the existing normalization, wildcard/regular-expression,
whole-word/plain-text, case, word-order and word-distance behavior behind one
private reusable matcher used by both indexed search and the new facade
operation; Widgets must not reproduce it. The plan contains ordered rendered-
text UTF-8 byte ranges and an exact literal byte string for each range. Every
literal must equal the corresponding valid boundary-aligned substring of the
supplied rendered text. Matches are deterministic, leftmost-first,
non-overlapping, and ordered by increasing byte offset; adjacent matches are
allowed, duplicate literal values at different ranges are retained, and zero-
length matches are discarded. After accepting a match, scanning resumes at its
exclusive end, so the ranges directly define later first, Previous and Next
order without storing UI position.

The operation is synchronous and side-effect free. It polls cancellation and
the request deadline during normalization, pattern work and result collection,
and never returns a partial plan on failure. Widgets may accept the result only
while P8-FT-63/P8-FT-64 accepted-query, lookup, search, navigation, tab, view and
page identities remain current; stale results are discarded without changing
presentation. P8-FT-62 indexed-document offsets, excerpts and match text are
not rendered-text or DOM coordinates and are not inputs to the plan.

P8-FT-65 is complete. The installed facade DTO/vtable, private shared Core
matcher, existing-target Core/facade/API tests, and installed C++ consumer are
implemented without changing configuration, the C API, index format, adapters,
dependencies, catalogs, generated files, executables, UI strings/ranges/
translations, or test registration. The Release baseline remains exactly 109
tests. DOM/JavaScript or literal application,
highlight-all behavior, first selection, Previous/Next controls, status wording
and ignore-diacritics behavior remain outside this leaf. No successor after
P8-FT-65 is selected, ranked, recommended or named. Completion unlocks only the
P8-FT-65 `DesktopFacade` matching-plan dependency boundary.

### Phase 8 P8-FT-66 private match-plan worker/controller prerequisite (complete)

The implementation starts from clean synchronized
migrated/local/upstream/live-remote revision
`7596259baab285526438af205df4172032401f62` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Completed P8-FT-60 through
P8-FT-65 provide exact-result navigation and activation, accepted query policy,
generation-safe rendered-page text, and the installed deterministic matching
plan. P8-FT-66 supersedes P8-FT-65's historical no-successor closure. GET
selects exactly the smallest private prerequisite that may consume that plan
without deciding its presentation.
Current `full_text_request_controller.cpp:21-188` supplies the established
cancellable worker/controller ownership pattern, `main_window.cpp:8320-8364`
supplies the existing GUI identity gates, and pinned legacy
`articleview.cc:2569-2791` supplies the downstream presentation behavior that
this prerequisite intentionally does not implement.

P8-FT-66 adds a private Widgets worker/controller around the synchronous
`DesktopFacade::BuildRenderedTextMatchPlan` operation. The controller accepts
the complete request and a monotonic work generation by value, invokes the
facade off the GUI thread with explicit cancellation, and queues the typed
result back to the GUI thread. It borrows the facade only while attached and
must cancel and stop running or pending work before facade replacement or
teardown. Replacement work cancels both the running token and any superseded
pending request.

The GUI retains GUI-only identity context. Delivery is accepted only while the
work generation, accepted-query generation and exact query policy, lookup
presentation generation, article-search generation, navigation generation,
tab, view and page all remain current. Replacement activation or article
search, lookup or navigation invalidation, tab/view/page replacement, tab close,
facade detachment and application teardown cancel or invalidate work. Successful
empty and nonempty plans and typed failures remain inert private state; stale,
cancelled, duplicate, detached and teardown completions are discarded without
presentation effects.

This leaf adds no public or installed interface and changes no Core DTO, C API,
index format, adapter, configuration, dependency, catalog, translation,
executable or test registration. It does not call `findText`, apply DOM or
JavaScript ranges, highlight occurrences, select a match, implement Previous or
Next navigation, change status text or consume `ignore_diacritics`. Result rows
remain headword-only with their exact dictionary-name tooltip, and existing
exact activation remains unchanged. No successor after P8-FT-66 is selected,
ranked, recommended or named. Completion unlocks only generation-safe private
match-plan availability.

P8-FT-66 is complete. The private controller owns cancellable worker execution,
by-value inputs and queued GUI-thread delivery. Main-window acceptance retains
only identity-current typed results as inert state, and existing-target coverage
pins cancellation, teardown, every identity gate and absence of presentation
effects. The installed surface and exactly 109 registered Release tests remain
unchanged.

### Phase 8 P8-FT-67 private CSS Custom Highlight plan application (completed)

The completed implementation starts from the selected clean synchronized
migrated/local/upstream/live-remote revision
`c8bfcd77e01a243e3b565ebc818151c2255a0a2c` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. P8-FT-67 supersedes P8-FT-66's
historical no-successor closure and selects exactly the smallest user-visible
application leaf unlocked by its generation-safe inert plan. Current
`main_window.cpp:8479-8695` and `main_window.h:499-523` provide the accepted
query, rendered-text, plan and identity state; `article_view.cpp:70-107`
provides the existing private `QWebEngineScript::ApplicationWorld` execution
pattern; and `conanfile.py:51-58` pins Qt 6.11.1. Pinned legacy
`articleview.cc:2569-2728` establishes highlight-all over unique literal
matches and first-match selection, while `articleview.cc:2730-2791` establishes
the ordered range state later Previous/Next navigation consumes.

Qt 6.11 WebEngine `findText` is not the application mechanism. Its public find
flags provide only backward and case-sensitive search, so the removed Qt5
WebKit `HighlightAllOccurrences` orchestration cannot retain simultaneous
highlights for multiple unique plan literals. P8-FT-67 instead runs one private,
bounded ApplicationWorld script after the complete P8-FT-66 identity check.
The script feature-probes `CSS.highlights`, `Highlight`, `Range`, `Selection`
and the required DOM traversal APIs; maps the ordered Core-authored rendered-
text ranges to the unchanged article DOM; validates every mapped range against
its supplied literal; and constructs all ranges before changing presentation.
It must not parse or reinterpret the accepted query or consume
`ignore_diacritics`.

Every transaction and callback carries a generation-bound application token.
Staged artifacts remain unpublished, token-matched cleanup cannot remove a
newer published owner, and explicit current lifecycle invalidation is the only
operation that force-clears that owner. Successful nonempty application
atomically replaces the tab/page's private
named CSS highlight with all DOM occurrences of every unique supplied literal,
using CSS system mark colors, then selects and scrolls the DOM range mapped from
the first ordered plan item. Literal grouping follows the retained match-case
policy; its presentation-only literal lookup must not change, drop or reorder
the separately mapped plan ranges. Those ordered DOM ranges and current position
zero remain private, generation-bound application state for later navigation.
Empty success clears only the private full-text highlight and
application state. Unsupported APIs, a text/range/literal mismatch, JavaScript
failure or a stale callback clears any partial private artifact and publishes no
applied state. Existing invalidation, replacement activation/search/navigation,
tab close, page replacement, facade detachment and teardown also remove the
private registry entry and selection without inserting wrappers, normalizing
text nodes or otherwise mutating article DOM structure.

P8-FT-67 changes only private Widgets/WebEngine presentation and focused
existing GUI smoke coverage. It changes no installed or Core interface, DTO,
C API, index format, configuration, dependency, catalog, translation,
executable or test registration. Headword-only result rows, the exact
dictionary-name tooltip, exact activation, ordinary article `findText`, status
wording and the existing 109-test Release baseline remain unchanged.
Previous/Next commands and their UI/status behavior remain outside this leaf.
No successor after P8-FT-67 is selected, ranked, recommended or named.
Completion will unlock only generation-bound ordered applied-range state.

### Phase 8 P8-FT-68 private ordered applied-range navigation command prerequisite (completed)

The completed implementation was selected by clean synchronized revision
`67dd57c26f1f7b7af1021ffb1041947ddb0c2f20` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It supersedes P8-FT-67's
historical no-successor closure and selects only P8-FT-68, the smallest
dependency-ready consumer of its private generation-bound ordered applied-range
state. Current `article_view.cpp:90-256` publishes token-owned ordered DOM
ranges and position zero, while `main_window.h:499-523` retains the complete
accepted identity and applied result. Pinned legacy `articleview.cc:2703-2704`
and `articleview.cc:2730-2788` establish one-step, non-wrapping Previous/Next
behavior and the state later used for boundary enablement and match-count
status.

P8-FT-68 adds one private `ArticleView` command that accepts the expected
application token and a backward or forward direction. Against a current,
nonempty published owner, an available direction moves exactly one ordered
range, atomically selects and scrolls that range, and updates the zero-based
published position. An unavailable boundary direction performs no DOM,
selection, scroll or position change. Missing, empty, stale or token-mismatched
state is rejected without effects. The private asynchronous result distinguishes
accepted current state from rejection and returns the token, current position,
ordered count and whether Previous and Next are available, so the composition
root can recheck identity and later project legacy UI semantics without
inferring WebEngine state.

The Shared-Library and GUI Boundary governs this private presentation command.
It does not add or bind controls, change status text, add F3 or other shortcuts,
reuse ordinary article `findText`, consume `ignore_diacritics`, or alter the
P8-FT-67 highlight-all and initial first-range selection. It changes no
installed/Core/C interface or DTO, index format, configuration, dependency,
catalog, translation, executable or test registration. Headword-only result
rows, exact dictionary-name tooltips, exact activation and the exactly 109-test
Release baseline remain unchanged. No successor after P8-FT-68 is selected,
ranked, recommended or named. Completion unlocks only private full-text
navigation UI/status binding.

### Phase 8 P8-FT-69 private per-article full-text navigation row binding (completed)

The completed implementation was selected by clean synchronized revision
`35b2ffb94d3b819b7fe6242585fc5fa0729906b9` and unchanged clean pinned
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` supersedes P8-FT-68's
historical no-successor closure and completed only P8-FT-69. Pinned legacy
`articleview.ui:58-100` establishes a dedicated `ftsSearchFrame` below the web
view, with exact source captions `&Previous` and `&Next` and an independent
status label. Pinned legacy `articleview.cc:220-230,2688-2706,2730-2788`
establishes translated status `%1 of %2 matches`, one-based presentation,
initial and boundary enablement, and one-step non-wrapping navigation.

P8-FT-69 recreates that row as a private `ArticleView` composite containing its
web content and dedicated navigation row. The row is owned by each migrated
`ArticleView`. It is visible only while that article owns a current nonempty
applied-range state; tab switching therefore presents only the active article's
row. Replacement, page/load/view invalidation, tab close and teardown hide and
clear the affected row. The initial snapshot returned by application and every
later navigation completion may update its status and buttons only after the
composition root revalidates the complete P8-FT-68 identity and accepts the
returned token, zero-based position, ordered count, `can_previous` and
`can_next`. Rejected, stale, token-mismatched, detached and teardown callbacks
make no optimistic or later UI change. Accepted status is
`tr("%1 of %2 matches").arg(position + 1).arg(ordered_count)`.

The dedicated row is strictly separate from the migrated ordinary find-in-page
toolbar, its state, status, shortcuts and `findText` path. GET Option A retains
F3 exclusively for the migrated Dictionaries action and adds no F3/Shift+F3
full-text binding in this leaf. The Shared-Library and GUI Boundary remains
intact: no installed/Core/C interface or DTO, index, configuration, dependency,
catalog, activation, dictionary-name tooltip or headword-only result contract
changes. `ignore_diacritics` consumption remains independently unresolved and
unranked. Focused coverage stays in the existing GUI smoke target, registers no
test, and preserves the exactly 109-test Release baseline. No successor after
P8-FT-69 is selected, ranked, recommended or named.

### Phase 8 P8-FT-70 ICU normalized matching and origin-map prerequisite (complete)

P8-FT-70 was implemented from synchronized selected migrated revision
`c91dfc628bea5382c2dc10182e848561c919305e` and unchanged clean pinned legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It completes only the selected
smallest Core prerequisite for consuming the accepted `ignore_diacritics`
policy.

The prior matcher normalized source text one UTF-8 scalar at a time and
bypassed normalization, including diacritic removal, when `match_case` was
true. Current `desktop_facade.h:169-177`,
`desktop_facade.cc:130-164` and `main_window.cpp:9040-9078` show that the
rendered-text request omits the flag, the facade deliberately supplies
`false`, and Widgets retains the accepted value outside the request. Pinned
legacy `fulltextsearch.cc:596-609` and
`articleview.cc:133-190,2569-2648` prove that case sensitivity and diacritic
handling were independent and that normalized positions were mapped back to
original text. GET selects ICU behavior as the migrated source of truth rather
than exact custom-folding equivalence. This intentionally differs from Qt5's
custom folding and trailing `Mark_NonSpacing` extension by using ICU full case
folding, canonical normalization and all Unicode `Mn`, `Mc` and `Me` marks.

P8-FT-70 applies one deterministic pipeline to query and source: ICU NFD; ICU
full case fold only when `match_case` is false; NFD again because folding may
emit decomposable characters or marks; removal of every `Mn`, `Mc` and `Me`
only when `ignore_diacritics` is true; then ICU NFC as the stable matcher
representation. The two policies are independent in all four combinations.
Normalization operates across complete sequences, not independently per
scalar.

The private Core normalizer carries original UTF-8 byte spans through every
source transformation. Decomposition and folding expansions copy the input
span to every output unit; canonical reordering moves units with their spans;
removal drops the normalized mark but retains the complete original cluster
extent associated with a retained base; contraction/recomposition assigns the
minimal contiguous span covering all contributors. An original cluster is a
non-Mark scalar plus immediately attached `Mn`, `Mc` and `Me` scalars. A
leading or unattached Mark is its own cluster and is not attached across
whitespace or punctuation.

A normalized match touching any part of an expansion maps to the minimal
contiguous complete-original-cluster range covering every touched normalized
unit. Thus multiple units with one origin, such as ICU `ß` to `ss`, produce at
most one accepted nonempty original occurrence. Accepted ranges begin and end
on complete UTF-8 scalar and cluster boundaries. After acceptance, matching
advances beyond both the normalized match and remaining normalized units whose
origins overlap that accepted range. Empty, backward, duplicate and overlapping
mapped candidates are skipped while the normalized cursor still advances.
Leftmost-first order, original-range non-overlap and forward progress remain
deterministic; returned match text remains the exact original UTF-8 slice.

The Shared-Library and GUI Boundary keeps this prerequisite in Core. The one
private normalizer/origin-map owner serves indexed and rendered-text matching.
The correction changes existing `FullTextQuery` behavior for the combined
flags but changes no public type layout, installed ABI, facade vtable, C API,
DTO, index serialization, dependency or test registration. The rendered-text
request continues to omit `ignore_diacritics`, its facade call continues to
supply `false`, and Widgets continues to retain without consuming the flag.
All completed activation, headword-only, dictionary-tooltip, navigation,
translation, configuration, index-format/serialization, adapter/document-
identity, ordinary-find and exactly 109 registered Release-test contracts
remain unchanged; only the selected combined-flag matching behavior changes.

No successor after P8-FT-70 is selected, ranked, recommended or named.
Completion unlocks only the Core normalized matching/origin-map dependency
boundary; rendered request and Widgets consumption require a fresh audit.

### Phase 8 P8-FT-71 rendered-text ignore-diacritics consumption (completed)

P8-FT-71 was implemented from synchronized migrated revision
`14b78a90dd5a37dfa4a3381aeebf7a559eb9ae5d` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Current
`desktop_facade.h:169-178`, `desktop_facade.cc:130-164`,
`main_window.h:455-522`, `main_window.cpp:9063-9344`,
`rendered_text_match_plan_controller.cpp:24-215` and
`article_view.cpp:88-205` establish one unambiguous ownership path. Widgets
owns the generation-bound accepted presentation and rendered-page capture;
the controller transports a request by value; Core owns matching and exact
original-range production; ArticleView only validates and maps those ranges
onto the current DOM. Pinned legacy `fulltextsearch.cc:596-609` and
`articleview.cc:133-190,2569-2648` confirm that case sensitivity and diacritic
handling are independent and that matching positions return to original text.

P8-FT-71 adds a default-false `ignore_diacritics` member
to the transport-neutral installed `RenderedTextMatchPlanRequest`, copies the
accepted value into that request before asynchronous submission, and passes
the request member to the existing private Core `FullTextQuery`. The request
itself becomes the authoritative immutable policy snapshot, so the redundant
Widgets-only identity member is removed and completion/navigation staleness
checks compare the request-owned value. This necessary public DTO source and
layout change does not alter the facade method signature or vtable, C API,
configuration, dependency graph, index format or serialization.

Core continues to return P8-FT-70's exact complete original UTF-8 slices.
ArticleView and its ApplicationWorld JavaScript continue to receive exact
ranges and literals with the existing `matchCase` mapping policy; neither
layer normalizes text or implements diacritic semantics. Exact per-range DOM
application, highlight-all, first selection, P8-FT-69 Previous/Next and status
snapshots, token ownership and tab/view/page/generation rejection remain
unchanged. Ordinary find-in-page, activation, translations, headword-only
rows and dictionary tooltips also remain unchanged. Migrated matching keeps
the documented ICU divergence from Qt5 custom folding and trailing
`Mark_NonSpacing` extension.

No further successor is selected, ranked, recommended or named.

### Phase 8 P8-FT-72 Core full-text index lifecycle contract prerequisite (completed)

P8-FT-72 was implemented from synchronized migrated revision
`0b84e6dc2ab627c613c483a931b995f6c0554191` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It resolves the previously blocked
ownership decision: Core is the authoritative owner of full-text index policy
and lifecycle coordination. Widgets may edit policy, issue rebuild and cancel
intents, and consume immutable lifecycle snapshots only. Format adapters may
report capability and source revision and perform bounded cancellable index
work, but they do not own policy or the global lifecycle.

Current `application.h:268-278` already persists the transport-neutral policy
inputs, while `dictionary_backend.h:30-40` exposes only private per-backend
search, resolution and availability and `full_text_index.h:35-67` exposes only
private created/reused/rebuilt state. No authoritative Core coordination
contract joins those seams. Pinned legacy `config.hh:156-180`,
`dictionary.hh:423-436`, `fulltextsearch.cc:34-125` and
`mainwindow.cc:1381-1393,2100-2101,2158-2165,2288-2303` establish the missing
policy, capability/work, cancellation/current-item and stop/apply/restart
responsibilities, but place their coordination in the Qt GUI.

The completed private contract defines the Core-owned indexing policy inputs,
generation- and dictionary-identified rebuild/cancel intents, immutable
lifecycle snapshots, bounded work request/result values and an abstract format-
work port. The port reports capability and opaque source revision, accepts an
explicitly bounded request with existing Core cancellation/deadline types, and
contains adapter failures and escaped exceptions. Generation and dictionary
identity make stale commands and snapshots distinguishable. A deterministic
fake port in `full_text_index_test` pins the boundary without a registration.

P8-FT-72 implements no coordinator execution, persistence application,
Preferences UI, facade or Widgets wiring, visible readiness/progress/status,
any real adapter conversion, index serialization or the complete rebuild
workflow. It defines no progress percentage, queue/concurrency or two-pass
ordering, retry policy or failure presentation. The contract remains private,
so installed C++/Core/C interfaces and DTOs, configuration ABI, dependencies,
`full-text-v1`, all completed P8-FT behavior, ordinary find-in-page,
Dictionaries-only F3, translations and exactly 109 Release registrations stay
unchanged. No successor is selected, ranked, recommended or named.

### Phase 8 P8-FT-73 private Core full-text index lifecycle coordinator (completed)

The post-P8-FT-72 audit was performed from synchronized migrated revision
`fbc50b18fb183f69c34b524db869140a3760da25` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The smallest dependency-ready
leaf was one private Core coordinator state machine. The completed
`full_text_index_lifecycle` implementation owns registered format-work-port
entries and immutable lifecycle snapshots while retaining the existing
exception-containing port boundary.
`dictionary_service.cc:1812-1912` and `desktop_facade.cc:68-88` establish the
current private service/composition seam without owning this lifecycle.

The coordinator owns one authoritative current generation per dictionary ID.
An accepted monotonically newer rebuild samples the selected port's capability
and opaque source revision and publishes requested. Only a separately submitted
exact-identity bounded request publishes working and invokes the port once.
Completion may publish current, cancelled or failed only when both generation
and dictionary ID still match. Capability
rejection publishes unavailable; a supported dictionary with no accepted work
publishes not-indexed. Replaced-generation results, exceptions and cancellation
completions are stale and cannot mutate the current immutable snapshot.
Cancellation is dictionary-generation scoped, idempotent and propagated by the
existing Core token; caller-owned threads may overlap explicit calls without a
Core scheduler, and the P8-FT-72 port continues to contain escaped adapter
exceptions.

Pinned legacy `fulltextsearch.cc:31-125` proves background execution, shared
cancellation, readiness checks and exception containment, while
`mainwindow.cc:1381-1393,2100-2101,2164-2165,2180-2181,2302-2303` proves that
dictionary or preference replacement first stops work and later restarts it.
Its two-pass small-then-remaining loop is evidence of legacy behavior, not
coordinator policy selected by this leaf.

P8-FT-73 includes no automatic startup or recomposition scheduling, policy
persistence/application, queue or concurrency policy, two-pass ordering,
retry, progress percentage, real adapter bridge, facade/Widgets transport,
visible readiness/status/failure UI, serialization or complete rebuild
workflow. Installed/public C++, facade, C, DTO and configuration ABI,
dependencies, `full-text-v1`, ordinary find-in-page, Dictionaries-only F3,
translations, completed full-text behavior and exactly 109 registrations remain
unchanged. P8-FT-73 completes only the private Core coordinator state machine;
no dependency beyond that boundary is selected or named.

### Phase 8 P8-FT-74 Core article-count lifecycle-policy ingress (completed)

The completed implementation used synchronized migrated base revision
`a5e013d8164550f4757c6d4a58949cb575624989` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The smallest dependency-ready
leaf is one private, value-only ingress from persisted application preferences
to the existing Core lifecycle policy. Current `application.h:268-278`,
`configuration.cc:141,189,219-220,746-750,1542,1557-1559` and
`legacy_configuration.cc:403,451,471-472` already persist and migrate the three
policy inputs. Pinned legacy `config.hh:156-181`,
`preferences.ui:1371-1395`, `preferences.cc:360-382,490-575` and the format
eligibility checks, for example `stardict.cc:202-206`, establish that
`maxDictionarySize` is an article count in `0..10000000`, with zero meaning
unlimited, rather than a byte or megabyte limit.

P8-FT-74 corrects C++ terminology before the value is consumed. The installed
application-preference member becomes `full_text_maximum_dictionary_articles`
and the private lifecycle-policy member becomes
`maximum_dictionary_articles`, retaining the existing `std::uint32_t` type,
member position and object layout. This is an intentional installed C++ source
compatibility change; facade, C and DTO contracts remain unchanged. The
current serialized key `full_text_maximum_dictionary_megabytes` remains the
canonical read/write spelling as a frozen, historically misnamed wire key, and
legacy XML import continues to read `fullTextSearch.maxDictionarySize`, so no
persisted-value migration or configuration-format break is introduced.

The private projection copies full-text enablement, the article-count
limit and disabled-format text byte-for-byte into a policy value. It is
distinct from `full_text_maximum_articles_per_dictionary`, which bounds
returned search results. Projection creates no generation, request, adapter
call, eligibility result, lifecycle transition, snapshot mutation,
cancellation, thread or scheduling. A future byte-size limit, if selected,
requires a new unambiguous field and key; P8-FT-74 adds no megabyte or dual-field
policy.

Focused acceptance extends the existing application-service and full-text-
index tests without a registration. It covers default zero, current-key round
trip, legacy migration, inclusive `10000000` and rejected `10000001`, query-
limit separation, corrected policy defaults/equality and exact by-value
projection. Real adapter bridging, coordinator composition, automatic apply or
restart, facade/Widgets transport, Preferences UI, visible lifecycle state,
serialization and the complete rebuild workflow remain excluded. All P8-FT-72
and P8-FT-73 identity, stale-result, cancellation, failure and synchronous-work
semantics remain locked, as do dependencies, `full-text-v1`, ordinary find,
Dictionaries-only F3, translations and exactly 109 registrations. P8-FT-74 is
complete. No next dependency is selected.

### Phase 8 P8-FT-75 private registration metadata and policy eligibility

P8-FT-75 completes one private Core prerequisite: immutable per-dictionary
registration metadata, the pure eligibility predicate that consumes P8-FT-74
policy, and a distinct policy-excluded lifecycle snapshot. Its implementation
is grounded in synchronized migrated base revision
`18023f3aebae9ad610fa1b9afcb505dc946b7a1a` and clean legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Current
`dictionary_service.cc:450-464`
and `dictionary_service.cc:687-1036` supply stable dictionary IDs and format-
specific catalog branches,
the twelve textual dictionaries assign authoritative reader article counts,
and `full_text_index_lifecycle.h:19-180` plus
`full_text_index_lifecycle.cc:15-246` supply the policy, narrow work port and
coordinator. Pinned legacy `stardict.cc:202-206` and the equivalent checks in
`aard.cc`, `bgl.cc`, `dictdfiles.cc`, `dsl.cc`, `epwing.cc`, `gls.cc`,
`mdx.cc`, `sdict.cc`, `slob.cc`, `xdxf.cc` and `zim.cc` establish the exact
format identifiers and eligibility behavior.

The private registration value contains the dictionary ID, canonical format
type and authoritative `std::size_t` article count. Format type accepts only
the exact case-sensitive ASCII identifiers `AARD`, `BGL`, `DICTD`, `DSL`,
`MDICT`, `SDICT`, `SLOB`, `STARDICT`, `XDXF`, `ZIM`, `EPWING` and `GLS`.
Empty, unknown, differently cased, NUL-containing or non-ASCII values are
rejected before duplicate lookup, port probing, registration, snapshot,
cancellation or identity mutation. Registration uses the metadata dictionary
ID as its sole key, stores a value copy and retains null-port and duplicate-ID
rejection.
Composition/catalog owns production of this immutable metadata and the
lifetime-safe port, but wiring that producer is outside this prerequisite.
The port remains limited to capability, opaque source revision and bounded
cancellable work.

Core eligibility is enabled, the canonical format identifier is absent from
raw disabled-format text under length-aware ASCII case-insensitive substring
comparison, and the article limit is zero or the count is less than or equal
to it. Only bytes `A` through `Z` fold to `a` through `z`; every other byte,
including embedded NUL and non-ASCII content, compares unchanged. There is no
locale dependence, tokenization, trimming or delimiter normalization, so
legacy partial-substring behavior remains exact.

An accepted newer unsupported generation publishes `kUnavailable`. A
supported but ineligible generation publishes the new `kPolicyExcluded`,
cancels and replaces older work, remains format-capable, carries an empty
source revision and invokes neither revision lookup nor bounded work. A
supported eligible generation captures source revision and publishes
`kWorkRequested`; escaped revision failure publishes `kFailed`. Exact cancel
is still accepted and idempotent for the excluded identity without changing
its state, bounded execution rejects it, stale completions cannot overwrite
it, and a newer eligible generation can resume the P8-FT-73 flow. Initial
generation-zero registration retains the existing not-indexed, unavailable or
failed technical probe because no policy has been accepted.

Focused acceptance stays in `full_text_index_test` and adds no registration.
It covers every canonical identifier, atomic rejection of every invalid class,
copied metadata, locale-independent raw substring edges, zero/inclusive article
limits, unavailable-versus-policy-excluded precedence, no excluded revision or
work call, exact cancel/execution behavior, stale replacement and later
eligible recovery. Real adapters, composition wiring, automatic persisted-
policy apply/restart, facade/Widgets transport, UI, scheduling, progress,
retry, two-pass ordering, serialization and the complete rebuild remain
excluded. Installed/public C++, facade, C, DTO and configuration ABI,
dependencies, `full-text-v1`, ordinary find, Dictionaries-only F3,
translations and exactly 109 registrations remain unchanged. P8-FT-75 is
complete. No successor is selected or named.

### Phase 8 P8-FT-76 private immutable full-text index publication contract (completed)

P8-FT-76 was implemented from synchronized migrated base revision
`c32aa1217bb9934b4b8ede1e1623a12c7a1777b3` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It implements one smallest
dependency-ready prerequisite: a private Core ownership and publication
contract for replaceable full-text indexes. Current textual adapters build a
`std::optional<FullTextIndex>` during dictionary construction and expose it
directly to search and document resolution. No adapter has a synchronized
replacement holder, so composition cannot yet supply the lifetime-safe real
work port required by P8-FT-75.

The implementation adds one narrow private Core snapshot abstraction for an
optional `std::shared_ptr<const FullTextIndex>`. Under SRP, the holder owns
only snapshot acquisition and replace-on-success publication; index building,
lifecycle policy, generation authorization and scheduling remain outside it.
Readers depend on that abstraction rather than atomic or mutex mechanics,
preserving encapsulation and DIP without introducing another layer or pattern.
A replacement is built completely off-side through bounded incremental
traversal and is atomically published
only after successful completion. Search and document-resolution calls acquire
one immutable shared snapshot and retain it for the entire call. In-flight
readers may finish against the prior immutable snapshot, while later
acquisitions observe the complete replacement; no reader can observe partial
construction,
in-place mutation or destruction. C++17 atomic shared-pointer operations or an
equivalently encapsulated synchronization mechanism are acceptable; lock-free
publication is not required. Null publication is rejected without changing
the current snapshot. No additional interface is justified unless it provides
concrete ownership, testability or maintenance value at this boundary.

Failure, cancellation, deadline expiry, resource-bound rejection and stale
work publish nothing. The future real adapter bridge must authorize publication
against the current lifecycle generation, but P8-FT-76 adds neither that bridge
nor its generation handoff. Core remains lifecycle and eligibility owner;
composition/catalog remains owner of immutable registration metadata plus the
lifetime-safe port; and the port remains limited to capability, opaque source
revision and bounded cancellable work. Persisted-policy application/restart,
scheduling, progress, facade/Widgets transport, UI, serialization and complete
rebuild orchestration remain excluded. All locked P8-FT-72 through P8-FT-75
semantics, dependencies, `full-text-v1`, ordinary find, Dictionaries-only F3,
translations, intentional ICU divergence and exactly 109 registrations remain
unchanged. No successor beyond P8-FT-76 is selected or named.

### Phase 8 P8-FT-77 private bounded AARD full-text traversal prerequisite (completed)

The completed implementation is grounded at migrated base revision
`a4393dcb1fd3bf4cd13938dddc909ef96af26135` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects one smallest
dependency-ready prerequisite after immutable snapshot publication: replacing
AARD's all-at-once full-text article extraction with a private incremental
traversal seam. The migrated base `aard_reader.h:42-68`,
`aard_reader.cc:483-499` and `aard_dictionary.cc:55-92` showed construction
first copying every retained article into one vector and then building a second
full-text document vector.
Pinned legacy `aard.cc:609-635` and `ftshelpers.cc:298-366` show AARD delegating
to shared indexing with cancellation checks during article traversal.

The AARD reader synchronously visits one `FullTextArticle` at a time through
a private callback and invokes a supplied checkpoint before every record
inspection, including duplicate records. Traversal preserves current record
order, first-record deduplication, `record_ordinal`, `article_ordinal`, headword
and payload. The callback receives a non-owning value valid only for its call;
the reader retains neither callback output nor traversal state after return.
Checkpoint or visitor exceptions stop traversal immediately and propagate to
the caller. This seam supplies cancellation/deadline checkpoints while the
eventual work consumer remains responsible for the request's document,
per-document-byte and corpus-byte limits.

AARD's construction-time consumer uses the same traversal without changing
document IDs, article assembly, generated-index behavior, search results or
error translation. This leaf adds no cross-format abstraction, real lifecycle
work port, registration, generation handoff or publication path. Core retains
lifecycle and eligibility ownership; the private adapter owns traversal
mechanics; complete immutable snapshot publication remains mandatory.
Composition, persisted-policy application/restart, scheduling, progress,
facade/Widgets transport, UI, serialization, two-pass ordering and every public
ABI remain excluded. P8-FT-77 is complete. No successor is selected or named.

### Phase 8 P8-FT-78 private generation-authorized immutable snapshot handoff prerequisite (completed)

The completed implementation is grounded at migrated base revision
`5c58b1ead60aece993bd41d49ce763ad67940a47` and clean pinned legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. At that base,
`full_text_index_lifecycle.cc:241-287` invoked bounded port work before it
reacquired the coordinator lock and rejected a replaced generation, while
`full_text_index_snapshot.cc:10-24` provides atomic complete-replacement
publication. That gap would have permitted a real AARD port to publish stale
work before Core performed its authoritative generation check. Pinned legacy
`fulltextsearch.cc:34-70,100-111`,
`mainwindow.cc:1381-1393,2100-2101,2158-2165,2180-2181,2288-2303` and
`aard.cc:609-635` confirm cancellation and stop/apply/restart ownership. This
leaf supplies the missing safe migrated publication handoff.

P8-FT-78 implements only the missing private handoff. Successful bounded work
returns a non-null `std::shared_ptr<const FullTextIndex>` candidate without
publishing it. Composition/catalog registers immutable dictionary metadata, a
lifetime-safe format-work port and the same lifetime-safe snapshot holder used
by that dictionary's readers. After work returns, the coordinator holds its
existing synchronization boundary, revalidates exact dictionary and generation
identity and cancellation, atomically publishes the candidate, and only then
transitions the lifecycle snapshot to `kCurrent`. Readers retain an acquired
snapshot for their complete search or resolution call.

Stale, cancelled, failed, exceptional, deadline-expired, over-budget and
identity-mismatched outcomes publish nothing. A completed result without a
candidate is contained as failure and cannot become current. The port remains
capability/source-revision/bounded cancellable work only: it neither owns nor
performs policy, generation authorization, lifecycle transition or
publication. Core retains lifecycle and eligibility ownership; the holder
retains only acquisition and atomic replacement.

The implementation adds no real AARD port, catalog/composition wiring,
automatic policy application/restart, scheduler, progress, facade/Widgets
transport, UI, serialization change or complete rebuild workflow. Canonical
formats, `kPolicyExcluded`, article and work limits, `full-text-v1`, intentional
ICU divergence, public/installed boundaries, ordinary find, Dictionaries-only
F3, translations and exactly 109 registrations remain unchanged. P8-FT-78 is
complete. No successor is selected or named.

### Phase 8 P8-FT-79 private AARD full-text format-work bridge (complete)

The fresh post-P8-FT-78 audit used synchronized migrated revision
`7a07e41b7ad0e9613a93129bd55c5cf598e06166` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Current
`aard_reader.cc:476-498` already captures the sole `.aar` source snapshot and
provides bounded incremental article traversal; `aard_dictionary.cc:32-118`
already defines the stable identity, first-record document ownership and
`aard-index:<record-ordinal>:<article-ordinal>` provenance; and
`full_text_index_lifecycle.cc:247-298` plus `full_text_index_snapshot.cc:10-24`
already provide unpublished result handoff and generation-authorized atomic
publication. Pinned legacy `aard.cc:609-635`, `ftshelpers.cc:298-366`,
`fulltextsearch.cc:34-111` and
`mainwindow.cc:1381-1393,2100-2101,2158-2165,2180-2181,2288-2303` confirm AARD
indexing, traversal cancellation and
stop/apply/restart ownership. No smaller format-specific builder, result or
source-revision prerequisite remains.

P8-FT-79 implements one private AARD `FullTextIndexFormatWorkPort` bridge. It is
capable only when an index destination is configured, reports a deterministic
opaque revision derived from the reader's sole-container source snapshot and
rejects work whose captured revision no longer matches. Its bounded work uses
the P8-FT-77 traversal and preserves current article assembly, first-record
deduplication, document ordering and IDs. It checks cancellation and deadline
before every record inspection and enforces the request's nonzero document,
per-document-byte and corpus-byte limits with overflow-safe accounting before
returning one complete immutable `FullTextIndex` candidate. Preparation reads
but does not mutate the canonical generated index. After exact generation and
cancellation revalidation, the coordinator alone finalizes any prepared
artifact and publishes its snapshot. The port never authorizes publication.

The AARD dictionary uses the same lifetime-safe snapshot holder for its
construction-time created or reused index and for later replacements. Search,
availability, state and document resolution each acquire one immutable
snapshot and retain it for the complete call. Catalog composition owns the
dictionary, holder, port and immutable `{dictionary ID, "AARD", article count}`
registration and supplies them to the Core coordinator with shared lifetime.
Registration itself starts no work and applies no policy.

Focused implementation extends existing AARD dictionary, lifecycle and
application-service tests without adding a registration. It must prove exact
metadata and revision, create/reuse/rebuild candidate return, holder-backed
search and resolution, stable provenance, and lifetime-safe catalog
registration. Zero or exceeded bounds, overflow, revision mismatch,
cancellation, deadline expiry, corrupt or stale index handling and escaped
traversal, assembly or build failures return no publishable candidate and
leave the holder unchanged. Only the coordinator may publish after exact
generation and cancellation revalidation; stale work cannot replace the
active snapshot or reach `kCurrent`.

Automatic policy application/restart, scheduling, progress, facade/Widgets
transport, UI, serialization changes and the complete rebuild workflow remain
excluded. P8-FT-72 through P8-FT-78, canonical format IDs,
`kPolicyExcluded`, article and work bounds, `full-text-v1`, intentional ICU
divergence, public/installed boundaries, dependencies, ordinary find,
Dictionaries-only F3, translations and exactly 109 registrations remain
locked. P8-FT-79 is complete. No successor
beyond it is selected or named.

WebEngine's default-profile cache path, size, type, cookies, and persistent
storage are not changed or cleared by these controls. A WebEngine profile
policy or broader browser-data deletion promise requires a separate reviewed
prerequisite. Mandatory article CSP and sanitization also cannot be weakened
by restoring the legacy optional cross-site-content checkbox.

See [project-design-rules.md](project-design-rules.md) for project design rules
and design-boundary rationale.
