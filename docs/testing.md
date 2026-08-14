# Testing

This document contains GoldenDict's test commands, verification strategy, and
pre-PR verification guidance.

## Local Tests

Official local test workflow:

```sh
ctest --preset conan-debug
ctest --preset conan-release
```

Tests are built by default. Disable them explicitly with `-DBUILD_TESTS=OFF`
only when a task does not need local test targets.

The Phase 2 focused test is `goldendict_smoke`. It exercises the executable's
non-GUI startup path and therefore does not require a display server.

Phase 4 adds `core_api_test` for the bounded headless API defaults and a C++
`headless_api_test` consumer under `test_package/`. The latter must compile and
link only against the installed `goldendict::core` target, without Qt Widgets,
Qt Gui, or Qt WebEngine.

`stardict_reader_test` generates deterministic uncompressed and gzip/dictzip-
compatible `.dict.dz` StarDict fixtures at runtime. It verifies metadata,
duplicate and UTF-8 headwords, exact and missing lookup, uncompressed-file
precedence, Unicode-folded ranked prefix lookup, distinct lightweight headword
suggestions, scan checkpoints, and stable error categories for invalid
metadata, corrupt compression, truncated indexes, missing companion files, and
article ranges outside dictionary data. It also verifies generated-index
creation and reuse, source-stamp invalidation for either data representation,
checksum corruption recovery, temporary-file cleanup, and rejection of a
directory used as an index-file target.

`stardict_dictionary_test` verifies the private backend contract and StarDict
adapter: identity and provenance, bounded exact results, cancellation,
deadlines, bounded prefix results, translated format errors, raw formatted
article preservation, and typed resources. Resource checks cover legacy
delimiters, missing files, traversal and absolute paths, symlink escapes,
oversized data, and cancellation.

`stardict_discovery_test` verifies recursive and explicit-file discovery,
stable deduplication, unrelated-file filtering, and partial results when a
configured dictionary root is missing.

`article_assembler_test` verifies browser-independent plain-text and HTML
assembly, a strict formatting allowlist, active-content and event-attribute
removal, inert malformed-markup fallback, bounded document size, and canonical
typed lookup and resource URLs. It also verifies that unsafe resource paths and
non-internal navigation are not emitted into rendered HTML.

`text_encoding_test` verifies strict bounded conversion between UTF-8 and the
representative legacy encodings Latin-1, UTF-16LE, GB18030, and EUC-JP. It also
rejects malformed byte sequences, malformed UTF-8, unrepresentable target
characters, unknown or oversized encoding names, and output-limit violations.

`dictd_discovery_test`, `dictd_reader_test`, and `dictd_dictionary_test` use
generated `.index` and data fixtures. They verify companion discovery,
base-64 offset/size parsing, the optional original-headword column,
`00databaseshort` naming, Unicode-folded ranking, distinct suggestions, plain
and gzip/dictzip-compatible data, scan checkpoints, cancellation, malformed
indexes, corrupt compression, and out-of-range article rejection.
`application_service_test` also verifies that Dictd and StarDict coexist behind
the same catalog and lookup facade.

`sdict_discovery_test`, `sdict_reader_test`, and `sdict_dictionary_test` use a
generated packed `.dct` container. They verify recursive discovery, title and
language metadata, little-endian index/article ranges, Unicode-folded ranking,
suggestions, plain/zlib/bzip2 fields, markup and typed word-reference
conversion, scan checkpoints, cancellation, invalid signatures, and truncated
article rejection. The application service and installed headless consumer
also verify sanitized SDict HTML through the format-neutral facade.

`xdxf_discovery_test`, `xdxf_reader_test`, and `xdxf_dictionary_test` generate
minimal XDXF XML and gzip-compatible `.xdxf.dz` fixtures. They verify recursive
discovery, metadata and aliases, Unicode-folded ranking, bounded gzip input,
standard document types, malformed XML and UTF-8 rejection, safe markup and
word-link conversion, cancellation, confined resource paths, and bounded image
resource loading. The application service test verifies sanitized HTML and
typed resource retrieval through the format-neutral facade.

`zipsounds_discovery_test`, `zipsounds_reader_test`, and
`zipsounds_dictionary_test` generate legal classic ZIP archives with stored and
raw-deflated audio entries. They verify recursive `.zips` discovery, folded
lookup and suggestions, safe nested member names, bounded decompression,
CRC-32 rejection, typed audio resources, and sanitized HTML5 playback. The
application service test verifies discovery, article assembly, MIME typing,
and resource retrieval through the format-neutral facade. Dictionary and
reader tests also verify bounded exact-unique enumeration of safe derived
member paths without extracting or checksum-validating audio members.

