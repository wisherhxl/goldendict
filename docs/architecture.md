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
state and legacy article sessions remain excluded.
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
overrides retain fixed meanings. Persisted-session migration remains later.

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

See [project-design-rules.md](project-design-rules.md) for project design rules
and design-boundary rationale.
