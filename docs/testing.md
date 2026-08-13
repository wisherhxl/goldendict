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
and resource retrieval through the format-neutral facade.

`sounddir_reader_test` and `sounddir_dictionary_test` generate nested regular
audio files under an explicitly configured root. They verify filename-based
headwords, recursive indexing without treating unrelated files as entries,
folded lookup and suggestions, configured identity, bounded confined resource
reads, MIME typing, and empty-directory rejection. Configuration and
application-service tests verify path/name persistence and end-to-end HTML5
playback resource retrieval.

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
navigation limits.

`goldendict_article_tabs_smoke` uses the checked-in Dictd fixture with the real
offscreen Qt Widgets/WebEngine presentation. It verifies single-tab startup,
foreground and background lookup tabs, independent rendered/query/group state,
activation, close/middle-close/close-others and last-tab fallback, current-tab
and background internal links, facade-backed back/forward restoration and
forward truncation, and atomic tab/navigation-limit handling.

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
returned headword enters the normal lookup path.

`goldendict_dictionary_browser_export_smoke` uses the same fixture and real
dialog to export the displayed prefix results, then verifies the compatibility
UTF-8 BOM, deterministic suggestion order, one-headword-per-line format, and
exact file contents in an isolated configuration directory.

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