`sounddir_reader_test` and `sounddir_dictionary_test` generate nested regular
audio files under an explicitly configured root. They verify filename-based
headwords, recursive indexing without treating unrelated files as entries,
folded lookup and suggestions, configured identity, bounded confined resource
reads, MIME typing, and empty-directory rejection. Configuration and
application-service tests verify path/name persistence and end-to-end HTML5
playback resource retrieval. Enumeration coverage pins basename-only derived
headwords, exact collision handling, hidden-file inclusion, symlink exclusion,
and reuse of the open-time index without a filesystem rewalk.

`application_service_test` also pins the first legacy configuration migration
slice. It imports dictionary paths and named sound directories from bounded
legacy XML only when the new configuration is absent, verifies that a current
configuration always wins, rejects entity declarations and malformed input,
persists the new format atomically, and proves that the legacy source remains
unchanged after both success and failure.

`application_service_test` also verifies the current dictionary-group model:
empty older configurations remain compatible, nontrivial ordered groups round
trip, group and membership bounds plus duplicate IDs are rejected
deterministically, malformed fields fail to load, and a rejected save leaves
the previous configuration unchanged without a temporary file.

The same test pins the public local-source validation seam: both 256-entry
bounds, empty sound-path and NUL rejection, duplicate and empty-name
acceptance, exact ordering, and atomic rejected-save behavior.
The same service test verifies the single-dictionary enumeration contract:
legacy-compatible unique ordering, bounded continuation, cursor integrity and
snapshot binding, cancellation, and deterministic unsupported results.
Focused export coverage verifies multi-page UTF-8 BOM/LF output, exact ordering
and duplicate semantics, empty dictionaries, cancellation/deadlines, error
translation, collision-safe sibling temporary files, cleanup, and preservation
of an existing destination on every unsuccessful path.
Dictionary tests for StarDict, the 11 R3a.2 article formats, and the three
R3a.3 audio formats pin capability publication and extraction from natural
records, including aliases, redirects, reserved Dictd entries, and derived
audio names. LSA coverage pins case-insensitive `.wav` removal without Vorbis
decoding. The installed headless consumer pins the unchanged exported DTO and
virtual API, including sound-directory enumeration.
`application_service_test` also pins the Phase 8 P4a current online-source
store. Ordered MediaWiki, website, Forvo, and DICT DTOs round-trip through
canonical v1 records; older current files receive the disabled ordered
`en,ru` Forvo default. Bounds, global duplicate IDs, malformed UTF-8 and
controls, URL/template/host/port/atom rules, duplicate languages, exact output,
and atomic rejected-save preservation are covered. Credentials, external
programs, legacy-source migration, composition, UI, and network calls remain
outside P4a.
Exact empty and nonempty Forvo collection round trips also pin the canonical
presence/count marker, malformed or duplicate marker rejection, and the older
current-file default when the marker and records are both absent.
The P4b coverage in the same test pins exact ordered external-program parent
and argument records, explicit empty and older-current behavior, cross-family
identity uniqueness, adapter-compatible path and template bounds, output-kind
validation, malformed UTF-8 and controls, orphan and ordering rejection, and
atomic rejected-save preservation. Legacy migration, composition, UI, and
process execution remain outside P4b.
P4c coverage performs one complete legacy configuration migration containing
MediaWiki, website, Forvo, DICT, and external-program records. It pins family
order, safe missing and empty defaults, secret and icon exclusion, legacy DICT
defaults, and the exact pinned command-line token grammar. Duplicate
containers or global IDs, malformed values, entities or markup, unsupported
iframe/encoding/list intent, userinfo, bounds, and unsafe or unrepresentable
programs abort without a current or temporary file and leave the legacy bytes
unchanged. A current file still takes precedence without parsing malformed
legacy XML. P4c adds persistence only; P5 composition, network calls, and
process execution remain separate.
`runtime_composition_test` pins the P5a.1 extension seam. It verifies exact
MediaWiki-then-website enabled ordering and configured identities, disabled
omission without DTO mutation, duplicate runtime-ID rejection before service
construction, cancellation before transport activity, and loopback HTTP lookup
through the existing adapters with core-owned HTML sanitization. No public
network service or credential is used.
It also covers the intentionally generic unconfigured-identity seam, null and
empty-ID injection, collisions with local and runtime identities, unchanged DTOs
after disabled omission, atomic failure without a partially returned service,
and pre-cancelled/pre-expired behavior for every method on both wrappers.
Zero result limits are also verified to return without source I/O.
P5a.2 coverage adds four-family ordering, Forvo language-child identities and
configured-source provenance, redacted missing-credential diagnostics, secret
non-disclosure, and atomic malformed/inconsistent credential rejection. Local
HTTP and DICT fixtures exercise Forvo lookup/audio resources and DICT
definitions/suggestions without public network access.
P5a.3 coverage adds exact external-program family ordering and identity,
disabled omission, literal argv and UTF-8 stdin input without a shell,
plain-text/HTML article mapping and sanitization, bounded line-oriented prefix
suggestions, empty resource behavior, and runtime error translation. It also
pins complete desktop-facade composition, tab-session restoration, and failure
preservation of an already active facade.
`goldendict_source_directories_smoke` exercises offscreen remove, reorder,
inline edit, and cancel behavior plus invalid-candidate rollback, forced save
failure, duplicate preservation, successful facade replacement, exact tab
session restoration, and unrelated configuration preservation.
It also covers the P5b online-source presentation: hidden stable IDs,
MediaWiki reorder and enable edits, ordered Forvo language edits, synchronous
apply-failure feedback, retry, exact empty Forvo persistence, complete-facade
replacement, and preservation of unrelated configuration and the article
session. Website and DICT DTO round trips remain pinned by the same atomic
application path, with no credential field or public network request.
P5c coverage in the same smoke pins hidden external-program IDs, enabled and
program ordering, result-kind editing, absolute executable and optional
working-directory staging, ordered `%GDWORD%` and empty argument templates,
core validation feedback, retry after synchronous apply failure, exact empty
collection behavior, and preservation of unrelated configuration and the
article session. The editor never starts a process; shell-free execution and
runtime error behavior remain covered by the focused external and runtime
composition tests.
It also imports ordered legacy groups with favorites folders, shortcuts,
separate muted-ID collections, external icons, and canonical Base64 embedded
icon metadata. Unknown nonempty dictionary IDs remain stable, while malformed,
duplicate, or oversized group input leaves the current destination absent and
the legacy source byte-for-byte unchanged.

