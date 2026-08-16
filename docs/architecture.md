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

WebEngine's default-profile cache path, size, type, cookies, and persistent
storage are not changed or cleared by these controls. A WebEngine profile
policy or broader browser-data deletion promise requires a separate reviewed
prerequisite. Mandatory article CSP and sanitization also cannot be weakened
by restoring the legacy optional cross-site-content checkbox.

See [project-design-rules.md](project-design-rules.md) for project design rules
and design-boundary rationale.