`application_service_test` also verifies the current preferences model:
missing and older current files receive deterministic defaults, nontrivial
portable preferences round trip with canonical output, and malformed boolean,
enum, numeric, floating-point, modifier, duplicate, unknown, or over-limit
records and malformed UTF-8 strings are rejected. DTO value equality and
inequality are also pinned. Invalid saves preserve the previous configuration
and leave no temporary file.

The same test now pins bounded legacy preference migration: representative
portable values map exactly into the current DTO, absent values retain current
defaults, and excluded credentials plus tab/layout fields are ignored.
Malformed or duplicate recognized values abort without a current or temporary
file, existing current configuration wins, and the legacy XML remains
byte-for-byte unchanged after success and failure.

T3c coverage pins both tab-opening defaults, equality, canonical current
records, older-file compatibility, and exact strict legacy names. Binary
main-window geometry round-trips through the current store with a 64 KiB
decoded limit; malformed, duplicate, and oversized current or legacy data is
rejected atomically while `mainWindowState` remains ignored.
Current Qt 6 main-window state has an independent 64 KiB current-format bound;
binary round-trip, duplicate and oversized rejection, atomic save failure, and
older-file compatibility are pinned while legacy `mainWindowState` remains
ignored.

`legacy_configuration_location_test` pins R6c with injected synthetic roots
for portable, Linux/Unix, Windows, and macOS. It verifies exact names and
casing, old-directory and portable precedence, no fallback after selection,
current-state precedence without inspecting legacy state, missing and unsafe
candidate handling, malformed-input atomic failure, one-time idempotent
migration, and byte-for-byte source immutability. The test never reads or
writes the executing user's real configuration locations.

R6d coverage extends the same injected resolver to exact lowercase `history`
and `favorites` companions and current `history-v1`/`favorites-v1`
destinations. It pins portable and platform paths, the Linux profile-history
then generic-data fallback, independent current precedence, no unselected
profile probes, unsafe candidate rejection, independent success and failure,
temporary cleanup, repeated-startup idempotency, and byte-for-byte legacy
source immutability. Focused store cases also reject truncated, invalid-UTF-8,
malformed, entity-bearing, and oversized inputs without creating a partial
destination.

`history_store_test` verifies the first user-history migration slice: strict
bounded UTF-8/group-aware current-format round trips, bounded import of the
legacy line format, entry-limit truncation, current-state precedence, atomic
new-format persistence, and recoverable malformed-input failure that leaves
the legacy source untouched.

`goldendict_history_smoke` exercises the application composition path in an
isolated configuration directory. It submits a lookup through the window,
checks that the history dock is refreshed, and reloads the atomically persisted
entry through the public core history API.

`goldendict_dictionary_groups_smoke` uses the checked-in Dictd fixture through
the application composition path. It covers all/configured selection,
group-aware lookup and history restoration, create/rename/reorder/delete,
ordered available and unavailable membership, muted sets, metadata, atomic
persistence, invalid-save rejection, unrelated-field preservation, and
deleted-selection fallback.

`article_tabs_test` verifies the bounded transport-neutral desktop tab model:
initial single-tab compatibility, stable ordered creation and activation,
deterministic close and close-others fallback, exact reuse versus explicit new
tabs, per-tab query/group/title/internal-link state, back/forward restoration
and truncation, and atomic errors for invalid IDs, malformed state, and tab or
navigation limits. It also verifies complete session export and atomic restore,
including ordered histories/cursors, sparse stable IDs, deterministic
collision-free ID continuation, and atomic rejection of duplicate,
inconsistent, invalid, overflowing, or over-limit sessions.
It also verifies append versus after-active placement independently of
foreground/background activation.

`application_service_test` verifies the optional canonical current-config
session block, deterministic round trips, older-file compatibility, atomic
invalid-save behavior, and complete rejection of partial, duplicate,
inconsistent, malformed, or over-limit serialized sessions.

`goldendict_article_tabs_smoke` uses the checked-in Dictd fixture with the real
offscreen Qt Widgets/WebEngine presentation. It verifies single-tab startup,
foreground and background lookup tabs, independent rendered/query/group state,
activation, close/middle-close/close-others and last-tab fallback, current-tab
and background internal links, facade-backed back/forward restoration and
forward truncation, and atomic tab/navigation-limit handling.
It additionally covers configurable default activation, after-current visible
ordering, valid geometry capture/restore, and deterministic Qt rejection
fallback.
It also pins the current dock/toolbar hierarchy, areas, visibility,
transactional state restoration, malformed and oversized fallback, and safe
handling of an off-screen floating-state topology. The two-start restart smoke
verifies that dock and toolbar placement and visibility survive alongside the
article session without changing tab identities.
The shell assertions additionally pin the unique `favoritesPane` and
`historyPane` identities, visible right-side vertical default ordering,
non-tabification, real child widgets, toggle actions, and usable central article
content. They also verify transactional restoration of state captured by the
immediately preceding current Qt 6 dock identities without admitting pinned
legacy version-1 state.

`goldendict_article_context_menu_smoke` verifies the application-private menu
model without opening a real popup. It covers resolved internal links,
allowlisted external links, rejected schemes, bounded exact selections, image
contexts, clipboard output, and action dispatch from a captured context.

`goldendict_system_print_smoke` injects print-dialog, preview, availability,
and renderer fakes. It verifies cancellation, accepted printing, preview paint,
asynchronous success/failure reporting, and the absence of real system dialogs
in automated runs; existing WebEngine interaction coverage continues to pin
HTML and PDF export behavior.

`goldendict_article_tab_session_restart_smoke` launches the real offscreen
application twice against one isolated configuration directory. The first run
persists a facade-owned session on mutation and orderly shutdown; the second
verifies tab order, active identity, complete histories and cursors,
group/internal-link identity, current-entry rendering without duplicate
history, and collision-free next-ID continuation.

The Phase 8 R6b legacy article-session audit requires no migration fixture or
parser test. The pinned legacy configuration and save path contain no persisted
tab, article, navigation-history, or scroll-state representation; legacy
startup instead creates a fresh welcome tab. Existing configuration migration
tests continue to pin current-state precedence, atomic replacement, malformed
input rejection, and source immutability, while the current-session tests above
cover the only persisted article-session format.

`favorites_store_test` verifies hierarchical current-format round trips and
recoverable migration of the legacy favorites XML, including folder ordering,
expansion state, Unicode headwords, current-state precedence, entity and
malformed-input rejection, atomic persistence, and untouched legacy fixtures.

`goldendict_favorites_smoke` creates a root folder, adds a headword inside the
selected folder, renames that nested headword, verifies the updated view path,
adds a sibling, moves it upward and then to the root, removes the moved item and
remaining subtree, and reloads the atomically persisted empty tree through the
public core API in an isolated configuration directory.

`favorites_store_test` also round-trips the legacy-compatible XML transfer
format, including Unicode and XML escaping, and verifies that invalid exports
do not replace an existing destination.

`goldendict_favorites_transfer_smoke` exports a fixture folder through the
application command, removes it, imports the generated XML, checks the restored
tree, and reloads the atomically persisted replacement through the core API.

`favorites_store_test` also verifies arbitrary headword and subtree moves
between nested folders and root, exact pre-removal insertion boundaries,
no-op behavior, expansion and unrelated-order preservation, and rejection of
stale paths, invalid destinations, cycles, duplicate siblings, and root moves
without modifying the persisted file.

`goldendict_favorites_cross_folder_move_smoke` moves a selected expanded
subtree between arbitrary folders and then moves its headword to root through
the presentation command, verifying current selection, expansion, order, and
the atomically reloaded tree.

`goldendict_history_export_smoke` exports a history containing ordinary and
embedded-newline headwords, then verifies the compatibility UTF-8 BOM,
one-headword-per-line sanitization, and exact file contents in an isolated
configuration directory.

`history_store_test` also verifies bounded UTF-8 text import, optional BOM and
whitespace handling, entry limits, and invalid-UTF-8 rejection.

`goldendict_history_import_smoke` imports a compatibility text file through
the application command, checks the refreshed history pane, and reloads the
atomically persisted replacement through the public core API.

`goldendict_dictionary_browser_smoke` loads a small checked-in Dictd fixture,
opens the real dictionary browser offscreen, verifies catalog identity and
source provenance plus backend-owned article/headword counts, performs a
dictionary-filtered prefix query, verifies the copy-source action and the
availability of the containing-folder action, and confirms that activating the
returned headword enters the normal lookup path. It also exercises wildcard
and regular-expression modes, retained current-headword selection, and
deterministic invalid-pattern clearing. Core service tests pin leading-literal
requirements, Unicode and case behavior, ordering, invalid-pattern errors, and
the existing 100-candidate bound.

The same smoke also pins empty language/description presentation and the
read-only description control. Format dictionary tests cover BGL, Dictd, DSL,
EPWING, GLS, SDict, StarDict, and XDXF description provenance; application service
tests verify that catalog languages and descriptions cross the public facade
without affecting lookup identity or ordering.

`goldendict_dictionary_browser_export_smoke` uses the same fixture and real
browser to export the selected dictionary's complete list independently of the
displayed prefix results, then verifies the compatibility UTF-8 BOM,
enumeration order, one-headword-per-line format, and exact file contents in an
isolated configuration directory.

`goldendict_history_management_smoke` verifies case-insensitive live history
filtering, sends the clear command through the real application composition
path, checks the refreshed empty pane, and reloads the atomically persisted
empty history through the public core API.

Use `ctest --preset conan-debug` after a Debug build and
`ctest --preset conan-release` after a Release build. Before considering a
change complete, prefer Release tests unless the change is Debug-specific or
Release cannot be built locally.

## Full Verification

Before considering a change complete, agents should prefer running the smallest
relevant verification command that is already documented or confirmed by the
project owner.

Preferred full Windows Release verification workflow:

```sh
conan install . --build=missing -s build_type=Release
cmake --fresh --preset conan-default
cmake --build --preset conan-release
ctest --preset conan-release --output-on-failure
cmake --install build --config Release
```

Preferred full Linux Release verification workflow:

```sh
conan export conan/recipes/python-html5lib
conan install . --build=missing -s build_type=Release \
  -pr:h=profiles/qt-webengine -pr:b=default
cmake --fresh --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release --output-on-failure
cmake --install build/Release
```

Run the full workflow after changes to CMake, Conan, modules, applications,
tests, install behavior, or dependency configuration. For
documentation-only changes, a full build is not required. If the full workflow
is skipped, mention why in the final response or pull request notes.

When install verification includes a runtime-dependency self-contained smoke
test, run the installed executable from a clean environment. On Linux, keep only
the normal system command path with `PATH=/usr/bin:/bin`. On Windows, clear
`Path`.

## Test Framework Policy

- Use QTest for sample and project tests.
- Qt is expected as a dependency for all tests.

## Expected Verification Areas

- configure;
- build;
- test, if tests exist;
- package verification through `test_package/`, if applicable;
- formatting, if formatting is changed.

## Pre-PR Verification Checklist

- Run the relevant Debug or Release build workflow, with Release preferred
  before completion.
- Run the relevant `ctest` preset when tests exist.
- Run install verification when install or package behavior changes.
- Run `conan install` after dependency changes.

See [agent-workflow.md](agent-workflow.md) for the full pre-PR checklist and
pull request notes policy.
