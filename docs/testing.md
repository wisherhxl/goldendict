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
It also pins explicit dictionary participation for lookup and suggestions:
inactive empty filters retain the all/group default, active empty filters
complete successfully without results, and nonempty filters preserve group
intersection, catalog order, duplicate normalization, missing-ID diagnostics,
and cancellation behavior. The installed headless consumer compiles and runs
the active-empty contract through the exported request DTOs.

`goldendict_dictionary_bar_smoke` pins the Widgets projection of that contract:
the real `dictionaryBar` identity, catalog/group ordering, accessible text-only
actions, group-scoped ephemeral checks, imported muting baselines, all-off
lookup and suggestions, hidden-toolbar fallback, and catalog-backed action
identity. The existing article-tabs smoke additionally pins active/background
request isolation and the private version-7 round trip plus transactional
version 6 through 2 compatibility and version-1 rejection.

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
`http_client_test` also pins the Phase 7 Qt Network cache owner. Loopback
coverage verifies cache hits through the shared manager, exact MiB conversion,
the injected owned path, retained clean-restart persistence, zero-limit
eviction, clear-on-exit isolation from a WebEngine sentinel, uncached
degradation after redacted setup failure, and cancellation/join before owned
directory cleanup. Preparation does not alter an active runtime; activation
is the point where reducing or disabling the limit may irreversibly discard
disposable cache bytes.
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
`goldendict_escape_hides_main_window_preferences_smoke` pins the existing
default-off preference's canonical current round trip and strict legacy name,
the exact General checkbox, unchanged disabled handling, focused query and
article fallback, child and modal precedence, and cancel/failure preservation.
Its isolated second process reloads the successful configuration and proves
that enabled unconsumed ESC continues to hide only the main window after
restart.
`goldendict_history_menu_smoke` pins the unique `menuHistory` placement and
legacy action identities, order, separator, roles, shortcuts, and exact reuse
of the `historyPane` toggle action. Injected import/export paths exercise the
same actions used by the pane buttons, selected-group import, cancellation,
single dispatch, empty/nonempty/busy enablement, pane synchronization, atomic
compatibility export, unchanged private state version, and usable central
content. Existing history management, import, export, activation, and core
store tests continue to cover persisted success and failure semantics.
`goldendict_favorites_menu_smoke` pins the unique `menuFavorites` placement,
supported legacy order and separator, action identities, roles, shortcuts,
and exact reuse of the `favoritesPane` toggle and article-toolbar XML
transfer/Add actions. It verifies unique shortcut ownership, single dispatch,
pane synchronization, query/tree/empty/busy enablement, selected-folder Add
targeting, canonical selected-item removal, dialog cancellation, malformed
import and failed-export preservation, successful atomic XML transfer, and
unchanged query, central article usability, and private state version. The
unsupported legacy plain-list export and a menu-only removal proxy are
explicitly absent; existing Favorites management, transfer, move, and core
store tests remain authoritative for persistence and validation semantics.
`goldendict_help_menu_smoke` pins the unique trailing `menu_Help`, supported
legacy action identities, relative order, separators, roles, empty shortcuts,
and single dispatch. Its private dispatcher captures the exact HTTPS homepage
and current configuration-directory targets without opening them, and rejects
HTTP, credential-bearing, unrelated-path, JavaScript, and unapproved HTTPS
targets. The smoke inspects and safely closes the parent-owned modal About
dialog, verifies build/runtime product, version, Qt, and license content, and
confirms unchanged central, article-tab, query, and private layout state. The
unsupported offline reference, F1 shortcut, Forum, About Qt, and updater
surfaces are explicitly absent.
`goldendict_file_menu_smoke` pins the unique leading `menuFile`, exact supported
legacy identities, order, separators, roles, shortcuts, and single shortcut
ownership. It verifies exact action reuse by the tab control, article toolbar,
and menu; preference-backed new-tab and orderly-quit dispatch; print
cancellation, failure, and busy exclusion; HTML-save cancellation and atomic
failure preservation; unchanged facade session on cancelled or failed commands;
usable central content; and unchanged private Qt state version 7. Page setup, rescan,
close-to-tray, and toolbar-only PDF export are intentionally absent from the
menu because they do not form supported pinned File-menu entries.
`goldendict_edit_menu_smoke` pins the unique `menu_Edit` between View and
History and the sole supported legacy `dictionaries` identity, text, F3
shortcut, role, and separator-free order. It verifies exact QAction reuse by
the source button and menu, unique shortcut ownership, single dispatch, modal
busy/reentrant exclusion, cancellation and application-failure preservation,
successful acceptance, unchanged facade session and private Qt state, and
continued central usability. The dependent Preferences coverage pins the
canonical action identity, text, F4 shortcut, role, and order, plus the bounded
General/Tabs controls. It verifies cancel/no mutation, reentrant exclusion,
failed apply preservation, successful complete-candidate submission, and
continued session/layout usability; existing core configuration tests remain
the source of truth for validation and atomic replacement semantics.
The General/History extension adds exact offscreen checks for the legacy
checkbox and always-enabled `0..99999` spin box, cancel/failure preservation,
and successful complete-candidate application.
`goldendict_history_preferences_smoke` verifies newest-first trimming, disabled
future recording, bounded replacement import while recording is disabled,
re-enabled recording, persisted history/configuration, and an unchanged article
session. `history_store_test` pins zero, legacy-maximum, and out-of-range import
limits; configuration tests reject values above the exposed maximum atomically.
The General/Favorites extension pins the exact legacy confirmation control and
the intentional absence of the unbacked save interval. The deterministic
`goldendict_favorites_preferences_smoke` verifies rejected word and folder
removal, accepted subtree removal, confirmation-disabled removal, unchanged
selected-folder add targeting, persisted configuration/favorites, and an
unchanged article session and layout. Core configuration tests cover the
default, current round trip, strict legacy boolean migration, and value
comparison; existing favorites and menu smokes retain atomic failure,
selection/expansion, transfer, ordering, and canonical-action coverage.
The General/Articles extension pins the exact backed checkbox, tooltips,
`1..100000` spin-box range, 50-symbol step, 2000-symbol default, dependent
enabled state, and the absence of unrelated appearance controls.
`goldendict_articles_preferences_smoke` verifies cancellation, forced apply
failure, successful complete-candidate persistence, unchanged article session
and layout, and facade replacement under isolated offscreen Chromium. Core
article-composer tests pin the strict threshold, multi-result-only collapse,
single-result exemption, trusted wrapper markup, print expansion rule,
sanitized content preservation, ordering, and size bound. Existing WebEngine,
search, context-menu, print/export, session, and configuration suites cover
the preserved surrounding behavior.
The General/Input phrase length extension pins the backed checkbox, tooltip,
`1..1000000` spin-box range, 10-symbol step, 1000-symbol default, dependent
enabled state, and complete-candidate cancel/failure/success behavior. Core
application tests cover Unicode scalar counting across ASCII, supplementary
characters, and combining sequences; exact-boundary, disabled, lookup,
suggestion, asynchronous, navigation, and atomic session/configuration
rejection behavior. The offscreen Preferences smoke verifies rejected input
does not submit history or mutate the article session and reports the
configured limit while clearing suggestions.

The General/Ignore diacritics extension pins default-off persistence and
legacy migration, the NFC/case-fold/NFD/mark-removal comparison pipeline,
diacritic-sensitive disabled lookup, enabled local collision matching, and the
public runtime capability/request contract. Unsupported runtime sources report
an explicit per-source error. Prefix suggestions and headword enumeration are
unchanged. The offscreen Preferences smoke covers cancel, failed apply,
successful persistence, session preservation, and the backed checkbox.
The General/Extra search via synonyms extension pins the legacy default-on
checkbox, tooltip, cancel/failure/success transaction, and persistence. Core
fixtures verify disabled same-dictionary alias behavior, enabled
synonym-to-primary expansion across participating dictionaries, result
deduplication, original requested text, and unchanged suggestions. StarDict
reader coverage validates bounded `.syn` parsing and primary mappings across
private generated-index creation and reuse; malformed or stale source data
cannot be silently accepted.
The General/Expand optional parts extension pins default-off persistence,
strict legacy migration, exact checkbox and transaction behavior, bounded DSL
semantic markers, sanitizer isolation, disabled article-level expansion,
enabled visibility, and the legacy large-article threshold interaction.
`goldendict_optional_parts_preferences_smoke` covers cancel, failed apply,
successful persistence, reopen, complete session/layout preservation, and the
backed checkbox. DSL reader, article assembler, and article composer tests pin
the rendering behavior and unchanged structured plain text.

The Phase 8 Preferences completeness audit leaves the overall Preferences gate
open. P8-PREF-1 and P8-PREF-2 are accepted through current/legacy configuration
coverage, the installed consumer, and their focused offscreen smokes. The
`goldendict_hide_single_tab_preferences_smoke`
smoke pins the checkbox text and tooltip, cancel and forced-failure rollback,
successful persistence, one/multiple-tab visibility, complete session/layout
preservation, and reload through the startup preference path. Remaining ready
is joined by `goldendict_mru_tab_order_preferences_smoke`, which pins the exact
default-off checkbox without a tooltip, positional disabled traversal,
chord-stable symmetric forward/reverse MRU traversal, the existing 32-tab
capacity as the sole MRU bound, create/close/restore cleanup, cancel and forced
failure preservation, successful atomic application, persisted tab-order
preservation, and deterministic restart reconstruction. Remaining ready leaves
use the existing core configuration tests and one focused GUI/runtime smoke
each:

- P8-PREF-3 verifies the existing escape preference through query, article,
  child, and modal focus paths, including disabled behavior, cancel/failure,
  successful hiding, and restart.
- P8-PREF-4 is accepted by current/legacy configuration coverage and
  `goldendict_article_click_preferences_smoke`. Its isolated WebEngine view
  covers all four double-click-translation/single-click-selection combinations,
  the pinned controls, the 60-UTF-16-unit bound, link/input exclusions,
  exactly-once dispatch, cancel/failure preservation, unchanged security,
  history and tabs, and a second-process restart.
- P8-PREF-5 combines configuration/migration cases, a loopback origin/proxy
  runtime-composition test, and an offscreen dialog smoke to prove strict HTTP
  CONNECT host/port validation, direct versus proxied traffic, atomic facade
  preservation, secret-free diagnostics/persistence, and restart.

P8-PREF-5 is accepted. `dict_server_source_test` additionally pins its private
bounded CONNECT handshake, optional test-only Basic authentication, 407
mapping, tunneled response buffering, deadline/cancellation inheritance, and
credential/endpoint redaction. Runtime composition injects the credential-free
candidate into MediaWiki, website, Forvo, and DICT without affecting external
programs, WebEngine, system proxy policy, or application-global proxy state.
The first seven P8-PREF leaves are complete; remaining controls retain their
named prerequisites until their separately audited owners are ready. The
post-navigation completeness audit finds no independently ready next
Preferences leaf.

The Phase 7 bounded per-dictionary article-context navigation prerequisite is
accepted. `goldendict_dictionary_context_navigation_smoke` pins first-result
ordering, repeated-dictionary deduplication, catalog-name/ID fallback, the
default 20-entry bound, the pinned `.........` overflow handoff to the existing
results pane, cleared/non-lookup absence, stale presentation/document
invalidation, and closed-origin safety. The guarded app path also requires the
originating tab, view, and presentation generation to remain current. Existing
article-context-menu and article-tabs coverage continues to
pin link, selection, copy, image, tab scoping, and results-pane behavior. This
coverage now shares the accepted P8-PREF-7 runtime owner.

The selected next migration leaf is the separate Phase 5 bounded full-text
indexing and query contract. Its focused core coverage must use a generated
reference corpus to pin index creation, reuse, stale rebuild, and corrupt-index
rejection; whole-word, plain-text, wildcard, and regular-expression modes;
case, diacritic, word-order, and word-distance policy; dictionary and result
limits; deterministic provenance and bounded match metadata; and cancellation,
deadlines, malformed queries, and oversized input. The installed headless
consumer must cover any public contract evolution. This prerequisite adds no
format adapter, full-text workflow, Preferences widget, or presentation
placeholder; those remain separately gated Phase 6 and Phase 8 work.

`full_text_index_test` supplies that generated reference corpus. It covers
atomic creation, reuse, stale and corrupt rebuilds; whole-word, plain-text,
wildcard and regular-expression matching; case/diacritic folding,
order/distance and dictionary filtering; deterministic provenance and result
bounds; malformed patterns, oversize work, cancellation, and deadlines. The
application service retains typed unsupported results until Phase 6 adapters
provide private ingestion, while `headless_api_test` compiles and runs the new
installed operation and its active-empty filter behavior.

P6-FT-1 extends the generated StarDict fixture rather than adding downloaded
data. Focused StarDict coverage proves that plain and HTML articles yield
searchable assembled plain text;
primary records produce stable dictionary, headword, and record-derived
document provenance; synonym aliases do not duplicate documents; and source
changes or corruption produce create, reuse, stale-rebuild, and corrupt-rebuild
states for the distinct private full-text artifact. Application-service tests
exercise installed `SearchFullText` filtering, deterministic bounds,
cancellation and deadlines, successful StarDict results alongside typed
unsupported errors from non-adapted formats, and unavailable requested IDs.
The installed consumer demonstrates StarDict full-text results without any
public contract change. No GUI, Preferences, highlighting, other-format,
resource-text, metadata-text, or legacy-index coverage belongs to this leaf.

P6-FT-2 extends the generated SDict fixtures without downloaded data. Focused
reader/backend coverage exercises plain, zlib, and bzip2 articles,
converted markup and word links, complete full-index traversal, and multiple
headwords sharing one article offset. It proves one document per distinct
article offset, first-record canonical headword selection, stable
record-ordinal/article-offset document provenance, searchable bounded plain
text from the existing article assembler, and exclusion of metadata, link
targets, resources, and raw markup. The distinct private `.gdfts` artifact
covers create, reuse, `.dct`-stamp stale rebuild, corrupt rebuild, disabled
indexing when no index directory is configured, resource limits, cancellation,
deadlines, and contained storage failures.

Application-service coverage combines generated StarDict and SDict
fixtures and pin deterministic merged ordering, dictionary filters, the global
result bound, successful adapted dictionaries with no matches, typed
unsupported errors for every requested non-adapted local or runtime source,
typed unavailable errors for missing requested IDs, cancellation, and
deadlines. The installed consumer returns SDict results through the
unchanged installed `SearchFullText` API and DTOs. The later implementation
gate is the Linux Release configure/build and `ctest --preset conan-release`,
library install and installed `test_package` consumer, then clean committed
exact-SCM `conan create` with the Release Qt WebEngine host profile. GUI,
Preferences, highlighting, other adapters, legacy `_FTS` indexes, metadata or
resource text, dependency changes, and index-format evolution are excluded.

P6-FT-3 extends the generated XDXF fixtures without downloaded data. Focused
reader/backend coverage exercises plain and `.xdxf.dz` sources, complete
article/key traversal, multiple keys sharing one article, multiple articles,
safe markup and links, and source mutation. It proves one document per
validated article ordinal, first-key canonical headword selection, stable
XDXF-prefixed provenance derived from first-key record ordinal plus article
ordinal, and searchable bounded plain text from the existing inert assembler.
It also proves that metadata, resource contents and paths, link targets, image
references, raw XML and markup, and alias-only text are excluded.

The XDXF private artifact tests cover create, reuse, source-stamp stale rebuild,
corrupt rebuild, disabled indexing without a configured index directory,
resource limits, cancellation, deadlines, and contained storage failures.
Application-service coverage combines generated StarDict, SDict, and XDXF
fixtures and pins deterministic merged ordering, dictionary filters, the global
result bound, adapted no-match behavior, typed unsupported errors for every
requested non-adapted local or runtime source, typed unavailable errors for
missing requested IDs, cancellation, and deadlines. The installed consumer
returns an XDXF result through the unchanged installed `SearchFullText` API and
DTOs.

The implementation gate is the Linux Release configure/build and
`ctest --preset conan-release`, library install and installed `test_package`
consumer, then clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile. GUI, Preferences, highlighting, other adapters, legacy
`_FTS` indexes, metadata or resource indexing, dependency changes, and private
index-format evolution are excluded.

P6-FT-4 extends the generated Dictd fixtures without downloaded data. Focused
reader/backend coverage exercises plain and `.dict.dz` companions, complete
source-order index traversal, optional original-headword aliases, repeated
article ranges, reserved metadata records, article-range corruption, and
source mutation. It proves one document per distinct validated non-metadata
`(article_offset, article_size)` range, first-record canonical headword
selection, exact `dictd-index:<record-ordinal>:<article-offset>:<article-size>`
provenance, and bounded plain text from the existing inert assembler. Reserved
metadata, alias-only text, resource data, and generated link or markup
interpretation are excluded.

The Dictd private artifact tests cover create, reuse, `.index` and selected
data-companion stale rebuild, plain/dictzip selection changes, corrupt rebuild,
disabled indexing without a configured index directory, resource limits,
cancellation, deadlines, and contained storage failures. Application-service
coverage combines generated StarDict, SDict, XDXF, and Dictd fixtures and pins
deterministic merged ordering, dictionary filters, the global result bound,
adapted no-match behavior, typed unsupported errors for every requested
non-adapted local or runtime source, typed unavailable errors for missing IDs,
cancellation, and deadlines. The installed consumer returns a Dictd result
through the unchanged installed `SearchFullText` API and DTOs.

The implementation gate is the Linux Release configure/build and
`ctest --preset conan-release`, library install and installed `test_package`
consumer, then clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile. GUI, Preferences, highlighting, other adapters, legacy
`_FTS` compatibility, metadata or resource indexing, dependency changes, and
private index-format evolution are excluded.

P6-FT-4 is accepted with the focused generated-fixture, lifecycle,
contained-failure, mixed-service, Release build/install/test, exact-SCM Conan
package, and installed C/C++ consumer coverage described above. The required
fresh independent readiness audit selects P6-FT-5 below.

P6-FT-5 extends the generated GLS fixtures without downloaded data. Focused
reader/backend coverage exercises plain and `.gls.dz` sources, supported
UTF-8/UTF-16 decoding, complete source-order record and article traversal,
multiple articles, pipe-separated aliases, safe markup, links and images,
resource exclusion, malformed input, and source mutation. It proves one
document per validated article ordinal, first-record canonical primary
headword selection, exact
`gls-index:<first-record-ordinal>:<article-ordinal>` provenance, and bounded
plain text from the existing inert assembler. Alias-only text, glossary
metadata, resource paths and bytes, image/link targets, raw markup, `.files`
contents, and future resource ZIP contents are excluded.

The GLS private artifact tests cover create, reuse, source-stamp stale rebuild,
plain/compressed discovery changes, corrupt rebuild, disabled indexing without
a configured index directory, resource limits, cancellation, deadlines, and
contained storage failures. Resource-only changes must not stale the artifact.
Application-service coverage combines generated StarDict, Dictd, SDict, XDXF,
and GLS fixtures and pins existing backend ordering, dictionary filters, the
global result bound, adapted no-match behavior, typed unsupported errors for
every requested non-adapted local or runtime source, typed unavailable errors
for missing IDs, cancellation, and deadlines. The installed consumer returns
a GLS result through the unchanged installed `SearchFullText` API and DTOs.

The implementation gate is the Linux Release configure/build and
`ctest --preset conan-release`, library install and installed `test_package`
consumer, then clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile. GUI, Preferences, highlighting, other adapters, legacy
`_FTS` compatibility, metadata or resource indexing, dependency changes, and
private index-format evolution are excluded. No later adapter is preselected.

P6-FT-5 is accepted with generated encoding, compressed-source, alias,
inert-assembly, lifecycle, contained-failure, mixed-service, Release
build/install/test, exact-SCM package, and installed-consumer coverage. No
successor is selected.

P6-FT-6 Aard is accepted. Focused Aard reader/backend coverage extends the
generated fixture without downloads and
exercises 32/64-bit indexes, raw/zlib/bzip2 articles, complete record/article
traversal, aliases, redirect-only articles, safe links, malformed input, and
source mutation. It proves one document per unique article ordinal,
first-record canonical ownership, exact
`aard-index:<first-record-ordinal>:<article-ordinal>` provenance, and bounded
plain text from inert assembly of the migrated `text/html`. Metadata,
alias-only text, link targets, raw markup, icons, resources, and future
multi-volume data remain excluded.

The private artifact coverage must prove create, reuse, sole-`.aar` source
stale rebuild, replacement stale rebuild, corrupt rebuild, typed unsupported
without an index directory, resource limits, cancellation, deadlines, and
contained storage failures. Application-service coverage combines generated
StarDict, Dictd, SDict, XDXF, GLS, and Aard fixtures and pins stable
dictionary-ID ordering, filters, the global bound, adapted no-match behavior,
typed unsupported errors for requested non-adapted local/runtime sources,
typed unavailable errors for missing IDs, cancellation, and deadlines. The
installed consumer returns Aard through the unchanged `SearchFullText` API and
DTOs.

The acceptance gate is Linux Release configure/build and
`ctest --preset conan-release`, library install and installed `test_package`
consumer, then clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile.
Dependencies, GUI, Preferences, highlighting, other adapters, legacy `_FTS`
compatibility, metadata/resource indexing, and private index evolution are
excluded. No leaf after P6-FT-6 is selected.

The selected P6-FT-7 DSL implementation leaf extends only generated fixtures.
Reader/backend tests must prove complete source-order article and headword
traversal, one document per article ordinal, first-expanded-record canonical
ownership, optional/tilde/alternate-headword alias deduplication, exact
`dsl-index:<first-record-ordinal>:<article-ordinal>` provenance, multiple
articles, UTF-8 and UTF-16 decoding, and plain and gzip sources. Inert-assembly
checks must accept visible article text while excluding directives,
annotations, alias-only text, resource paths and bytes, link/image targets,
raw DSL/HTML markup, abbreviation dictionaries, future resource ZIPs, and
unsupported nested cards.

Private lifecycle tests must cover create, reuse, selected-source mutation and
replacement, switching between `.dsl` and `.dsl.dz`, corrupt rebuild, typed
unsupported without an index directory, resource limits, cancellation,
deadlines, and contained storage failures. They must also prove that `.ann`
and `.files` changes do not stale the article-text index. Application-service
coverage combines StarDict, SDict, XDXF, Dictd, GLS, Aard, and DSL fixtures and
pins stable dictionary-ID ordering, filtering, the global bound, adapted
no-match behavior, typed unsupported for requested non-adapted local/runtime
sources, typed unavailable for missing IDs, and contained failures. The
installed consumer returns DSL through unchanged `SearchFullText` APIs and
DTOs.

The implementation acceptance gate is Linux Release configure/build and
`ctest --preset conan-release`, install plus the installed `test_package`
consumer, and clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile. Dependencies, GUI, Preferences, highlighting, other
adapters, legacy `_FTS` compatibility, metadata/resource indexing, and private
serialization changes are excluded. No leaf after P6-FT-7 is selected.

P6-FT-7 is accepted with generated encoding, compression, expansion,
alias-deduplication, inert-assembly, lifecycle, contained-failure,
seven-format service, Release build/install/package, and installed C/C++
consumer coverage. No successor is selected.

The accepted P6-FT-8 BGL implementation leaf extends only generated fixtures.
Reader/backend tests must prove complete source-order traversal of supported
entry-block layouts, one document per referenced article ordinal, first-
retained-record canonical ownership, primary/alternate alias deduplication,
the empty-nominal-primary case, multiple articles, UTF-8 and representative
non-UTF-8 code pages, gzip/block decoding, and exact
`bgl-index:<first-record-ordinal>:<article-ordinal>` provenance.

Inert-assembly tests accept visible sanitized article text while excluding
metadata, alias-only text, embedded resource names and bytes, image/link
targets, raw blocks or markup, and code-page control data. Lifecycle tests
cover create, reuse, sole-`.bgl` mutation and replacement, embedded-resource-
only stale rebuild, corrupt rebuild, typed unsupported without an index
directory, resource limits, cancellation, deadlines, and contained storage
failures.

Application-service coverage combines StarDict, SDict, XDXF, Dictd, GLS,
Aard, DSL, and BGL fixtures and pins stable dictionary-ID ordering, filtering,
the global bound, adapted no-match behavior, typed unsupported for requested
non-adapted local/runtime sources, typed unavailable for missing IDs, and
contained failures. The installed consumer returns BGL through unchanged
`SearchFullText` APIs and DTOs.

The implementation acceptance gate is Linux Release configure/build and
`ctest --preset conan-release`, install plus the installed `test_package`
consumer, and clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile. Dependencies, GUI, Preferences, highlighting, other
adapters, legacy `_FTS` compatibility, metadata/resource indexing, and private
serialization changes are excluded. No leaf after P6-FT-8 is selected or
ranked.

The selected P6-FT-9 SLOB implementation leaf extends only generated fixtures.
Reader/backend tests must prove complete source-order reference traversal,
retention of only `text/html*` and `text/plain*` content types, zero-based
retained-textual `record_ordinal` assignment with interleaved excluded
resources, and one document per distinct `(item_index, bin_index)` pair. They
pin first-reference canonical ownership, zero-based first-encounter
`article_ordinal` assignment, shared-pair alias deduplication, distinct bins in
one item, identical bin indices in different items, repeated identical pairs,
and exact
`slob-index:<first-record-ordinal>:<article-ordinal>:<item-index>:<bin-index>`
provenance with canonical unsigned base-10 components, including multi-digit
values.

Materialization tests cover declared encodings and raw, zlib, and bzip2 stores;
accept visible sanitized HTML and escaped plain text; and exclude metadata,
alias-only text, non-text bins, resource names and bytes, icons, raw markup,
and advanced conversion. Lifecycle tests cover create, reuse, sole-`.slob`
mutation and replacement, excluded-resource-only stale rebuild, corrupt
rebuild, typed unsupported without an index directory, limits, cancellation,
deadlines, and contained storage failures.

Application-service coverage combines the existing eight accepted adapters
with SLOB and pins stable dictionary-ID ordering, filtering, the global bound,
adapted no-match behavior, typed unsupported for requested non-adapted local
or runtime sources, typed unavailable for missing IDs, and contained failures.
The installed consumer returns SLOB through unchanged `SearchFullText` APIs
and DTOs.

The implementation acceptance gate remains Linux Release configure/build and
`ctest --preset conan-release`, install plus the installed `test_package`
consumer, and clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile. Dependencies, GUI, Preferences, highlighting, other
adapters, legacy `_FTS` compatibility, metadata/resource indexing, and private
serialization changes are excluded. No leaf after P6-FT-9 is selected or
ranked.

P6-FT-9 is complete with generated SLOB ownership/materialization fixtures,
private index lifecycle and failure coverage, nine-format mixed-service
coverage, and installed C++ full-text consumption. The registered suite
remains 103 tests because coverage extends existing test executables; the
installed C consumer remains unchanged and passing.

The P6-FT-10 ZIM implementation extends only generated fixtures.
Reader/backend tests must prove complete directory-table traversal, old
namespace `A` and ZIM 6.1+ namespace `C` eligibility, terminal `text/html*` or
`text/plain*` filtering, interleaved exclusions, and bounded direct and chained
redirect resolution. They pin zero-based retained-source `record_ordinal`
assignment, terminal-entry deduplication, first-source title-or-URL ownership,
zero-based first-encounter `article_ordinal`, shared-target aliases, distinct
targets with related cluster/blob coordinates, redirect cycles and invalid
targets, and exact
`zim-index:<first-record-ordinal>:<article-ordinal>:<target-entry-index>:<cluster-index>:<blob-index>`
provenance. Every component is canonical unsigned base-10 without signs or
padding, including multi-digit cases.

Materialization tests cover validated UTF-8 HTML, escaped plain text, raw,
zlib, and bzip2 clusters, and 32/64-bit blob offsets. They exclude metadata,
alias-only text, resource names and bytes, non-text blobs, icons, raw markup,
unsupported LZMA2/Zstd or other conversion, and new link rewriting. Lifecycle
tests cover create, reuse, sole `.zim` revision, the complete ordered
consecutive `.zimaa` split set, mutation or replacement of every part, part
addition/removal, resource-only stale rebuild, corrupt rebuild, typed
unsupported without an index directory, limits, cancellation, deadlines, and
contained storage failures.

Application-service coverage combines the nine accepted adapters with ZIM and
pins stable dictionary-ID ordering, filtering, the global bound, adapted
no-match behavior, typed unsupported for requested non-adapted local or
runtime sources, typed unavailable for missing IDs, and contained failures.
The installed consumer returns ZIM through unchanged `SearchFullText` APIs and
DTOs.

The implementation acceptance gate remains Linux Release configure/build and
`ctest --preset conan-release`, install plus the installed `test_package`
consumer, and clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile. Dependencies, GUI, Preferences, highlighting, other
adapters, legacy `_FTS` compatibility, metadata/resource indexing, and private
serialization changes are excluded. No leaf after P6-FT-10 is selected or
ranked.

P6-FT-10 is complete with generated ZIM ownership/materialization fixtures,
complete split-volume index lifecycle and failure coverage, ten-format
mixed-service coverage, and installed C++ exact-provenance consumption. The
registered suite remains 103 tests because coverage extends existing test
executables; the installed C consumer remains unchanged and passing.

P6-FT-11 is complete as only the private MDict ownership prerequisite, not an
adapter. Focused reader and generated-fixture tests prove complete
source-order traversal and zero-based `record_ordinal` assignment before
redirect resolution. Direct, chained, missing-target, and cyclic folded
`@@@LINK=` cases pin explicit resolution outcomes; failed resolutions retain
their source ordinal but yield no terminal document. MDD entries never consume
an ordinal.

Ownership tests deduplicate only by the terminal zero-based MDX key
ordinal plus exact decoded record offset and size. They cover first-resolving
source headword and `first_record_ordinal` ownership, alias retention, folded
headword collisions, duplicate aliases, equal bytes in distinct physical
ranges, zero-based first-encounter `article_ordinal`, empty inert output, and
multi-digit components. The following adapter audit must consume exact
`mdict-index:<first-record-ordinal>:<article-ordinal>:<terminal-key-ordinal>:<record-offset>:<record-size>`
provenance with canonical unsigned base-10 components without signs or
padding.

Materialization-contract tests limit output to the resolved terminal's bounded
decoded/styled HTML and exclude missing/cyclic redirect payloads, metadata,
MDD resource names and bytes, and alias headwords as independent content.
Revision tests pin the ordered MDX, base MDD, and consecutive numbered MDD
set, including mutation or replacement of each member, companion addition and
removal, and resource-only MDD changes. Checkpoint and cancellation coverage
show that traversal and redirect resolution terminate without publishing
a partial immutable view.

The prerequisite acceptance gate remains focused Debug tests, then Linux
Release configure/build and `ctest --preset conan-release`, install plus the
installed `test_package` consumer, and clean committed exact-SCM `conan create`
with the Release Qt WebEngine host profile. The installed consumer and mixed
service remain behaviorally unchanged because neither MDict nor EPWING is
adapted. APIs/DTOs, runtime interfaces, capability flags, configuration,
Preferences, dependencies, GUI, `.gdfts` serialization, legacy `_FTS`,
metadata/resource indexing, highlighting, other adapters, and unrelated
refactors are excluded. No adapter or leaf after P6-FT-11 is selected or
ranked. The registered suite remains 103 tests because the coverage extends
the existing MDict reader and discovery executables.

P6-FT-12 is complete as the private MDict adapter. Generated adapter fixtures
reuse the prerequisite's source-order view and
cover direct, chained, and folded redirects; terminal, missing-target, and
cycle outcomes; folded headword collisions; duplicate aliases; equal bytes at
distinct physical ranges; multi-digit provenance components; and empty or
otherwise excluded materialization. Exact expected provenance is
`mdict-index:<first-record-ordinal>:<article-ordinal>:<terminal-key-ordinal>:<record-offset>:<record-size>`
with canonical unsigned base-10 components.

Lifecycle fixtures cover initial creation and reuse plus mutation or
replacement of the MDX, base MDD, and every consecutive numbered MDD;
companion addition and removal; ordering/topology changes; resource-only MDD
changes; corrupt rebuilds; limits; checkpoints, cancellation, and deadlines;
and contained storage failures. No fixture may treat MDD content as a text
document. Materialization assertions accept only inert text assembled from the
resolved terminal's bounded decoded/styled HTML and reject redirect payloads,
empty output, metadata, MDD resource names/bytes, alias-only documents, and
active markup.

Application-service coverage adds MDict to the ten previously accepted
adapters and pins
eleven-format dictionary-ID ordering, filtering, the global bound, adapted
no-match behavior, typed unsupported for EPWING and requested non-adapted local
or runtime sources, typed unavailable for missing IDs, and contained
per-dictionary failures. The installed C++ consumer must return MDict through
the unchanged `SearchFullText` API and verify exact provenance; the installed
C consumer remains unchanged.

The implementation acceptance gate remains focused generated-fixture tests,
Linux Release configure/build and `ctest --preset conan-release`, install plus
the
installed `test_package` consumer, and clean committed exact-SCM `conan create`
with the Release Qt WebEngine host profile. It changes no API/DTO, runtime
interface, capability, configuration, Preferences, dependency, GUI/Phase 8
behavior, or `.gdfts` serialization and excludes legacy `_FTS`,
metadata/resource indexing, highlighting, other adapters, dependencies, and
unrelated refactors. EPWING
remains unselected and unranked, and no leaf after P6-FT-12 is selected or
ranked. The registered suite remains 103 tests because coverage extends
existing executables; the installed C consumer remains unchanged and passing.

P6-FT-13 focused coverage extends the existing EPWING reader,
discovery, dictionary, and application-service executables. Generated fixtures
must cover catalog/subbook/index source order; duplicate and distinct
headwords sharing one physical page/offset; equal bytes at different physical
identities; multiple subbooks and text files; all supported index types;
internal references; exact multi-digit
`epwing-index:<first-record-ordinal>:<article-ordinal>:<text-file-ordinal>:<page>:<offset>`
provenance; empty and excluded materialization; corruption; limits;
checkpoints; cancellation; and deadlines. Assertions must prove first-record
ownership, alias retention, headword-independent deduplication, canonical
unsigned decimal components, inert output, and no partial view after failure.

Revision fixtures must pin `CATALOGS`, optional `LANGUAGE`, and every regular
non-symlink file in each catalog-selected subbook/content tree. They cover
mutation or replacement of every member, addition/removal, path and ordering
changes, selected-tree topology, resource-only changes, symlink exclusion,
unrelated sibling exclusion, corrupt snapshots/rebuilds, and contained storage
failures. Reference links must not redirect ownership or create documents;
copyright/metadata, resource names/bytes, aliases, unindexed targets, active
markup, scripts, media, and gaiji payloads must not enter inert text.

Mixed-service coverage remains eleven-format and pins dictionary-ID ordering,
filtering, the global bound, adapted no-match behavior, typed unsupported for
EPWING and other non-adapted sources, typed unavailable for missing IDs, and
contained per-dictionary failures. The implementation gate is focused
tests, a fresh Linux Release configure/build, full
`ctest --preset conan-release` with 103 registered tests unless intentionally
changed, Release install and installed C/C++ consumers, then clean committed
exact-SCM `conan create` with the Qt WebEngine host profile and packaged
consumers. P6-FT-13 changes
no installed API/DTO, capability, configuration/Preferences, dependency,
GUI/Phase 8 behavior, `.gdfts` serialization, or completed-format behavior.
EPWING remains typed unsupported; no adapter, successor, or later leaf is
selected or ranked.

The accepted P6-FT-14 implementation extends the generated EPWING fixtures to
the private adapter and lifecycle. It proves one document per retained physical
article, first-record canonical headword and aliases, distinct equal-content
identities, empty-output exclusion, inert plain text assembled from bounded
HTML, inert references, and exact canonical unsigned-decimal
`epwing-index:<first-record-ordinal>:<article-ordinal>:<text-file-ordinal>:<page>:<offset>`
provenance, including multi-digit ordinals. Copyright/metadata, resource
names/bytes, aliases, unindexed targets, active markup, scripts, media, and
gaiji payloads must remain absent.

Lifecycle cases cover create, reuse, stale rebuild, corrupt rebuild, and
contained reader/index/storage failure against the complete ordered
`SourceSnapshot`. Fixtures mutate, replace, add, remove, rename, and reorder
selected files and topology, including `CATALOGS`, optional `LANGUAGE`, and
resource-only members; they also prove non-symlink ordering, symlink and
unselected-sibling exclusion, bounded document/corpus limits, checkpoints,
cancellation, deadlines, and no partial publication.

Mixed-service coverage becomes twelve-format and retains dictionary-ID order,
filtering, the global result bound, adapted no-match behavior, typed
unavailable requested IDs, and contained per-dictionary failures. Installed
C++ coverage requires an exact-provenance EPWING result; installed C coverage
remains unchanged. Acceptance requires focused tests, a fresh Linux Release
configure/build, full `ctest --preset conan-release` with 103
registered tests unless an intentional delta is justified, Release install
and installed C/C++ consumers, and a clean committed exact-SCM `conan create`
with the Qt WebEngine host profile and packaged consumers. No successor after
P6-FT-14 is selected or ranked.

P8-PREF-7 configuration coverage pins current and strict legacy persistence at
zero and the `9999` maximum plus atomic rejection above it. Its offscreen
Preferences and WebEngine coverage pins the exact label, tooltip, range, step,
default, cancel/failure/success transaction, live existing-tab revision,
new-lookup behavior, zero/minimum/default/maximum/overflow cases, stale-action
invalidation, complete results-pane capacity, and unchanged session/layout and
unrelated facade state.

The Phase 7 network-cache ownership audit and runtime owner are accepted: Qt
Network exclusively owns the managed HTTP/HTTPS cache, and WebEngine is
outside the preference contract. `http_client_test` covers zero and positive
limits, exact MiB conversion, the dedicated path, cache hits and restart
persistence, reduction/eviction, setup failure degrading to uncached traffic,
transaction rollback, request quiescence, owned-cache-only clearing, and
non-fatal redacted cleanup failure. The P8-PREF-6 offscreen/restart smoke adds
the pinned controls, complete-candidate transaction, persisted restart policy,
owned-directory cleanup, and WebEngine sentinel isolation. Other blocked
Phase 5/6/7/9/10 capabilities acquire focused Preferences coverage only after
their named runtime prerequisite is accepted; intentionally excluded controls
receive no inert-widget smoke.

The General/Network cache Preferences leaf adds focused current/legacy
configuration bounds and preserves the runtime-owner coverage above. Its
offscreen smoke pins the legacy labels, platform suffix, 0--2000 range,
defaults, owned-directory tooltip, zero-dependent enablement, cancel, failed
apply, successful persistence/activation, and unchanged facade/session/layout.
A two-process restart smoke verifies retained positive policy, exact active MiB
limits, subsequent clear-on-exit cleanup of only `qt-network-http`, and an
untouched WebEngine sentinel.
`goldendict_search_menu_smoke` pins the unique `menuSearch` between Edit and
History and its sole backed legacy `searchInPageAction`, including exact text,
role, Ctrl+F ownership, canonical instance reuse, focus/select dispatch, and
the explicit absence of full-text placeholders. Real offscreen Chromium finds
verify match/no-match status, per-tab query and presentation restoration,
closed-tab cleanup, stale-completion isolation, single dispatch, unchanged
facade session and private Qt state, and continued central usability.
They also pin the unique `navToolbar` identity, top-area default and ordering,
top/bottom movement policy, Back/Forward/group/query/lookup control order and
parentage, focus chain, minimum query usability, Alt+D/Ctrl+L focus behavior,
and backed lookup/navigation workflows. State coverage includes the new
version round trip, the immediately preceding Qt 6 layout, the older dock-name
transition, malformed and unreachable rollback, and two-start toolbar
placement and visibility without admitting legacy version-1 bytes.
The same smoke pins the unique visible right-side `dictsPane` and `dictsList`,
its default order above favorites/history, lookup-response row identity and
active-tab isolation, keyboard and mouse activation, focus traversal,
stale-row clearing, and usable central content. State coverage advances through
the three immediately preceding private Qt 6 versions, and the two-start smoke
preserves results-pane placement and visibility while legacy version 1 remains
rejected.

`goldendict_suggestion_pane_smoke` uses the same Dictd fixture to pin the
unique visible left-side `searchPane` and `wordList`, bounded service ordering,
rapid request replacement, keyboard focus transfer, mouse and keyboard lookup
activation, history dispatch, and clearing after activation, empty input, and
invalid input. The article-tabs and two-start restart smokes additionally pin
version-6 state round trips, deterministic seeding while accepting Qt 6
versions 5 through 2, placement and visibility persistence, usable central
content, rollback safety, and continued version-1 rejection. Existing
`application_service_test` cases remain the source of truth for Unicode/case
folding, group ordering, cancellation, partial errors, and bounded limits.

`goldendict_view_menu_smoke` pins the unique `menubar` and `menuView`, exact
action order and separator placement, direct reuse of all four dock and both
toolbar toggle actions, legacy pane shortcuts, accessible action text, and
single checked/visibility transitions from both action and widget changes. It
also verifies that a complete toggle round trip preserves private Qt state
version 7 and the usable central article shell.

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

## Phase 8 Full-Text Request Controller Gate

The Phase 6 milestone audit confirms all twelve private format adapters against
the installed `SearchFullText` contract. P8-FT-1 completes only the private
Widgets asynchronous request controller and does not add a visible workflow.
`full_text_request_controller_test` uses a deterministic fake service to
verify off-GUI-thread execution, GUI-thread terminal
delivery, unchanged successful and partial responses, explicit cancellation,
replacement cancellation, typed cancellation/deadline/error pass-through,
boundary-exception conversion to `kInternal`, stale-generation rejection,
consumer detachment, facade replacement and shutdown join. It must also prove
that a cancelled pending request never starts and that cancel/stop are
idempotent.

No UI smoke is required for this nonvisual prerequisite. Any later dialog or
visible result leaf must add its own offscreen smoke and cannot claim coverage
from the controller test. The implementation gate is Release configure and
build plus full `ctest --preset conan-release` with the intentional 104-test
baseline, including the new focused private executable. Run
install and installed-consumer checks only if an installed surface is affected;
P8-FT-1 is specified not to affect one. The exact-SCM Conan package and packaged
consumers must nevertheless be verified from the implementation revision.
No successor after P8-FT-1 is selected or ranked.

## Phase 8 Full-Text Query-Mode Persistence Gate

P8-FT-2 completes the nonvisual configuration prerequisite selected by the
documentation-only post-P8-FT-1 audit. Focused core QTests and generated
fixtures cover migrated mode values `0/1/2/3` mapping respectively to
whole words/wildcard/regular expression/plain text, exact four-mode migrated
round trips, preservation of existing migrated `0/1/2`, and legacy XML values
`0/1/2/3` mapping respectively to whole words/plain text/wildcard/regular
expression. Current and legacy unknown values must fail atomically without
publishing or overwriting a partial configuration.

No offscreen smoke is required because P8-FT-2 adds no visible behavior. A
later dialog/query-controls leaf must provide its own focused widget QTests and
offscreen smoke; selection, result presentation/activation, highlighting,
Preferences, and index availability/status/background lifecycle likewise
require separate later gates and cannot claim coverage from P8-FT-2.

The completed implementation gate is the focused QTests followed by a Release
build and full `ctest --preset conan-release` with the 104-test baseline unless
test registration creates an intentional delta. Because P8-FT-2 adds an
installed/public enum value, run install and installed-consumer checks. Verify
the exact-SCM Conan package and packaged consumers from the implementation
revision. No successor after P8-FT-2 is selected or ranked.

## Phase 8 Full-Text Per-Dictionary Limit Gate

P8-FT-3 completes the narrow installed-query prerequisite selected by the
post-P8-FT-2 audit. Focused application-service tests verify
that the default `maximum_articles_per_dictionary = 100` preserves existing
callers; `result_limit` retains its independent existing validation; and the
new field independently accepts `1..100000` and rejects zero and values above
`100000` without starting backend work.

Generated multi-dictionary fixtures prove that one dictionary is capped
by `maximum_articles_per_dictionary`, multiple selected dictionaries may each
contribute up to that cap, and the combined response always stops at the
unchanged global `result_limit`. Backend requests receive
`min(maximum_articles_per_dictionary, remaining global result_limit)`, with no
multiplication or derivation between limits. Existing dictionary order and
filtering, cancellation, deadlines, unavailable/unsupported errors, contained
backend failures and partial responses remain covered and unchanged.

The installed C++ consumer constructs the expanded `FullTextQuery` and
observes both limits; the installed C consumer is unchanged. P8-FT-3 adds no
offscreen smoke because it has no visible behavior. Dialog controls,
dictionary/group/muting selection, results/activation, highlighting,
Preferences UI/policy and index visibility/status/background lifecycle require
their own later gates and cannot claim this coverage.

The completed implementation gate is focused tests followed by a Release
configure and build and full `ctest --preset conan-release`, accepting only an
intentional registered-test delta. Because P8-FT-3 expands an installed/public
DTO, it includes install and installed-consumer checks plus exact-SCM Conan
package and packaged-consumer verification from the implementation revision.
No successor after P8-FT-3 is selected or ranked.

P8-FT-4 completes the sole next leaf selected by the documentation-only
post-P8-FT-3 audit. Its focused Core and application-service QTests preserve the
installed defaults `result_limit == 20` and engaged
`maximum_articles_per_dictionary == 100`; accept global limits `1` and
`1000000` while rejecting `0` and values above `1000000`; and accept engaged
per-dictionary values `1..100000` while rejecting values outside that range.

Aggregator coverage proves that an engaged per-dictionary limit requests
at most the minimum of that value and remaining global capacity, while a
disengaged optional requests at most the remaining global capacity. Multiple
selected dictionaries demonstrate that the global cap remains
independent, always terminates aggregation, and is never multiplied by
dictionary count or the per-dictionary value. Existing deterministic ordering,
filtering, cancellation, deadlines, typed errors, contained failures, and
partial-response behavior remain covered and unchanged.

The installed C++ consumer constructs and observes both engaged and
disengaged forms of the expanded DTO; the installed C consumer remains
unchanged. The completed implementation gate is focused QTests followed by
Linux Release configure/build and `ctest --preset conan-release`, Release
install and the installed consumers, then clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile. The public ABI
expansion authorizes the consumer rebuild and package revision. No
widget/offscreen test belongs to P8-FT-4 because it adds no visible behavior;
the later dialog-composer leaf must cover checked-to-engaged,
unchecked-to-`std::nullopt`, and fixed global cap `100000` mapping with focused
offscreen tests.

P8-FT-4 excludes dialog
composition, selection/muting, results/activation, highlighting, Preferences
and index policy, index visibility/background lifecycle, adapters, `.gdfts`,
legacy `_FTS`, dependencies, and unrelated behavior. The completed P8-FT-4
selects or ranks no successor.

## Phase 8 Full-Text Query Composer Gate

P8-FT-5 is complete as the private non-integrated Widgets query composer.
Focused Qt Widgets QTests construct the private controls from persisted
defaults and verify deterministic `FullTextQuery` composition for UTF-8 text
and all four explicit mode mappings: whole words to `kWholeWords`, plain text
to `kPlainText`, wildcard to `kWildcard`, and regular expression to
`kRegularExpression`. Match case, ignore diacritics, and ignore word order map
directly.

The focused tests cover checked word distance at the `0` and `1000` boundaries
and unchecked to `std::nullopt`; checked maximum articles per dictionary at the
`1` and `100000` boundaries and unchecked to `std::nullopt`; and the fixed
independent `result_limit == 100000`. They also prove
that the existing default timeout is retained, dictionary IDs are empty with
`dictionary_filter_active == false`, and repeated composition does not mutate
persisted preferences or invoke a fake service/controller.

Offscreen widget coverage verifies that whole-word and plain-text modes enable
word-order and optional-distance controls, while wildcard and regular-
expression modes compose `ignore_word_order == false` and a disengaged word
distance without losing retained values across mode changes. P8-FT-1 controller
tests do not substitute for this coverage. No main-window or WebEngine smoke
belongs to this non-integrated prerequisite.

The accepted implementation gate is the focused composer QTest followed by
Linux Release configure/build and full `ctest --preset conan-release`. The new
focused executable intentionally raises the registered suite baseline from 104
to 105 tests. The final package gate is clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers. Install and standalone installed-consumer checks are unnecessary
unless implementation unexpectedly changes an installed surface; the selected
leaf is specified not to do so.

P8-FT-5 excludes modeless dialog/main-window action integration,
dictionary/group/muting selection, request completion UI, results/activation,
highlighting, Preferences enablement and index policy, index
visibility/background lifecycle, public API or persistence changes, adapters,
`.gdfts`, legacy `_FTS`, dependencies, and unrelated behavior. Evidence is the
migrated `dictionary_service.h:28-32,47-52,149-160`,
`application.h:193-198,268-277`, the completed private controller and its test,
and pinned legacy `fulltextsearch.ui` plus
`fulltextsearch.cc:232-315,387-446`. No successor after P8-FT-5 is selected or
ranked.

## Phase 8 Full-Text Capability Projection Gate

P8-FT-6 is complete from clean pushed migrated revision
`ac8a01c6b6212d6313364e2107a3bcb8b13df535`. Its focused Core coverage
constructs all twelve completed textual full-text backends and unsupported
runtime and resource backends, verifies exact `supports_full_text_search`
true/false values without changing catalog order or other identity fields,
checks that every returned `FullTextResult::dictionary` agrees with the
corresponding catalog entry, and proves that replacing the service recomputes,
rather than persists, the capability even when private index state fails.

The installed C++ consumer must compile against and read the additive field for
both supported and unsupported entries. The installed C consumer is unchanged.
Because P8-FT-6 expands an installed C++ DTO, its completed implementation
gate is the focused Core/consumer tests followed by Linux Release
configure/build and full `ctest --preset conan-release`, install verification,
standalone installed C++ consumer verification, and clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers. Coverage extends existing executables, so the registered suite
remains 105 tests.

No Widgets, MainWindow, WebEngine, persistence, controller, index-lifecycle,
or adapter behavior test belongs to P8-FT-6. Dialog/action integration,
dictionary/group/muting application, request states, results/activation,
highlighting, Preferences/index policy, index visibility/background lifecycle,
`.gdfts`, legacy `_FTS`, dependencies, and unrelated behavior remain excluded.
Evidence is migrated `dictionary_service.h:85-96,182-195,256-272`,
`dictionary_service.cc:468-481,1075-1119`, `main_window.cpp:5535-5770`, and
pinned legacy `fulltextsearch.cc:613-659`. No successor after P8-FT-6 is
selected or ranked.

## Phase 8 Full-Text Dictionary Participation Projection Gate (Complete)

P8-FT-7 is complete. It was the sole leaf selected by the documentation-only
post-P8-FT-6 readiness audit. Focused private Widgets tests compose a query and
verify that All Dictionaries preserves catalog order while a named group preserves
configured member order, drops unresolved members and configured
`muted_dictionary_ids`, and that both retain only catalog identities whose
`supports_full_text_search` value is true.

The tests exercise visible `dictionaryBar` checks as a further ordered filter
and prove that hidden-bar ephemeral state does not filter the baseline. They
also cover an empty projection with `dictionary_filter_active == true`, catalog
and group replacement recomputation, and exact preservation of query text,
mode, booleans, optionals, limits, and timeout from P8-FT-5. A fake controller
and immutable preference/configuration fixtures prove that projection neither
submits a request nor mutates persisted state.

A focused offscreen MainWindow smoke drives the real group selector and
dictionary bar and inspects the projected query without opening a full-text
dialog or starting backend work. Controller coverage from P8-FT-1 and composer
coverage from P8-FT-5 do not substitute for this selection smoke.

The implementation gate is the focused tests followed by Linux Release
configure/build and full `ctest --preset conan-release`, accepting only an
intentional registered-test delta, then clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers. The dedicated smoke raises the registered suite from 105 to 106
tests. P8-FT-7 changes no installed surface, so install and standalone
installed-consumer checks are unnecessary.

No modeless dialog/Search action, request submission/completion UI,
results/activation, highlighting, Preferences/index policy, index
visibility/status/background lifecycle, public API, persistence, adapter,
`.gdfts`, legacy `_FTS`, dependency, or unrelated behavior test belongs to
P8-FT-7. Evidence is migrated `full_text_query_composer.h/.cpp`,
`dictionary_service.h:85-96,149-160`, `main_window.cpp:5535-5770`, the
P8-FT-5/P8-FT-6 focused tests, and pinned legacy
`fulltextsearch.cc:613-659`. No successor after P8-FT-7 is selected or ranked.

## Phase 8 Full-Text Dialog Shell Gate (Complete)

The completed P8-FT-8 leaf implements the post-P8-FT-7 audit's sole smallest
dependency-ready leaf: the private modeless full-text dialog shell and
MainWindow/Search-action integration. The audit is pinned to clean pushed
migrated revision `1eeea0a73ff29b832f68012bd2b99b7f6208cf87` and unchanged
clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Focused private Widgets tests must
verify that `fullTextSearchAction` follows `searchInPageAction` in `menuSearch`
and has exact text `Full-text search`, shortcut `Ctrl+Shift+F`,
`WidgetWithChildrenShortcut`, and `TextHeuristicRole`. The action must be
disabled without a usable facade and enabled when one is attached, without
consulting persisted full-text enablement or index readiness.

The tests must prove that repeated triggers maintain one non-modal
MainWindow-owned dialog, showing, raising, and activating the existing
instance; close destroys it and a later trigger creates a fresh instance. The
shell has title `Full-text search`, no window-context-help button, and the
P8-FT-5 composer as its query body. First creation copies the main lookup text
and selects it. Every show recomputes the P8-FT-7 dictionary projection from
the current catalog, selected group, muting, and visible dictionary-bar state.

Controller-spy coverage must prove zero submissions and completions, and safe
detach/stop before dialog close, facade replacement, or MainWindow destruction
can invalidate the borrowed service. It must also prove that the shell does
not persist geometry or changed controls. An offscreen MainWindow smoke uses
the real Search menu, lookup field, group selector, and dictionary bar to
exercise create, repeat trigger, changed-state reprojection, close, and reopen.

The full implementation gate is the focused tests, Linux Release
configure/build, full `ctest --preset conan-release` accepting only the
intentional registered-test delta, then clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers. No install or standalone installed-consumer check is required
because P8-FT-8 changes no installed interface.

No request submission/cancellation/replacement/completion behavior,
results/activation, highlighting, Preferences/index policy, index
visibility/status/background lifecycle, public API, persistence, adapter,
`.gdfts`, legacy `_FTS`, dependency, or unrelated test belongs to P8-FT-8.
Evidence is migrated `main_window.cpp:425-433,6064-6086`,
`full_text_query_composer.h/.cpp`, `full_text_dictionary_projection.h/.cpp`,
and `full_text_request_controller.cpp:143-197`, plus pinned legacy
`mainwindow.ui:614-627`, `mainwindow.cc:4754-4791`, and
`fulltextsearch.cc:195-340`. No successor after P8-FT-8 is selected or ranked.

## Phase 8 Full-Text Request-State Gate (Complete)

P8-FT-9 completes private dialog request submission, cancellation,
replacement and terminal-state integration. It is based on clean migrated
revision `750177f4f8a2d9fd709a2c27efe3e254505308d6` and unchanged clean
read-only legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
No successor after P8-FT-9 is selected or ranked.

Focused private Widgets tests prove that Search composes the current
P8-FT-5 controls, reapplies the latest P8-FT-7 dictionary IDs and active-filter
flag, clears the prior private terminal response, advances a monotonically
increasing generation and submits exactly once through P8-FT-1. They must
cover a later private submission replacing running and pending work while the
visible Search control is disabled, current-generation terminal delivery, and
suppression of stale or cancelled completions.

State tests verify an indeterminate progress indicator and available
cancellation while running; idempotent Cancel invalidating the active
generation and restoring idle Search state; current completion retaining the
unchanged structured response privately, hiding progress and restoring idle;
and P8-FT-8 close, facade replacement and MainWindow destruction stopping and
detaching safely. The offscreen dialog/MainWindow smoke exercises the real
composer, current projection, Search and Cancel controls and terminal state.

P8-FT-9 extends the existing focused test and offscreen smoke registrations,
so the registered Release baseline remains 108 tests. The implementation gate
is the focused tests, Linux Release configure/build,
full `ctest --preset conan-release` accepting only the intentional registered-
test delta, then clean committed exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers. No install or standalone
installed-consumer check is required because P8-FT-9 changes no installed
interface.

No result model/projection, merge/order/metadata/count presentation,
selection/article activation, highlighting handoff/article-view behavior,
beep, user-facing validation/error/partial-response policy,
Preferences/full-text/index policy, index readiness/visibility/status or
background lifecycle, public API, persistence, adapter, `.gdfts`, legacy
`_FTS`, dependency or unrelated test belongs to P8-FT-9. Evidence is migrated
`full_text_request_controller.h/.cpp`, `full_text_query_composer.h/.cpp`,
`full_text_dictionary_projection.h/.cpp` and
`full_text_search_dialog.h/.cpp`, plus pinned legacy
`fulltextsearch.cc:338-570` and `fulltextsearch.ui:99-238`. No successor after
P8-FT-9 is selected or ranked.

## Phase 8 Full-Text Per-Document Response Projection Gate (Complete)

The documentation-only post-P8-FT-9 audit selected P8-FT-10 as the sole
smallest independently dependency-ready prerequisite: a private,
non-integrated Qt item model that projects one row per ordered
`FullTextResponse::results` element. No successor after P8-FT-10 is selected or
ranked.

The focused offscreen Widgets QTest verifies zero rows for an empty
response; exact row count and Core order; separate rows for case-insensitively
equal headwords; UTF-8 headwords through `Qt::DisplayRole`; and field-for-field
typed access to dictionary identity, headword, document ID, `MatchInfo`,
excerpt, and every byte match. Reset and replacement are atomic, the model
owns an immutable snapshot independent of later source-response mutation,
and invalid indexes or non-display roles must follow normal Qt model behavior.
The test also proves that construction and reset do not submit, cancel, or
otherwise touch a controller, service, dialog, configuration, or persistence.

The focused gate is the new private model QTest. Its registration intentionally
raises the Release baseline from 108 to 109 tests. The full implementation gate
is Linux Release configure/build, full `ctest --preset conan-release` with only
the intentional registered-test delta, then clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers. P8-FT-10 changes no installed interface, so install and standalone
installed-consumer checks are not required.

Visible result-list/table/tree choice, counts, selection, empty/error/partial
states, article activation/navigation and lookup handoff, highlighting and
WebEngine behavior, Preferences enablement/index policy, index
readiness/visibility/status and background lifecycle, public APIs,
persistence, adapters, `.gdfts`, legacy `_FTS`, dependencies, and unrelated
tests do not belong to P8-FT-10. Evidence is migrated
`dictionary_service.h:164-196`,
`application/dictionary_service.cc:1078-1157`, and
`full_text_search_dialog.h/.cpp`, plus pinned legacy
`fulltextsearch.hh:41-65,135-156`,
`fulltextsearch.cc:129-186,518-610,685-750`, and
`fulltextsearch.ui:99-238`. No successor after P8-FT-10 is selected or ranked.

P8-FT-10 is complete. Focused coverage includes empty, success, contained-error
and partial responses, ordering, duplicate headwords, complete typed metadata,
copy/move and snapshot lifetime, atomic replacement, invalid model access, and
deterministic repeated projection. No successor is selected or ranked.

## Phase 8 Full-Text Dialog Response-Model Integration Gate (Complete)

The documentation-only post-P8-FT-10 audit selects P8-FT-11 as the sole
smallest independently decision-complete leaf: private synchronization of the
dialog's retained current response with its child `FullTextResponseModel`. No
successor after P8-FT-11 is selected or ranked.

The focused offscreen dialog QTest must verify an initially empty model; exact
ordered projection for a generation-current success; zero rows while retained
errors and `partial` remain unchanged for empty, contained-error, and partial
responses; clearing when a replacement submission clears the retained
response; atomic replacement by the next current completion; and no model or
response update from stale or cancelled completion. Existing coverage must
continue to prove idempotent cancellation, service replacement, controller
detachment, teardown safety, exact request composition, and no configuration or
persistence effect. The dialog must create no visible result view in this leaf.

The focused gate extends the existing private dialog QTest and registration, so
the Release suite baseline remains 109 tests. The full implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, then clean committed exact-SCM `conan create`
with the Release Qt WebEngine host profile and packaged consumers. P8-FT-11
changes no installed interface; Release install and standalone installed C and
C++ consumers are nevertheless required as a stronger verification gate.

List/table/tree presentation, columns/additional roles, counts, selection,
empty/error/partial presentation, article activation/navigation and lookup
handoff, highlighting/WebEngine behavior, Preferences enablement/index policy,
index readiness/visibility/status/background lifecycle, public APIs,
persistence, adapters, `.gdfts`, legacy `_FTS`, dependencies, and unrelated
tests do not belong to P8-FT-11. Evidence is migrated
`full_text_search_dialog.h/.cpp`, `full_text_response_model.h/.cpp`, and their
focused tests, plus pinned legacy `fulltextsearch.hh:135-156`,
`fulltextsearch.cc:518-610,685-750`, and `fulltextsearch.ui:99-238`.
No successor after P8-FT-11 is selected or ranked.

P8-FT-11 is complete. The existing offscreen dialog QTest covers single child
model ownership, initial and replacement emptiness, exact current-completion
projection, unchanged retained error and partial state, atomic repeated reset,
stale and cancelled suppression, service replacement and detach safety, and
the absence of a visible result view. The registered Release baseline remains
109 tests. No successor is selected or ranked.

## Phase 8 Full-Text Visible Result-List Gate (Complete)

The documentation-only post-P8-FT-11 audit selects P8-FT-12 as the sole
smallest dependency-ready and independently decision-complete leaf: private
attachment of one visible `QListView` to the dialog-owned
`FullTextResponseModel`. It depends only on completed P8-FT-10 projection and
P8-FT-11 synchronization. No public interface, persistence, adapter,
dependency, or index behavior changes.

The focused offscreen dialog QTest must verify exactly one visible result list
whose model is the existing child response model; zero rows initially and while
a replacement is pending; zero rows for empty and error-only responses; exact
Core-order UTF-8 rows, including duplicates, for generation-current successful
and partial responses; atomic repeated replacement; and no visible update from
stale or cancelled completions. Existing coverage must continue to prove
unchanged retained errors/partial state, request behavior, service replacement,
controller detachment, teardown safety, and no configuration or persistence
effect. No selection or activation behavior is asserted in this leaf.

The focused gate extends the existing private dialog QTest and registration, so
the Release baseline remains 109 tests. The full future implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean committed exact-SCM `conan create` with
the Release Qt WebEngine host profile and packaged consumers, Release install,
and standalone installed C and C++ consumer checks.

Dictionary tooltip/name decoration, columns, extra roles, richer delegates,
selection/focus/retention, counts, and empty/error/partial presentation remain
separate. Click, double-click, and Return/Enter activation; article lookup,
dictionary scoping, and MainWindow navigation remain separate. Highlighting
and WebEngine handoff, Preferences enablement/index policy, index
readiness/visibility/status/background lifecycle, public APIs/DTOs,
persistence, adapters, `.gdfts`, legacy `_FTS`, dependencies, build-system
changes, and unrelated tests do not belong to P8-FT-12.

Evidence is migrated `full_text_search_dialog.h/.cpp`,
`full_text_response_model.h/.cpp`, their focused tests, and completed
P8-FT-10/P8-FT-11 coverage, plus pinned legacy
`fulltextsearch.hh:135-156`, `fulltextsearch.cc:518-610,685-750`, and
`fulltextsearch.ui:99-238`. No successor after P8-FT-12 is selected or ranked.

P8-FT-12 is complete. The existing offscreen dialog QTest proves exactly one
dialog-owned visible result list uses the existing child response model;
initial, replacement-pending, empty, and error-only responses expose zero rows;
current successful and partial responses expose exact ordered UTF-8 rows,
including duplicates; repeated response replacement remains atomic; and stale
or cancelled completions remain invisible. The registered Release baseline
remains 109 tests. No selection, focus, activation, decoration, count, state
presentation, or successor is selected or ranked.

## Phase 8 Full-Text Result Activation-Intent Gate (Complete)

The documentation-only post-P8-FT-12 audit selects P8-FT-13 as the sole
smallest dependency-ready and independently decision-complete leaf: private
Widgets-owned activation intent from the existing result list. A valid row
activates exactly once on a single primary-button click or Return/Enter and
delivers a safe value snapshot of its existing `FullTextResult`, resolved by
`FullTextResponseModel::ResultAt()`. Invalid, empty, stale, or reset indexes
deliver nothing. Return/Enter requires a current valid index. Double-click
adds no second activation.

The focused offscreen dialog QTest must verify exact one-delivery for a valid
single click and for Return and Enter on a current valid row; equality of all
copied result fields, including dictionary identity, headword, document ID,
match, excerpt, and full match vector; no delivery for invalid indexes, an
empty model, or an index invalidated by replacement reset; and no duplicate
delivery from a double click. Existing coverage must continue to prove Core
order and duplicates, immediate replacement clearing, current-generation-only
projection, stale/cancelled suppression, service replacement, controller
detachment, and teardown safety.

The focused gate extends the existing private dialog QTest and registration,
so the Release baseline remains 109 tests. The full future implementation gate
is Linux Release configure/build, full `ctest --preset conan-release` without
registration drift, clean committed exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumer checks. P8-FT-13 changes no installed
surface, but package and consumer checks remain the stronger full gate.

Selection/focus/retention, dictionary decoration and extra roles, counts and
empty/error/partial presentation, MainWindow lookup and exact dictionary
scoping, tab/history/navigation mutation, highlighting/WebEngine handoff,
Preferences enablement/index policy, index readiness/visibility/status/
background lifecycle, public APIs/DTOs, persistence, adapters, `.gdfts`,
legacy `_FTS`, dependencies, build-system changes, and unrelated tests do not
belong to P8-FT-13. Evidence is migrated `full_text_search_dialog.h/.cpp`,
`full_text_response_model.h/.cpp`, and focused tests plus pinned legacy
`fulltextsearch.cc:292-293,594-610,664-673` and
`fulltextsearch.hh:227,232-233`. No successor after P8-FT-13 is selected or
ranked.

P8-FT-13 is complete. The existing offscreen dialog executable now covers
exact one-delivery for primary single click, Return, and keypad Enter; exact
copied result metadata and lifetime; double-click non-duplication; invalid,
no-current, reset, stale, and cancelled safety; and repeated model resets.
No test executable or production navigation side effect was added. The
registered Release baseline remains 109, and no successor is selected or
ranked.

## Phase 8 Full-Text Scoped Navigation Prerequisite Gate (Complete)

The post-P8-FT-13 documentation audit selected only P8-FT-14. Before consuming
the activation signal, lookup navigation must retain the authoritative ordered
dictionary IDs submitted for the full-text response, including the meaningful
active-empty case. This prevents history replay, restored tabs, and dictionary
participation refresh from widening a full-text result lookup.

Focused Core coverage proves bounded validation; absent, active-nonempty,
and active-empty scope semantics; navigation equality and back/forward
retention; complete session export/restore; compatibility with the prior
ten-field navigation record; count mismatch, more than
`kMaximumLookupDictionaryFilters` IDs, empty/oversized/invalid UTF-8 IDs, and
NUL rejection without tab/session mutation.
Focused MainWindow coverage proves that retained scope is copied unchanged
to `LookupQuery`, active-empty starts no dictionary work, and absent scope
continues to use current group/dictionary-bar participation. Existing unscoped
lookup, tab, history, restore, and dictionary-context tests must remain green.

P8-FT-14 does not connect the dialog activation signal or test click-to-article
behavior. Headword handoff across the complete submitted full-text dictionary
scope, tab disposition, query-edit/history behavior, exact article identity,
and highlighting remain later independent surfaces. Result-list selection and
decoration, counts and response states, Preferences/index policy, index
lifecycle, adapters, index formats including legacy `_FTS`, dependencies,
build-system changes, and unrelated suites are excluded.

The focused gate runs `application_service_test`, `article_tabs_test`,
`goldendict_article_tabs_smoke`,
`goldendict_article_tab_session_restart_smoke`, and
`goldendict_dictionary_context_navigation_smoke`. The full gate remains
Linux Release configure/build, full
`ctest --preset conan-release`, clean exact-SCM `conan create` with the Qt
WebEngine host profile and packaged consumers, Release install, and standalone
installed C and C++ consumers. The installed C++ consumer covers absent,
active-empty, and ordered non-empty scope; the C API and consumer remain
unchanged. Evidence is migrated
`desktop_facade.h`, Core tab/session tests, `main_window.cpp`
lookup/navigation/filter construction, P8-FT-7 projection, and pinned legacy
`fulltextsearch.cc:594-610` plus `mainwindow.cc:3002-3014`.
No successor after P8-FT-14 is selected or ranked.

## Phase 8 Full-Text Accepted-Response Activation-Context Gate (Complete)

The post-P8-FT-14 documentation audit selected only P8-FT-15. A direct
activation-to-MainWindow test remains premature. The completed private intent
now carries response-associated dictionary scope even when `ProjectedQuery()`
changes while the displayed response remains. The focused gate proves that
submission snapshots the authoritative active flag and ordered
dictionary IDs; only the generation-current accepted response owns that
snapshot; replacement clears response, rows, and context atomically; and
stale/cancelled completion, service replacement, detach, and teardown cannot
restore or emit obsolete context.

Focused dialog coverage includes absent, ordered nonempty, and
authoritative-empty scope, exact by-value activation delivery, copy lifetime
across later model reset, deterministic repeated submissions, and a regression
in which
`SetProjectedQuery()` changes after completion without changing the scope
delivered for an existing row. Existing P8-FT-9 through P8-FT-14 tests remain
green. No public or installed-consumer coverage is added because the boundary
remains private to Widgets. The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built.

The downstream connection acceptance contract remains fixed but unselected:
exact result headword; current activated article tab; unchanged main query edit;
`kLookup` navigation with headword query/title, active MainWindow group, and
unchanged accepted-response scope; then successful tab synchronization,
`ArticleTabSessionMutated`, and `StartNavigationLookup(..., true)`. Tests must
eventually cover ordered and authoritative-empty scopes, Core history/session
identity, lookup-history emission, replay/restoration without widening, and
missing-context/facade, invalid-navigation, and tab-limit no-op behavior.

Exact `document_id` and source-dictionary targeting, match/excerpt metadata,
highlighting, ignore-diacritics, WebEngine handoff, selection/focus/retention,
decoration, counts and response states, Preferences/index policy, index
lifecycle, adapters/index formats including legacy `_FTS`, persistence beyond
existing navigation, dependencies/build-system work, and unrelated suites are
excluded. The full implementation gate is Linux Release configure,
full `ctest --preset conan-release`, clean exact-SCM `conan create`, packaged
consumers, Release install, and standalone installed consumers.

Evidence is migrated dialog submission/completion/activation code and focused
tests, P8-FT-7, P8-FT-13, P8-FT-14, and MainWindow current-tab lookup/history
conventions, plus pinned legacy `fulltextsearch.cc:594-610` and
`mainwindow.cc:3002-3014`.
No successor after P8-FT-15 is selected or ranked.

## Phase 8 Full-Text Scoped Result Navigation Connection Gate (Complete)

The post-P8-FT-15 documentation audit selected only P8-FT-16. P8-FT-13 now
delivers exact result activation, P8-FT-14 retains authoritative optional scope
through Core navigation history/session/restoration/replay, and P8-FT-15 binds
immutable submitted scope to the accepted response. The private MainWindow
connection can therefore be tested without adding a public API or reading
mutable dialog projection state.

Focused MainWindow/full-text-dialog coverage must prove that activation uses
the exact result headword as `kLookup` query and title, targets and activates
the current article tab without creating a new or background tab, leaves the
main query edit unchanged, captures the active MainWindow group, and copies
ordered nonempty or authoritative-empty accepted scope unchanged. After a
successful `OpenArticleTab`, assertions must observe tab synchronization,
`ArticleTabSessionMutated`, and `StartNavigationLookup(..., true)` effects,
including one navigation identity across Core history/session state, ordinary
lookup-history emission, restoration, and replay without scope widening.

Failure cases cover missing facade, dialog, or accepted context; invalid result
activation or navigation; and tab-limit failure. Each must start no dictionary
lookup, emit no lookup history, and cause no session mutation while preserving
the existing MainWindow failure status. Existing P8-FT-9 through P8-FT-15
focused and Core navigation/session tests remain green. The focused command is
`ctest --preset conan-release -R '^goldendict_full_text_dialog_smoke$'` after
the Release target has been built.

Exact `document_id` and source-dictionary targeting, match/excerpt metadata,
highlighting, ignore-diacritics, WebEngine handoff, selection/focus/retention,
decoration, counts and response states, Preferences/index policy, index
lifecycle, adapters/index formats including legacy `_FTS`, persistence beyond
existing navigation, dependency/build work, and unrelated suites are excluded
and remain separately decomposed without ranking. The full implementation gate
is Linux Release configure/build, full `ctest --preset conan-release`, clean
exact-SCM `conan create`, packaged consumers, Release install, and standalone
installed consumers.

Evidence is migrated dialog activation/context code and focused tests,
P8-FT-7, P8-FT-13, P8-FT-14 Core tab/session coverage, P8-FT-15, and MainWindow
current-tab `OpenArticleTab`, synchronization, session-mutation, failure-status,
and `StartNavigationLookup` conventions, plus pinned legacy
`fulltextsearch.cc:594-610` and `mainwindow.cc:3002-3014`.
P8-FT-16 is complete. The existing dialog smoke exercises ordered and
authoritative-empty activation, exact current-tab navigation identity,
sequencing and replay, safe replacement, excluded metadata, and failure no-op
paths without changing the 109-test baseline.
No successor after P8-FT-16 is selected or ranked.

## Phase 8 Full-Text Accepted-Result Count Presentation Gate (Complete)

The post-P8-FT-16 documentation audit selects only P8-FT-17. The retained
generation-current response and atomically reset visible model make a private
dialog-owned result count independently testable without changing Core or an
installed interface.

Focused dialog coverage proves an initial `Articles found: 0`, immediate
reset to zero on replacement submission, and the exact accepted model row
count for successful and partial responses, including duplicates. Empty and
contained-error-only accepted responses show zero. Partial counts describe
retained rows only. Repeated accepted responses replace the count, while stale
or cancelled completions, service replacement, detach, and teardown cannot
overwrite the current value. Existing response/model ordering, error, partial,
activation, and scoped-navigation coverage remains green.

The focused command is
`ctest --preset conan-release -R '^(full_text_search_dialog_test|goldendict_full_text_dialog_smoke)$'`
after the Release targets have been built. The full implementation gate remains
Linux Release configure/build, full `ctest --preset conan-release`, clean
exact-SCM `conan create`, packaged consumers, Release install, and standalone
installed consumers. Coverage extends the existing test executable and smoke,
so the registered Release baseline remains 109 tests.

Exact `document_id` lookup and source-dictionary targeting; initial/current
selection, keyboard focus, and retention; decoration, tooltips, and metadata;
empty/error/partial messaging beyond the numeric retained-result count;
match/excerpt display; highlighting, ignore-diacritics transfer, and WebEngine
handoff; Preferences/index policy and persistence; index readiness,
visibility, status, progress, background lifecycle, rebuild, and failure UI;
adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies, builds, and
unrelated suites are excluded and remain separately decomposed without
ranking. No public API, DTO, persistence, adapter, index, dependency, or build
surface belongs to P8-FT-17.

Evidence is migrated `full_text_search_dialog.cpp`,
`full_text_response_model.cpp`, and focused tests plus pinned legacy
`fulltextsearch.cc:290,448-449,570-571` and
`fulltextsearch.ui:99-129`.
P8-FT-17 is complete without changing a public or installed interface.
No successor after P8-FT-17 is selected or ranked.

## Phase 8 Full-Text Result Dictionary-Name Tooltip Gate (Complete)

The post-P8-FT-17 documentation audit selects only P8-FT-18. The complete
immutable result snapshot, accepted-response synchronization, and direct list
attachment make the private response model's dictionary-name tooltip
independently testable without changing Core or an installed interface.

Focused response-model coverage must prove that `Qt::ToolTipRole` for a valid
row returns the exact Unicode value decoded from
`FullTextResult::dictionary.name`. Duplicate-headword rows from different
dictionaries expose their own names. An empty name yields no visible tooltip
and never falls back to dictionary ID, source, edition, or other metadata.
Copied and moved response lifetimes and deterministic reset replacement retain
the correct tooltip, while invalid, foreign, out-of-range, nonzero-column, and
unsupported-role requests return no value. Existing display-role, ordering,
duplicate, complete-metadata, activation, dialog synchronization, and count
coverage remains green.

The focused command is
`ctest --preset conan-release -R '^full_text_response_model_test$'` after the
Release target has been built. The full implementation gate is
Linux Release configure/build, full `ctest --preset conan-release`, clean
exact-SCM `conan create`, packaged consumers, Release install, and standalone
installed consumers. P8-FT-18 adds no test executable or public/installed
interface, and the registered Release baseline remains 109 tests.

Exact `document_id` navigation and source-dictionary targeting; initial/current
selection, keyboard focus, and selection retention; non-tooltip decoration,
columns, delegates, icons, and additional metadata roles; empty/error/partial
messaging beyond the numeric retained-result count; match ranges and excerpt
presentation; highlighting, ignore-diacritics transfer, and WebEngine handoff;
Preferences enablement, format exclusions, size/index policy, and persistence;
index readiness, visibility, status, progress, background lifecycle, rebuild,
and failure UI; adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies,
build work, and unrelated suites are excluded and remain separately decomposed
without ranking. No public API, DTO, persistence, Core, adapter, index,
dependency, or build surface belongs to P8-FT-18.

Evidence is migrated `full_text_response_model.h/.cpp`, its focused tests, and
the P8-FT-11/P8-FT-12 dialog synchronization and attachment, plus pinned legacy
`fulltextsearch.cc:690-721`.
No successor after P8-FT-18 is selected or ranked.

## Phase 8 Full-Text Result Edit-Role Projection Gate (Complete)

The post-P8-FT-18 documentation audit selects only P8-FT-19. The complete
immutable result snapshot, accepted-response synchronization, and direct list
attachment make the private response model's headword edit-role projection
independently testable without changing Core or an installed interface.

Focused response-model coverage must prove that `Qt::EditRole` for a valid row
returns the exact Unicode value decoded from `FullTextResult::headword` and is
identical to that row's `Qt::DisplayRole`. Duplicate rows retain independent
headwords. Copied and moved response lifetimes and deterministic reset
replacement retain the correct value, while invalid, foreign, out-of-range,
nonzero-column, and unsupported-role requests return no value. Existing
display-role, tooltip, ordering, duplicate, complete-metadata, activation,
dialog synchronization, count, selection, focus, and retention coverage
remains green.

The focused command is
`ctest --preset conan-release -R '^full_text_response_model_test$'` after the
Release target has been built. The full implementation gate is
Linux Release configure/build, full `ctest --preset conan-release`, clean
exact-SCM `conan create`, packaged consumers, Release install, and standalone
installed consumers. P8-FT-19 adds no test executable or public/installed
interface, and the registered Release baseline remains 109 tests.

Exact `document_id` navigation and source-dictionary targeting; initial/current
selection, keyboard focus, and selection retention; non-edit-role decoration,
columns, delegates, icons, and additional metadata roles; empty/error/partial
messaging beyond the numeric retained-result count; match ranges and excerpt
presentation; highlighting, ignore-diacritics transfer, and WebEngine handoff;
Preferences enablement, format exclusions, size/index policy, and persistence;
index readiness, visibility, status, progress, background lifecycle, rebuild,
and failure UI; adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies,
build work, and unrelated suites are excluded and remain separately decomposed
without ranking. No public API, DTO, persistence, Core, adapter, index,
dependency, or build surface belongs to P8-FT-19.

Evidence is migrated `full_text_response_model.h/.cpp`, its focused tests, and
the P8-FT-10/P8-FT-11/P8-FT-12 model ownership, synchronization, and attachment,
plus pinned legacy `fulltextsearch.cc:690-721`.
No successor after P8-FT-19 is selected or ranked.

## Phase 8 Full-Text Result-List Selection Gate (Complete)

P8-FT-20 is complete. The post-P8-FT-19 documentation audit selected only
P8-FT-20. Completed model
ownership, accepted-response synchronization, visible list attachment, and
current-row activation make the private result list's selection and reset
contract independently testable without changing Core or an installed
interface.

Focused dialog coverage proves that the list has at most one current and
selected row; initial, empty, and error-only states have neither; and a
generation-current successful or partial response does not select a row or
steal keyboard focus. Ordinary user interaction may establish one current and
selected row. Starting a replacement clears rows, current index, and selection
atomically, and its accepted response remains unselected even
when the same headword or row position returns. Stale and cancelled completions
do not restore selection, current index, or focus.

Coverage separately begins with the list and another widget owning keyboard
focus and proves that reset preserves that ownership. Existing click, Return,
and Enter activation remains unchanged and requires a valid current row.
Existing ordered projection, duplicate rows, display/edit/tooltip roles,
complete metadata, accepted-generation synchronization, retained-result count,
service replacement, controller detachment, and teardown coverage remains
green.

The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The full implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-20 adds no test executable or
public/installed interface, so the registered Release baseline remains 109
tests.

Exact `document_id` navigation and source-dictionary targeting; non-selection
decoration, columns, delegates, icons, and additional metadata roles;
empty/error/partial messaging beyond the numeric retained-result count; match
ranges and excerpt presentation; highlighting, ignore-diacritics transfer, and
WebEngine handoff; Preferences enablement, format exclusions, size/index policy,
and persistence; index readiness, visibility, status, progress, background
lifecycle, rebuild, and failure UI; adapters, `.gdfts`, legacy `_FTS`, index
formats, dependencies, build work, and unrelated suites are excluded and remain
separately decomposed without ranking. No public API, DTO, persistence, Core,
adapter, index, dependency, or build surface belongs to P8-FT-20.

Evidence is migrated `full_text_search_dialog.h/.cpp`,
`full_text_response_model.h/.cpp`, their focused tests, and completed
P8-FT-10 through P8-FT-13, plus pinned legacy
`fulltextsearch.cc:287-315,448-449,567-579,662-673` and
`fulltextsearch.ui:99`.
No successor after P8-FT-20 is selected or ranked.

## Phase 8 Full-Text Bidirectional Result Rendering Gate (Complete)

The post-P8-FT-20 documentation audit selected only P8-FT-21. The completed
visible result list, exact display and tooltip projection, and deterministic
selection/reset contract make private per-row bidirectional painting and
elision independently testable without changing Core or an installed
interface. P8-FT-21 is complete.

Focused coverage proves that each left-to-right, right-to-left, and mixed
Unicode headword independently supplies its Qt-derived direction to painting.
Right-to-left rows paint with `Qt::RightToLeft` and `Qt::ElideLeft` when
elision is enabled; all other rows paint with `Qt::LeftToRight` and
`Qt::ElideRight`. An existing `Qt::ElideNone` remains unchanged.

Coverage includes duplicate rows and accepted replacement and proves
that delegate painting changes no model role or value, row order, tooltip,
retained-result count, selection, focus, activation, response ownership, or
accepted-generation synchronization. Existing display/edit/tooltip,
complete-metadata, count, selection, activation, service-replacement,
controller-detachment, and teardown coverage must remain green.

The focused command is
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
formats, dependencies, build work, and unrelated suites are excluded and
remain separately decomposed without ranking. No public API, DTO, persistence,
Core, adapter, index, dependency, or build surface belongs to P8-FT-21.

Evidence is migrated `full_text_search_dialog.h/.cpp`,
`full_text_response_model.h/.cpp`, their focused tests, and completed
P8-FT-12/P8-FT-18/P8-FT-19/P8-FT-20, plus pinned legacy
`fulltextsearch.cc:287-315`, `delegate.hh`, `delegate.cc:5-31`, and
`fulltextsearch.ui:99`.
No successor after P8-FT-21 is selected or ranked.

## Phase 8 Full-Text Partial-Response Status Gate (Complete)

The post-P8-FT-21 documentation audit selects only P8-FT-22. The completed
generation acceptance, response retention, and retained-result count make the
existing transport-neutral `FullTextResponse::partial` fact independently
presentable through one private Widgets status label without changing Core or
an installed interface.

Focused coverage proves that the exact text
`Results may be incomplete.` is visible only for a generation-current accepted
response whose `partial` flag is true. Initial state and replacement submission
hide it. Complete responses keep it hidden when empty, nonempty, or
error-containing. Partial responses show it with zero or nonzero retained
rows and with or without errors. Coverage must prove that Widgets does not
infer the state from result count, error presence, cancellation, or error code
and does not expose dictionary IDs, backend messages, or error details.

Repeated accepted complete/partial transitions and stale, cancelled, detached,
replaced-service, and teardown completions cannot leave or introduce an
incorrect status. Existing response/model synchronization, retained count,
selection, focus, activation, service-replacement, controller-detachment, and
teardown coverage remains green.

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
build work, and unrelated suites are excluded and remain separately decomposed
without ranking. No public API, DTO, persistence, Core, adapter, index,
dependency, or build surface belongs to P8-FT-22.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, completed P8-FT-9/P8-FT-11/P8-FT-17,
and pinned legacy `fulltextsearch.cc:448-449,499-586`. P8-FT-22 is complete.
No successor after P8-FT-22 is selected or ranked.

## Phase 8 Full-Text Empty-Result Status Gate (Complete)

The post-P8-FT-22 documentation audit selects only P8-FT-23. Completed current-
generation acceptance, complete-response retention, retained-result count, and
partial-status presentation make a conclusive empty response independently
presentable through one private Widgets status without changing Core or an
installed interface.

Focused coverage proves that exact text `No matches` is visible only
for a generation-current accepted response with zero retained results,
`partial == false`, and no errors. Initial state and replacement submission
hide it. Nonempty, partial-empty, partial-nonempty, error-only, and
result-plus-error responses hide it. The existing partial status remains
independently correct, and no dictionary ID, backend message, or error detail
is exposed.

Repeated accepted transitions and stale, cancelled, detached,
replaced-service, and teardown completions do not leave or introduce an
incorrect empty status. Existing response/model synchronization, retained
count, partial status, selection, focus, activation, service-replacement,
controller-detachment, and teardown coverage must remain green.

The focused command is
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
adapters, `.gdfts`, legacy `_FTS`, index formats, dependencies, build work, and
unrelated suites are excluded and remain separately decomposed without
ranking. No public API, DTO, persistence, Core, adapter, index, dependency, or
build surface belongs to P8-FT-23.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, completed
P8-FT-9/P8-FT-11/P8-FT-17/P8-FT-22, pinned legacy
`fulltextsearch.cc:448-449,499-586`, and migrated
`main_window.cpp:5122-5136,7706-7723`. P8-FT-23 is complete. No successor after
P8-FT-23 is selected or ranked.

## Phase 8 Full-Text Terminal Failure Status Gate (Complete)

The post-P8-FT-23 documentation audit selects only P8-FT-24. Completed current-
generation acceptance, complete-response retention, retained-result count,
partial-status presentation, and conclusive-empty presentation make an
error-only terminal response independently presentable through one private
Widgets status without changing Core or an installed interface.

Focused coverage proves that exact text `Full-text search failed`
is visible only for a generation-current accepted response with zero retained
results, `partial == false`, and one or more errors. Initial state and
replacement submission hides it. Conclusive empty, nonempty, partial-empty,
partial-nonempty, and result-plus-error responses hide it. Each existing
error-code category and multiple-error responses produce
the same generic status without exposing dictionary IDs, error codes, backend
messages, or raw details.

The result count, partial status, and empty status remain independently
correct. Repeated accepted transitions and stale, cancelled, detached,
replaced-service, and teardown completions do not leave or introduce an
incorrect failure status. Existing response/model synchronization, selection,
focus, activation, service-replacement, controller-detachment, and teardown
coverage remains green.

The focused command is
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
build work, and unrelated suites are excluded and remain separately decomposed
without ranking. No public API, DTO, persistence, Core, adapter, index,
dependency, or build surface belongs to P8-FT-24.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, completed
P8-FT-9/P8-FT-11/P8-FT-17/P8-FT-22/P8-FT-23, migrated
`main_window.cpp:6776-6791`, and pinned legacy
`fulltextsearch.cc:499-586`. P8-FT-24 is complete. No successor after P8-FT-24
is selected or ranked.

## Phase 8 Full-Text Mixed-Result Error Summary Gate (Complete)

The post-P8-FT-24 documentation audit selects only P8-FT-25. Completed current-
generation acceptance, complete-response retention, retained-result count,
partial-status presentation, and terminal-failure presentation make an
accepted response containing both retained results and errors independently
presentable through one private Widgets status without changing Core or an
installed interface.

Focused coverage proves that exact text
`Some dictionaries could not be searched` is visible only for a generation-
current accepted response with one or more retained results and one or more
errors. Initial state and replacement submission hide it. Result-free,
error-free, empty, conclusive-empty, and terminal error-only responses hide
it. One and multiple results with each existing error-code category and
multiple errors produce the same generic status without exposing
dictionary IDs, error codes, backend messages, or raw details.

Authoritative `partial == true` and `partial == false` combinations prove
that Widgets does not infer or change partiality. The mixed-result status may
coexist with `Results may be incomplete.` only when `partial == true`. The
result count, empty status, and terminal-failure status remain
independently correct. Repeated accepted transitions and stale, cancelled,
detached, replaced-service, and teardown completions do not leave or
introduce an incorrect mixed-result status. Existing response/model
synchronization, selection, focus, activation, service-replacement,
controller-detachment, and teardown coverage remains green.

The focused command is
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
build work, and unrelated suites are excluded and remain separately decomposed
without ranking. No public API, DTO, persistence, Core, adapter, index,
dependency, or build surface belongs to P8-FT-25.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, completed
P8-FT-9/P8-FT-11/P8-FT-17/P8-FT-22/P8-FT-24, and pinned legacy
`fulltextsearch.cc:499-586`. P8-FT-25 is complete. No successor after P8-FT-25
is selected or ranked.

## Phase 8 Full-Text Partial-Without-Results Status Gate (Complete)

The post-P8-FT-25 documentation audit selects only P8-FT-26. Completed current-
generation acceptance, complete-response retention, retained-result count,
partial-status presentation, and conclusive-empty presentation make an
accepted partial response with no retained results independently presentable
through one private Widgets status without changing Core or an installed
interface.

Focused coverage proves that exact text
`No matches in searched dictionaries` is visible only for a generation-current
accepted response with zero retained results and authoritative
`partial == true`. Initial state and replacement submission hide it. Complete
and nonempty responses hide it. Zero-result partial responses with no, one, and
multiple errors across each existing error-code category produce the same
generic status without exposing dictionary IDs, error codes, backend messages,
or raw details.

The status coexists with `Results may be incomplete.` and does not infer or
change authoritative partiality. The result count, conclusive-empty,
terminal-failure, and mixed-result statuses remain independently correct.
Repeated accepted transitions and stale, cancelled, detached, replaced-
service, and teardown completions do not leave or introduce an incorrect
partial-without-results status. Existing response/model synchronization,
selection, focus, activation, service-replacement, controller-detachment, and
teardown coverage remains green.

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
legacy `_FTS`, index formats, dependencies, build work, and unrelated suites
are excluded and remain separately decomposed without ranking. No public API,
DTO, persistence, Core, adapter, index, dependency, or build surface belongs to
P8-FT-26.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, completed
P8-FT-9/P8-FT-11/P8-FT-17/P8-FT-22/P8-FT-23, and pinned legacy
`fulltextsearch.cc:448-449,499-586`. P8-FT-26 is complete. No successor after
P8-FT-26 is selected or
ranked.

## Phase 8 Full-Text Accepted-Error Count Presentation Gate (Complete)

The post-P8-FT-26 documentation audit selects only P8-FT-27. Completed current-
generation acceptance and complete-response retention make the authoritative
accepted error count independently presentable through one private Widgets
status without changing Core or an installed interface.

Focused coverage proves that exact text `Errors: %1`, with `%1`
replaced by the decimal error count, is visible only for a generation-current
accepted response with one or more errors. Initial state, replacement
submission, and accepted error-free responses hide it. Zero, one, and multiple
errors across every existing error-code category preserve the ordered
collection's exact size without deduplication or inference of a dictionary
count.

The status may coexist with terminal-failure, mixed-result, partial, or
partial-empty statuses without changing their predicates or authoritative
partiality. Error-only, mixed-result, partial-empty, partial-nonempty, and
error-free responses; repeated accepted transitions; and stale, cancelled,
detached, replaced-service, and teardown completions do not leave or
introduce an incorrect count. No dictionary ID, error code, backend message,
or raw detail is displayed. Existing response/model synchronization, result
ordering, count, selection, focus, activation, and status coverage remains
green.

The focused command is
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
legacy `_FTS`, index formats, dependencies, build work, and unrelated suites
are excluded and remain separately decomposed without ranking. No public API,
DTO, persistence, Core, adapter, index, dependency, or build surface belongs to
P8-FT-27.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextResponse` contract, completed P8-FT-9/P8-FT-11/P8-FT-22
through P8-FT-26, and pinned legacy `fulltextsearch.cc:448-449,499-586`. No
successor after P8-FT-27 is selected or ranked.

P8-FT-27 is complete. The existing private dialog QTest covers exact accepted
counts, all error categories, response-status coexistence, replacement and
lifecycle safety, and raw-detail privacy without changing the 109-test
registration baseline. No successor is selected or ranked.

## Phase 8 Full-Text Accepted-Query Activation Context Gate (Complete)

The post-P8-FT-27 documentation audit selects only P8-FT-28. Completed current-
generation acceptance, response retention, immutable activation scope, and
exact result delivery make the submitted query text and authoritative
`ignore_diacritics` value independently retainable as private activation
context without changing Core or an installed interface.

Focused coverage proves that each submitted generation captures its
exact UTF-8 query text and `ignore_diacritics` value and that only acceptance of
the current terminal generation makes that pair activatable. Activation must
deliver the accepted pair by value with the existing exact result and ordered,
authoritative-empty, or absent dictionary scope. Editing the composer or
projected query after acceptance must not change the delivered context.

Replacement submission must clear the previous accepted pair. Repeated
accepted generations, copied-lifetime independence, stale and cancelled
completion, controller detachment, service replacement, and teardown must not
retain, revive, or overwrite incorrect context. Existing result projection,
ordering, selection, focus, count, statuses, response synchronization, scope,
and current scoped-navigation coverage must remain green.

The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The completed implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-28 adds no test executable or
public/installed interface, so the registered Release baseline remains 109
tests.

Highlighting, ignore-diacritics transfer, WebEngine handoff, match-range or
excerpt presentation, exact `document_id` navigation and source targeting,
columns/icons/additional metadata roles and other decoration, Preferences and
index lifecycle, adapters and index formats, dependencies, build work, and
unrelated suites are excluded and remain separately decomposed without
ranking. No navigation, article presentation, WebEngine, public API, DTO,
persistence, Core, adapter, index, dependency, or build surface belongs to
P8-FT-28.

Evidence is migrated `full_text_search_dialog.h/.cpp`, its focused tests, the
installed `FullTextQuery` and `FullTextResult` contracts, completed
P8-FT-9/P8-FT-11/P8-FT-13/P8-FT-15/P8-FT-16, migrated
`main_window.cpp:5887-5920`, and pinned legacy
`fulltextsearch.cc:499-586`. No successor after P8-FT-28 is selected or ranked.

## Phase 8 Full-Text Accepted-Query Article-Search Handoff Gate (Complete)

The post-P8-FT-28 documentation audit selects only P8-FT-29. Completed exact
accepted-query delivery and current-tab scoped navigation make a private
handoff to MainWindow's existing per-tab article-search state and literal Qt
WebEngine find operation independently testable without changing Core or an
installed interface.

Focused coverage proves that successful full-text activation and
navigation acceptance replace the target tab's prior article-search query and
status with the exact accepted UTF-8 query text. Successful loading of the
corresponding nonempty lookup page dispatches that same text exactly once
through the existing literal find operation. Match and no-match callbacks must
update only the generation-current per-tab status and project it through the
search-in-page controls only while that tab is active.

Coverage must isolate inactive tabs and reject stale lookup, load, and find
callbacks by tab ID, lookup/presentation generation, and live `ArticleView`.
Failed navigation must preserve existing article-search state. Failed or empty
lookup, tab closure or replacement, facade replacement, and teardown must not
dispatch, retain, revive, or project the pending full-text query. Ordinary
lookup, internal-link navigation, history/session replay, and manual
search-in-page behavior must remain unchanged, as must existing full-text
activation, dictionary scope, navigation identity, and main-query preservation.

The focused command is
`ctest --preset conan-release -R '^(goldendict_full_text_dialog_smoke|goldendict_webengine_interaction_smoke)$'`
after the Release target has been built. The completed implementation gate is
Linux Release configure/build, full `ctest --preset conan-release`
without an unintended registration delta, clean exact-SCM `conan create` with
the Release Qt WebEngine host profile and packaged consumers, Release install,
and standalone installed C and C++ consumers. P8-FT-29 adds no test executable
or public/installed interface, so the registered Release baseline remains 109
tests.

The delivered `ignore_diacritics` value is excluded because Qt WebEngine's
literal find interface exposes no corresponding policy. Legacy regular-
expression equivalence, match ranges and excerpts, exact `document_id`
navigation and source targeting, decoration, Preferences and index lifecycle,
adapters and index formats, dependencies, build work, and unrelated suites are
excluded and remain separately decomposed without ranking. No public API, DTO,
persistence, Core, adapter, index, dependency, or build surface belongs to
P8-FT-29.

Evidence is completed P8-FT-16/P8-FT-28, migrated
`full_text_search_dialog.h/.cpp`, `main_window.cpp:5887-5920,7556-7730`, the
existing full-text-dialog and WebEngine interaction smokes, and pinned legacy
`fulltextsearch.cc:499-586`. P8-FT-29 is complete. No successor after P8-FT-29
is selected or ranked.

## Phase 8 Full-Text Accepted-Completion Notification Gate (Complete)

The post-P8-FT-29 documentation audit selected only P8-FT-30. The completed
generation-current terminal-response acceptance boundary makes one private
Widgets completion notification independently testable without changing Core
or an installed interface.

Focused coverage proves exactly one `QApplication::beep()` for every
generation-current accepted response: nonempty success, conclusive empty,
partial-empty, partial-nonempty, error-only, and mixed-result. It also proves
one notification for each later accepted replacement generation without
duplicating a notification for the same generation.

Initial, pending, progress, and submission states remain silent. Explicit
cancellation, stale or duplicate completion, controller detachment, service or
facade replacement, dialog destruction, and MainWindow teardown must not
notify. Existing response retention, rows, ordering, selection, focus,
activation, counts, statuses, navigation, and article-search behavior must
remain unchanged.

The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The full implementation gate is Linux Release
configure/build, full `ctest --preset conan-release` without an unintended
registration delta, clean exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers, Release install, and standalone
installed C and C++ consumers. P8-FT-30 adds no test executable or public/
installed interface, so the registered Release baseline remains 109 tests.

Ignore-diacritics semantics and legacy regular-expression equivalence, match
ranges and excerpts, exact `document_id` navigation and source targeting,
decoration, Preferences and index lifecycle, adapters and index formats,
dependencies, build work, and unrelated suites are excluded and remain
separately decomposed without ranking. No public API, DTO, persistence, Core,
adapter, index, dependency, or build surface belongs to P8-FT-30.

Evidence is completed P8-FT-9 and the extended focused dialog tests, migrated
`full_text_search_dialog.cpp:203-253`, and pinned legacy
`fulltextsearch.cc:547-579`. P8-FT-30 is complete. No successor after P8-FT-30
is selected or ranked.

## Phase 8 Full-Text Idle Dialog Dismissal Gate (Complete)

The post-P8-FT-30 documentation audit selected only P8-FT-31. The completed
modeless-dialog ownership and active-request cancellation boundaries make idle
dismissal through the existing Cancel control independently testable without
changing Core or an installed interface.

Focused coverage must prove that Cancel is available initially and after every
return to idle. Activating it initially, after a generation-current accepted
response, or after explicit active-request cancellation must close and destroy
the dialog, clear MainWindow's guarded ownership, and allow a later Search-menu
trigger to create a fresh dialog with the established initialization behavior.

Activating the same control while a generation is active must cancel only that
request, restore idle controls, and leave the dialog open. Submission,
generation-current completion, stale or duplicate completion, service or
facade replacement, controller detachment, and teardown must not dismiss it.
Repeated cancellation or dismissal commands and destroyed-dialog callbacks
must remain safe. Existing window-manager close, response, row, ordering,
selection, focus, activation, count, status, notification, navigation, and
article-search behavior must remain unchanged.

The focused command is
`ctest --preset conan-release -R '^(full_text_search_dialog_test|goldendict_full_text_dialog_smoke)$'`
after the Release targets have been built. The full implementation gate is
Linux Release configure/build, full `ctest --preset conan-release` without an
unintended registration delta, clean exact-SCM `conan create` with the Release
Qt WebEngine host profile and packaged consumers, Release install, and
standalone installed C and C++ consumers. P8-FT-31 adds no test executable or
public/installed interface, so the registered Release baseline remains 109
tests.

Ignore-diacritics semantics and legacy regular-expression equivalence, match
ranges and excerpts, exact `document_id` navigation and source targeting,
decoration, Preferences and index lifecycle, adapters and index formats,
dependencies, build work, and unrelated suites are excluded and remain
separately decomposed without ranking. No public API, DTO, persistence, Core,
adapter, index, dependency, or build surface belongs to P8-FT-31.

Evidence is completed P8-FT-8/P8-FT-9, migrated
`full_text_search_dialog.cpp:184-218,285-289`, the focused dialog and MainWindow
full-text tests, and pinned legacy `fulltextsearch.cc:582-592`. P8-FT-31 is
complete. No successor after P8-FT-31 is selected or ranked.

## Phase 8 Full-Text Dialog Geometry Persistence Gate (Complete)

The post-P8-FT-31 documentation audit selects only P8-FT-32. The existing
bounded opaque main-window geometry contract makes an independent full-text
dialog geometry persistence prerequisite testable before any Widgets capture
or restoration behavior is selected.

`application_service_test` covers an empty default when current and legacy
data are absent, exact current-format round-trip and canonical save, and exact
legacy migration from `preferences/fullTextSearch/dialogGeometry`. The decoded
value must accept the 64 KiB boundary and reject a larger value. Duplicate,
malformed, and oversized recognized current or legacy input must reject the
complete operation atomically without modifying the legacy source, emitting a
partial current file, or changing unrelated configuration fields.

Installed C and C++ consumer checks compile against the expanded installed
surface, and the C++ consumer accesses the transport-neutral configuration
DTO; the C consumer's version-header contract is unchanged. Core tests prove
that persistence does not interpret the opaque value. No dialog test or Widgets
smoke may claim capture, restore, save-on-close, placement, screen validation,
or fallback behavior in this leaf.

The focused command is
`ctest --preset conan-release -R '^application_service_test$'` after the
Release target has been built. The full implementation gate is Linux Release
configure/build, full `ctest --preset conan-release` without an unintended
registration delta, clean exact-SCM `conan create` with the Release Qt
WebEngine host profile and packaged consumers, Release install, and standalone
installed C and C++ consumers. P8-FT-32 adds no test executable or dependency,
so the registered Release baseline remains 109 tests.

Widgets geometry capture/restoration, dialog behavior, requests, response
presentation, activation, navigation, article search, ignore-diacritics and
regular-expression equivalence, excerpts, exact document/source targeting,
decoration, Preferences/index policy, index lifecycle, adapters/index formats,
dependencies, builds, and unrelated suites are excluded and remain separately
decomposed without ranking.

Evidence is the current and legacy main-window geometry coverage in
`application_service_test` and pinned legacy `config.hh:156-181`,
`config.cc:1008-1044,1990-2027`, and
`fulltextsearch.cc:195-221,387-399`. P8-FT-32 is complete. No successor after
P8-FT-32 is selected or ranked.

## Phase 8 Full-Text Dialog Geometry Widgets Connection Gate (Complete)

The post-P8-FT-32 documentation audit selected only P8-FT-33. Existing bounded
Core persistence, the modeless dialog's Qt ownership, and the composition
root's atomic save path make the Widgets connection independently testable.

The completed implementation extends `full_text_search_dialog_test` and
`goldendict_full_text_dialog_smoke`; add no executable or registered test.
Coverage must prove absent geometry preserves the default, valid geometry is
restored exactly once on new-dialog creation, and Qt-invalid geometry leaves
the default intact without rewriting stored bytes. Idle Cancel and window-
manager close must capture exact `saveGeometry()` bytes before destruction,
and application reconstruction must restore them. Active Cancel remains
cancellation-only and must not capture, save, or dismiss. Completion,
activation, replacement, service replacement, detachment, and teardown must
not independently save geometry; existing workflow assertions remain green.

The focused command is
`ctest --preset conan-release -R '^(full_text_search_dialog_test|goldendict_full_text_dialog_smoke)$'`
after the Release target has been built. The full implementation gate is Linux
Release configure/build, exactly 109 registered tests and full
`ctest --preset conan-release`, clean committed exact-SCM `conan create` with
the Release Qt WebEngine host profile and packaged consumers, Release install,
and standalone installed C and C++ consumers. P8-FT-33 changes no installed
interface, so both consumers remain unchanged and source-compatible; they are
still required as the stronger package gate and continue to exercise the
P8-FT-32 installed configuration surface.

The Release registration baseline remains exactly 109 tests. Screen/topology
normalization, placement fallback, other dialog state,
query semantics, excerpts, exact document/source targeting, decoration,
Preferences/index policy, index lifecycle, adapters/index formats,
dependencies, builds, and unrelated suites remain excluded, unselected, and
unranked. No successor after P8-FT-33 is selected or ranked.

## Phase 8 Full-Text Dialog Minimum-Size Gate (Complete)

The post-P8-FT-33 documentation audit selects only P8-FT-34. The existing
private modeless dialog and Widgets-owned geometry restoration make the pinned
legacy minimum-size contract independently testable without a Core or public
interface change.

Extend `full_text_search_dialog_test`; add no executable or registered test.
Coverage must prove every new dialog reports `minimumWidth() == 430` and
`minimumHeight() == 450`, direct attempts to resize below either bound are
clamped, and restored geometry below either bound cannot leave the dialog
undersized. Valid larger geometry must retain its P8-FT-33 behavior. Absence or
Qt rejection must still preserve the default layout, and geometry capture,
idle dismissal, active cancellation, completion, activation, replacement,
service replacement, detachment, and teardown must remain unchanged.

The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The full implementation gate is Linux Release
configure/build, exactly 109 registered tests and full
`ctest --preset conan-release`, clean committed exact-SCM `conan create` with
the Release Qt WebEngine host profile and packaged consumers, Release install,
and standalone installed C and C++ consumers. P8-FT-34 changes no installed
interface, so both consumers remain unchanged and source-compatible; they are
still required as the stronger package gate.

The completed implementation extends the existing test executable without a
registration change. Default or initial size, maximum size, aspect ratio,
screen/topology normalization, placement fallback, DPI policy beyond Qt
logical sizing, other dialog state, query semantics, excerpts, exact
document/source targeting, decoration, Preferences/index policy, index
lifecycle, adapters/index formats, dependencies, builds, and unrelated suites
remain excluded, unselected, and unranked. No successor after P8-FT-34 is
selected or ranked. The implementation gate must stop on ref/worktree drift,
legacy dirtiness, ambiguous sizing evidence or acceptance behavior, an
architectural choice requiring HTTP GET policy, or scope expansion; pinned
checks repeat before commit and push.

### Phase 8 full-text dialog initial-size acceptance (complete)

The documentation-only post-P8-FT-34 audit selected P8-FT-35 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:5-12` requires the private Widgets dialog
to initialize at exactly 492 by 593 logical pixels when stored geometry is
absent or rejected by Qt. P8-FT-34's 430-by-450 minimum is established first,
and P8-FT-33's valid one-time restore continues to override the initial size.

The completed implementation extends `full_text_search_dialog_test`; it adds
no executable or registered test. Coverage proves exact
`size() == QSize(492, 593)` for absent and rejected
geometry, valid larger restoration, undersized restoration clamping, and later
resizing to another valid size. Existing geometry capture, dismissal,
cancellation, completion, activation, replacement, service replacement,
detachment, teardown, and minimum-size coverage remain unchanged.

The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release build. The full implementation gate is Linux Release configure/build,
exactly 109 registered tests and full `ctest --preset conan-release`, clean
committed exact-SCM `conan create` with the Release Qt WebEngine host profile
and packaged consumers, Release install, and standalone installed C and C++
consumers. P8-FT-35 changes no installed interface, so both consumers remain
unchanged and source-compatible but are retained as the stronger package gate.

Help integration, maximum/aspect constraints, screen/topology normalization,
placement fallback, DPI policy beyond Qt logical sizing, other dialog state,
query semantics, excerpts, document/source targeting, decoration,
Preferences/index policy, index lifecycle, adapters/index formats,
dependencies, builds, and unrelated suites remain excluded and unranked. No
successor after P8-FT-35 is selected or ranked. Implementation must stop on
ref/worktree drift, legacy dirtiness, ambiguous sizing evidence or acceptance,
an architectural choice requiring HTTP GET policy, or scope expansion; pinned
checks repeat before commit and push.

### Phase 8 full-text Help activation-intent acceptance (complete)

The documentation-only post-P8-FT-35 audit selects P8-FT-36 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:250-255` and
`fulltextsearch.cc:300-309,676-680` require one private `Help` button and one F1
action with `Qt::WidgetWithChildrenShortcut` to produce the same dialog help
request. Pre-P8-FT-36 Widgets had neither activation path.

The completed focused coverage extends `full_text_search_dialog_test`; it adds
no executable or registered test. It proves `fullTextHelpButton` and its exact
`Help` text, `fullTextHelpAction` and its exact F1 shortcut and
widget-with-children context, exactly one argument-free private
`HelpRequested()` signal per button or shortcut activation,
and deterministic repeated independent activations. Both idle and active
search cases must leave the modeless dialog open and preserve request,
cancellation, query, accepted response, model, selection, completion,
geometry capture, configuration, service replacement, detachment, and teardown
behavior; no focus policy beyond normal Qt button/shortcut activation is added.

The focused command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release build. The full implementation gate is Linux Release
configure/build, exactly 109 registered tests and full
`ctest --preset conan-release`, clean committed exact-SCM `conan create` with
the Release Qt WebEngine host profile and packaged consumers, Release install,
and standalone installed C and C++ consumers. P8-FT-36 changes no installed
header, DTO, ABI, dependency, CMake export, or Conan requirement, so both
consumers remain unchanged and source-compatible.

Help content, destination and transport, composition-root consumption,
Help-menu changes, Core/public contracts, dependencies, installed surfaces,
and every other parity gap remain excluded and unranked. No successor after
P8-FT-36 is selected or ranked. Implementation must stop on ref/worktree
drift, legacy dirtiness, ambiguous help evidence or acceptance, any help
destination or transport choice, a public/Core or composition-root contract,
dependency or installed-surface change, an architectural decision requiring
HTTP GET policy, or scope expansion.

### Phase 8 full-text Search button default-policy acceptance (complete)

The documentation-only post-P8-FT-36 audit selects P8-FT-37 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:204-214` makes the Search `OKButton` the
explicit default and disables its auto-default property. The pre-P8-FT-37
`full_text_search_dialog.cpp:133-136` preserved the explicit default but not
the non-auto-default policy, and focused coverage pinned neither property.

Completed focused acceptance extends only `full_text_search_dialog_test`. It
identify `fullTextSearchButton`, verify `isDefault() == true` and
`autoDefault() == false` after construction, and prove both remain unchanged
through idle state, active submission, accepted completion, and active
cancellation. Existing Search submission, active/idle Cancel, and button/F1
Help coverage must remain green, while the property correction itself must not
submit, cancel, close, move focus, or mutate query, response, result model,
selection, geometry, or configuration.

The focused Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built. The full implementation gate remains
Linux Release configure/build, exactly 109 registered tests and full
`ctest --preset conan-release`, clean committed exact-SCM `conan create` with
the Release Qt WebEngine host profile and packaged consumers, Release install,
and standalone installed C and C++ consumers. P8-FT-37 adds no executable or
test registration and changes no installed header, DTO, ABI, dependency,
CMake export, or Conan requirement; both consumers remain unchanged and
source-compatible.

Tab order, focus assignment or transfer, Return/Enter behavior beyond Qt's
existing explicit-default semantics, other buttons' default policy, help
consumption, query/result/navigation behavior, index lifecycle/UI,
Preferences, adapters/index formats, dependencies/builds, and unrelated tests
are excluded and unranked. No successor after P8-FT-37 is selected or ranked.
Implementation must stop on ref/worktree drift, legacy dirtiness, ambiguous
default-button evidence or acceptance semantics, a broader focus, tab-order,
or keyboard-policy decision, public/Core or composition-root expansion,
dependency or installed-surface change, HTTP GET policy, or scope expansion.

This selection audit changes documentation only, so it intentionally skips
compiled verification. Its gate is exact four-file scope, cross-document
consistency, Phase terminology, successor language, and `git diff --check`.

### Phase 8 full-text dialog tab-sequence acceptance (complete)

The documentation-only post-P8-FT-37 audit selects P8-FT-38 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:274-285` authenticates one consecutive
forward tab chain through the mapped private Widgets controls:
`fullTextQueryText`, `fullTextSearchResults`,
`fullTextUseMaximumWordDistance`, `fullTextMaximumWordDistance`,
`fullTextQueryMode`, `fullTextUseMaximumArticles`,
`fullTextMaximumArticlesPerDictionary`, `fullTextMatchCase`,
`fullTextSearchButton`, and `fullTextCancelButton`.

Completed focused acceptance extends only
`full_text_search_dialog_test`. It inspects the exact named forward chain after
construction and through idle, submission, completion, and active cancellation,
including transitions that temporarily disable controls. It also retains
regressions for initial query focus, Search default/non-auto-default policy,
Cancel behavior, Help activation, and request lifecycle. The focused Release
command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built.

The implementation gate remains Linux Release configure/build, exactly 109
registered tests and full `ctest --preset conan-release`, clean committed
exact-SCM `conan create` with the Release Qt WebEngine host profile and
packaged consumers, Release install, and standalone installed C and C++
consumers. P8-FT-38 adds no executable, registration, installed interface,
dependency, CMake export, or Conan requirement; both consumers remain
unchanged and source-compatible.

Initial/transferred focus, focus-policy changes, traversal before the query or
after Cancel, wraparound, and placement of Ignore Diacritics, Ignore Word
Order, Help, or other omitted controls are outside acceptance. Return/Enter
dispatch, shortcuts, button default policies, search behavior, and unrelated
tests are also excluded and unranked. No successor after P8-FT-38 is selected
or ranked. Broader keyboard/focus policy, public/Core or composition-root
expansion, dependencies, installed surfaces, and unrelated coverage remain
separately reviewed and unranked.

## Phase 8 Full-Text Result-Count Minimum-Height Gate (Completed)

The documentation-only post-P8-FT-38 audit selected P8-FT-39 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:103-115` gives the result-count label a
minimum height of 21 logical pixels; the mapped current private
`fullTextArticlesFoundLabel` now has the equivalent minimum.

Completed focused acceptance extends only `full_text_search_dialog_test`. It
proves `minimumHeight() == 21` after construction and through idle, submission,
generation-current accepted completion, active cancellation, replacement, and
service/controller lifecycle transitions. Existing count text and visibility,
response statuses, progress behavior, layout ownership, sizing above the
minimum, and P8-FT-34/P8-FT-35 geometry coverage remain unchanged. The focused
Release command is
`ctest --preset conan-release -R '^full_text_search_dialog_test$'` after the
Release target has been built.

The implementation gate remains Linux Release configure/build, exactly 109
registered tests and full `ctest --preset conan-release`, clean committed
exact-SCM `conan create` with the Release Qt WebEngine host profile and
packaged consumers, Release install, and standalone installed C and C++
consumers. P8-FT-39 adds no executable, registration, installed interface,
dependency, CMake export, or Conan requirement; both consumers remain
unchanged and source-compatible.

Result-count wording/localization, width policy, fixed or maximum height,
font/style/DPI policy, count/progress rearrangement, response-status layout,
progress behavior/alignment, dialog geometry, focus/tab/key behavior,
search/results, index lifecycle/UI, Preferences, adapters/index formats,
dependencies/builds, and unrelated tests are excluded and unranked. No
successor after P8-FT-39 is selected or ranked.

The completed implementation extends the existing test executable without a
registration change. The Release registration baseline remains exactly 109
tests; no successor after P8-FT-39 is selected or ranked.

## Phase 8 Full-Text Progress-Bar Alignment Gate (Complete)

The documentation-only post-P8-FT-39 audit selected P8-FT-40 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:117-128` gives the progress bar the
explicit alignment `Qt::AlignCenter`; the mapped current private
`fullTextSearchProgress` now preserves it alongside the indeterminate range.

Completed focused acceptance extends only `full_text_search_dialog_test`. It
proves `alignment() == Qt::AlignCenter` after construction and through idle,
submission, generation-current accepted completion, active cancellation,
replacement, and service/controller lifecycle transitions. It also retains
the existing indeterminate range, visibility, request lifecycle,
result/count/status, layout, geometry, search, and cancellation regressions.
The focused Release command is:

```sh
ctest --preset conan-release -R '^full_text_search_dialog_test$'
```

The full implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers, Release install, and unchanged standalone installed C and C++
consumers. P8-FT-40 adds no executable, test registration, installed header,
DTO, ABI, dependency, CMake export, or Conan requirement.

Progress text/format and text visibility, value/range policy, orientation,
inversion, style/animation, size policy, layout rearrangement, indexing
progress UI/lifecycle, platform-specific styling, public/Core or
composition-root expansion, dependencies/builds, and unrelated tests are
excluded and unranked. No successor after P8-FT-40 is selected or ranked.

The implementation gate is exact production and focused-test scope, exactly
four governing documentation updates, cross-document consistency, Phase
terminology, successor language, and the full Release, install, consumer, and
exact-SCM package verification described above.

## Phase 8 Full-Text Result-Count/Progress Row Gate (Complete)

The documentation-only post-P8-FT-40 audit selects P8-FT-41 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:102-129` places the result-count label
first and progress bar second in one horizontal layout; the mapped current
private widgets are separate vertical-layout items with response statuses
between them. P8-FT-17, P8-FT-39, and P8-FT-40 supply the complete presentation
prerequisites.

Completed focused acceptance extends only `full_text_search_dialog_test`. It
proves one horizontal row attached to the enclosing vertical layout, containing
the unique direct dialog children `fullTextArticlesFoundLabel` first and
`fullTextSearchProgress` second, after construction and through idle,
submission, generation-current accepted completion, active cancellation,
replacement, service replacement, and controller detachment. It retains count
text/minimum-height, progress alignment/range/visibility, response-status
order/behavior, request-lifecycle, geometry, focus-chain, search, and
cancellation regressions. The focused Release command is:

```sh
ctest --preset conan-release -R '^full_text_search_dialog_test$'
```

The full implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers, Release install, and unchanged standalone installed C and C++
consumers. P8-FT-41 adds no executable, test registration, installed header,
DTO, ABI, dependency, CMake export, or Conan requirement.

Spacing, margins, stretch factors, size policies, widths, status-widget
rearrangement, broader layout redesign, progress behavior/style, geometry,
keyboard behavior, indexing lifecycle/UI, Preferences, adapters/index formats,
dependencies/builds, public/Core or composition-root expansion, and unrelated
tests are excluded and unranked. No successor after P8-FT-41 is selected or
ranked.

The completed implementation changes only private dialog construction, its
existing focused test, and these four governing documents. Its gate is exact
scope, cross-document consistency, Phase terminology, successor language, and
the full Release, install, consumer, and exact-SCM verification described
above. The implementation stops on ref/worktree drift, legacy dirtiness,
ambiguous layout evidence or acceptance semantics, any spacing, style, or
broader layout choice, public/Core or composition-root expansion, dependency or installed-
surface change, an architectural decision requiring HTTP GET policy, discovery
of another required file, or scope expansion.

## Phase 8 Full-Text Button-Row Spacer Sequence Gate (Complete)

The documentation-only post-P8-FT-41 audit selects P8-FT-42 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:190-270` places expanding horizontal
spacers before Search, between Search and Cancel, between Cancel and Help, and
after Help; the mapped current private row retains only the two outer
stretches. P8-FT-36 through P8-FT-38 supply the complete Help, Search-default,
and tab-sequence prerequisites.

Completed focused acceptance extends only `full_text_search_dialog_test`. It
proves one button row attached to the enclosing vertical layout with the exact
seven-item sequence spacer, `fullTextSearchButton`, spacer,
`fullTextCancelButton`, spacer, `fullTextHelpButton`, spacer; all four spacers
expand horizontally, and all three buttons remain unique direct dialog
children. The relationship is verified after construction and through idle,
submission, generation-current accepted completion, active cancellation,
replacement, service replacement, and controller detachment while retaining
button identity/text/order/parentage, Search default policy, tab chain, Help
intent, Cancel lifecycle, request/response, and geometry regressions. The
focused Release command is:

```sh
ctest --preset conan-release -R '^full_text_search_dialog_test$'
```

The full implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, clean committed exact-SCM
`conan create` with the Release Qt WebEngine host profile and packaged
consumers, Release install, and unchanged standalone installed C and C++
consumers. P8-FT-42 adds no executable, test registration, installed header,
DTO, ABI, dependency, CMake export, or Conan requirement.

Exact spacer size hints, stretch factors, margins, layout spacing, button sizes
or size policies, button reordering, broader layout/style work, indexing
lifecycle/UI, Preferences, adapters/index formats, dependencies/builds,
public/Core or composition-root expansion, HTTP GET policy, and unrelated tests
are excluded and unranked. No successor after P8-FT-42 is selected or ranked.

The completed implementation changes only private dialog construction, its
existing focused test, and these four governing documents. Its gate is exact
scope, cross-document consistency, Phase terminology, successor neutrality,
and the full Release, install, consumer, and exact-SCM verification described
above. Implementation stops on ref/worktree drift, legacy dirtiness, ambiguous
layout evidence or acceptance semantics, any exact spacer-size, stretch-factor,
margin, spacing, style, or broader layout choice, public/Core or composition-
root expansion, dependency or installed-surface change, an architectural
decision requiring HTTP GET policy, discovery of another required file, or
scope expansion.

## Phase 8 Full-Text Search Group-Box Boundary Gate (Complete)

The documentation-only post-P8-FT-42 audit selects P8-FT-43 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:23-96` places the query field and search-
option controls in one `QGroupBox` titled exactly `Search`; current
`full_text_search_dialog.cpp:65-70` instead attaches the mapped private
`FullTextQueryComposer` directly to the dialog's enclosing vertical layout.
P8-FT-1 through P8-FT-42 supply the complete query, tab-order, presentation,
layout, geometry, and request-lifecycle prerequisites.

Completed focused acceptance extends only `full_text_search_dialog_test`. It
proves exactly one private group box titled `Search`, the unique existing
composer directly contained by it, the group box attached once to the
enclosing dialog layout, and stable identity and hierarchy through
construction, idle, submission, generation-current accepted completion,
active cancellation, replacement, service replacement, and controller
detachment. It also retains regressions for query values, labels, control
ordering and enablement, composition semantics, focus/tab behavior,
submission, responses, result-count/progress row, statuses, button row,
geometry, and lifecycle behavior. Add no executable or registered test.

Run the focused Release test with:

```sh
ctest --preset conan-release -R '^full_text_search_dialog_test$'
```

The full implementation gate remains Linux Release configure/build,
exactly 109 registered tests, full Release CTest, Release install, packaged
consumers, standalone installed C and C++ consumers, and clean committed exact-
SCM creation with the profile's Debug build type explicitly overridden for the
host after both profiles:

```sh
conan create . --build=missing \
  -pr:h=profiles/qt-webengine -pr:b=default \
  -s:h build_type=Release
```

P8-FT-43 adds no executable, test registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. Group-box margins, spacing,
size policy, alignment, styling, checkability, flatness, mnemonic policy,
broader composer-layout parity, indexing lifecycle/UI, Preferences, adapters
and index formats, dependencies/builds, public/Core or composition-root
changes, HTTP GET policy, and unrelated tests are excluded and unranked. No
successor after P8-FT-43 is selected or ranked.

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

## Phase 8 Full-Text Ignore-Words-Order Label Gate (Complete)

The documentation-only post-P8-FT-43 audit selects P8-FT-44 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:83-89` gives the existing checkbox the
exact translatable text `Ignore words order`; before P8-FT-44,
`full_text_query_composer.cpp:89-92` used `Ignore word order` for the mapped
unique private `fullTextIgnoreWordOrder` checkbox. P8-FT-1 through P8-FT-43
supply its complete query, persistence, control, tab, containing-layout, and
request-lifecycle prerequisites.

Completed focused acceptance extends only `full_text_query_composer_test`. It
proves the unique checkbox has exact text `Ignore words order` and preserves
its identity, parentage, checked/enabled state, ordering, focus/tab behavior,
mode-dependent behavior, and query composition through relevant control
transitions. Dialog submission, generation-current accepted completion,
active cancellation, replacement, service replacement, controller detachment,
responses, and geometry retain their existing regressions. Add no executable
or registered test.

Run the focused Release test with:

```sh
ctest --preset conan-release -R '^full_text_query_composer_test$'
```

The full implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, Release install, packaged consumers,
standalone installed C and C++ consumers, and clean committed exact-SCM
creation with:

```sh
conan create . --build=missing \
  -pr:h=profiles/qt-webengine -pr:b=default \
  -s:h build_type=Release
```

P8-FT-44 adds no executable, test registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. Layout, mnemonic policy,
translation-catalog work, other labels, grammar modernization, indexing
lifecycle/UI, Preferences, adapters and index formats, dependencies/builds,
public/Core or composition-root changes, HTTP GET policy, and unrelated tests
are excluded and unranked. No successor after P8-FT-44 is selected or ranked.

P8-FT-44 is complete. The implementation changes only the private composer,
its existing focused test, and these four governing documents. Implementation
must stop on ref/worktree drift, legacy dirtiness, ambiguous text mapping or
acceptance semantics, any second label or behavior change, layout, mnemonic,
translation-catalog, public/Core or composition-root expansion, dependency or
installed-surface change, an architectural decision requiring HTTP GET policy,
discovery of another required file, or scope expansion. Its gate is exact
six-file scope, cross-document consistency, Phase terminology, successor
neutrality, and the full Release, install, consumer, and exact-SCM verification
described above.

## Phase 8 Full-Text Query-Mode Label Gate (Complete)

The documentation-only post-P8-FT-44 audit selects P8-FT-45 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:41-53` associates the exact translatable
text `Mode:` with the unique search-mode selector. Before P8-FT-45,
`full_text_query_composer.cpp:60-74,131-134` maps that selector to the unique
private `fullTextQueryMode` combo box but labeled its `QFormLayout` row `Mode`.
P8-FT-1 through P8-FT-44 supply its complete persistence, mode, composition,
control, containing-layout, focus/tab, and request-lifecycle prerequisites.

Completed focused acceptance extends only `full_text_query_composer_test`. It uses
`QFormLayout::labelForField()` to prove exactly one label is associated with
the unique selector and has exact text `Mode:`. It preserves selector identity,
values, ordering, current/enabled state, focus/tab behavior, all four mode
transitions, repeated composition, and query semantics. Existing dialog tests
retain submission, generation-current accepted completion, active
cancellation, replacement, service replacement, controller detachment,
responses, geometry, and lifecycle regressions. Add no executable or
registered test.

Run the focused Release test with:

```sh
ctest --preset conan-release -R '^full_text_query_composer_test$'
```

The full implementation gate remains Linux Release configure/build,
exactly 109 registered tests, full Release CTest, Release install, packaged
consumers, standalone installed C and C++ consumers, and clean committed exact-
SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-45 adds no executable, test registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. Layout restructuring, any other
label, mnemonic policy, translation-catalog work, grammar modernization,
indexing lifecycle/UI, Preferences, adapters and index formats,
dependencies/builds, public/Core or composition-root changes, HTTP GET policy,
and unrelated tests are excluded and unranked. No successor after P8-FT-45 is
selected or ranked.

P8-FT-45 is complete. The implementation changes only the private composer,
its existing focused test, and these four governing documents. Implementation
must stop on ref/worktree drift, legacy dirtiness, ambiguous label mapping or
acceptance semantics, any
second label or behavior change, layout restructuring, mnemonic or
translation-catalog work, public/Core or composition-root expansion,
dependency or installed-surface change, an architectural decision requiring
HTTP GET policy, discovery of another required file, or scope expansion. This
audit's gate is exact four-file scope, cross-document consistency, Phase
terminology, successor neutrality, `git diff --check`, and clean pinned refs
and worktrees.

## Phase 8 Full-Text Query-Field Label Gate (Complete)

The documentation-only post-P8-FT-45 audit selects P8-FT-46 as the sole next
leaf. Pinned legacy `fulltextsearch.ui:28-31` places the unique query
`QLineEdit` directly in the Search group without a label. Before P8-FT-46,
`full_text_query_composer.cpp:57-58,131-133` mapped that field to the unique
private `fullTextQueryText` line edit but added a `QFormLayout` label with text
`Query`. P8-FT-1 through P8-FT-45 supply the field, composition, persistence,
containing-layout, focus/tab, request-lifecycle, and adjacent mode-label
prerequisites.

Completed focused acceptance extends only `full_text_query_composer_test`. It
uses `QFormLayout::labelForField()` to prove the unique query field has no associated
label while preserving identity, parentage, value, text mutation, mode and
option transitions, and repeated composition. Existing dialog tests retain
submission, generation-current accepted completion, active cancellation,
replacement, service replacement, controller detachment, responses, geometry,
focus, tab, and lifecycle regressions. Add no executable or registered test.

Run the focused Release test with:

```sh
ctest --preset conan-release -R '^full_text_query_composer_test$'
```

The implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, Release install, packaged consumers,
standalone installed C and C++ consumers, and clean committed exact-SCM
creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-46 adds no executable, test registration, installed header, DTO,
ABI, dependency, CMake export, or Conan requirement. Any second label or
behavior, broader layout restructuring, spacing, margins, mnemonic policy,
translation-catalog work, indexing lifecycle/UI, Preferences, adapters and
index formats, dependencies/builds, public/Core or composition-root changes,
HTTP GET policy, and unrelated tests are excluded and unranked. No successor
after P8-FT-46 is selected or ranked.

P8-FT-46 is complete. Its implementation is limited
to the private composer, its existing focused test, and these four governing
documents. Implementation must stop on ref/worktree drift, legacy dirtiness,
ambiguous mapping or acceptance semantics, any second label or behavior,
broader layout work, mnemonic or translation-catalog work, public/Core or
composition-root expansion, dependency or installed-surface change, an
architectural decision requiring HTTP GET policy, discovery of another
required file, or scope expansion. This documentation audit's gate is exact
four-file scope, cross-document consistency, Phase terminology, successor
neutrality, the exact Conan command, `git diff --check`, and clean pinned refs
and worktrees. Builds and tests are intentionally skipped for this
documentation-only audit.

## Phase 8 Full-Text Wildcard Mode-Text Gate (Complete)

The documentation-only post-P8-FT-46 audit selects P8-FT-47 as the sole next
leaf. Pinned legacy `fulltextsearch.cc:232-236` gives the third item of the
unique search-mode selector exact translatable text `Wildcards` and maps it to
the wildcard mode. Before P8-FT-47, `full_text_query_composer.cpp:60-74` mapped the same
third item of the unique private `fullTextQueryMode` selector to
`FullTextSearchMode::kWildcard` but displayed `Wildcard`. P8-FT-1 through
P8-FT-46 supply the selector, four-item order and values, persistence, query
composition, containing layout, focus/tab, and request-lifecycle prerequisites.

Completed focused acceptance extends only `full_text_query_composer_test`. It proves
that the unique selector's third item has exact text `Wildcards` and retains
its `FullTextSearchMode::kWildcard` data through construction, every mode and
option transition, and repeated composition. Existing dialog tests retain
submission, generation-current accepted completion, active cancellation,
replacement, service replacement, controller detachment, responses, geometry,
focus, tab, and lifecycle regressions. Add no executable or registered test.

Run the focused Release test with:

```sh
ctest --preset conan-release -R '^full_text_query_composer_test$'
```

The implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, Release install, packaged consumers,
standalone installed C and C++ consumers, and clean committed exact-SCM
creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-47 adds no executable, test registration, installed header, DTO,
ABI, dependency, CMake export, or Conan requirement. The regular-expression
item, any other label or behavior, maximum-distance or article-limit captions
and bounds, broader layout restructuring, spacing, margins, mnemonic policy,
translation-catalog work, indexing lifecycle/UI, Preferences, adapters and
index formats, dependencies/builds, public/Core or composition-root changes,
HTTP GET policy, and unrelated tests are excluded and unranked. No successor
after P8-FT-47 is selected or ranked.

P8-FT-47 is complete. Its implementation is limited
to the private composer, its existing focused test, and these four governing
documents. Implementation must stop on ref/worktree drift, legacy dirtiness,
ambiguous mapping or acceptance semantics, any second item, label, or behavior,
caption-bound policy, broader layout work, mnemonic or translation-catalog
work, public/Core or composition-root expansion, dependency or installed-
surface change, an architectural decision requiring HTTP GET policy, discovery
of another required file, or scope expansion. This documentation audit's gate
is exact four-file scope, cross-document consistency, Phase terminology,
successor neutrality, the exact Conan command, `git diff --check`, and clean
pinned refs and worktrees.

## Phase 8 Full-Text Regular-Expression Mode-Text Gate (Complete)

The documentation-only post-P8-FT-47 audit selects P8-FT-48 as the sole next
leaf. Pinned legacy `fulltextsearch.cc:232-236` gives the fourth item of the
unique search-mode selector exact translatable text `RegExp` and maps it to
regular-expression mode. Before P8-FT-48,
`full_text_query_composer.cpp:60-75` mapped the same fourth item of the unique
private `fullTextQueryMode` selector to
`FullTextSearchMode::kRegularExpression` but displayed `Regular expression`.
P8-FT-1 through P8-FT-47 supply the selector, four-item order and values,
persistence, query composition, containing layout, focus/tab behavior, and
request-lifecycle prerequisites.

Completed focused acceptance extends only `full_text_query_composer_test`. It
prove that the unique selector's fourth item has exact text `RegExp` and
retains its `FullTextSearchMode::kRegularExpression` data through construction,
every mode and option transition, and repeated composition. Existing dialog
tests retain submission, generation-current accepted completion, active
cancellation, replacement, service replacement, controller detachment,
responses, geometry, focus, tab, and lifecycle regressions. Add no executable
or registered test.

Run the focused Release test with:

```sh
ctest --preset conan-release -R '^full_text_query_composer_test$'
```

The implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, Release install, packaged consumers,
standalone installed C and C++ consumers, and clean committed exact-SCM
creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-48 adds no executable, test registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. The wildcard item, any other
label or behavior, maximum-distance or article-limit captions and bounds,
broader layout restructuring, spacing, margins, mnemonic policy,
translation-catalog work, indexing lifecycle/UI, Preferences, adapters and
index formats, dependencies/builds, public/Core or composition-root changes,
HTTP GET policy, and unrelated tests are excluded and unranked. No successor
after P8-FT-48 is selected or ranked.

P8-FT-48 is complete. Its implementation is limited to the private composer,
its existing focused test, and these four governing documents. Implementation
must stop on ref/worktree drift, legacy dirtiness, ambiguous mapping or
acceptance semantics, any second item, label, or behavior, caption-bound policy, broader
layout work, mnemonic or translation-catalog work, public/Core or
composition-root expansion, dependency or installed-surface change, an
architectural decision requiring HTTP GET policy, discovery of another
required file, or scope expansion. This documentation audit's gate is exact
four-file scope, cross-document consistency, Phase terminology, successor
neutrality, the exact Conan command, `git diff --check`, and clean pinned refs
and worktrees. Any later stateful WebEngine smoke must run from freshly
quarantined repository-owned build-tree `HOME` and XDG state, never real user
configuration.

## Phase 8 Full-Text Ignore-Options Row Gate (Complete)

The implementation completes P8-FT-49 as the sole leaf. Pinned legacy
`fulltextsearch.ui:75-99` places exactly
`checkBoxIgnoreWordOrder` followed by `checkBoxIgnoreDiacritics` in one
`QHBoxLayout`. Before P8-FT-49,
`full_text_query_composer.cpp:84-93,137-141` already
owns the corresponding private controls and query mappings, but adds them as
separate vertical items in the opposite order. P8-FT-1 through P8-FT-48 supply
the composer, both controls, exact text and state, mode behavior, composition,
containing layout, focus/tab behavior, and request-lifecycle prerequisites.

Focused acceptance extends only `full_text_query_composer_test`. It must
prove one unique two-item horizontal layout containing the existing unique
ignore-word-order control first and ignore-diacritics control second, while
preserving identity, parentage, object names, text, checked/enabled state, mode
transitions, and repeated `Compose()` results. Existing dialog tests retain
submission, generation-current completion, cancellation, replacement, service
replacement, controller detachment, responses, geometry, focus, tab, and
lifecycle regressions. Add no executable or registered test.

Run the focused Release test with:

```sh
ctest --preset conan-release -R '^full_text_query_composer_test$'
```

The implementation gate remains Linux Release configure/build, exactly
109 registered tests, full Release CTest, Release install, packaged consumers,
standalone installed C and C++ consumers, and clean committed exact-SCM
creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-49 adds no executable, test registration, installed header, DTO, ABI,
dependency, CMake export, or Conan requirement. Match-case/grid relocation;
distance/article rows; maximum-distance and article-limit captions or bounds;
spacing, margins, stretch, alignment, mnemonic or translation-catalog policy;
any third widget or broader layout work; indexing lifecycle/UI; Preferences;
adapters/index formats; dependencies/builds; public/Core or composition-root
changes; HTTP GET policy; and unrelated tests are excluded and unranked. The
legacy caption bounds conflict with current migrated bounds, so neither caption
is eligible without a new explicit product/Core decision. No successor after
P8-FT-49 is selected or ranked.

Its implementation is limited to the private composer, its existing focused
test, and these four governing documents. Implementation was required to stop
on ref/worktree drift, legacy dirtiness, ambiguous membership, order, or
acceptance semantics, any third widget or second behavior, caption-bound
policy, broader layout work, mnemonic or translation-catalog work, public/Core
or composition-root expansion, dependency or installed-surface change, an
architectural decision requiring HTTP GET policy, discovery of another
required file, or scope expansion. The implementation gate is exact
six-file scope, cross-document consistency, Phase terminology, successor
neutrality, the exact Conan command, `git diff --check`, and clean pinned refs
and worktrees. Any later stateful
WebEngine smoke must run from freshly quarantined repository-owned build-tree
`HOME` and XDG state, never real user configuration.

## Phase 8 Full-Text Coupled Search-Options Grid Gate (Complete)

P8-FT-50 is complete. Pinned legacy
`fulltextsearch.ui:28-99` provides the exact grid evidence, and current
`full_text_query_composer.cpp:56-157` plus its focused tests provide the
existing control, behavior, and numeric-contract evidence. After the existing
full-width unlabeled query widget, one unique `QGridLayout` must contain:

- row 0: word-distance toggle at column 0, word-distance spin box at column 1,
  and a two-item horizontal layout with `Mode:` then the selector at column 2;
- row 1: article-limit toggle at column 0, article-limit spin box at column 1,
  and `Match case` at column 2.

The completed ignore-word-order/ignore-diacritics row follows the grid. The
query, grid, and ignore row are the three ordered items of the composer's
top-level vertical layout. All seven participating widgets remain unique direct
child widgets of the composer; only the mode label/selector are nested in the
grid's row-0/column-2 horizontal layout. The composer and existing `Search`
group parentage remain unchanged.

Focused coverage belongs only to `full_text_query_composer_test`. It proves
unique grid membership, exact coordinates, nested mode order,
top-level order and layout parentage, and unchanged widget parentage, identity,
object names, existing captions, mode texts/data, checked/enabled transitions,
and repeated `Compose()` results. It explicitly retains the word-distance
range `0..1000` and articles-per-dictionary range `1..100000`; legacy Qt
`0..99` defaults and synthesized legacy range captions are rejected.

Existing `full_text_search_dialog_test` retains Search-group hierarchy,
explicit focus/tab order, submission, generation-current completion,
cancellation, replacement, service replacement, controller detachment,
responses, geometry, and lifecycle regressions. Add no executable or
registered test. Run the focused Release test with:

```sh
ctest --preset conan-release -R '^full_text_query_composer_test$'
```

The full implementation gate remains Linux Release configure/build,
exactly 109 registered tests, full Release CTest, Release install, packaged
consumers, unchanged standalone installed C and C++ consumers, and clean
committed exact-SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

P8-FT-50 changed no installed interface, but install and consumer checks remain
the stronger full gate. Existing controls, captions, state, mode behavior,
composition, persistence, request/response behavior, and P8-FT-1 through
P8-FT-49 regressions are preserved. Indexing lifecycle/status UI, Preferences,
adapters/index formats, public/Core/config and installed contracts,
dependencies/builds, layout styling policy, mnemonic/translation work, new
controls or behavior, HTTP GET policy, and unrelated tests are excluded and
unranked. Stop on ref/worktree or legacy drift, ambiguity, architectural
conflict, failed validation, another required file, or scope expansion. No
successor after P8-FT-50 is selected or ranked.

## Phase 8 Full-Text Word-Distance Caption Policy Closure

The post-P8-FT-50 documentation audit records GET's Option A without creating
an artificial test leaf. The exact visible label remains
`Maximum word distance`; labels describe settings, while the spin box owns and
exposes its unchanged `0..1000` bounds. Range text must not be synthesized into
the translatable caption. Existing composer production code and focused tests
already prove the required label, range, state transitions, and repeated query
composition, so no source, test, executable, or registration changes.

This four-document-only closure skips compiled checks and preserves the Release
baseline of exactly 109 tests. Linux Release CTest, Release install, packaged
consumers, standalone installed C and C++ consumers, and exact-SCM creation
remain the unchanged gate for later implementation work:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Indexing lifecycle/status UI, the articles-per-dictionary caption, other text
or accessibility policy, styling/layout, translation work, public/Core/config
or installed contracts, and unrelated parity remain excluded and unranked.
Stop on drift, ambiguity, design conflict, another required file, failed
validation, or scope expansion. No successor is selected or ranked; verification
after this closure begins only with a fresh bounded post-policy readiness audit.

## Phase 8 Full-Text Articles-Per-Dictionary Caption Policy Prerequisite

The fresh post-policy audit selects only P8-FT-51, the product-policy
prerequisite for the articles-per-dictionary caption. Pinned legacy
`fulltextsearch.cc:249-256` builds the range-bearing text
`Max articles per dictionary (%1-%2):` and assigns legacy bounds. Current
`full_text_query_composer.cpp:109-125` and existing focused coverage instead
prove exact label `Maximum articles per dictionary`, spin-box range
`1..100000`, identity, enablement, state transitions, and repeated composition.

Before any caption implementation can be authorized, GET must choose between
retaining the exact current label with sole spin-box ownership/exposure of
`1..100000`, requiring no source or test leaf, and a range-bearing caption
using those current bounds, potentially requiring a later private composer
test change. Restoring legacy Qt `0..99` defaults is excluded. P8-FT-51 makes
no choice and adds no source, test, executable, registration, public interface,
dependency, or build change.

Index lifecycle and readiness/status/progress coverage, full-text Preferences,
exact-document navigation, match/excerpt presentation, ignore-diacritics
consumption, accessibility, styling/layout, translation work, adapters/index
formats, public/Core/config and installed contracts, dependencies/builds, and
unrelated tests remain excluded and unranked.

Verification for this four-document-only selection is limited to the exact
file allowlist, cross-document policy/evidence consistency, no-successor
language, Phase terminology, pinned ref/worktree checks, and
`git diff --check`. Compiled checks are omitted and the Release baseline stays
exactly 109 tests. Stop on drift, further ambiguity, architectural conflict,
another required file, failed validation, or scope expansion. No successor
after P8-FT-51 is selected or ranked.

## Phase 8 Full-Text Articles-Per-Dictionary Caption Policy Closure

GET selected P8-FT-51 Option A: retain exact visible label
`Maximum articles per dictionary`; the spin box solely owns and exposes
`1..100000`, with no bounds in translatable caption text. The locked
`Maximum word distance` label and spin-box-owned `0..1000` range remain
unchanged.

Current `full_text_query_composer.cpp:95-125` and existing focused composer
coverage already prove both exact labels, both ranges, identity, enablement,
state transitions, and repeated composition. Pinned legacy
`fulltextsearch.cc:249-256` remains historical evidence only. No artificial
source, test, executable, registration, public interface, config/index-format
contract, dependency, build, or installed-surface change is required.

Verification for this four-document-only closure is limited to the exact
allowlist, cross-document policy and pinned-evidence consistency,
no-successor language, Phase terminology, pinned refs/worktrees, live remotes,
and `git diff --check`. Compiled checks are omitted and the Release baseline
remains exactly 109 tests. Stop on drift, ambiguity, architectural conflict,
another required file, failed validation, or scope expansion. No successor is
selected or ranked; verification resumes with a fresh independent bounded
full-text readiness audit.

## Phase 8 Full-Text Dialog Window-Title Translation

P8-FT-52 is complete from synchronized migrated base
`b25cee8fd95381ecd16f733107f7d201d5068eeb`. Pinned legacy
`fulltextsearch.cc:219-223` supplies exact translated-title evidence, and the
current dialog resolves exact source text `Full-text search` through its
existing private `tr()` context.

The focused change is limited to `full_text_search_dialog.cpp` and
`full_text_search_dialog_test.cpp`. Construction coverage proves the exact
English fallback and uses an automatically removed scoped test translator to
prove replacement through the exact `FullTextSearchDialog` context and source.
No catalog, locale loader, new test executable, or registration is added; the
Release baseline remains exactly 109 tests.

The implementation changes no behavior beyond title translation resolution and no
public/Core/config/index-format/dependency/build/composition-root or installed
contract. It preserves exact `Maximum word distance` with spin-box-owned
`0..1000` and exact `Maximum articles per dictionary` with spin-box-owned
`1..100000`. Index lifecycle/readiness/status/progress, full-text Preferences,
other translation, accessibility and styling surfaces, exact-document
navigation, match/excerpt presentation, ignore-diacritics consumption,
adapters/index formats, and unrelated tests remain separate and unranked.

Verification is the focused Release dialog test, fresh Linux Release
configure/build, exactly 109 registered tests and full Release CTest, Release
install, standalone installed C/C++ consumers, `git diff --check`, and clean
committed exact-SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Packaged C/C++ consumers must pass, and final local/upstream/live-remote refs
and both worktrees must agree and remain clean. No successor after P8-FT-52 is
selected or ranked.

## Phase 8 Full-Text Search Group-Box Translation

P8-FT-53 is complete from clean synchronized migrated base
`339d1dd6e8b3540923153628497af23b6fa7208b` and clean read-only legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Pinned legacy
`fulltextsearch.ui:23-27` marks exact group-box title `Search` as translatable;
current `full_text_search_dialog.cpp` supplies the same visible text through
dialog-owned `tr()`. Completed P8-FT-52 proves the exact private
`goldendict::app::FullTextSearchDialog` translation context.

The focused change is limited to `full_text_search_dialog.cpp` and
`full_text_search_dialog_test.cpp`. Construction coverage finds the sole
direct-child search group and proves exact-context replacement of source
`Search`, unchanged English fallback without a translator, stable group
identity/title, and automatic scoped translator removal. No
catalog, locale loader, executable, test registration, or baseline change is
needed; exactly 109 Release tests remain registered.

The implementation changes no public/Core/config/index-format/dependency/build/
composition-root, ABI, or installed contract and preserves the Shared-Library
and GUI Boundary. It retains exact `Maximum word distance` with spin-box-owned
`0..1000` and exact `Maximum articles per dictionary` with spin-box-owned
`1..100000`. Index lifecycle/readiness/status/progress, full-text Preferences,
translation catalogs and other strings, accessibility, styling/layout,
exact-document navigation, match/excerpt presentation, ignore-diacritics
consumption, adapters/index formats, and unrelated tests remain separate and
unranked.

Verification is the focused Release dialog test, fresh Linux Release dependency
install/configure/build, exactly 109 registered tests and full Release CTest,
Release install, standalone installed C/C++ consumers, `git diff --check`, and
clean committed exact-SCM creation with:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Packaged C/C++ consumers must pass, and final local/upstream/live-remote refs
and both worktrees must agree and remain clean. No successor after P8-FT-53 is
selected or ranked.

## Phase 8 Full-Text Partial-Status Translation

P8-FT-54 is complete from clean synchronized migrated base
`8dcf3d87fe4b25a916c864da56c307f5c78de24b`: exact existing status
`Results may be incomplete.` now resolves through
`goldendict::app::FullTextSearchDialog`. Completed P8-FT-22 supplies the text
and visibility contract; current `full_text_search_dialog.cpp` uses
dialog-owned `tr()`; P8-FT-52/P8-FT-53 and the focused dialog test supply the
exact private context and scoped-translator pattern. Pinned legacy has no
equivalent partial-response status and no conflicting translation contract.

The implementation is limited to `full_text_search_dialog.cpp` and
`full_text_search_dialog_test.cpp`. Focused coverage must prove exact-context
replacement, the exact English fallback, stable label identity/text, unchanged
initial/replacement/complete/stale/cancelled/detached and authoritative-partial
visibility behavior, and scoped translator cleanup. No catalog, locale loader,
new executable, registration, baseline, public/Core/config/index-format/
dependency/build/composition-root, ABI, or installed-interface change is
authorized.

The exact translated dialog/group titles and the locked concise captions with
control-owned ranges (`Maximum word distance` / `0..1000` and
`Maximum articles per dictionary` / `1..100000`) remain unchanged. Other
response strings/catalog readiness, accessibility, styling/layout,
exact-document navigation, match/excerpt presentation, ignore-diacritics
consumption, index lifecycle/Preferences, adapters/index formats, and unrelated
tests remain unselected and unranked.

Verification is the focused Release dialog test, fresh Linux Release
dependency install/configure/build, exactly 109 registered tests and full
Release CTest, Release install, standalone installed C/C++ consumers,
`git diff --check`, and clean committed exact-SCM Conan creation with packaged
consumers. Final refs and both worktrees must remain synchronized and clean.
Completion unlocks only a fresh independent bounded full-text readiness audit;
no successor is selected or ranked.

## Phase 8 Full-Text Empty-Status Translation

P8-FT-55 is complete from clean synchronized migrated base
`175a92926b1046798b51b9757a28ed156555c0aa` and unchanged clean read-only legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The existing
`fullTextEmptyResponseStatus` retains exact source and fallback text `No
matches` while resolving through `goldendict::app::FullTextSearchDialog::tr()`.

P8-FT-23 already supplies the unique identity and generation-current
conclusive-empty visibility contract. The dialog uses dialog-owned `tr()`;
P8-FT-52 through
P8-FT-54 and `full_text_search_dialog_test.cpp` supply the exact context and
scoped-translator pattern. Pinned legacy contains no equivalent empty-response
status and therefore no conflicting translation contract.

The implementation is limited to `full_text_search_dialog.cpp` and its existing
focused test. Coverage installs an exact-context translator and proves
replacement and English fallback after scoped cleanup. The same test retains
exact widget identity and text and all P8-FT-23 predicates:
hidden initially and during replacement; visible only for a generation-current
accepted response with zero results, `partial == false`, and no errors; hidden
for nonempty, partial, or error-containing responses; and immune to stale,
cancelled, and detached completions.

No catalog, locale loader, executable, registration, public/Core/config/index-
format/dependency/build/composition-root, ABI, installed-interface, or test-
registration change is authorized. Completed translations `Full-text search`,
`Search`, and `Results may be incomplete.` remain exact. The locked policies
remain `Maximum word distance` with control-owned `0..1000` and
`Maximum articles per dictionary` with control-owned `1..100000`. Other
response strings/catalog readiness, accessibility, styling/layout, exact-
document navigation, match/excerpt presentation, ignore-diacritics consumption,
adapters/index formats, and unrelated tests remain independent and unranked.
Index readiness/status/progress/rebuild/failure UI/background lifecycle and
full-text Preferences remain blocked on a separate Core lifecycle/policy
boundary.

Focused acceptance is the Release dialog test. The full gate is fresh Release
install/configure/build, exactly 109 registered tests and 109/109 passing,
Release install, standalone installed C/C++ consumers, clean committed exact-
SCM Conan creation with packaged consumers, exact six-file validation, and
`git diff --check`. No successor after P8-FT-55 is selected or ranked;
completion unlocks only a fresh independent bounded full-text readiness audit.

## Phase 8 Full-Text Terminal-Failure-Status Translation (Complete)

P8-FT-56 is complete from clean synchronized migrated revision
`7bc3fbeee4af637e25dff8656ce7d22406d8ea2d` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The existing
`fullTextFailureResponseStatus` retains exact source and fallback text
`Full-text search failed`; its construction moved from `QStringLiteral` to
dialog-owned `tr()` without changing the completed P8-FT-24 behavior.

The existing `full_text_search_dialog_test.cpp` uses an exact-context translator
to prove replacement in `goldendict::app::FullTextSearchDialog`, English fallback
after scoped cleanup, the unique widget identity and parentage, and unchanged
text and visibility. Existing acceptance remains hidden initially and during
replacement; visible only for a generation-current response with zero results,
`partial == false`, and one or more errors; hidden for conclusive empty,
nonempty, partial-empty, partial-nonempty, and result-plus-error responses; and
safe for repeated, stale, cancelled, detached, replaced-service, and teardown
completions. Existing result-count, partial, empty, mixed-result, partial-empty,
error-count, selection, activation, focus, and lifecycle coverage remains
unchanged. No executable or test registration is added.

The focused command is:

```sh
ctest --preset conan-release -R '^full_text_search_dialog_test$'
```

The implementation gate is the exact six-file allowlist, current and pinned-
legacy evidence, cross-document consistency, completed translation strings,
locked caption/control ranges, Shared-Library and GUI Boundary, Phase
terminology, exactly 109 registered Release tests and 109/109 passing, Release
install, standalone and packaged C/C++ consumers, exact-SCM Conan creation,
`git diff --check`, exact refs/remotes, and both clean worktrees.

P8-FT-56 authorizes no catalog, locale loader, executable, registration,
public/Core/configuration/index-format/dependency/build/composition-root, ABI,
installed-interface, or baseline change. Completed translations `Full-text
search`, `Search`, `Results may be incomplete.`, and `No matches` remain exact;
`Maximum word distance` / `0..1000` and `Maximum articles per dictionary` /
`1..100000` remain locked. All other visible and private full-text gaps remain
independent and unranked. Index readiness/status/progress/rebuild/failure
reporting/background lifecycle and full-text Preferences remain blocked on a
separate fully evidenced Core lifecycle/policy boundary. P8-FT-56 completion
unlocks only a fresh independent bounded full-text readiness audit.

## Phase 8 Full-Text Mixed-Result-Status Translation

P8-FT-57 was the sole leaf selected by the fresh independent bounded
post-P8-FT-56 audit from clean synchronized migrated revision
`491d85500d27df280c19d4a62a2adc9e14d55a33` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. P8-FT-25 already
owns the unique `fullTextMixedResultResponseStatus`, exact source and fallback
text `Some dictionaries could not be searched`, and its generation-current
result-plus-error visibility, authoritative-partial coexistence, and no-raw-
detail contract. P8-FT-57 replaces the former `QStringLiteral` construction
with dialog-owned `tr`; P8-FT-52 through P8-FT-56 establish the exact
`goldendict::app::FullTextSearchDialog` translation context and focused scoped-
translator pattern. Pinned legacy has no equivalent status or conflicting
translation contract.

The completed P8-FT-57 implementation changes exactly
`apps/goldendict/src/full_text_search_dialog.cpp` and
`apps/goldendict/tests/full_text_search_dialog_test.cpp`. Focused coverage must
prove dialog-owned `tr("Some dictionaries could not be searched")` replacement
through the exact context, English fallback after scoped cleanup, stable widget
identity and text, unchanged initial/replacement/result-free/error-free/
result-plus-error predicates, unchanged coexistence with the partial status,
and stale/cancelled/detached/replaced-service/teardown safety. No executable or
test registration is added.

The Shared-Library and GUI Boundary applies. Public/Core/configuration/index-
format/dependency/build/composition-root/ABI/installed interfaces, catalogs,
locale loading, and exactly 109 registered Release tests remain unchanged.
Completed translations `Full-text search`, `Search`,
`Results may be incomplete.`, `No matches`, and `Full-text search failed`
remain exact. Locked policies remain `Maximum word distance` with control-owned
`0..1000` and `Maximum articles per dictionary` with control-owned `1..100000`.

Index readiness/status/progress/rebuild/failure reporting/background lifecycle
and full-text Preferences remain blocked on a separate fully evidenced Core
lifecycle/policy boundary. Exact-document navigation, result/match/excerpt
presentation, ignore-diacritics consumption, adapters/index formats,
accessibility, styling/layout, catalogs, and unrelated parity remain independent
and unranked. Validation is the focused Release dialog test, fresh Release
configure/build, exactly 109 registered tests and 109/109 passing, Release
install, standalone installed C/C++ consumers, clean committed exact-SCM
package creation with its packaged consumers, the exact six-file allowlist,
`git diff --check`, exact refs/remotes, and clean worktrees. P8-FT-57 completion
unlocks only a fresh independent bounded readiness audit; no successor is
selected, ranked, recommended, or named.

## Phase 8 Full-Text Partial-Empty-Status Translation (Complete)

P8-FT-58 is complete from synchronized migrated
revision `58612007652ac24f08fc0bd8e2a4fb2b59839366` and clean read-only legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The existing
`fullTextPartialEmptyResponseStatus` resolves through
`goldendict::app::FullTextSearchDialog` while retaining exact source and
fallback text `No matches in searched dictionaries`.

P8-FT-26 continues to own the generation-current zero-result and authoritative-
partial predicate, coexistence with `Results may be incomplete.`, lifecycle
safety, and raw-detail suppression. The dialog now constructs this status with
dialog-owned `tr()`; P8-FT-52 through P8-FT-57 establish the exact-context translator pattern, and
pinned legacy full-text sources contain no equivalent status or conflicting
contract.

Completed focused acceptance extends only `full_text_search_dialog_test.cpp`
and proves dialog-owned `tr("No matches in searched dictionaries")` replacement in
the exact context, English fallback after scoped cleanup, stable widget
identity and text, unchanged initial/replacement/complete/nonempty/partial-empty
predicates, coexistence with the partial and error-count statuses, and stale/
cancelled/detached/replaced-service/teardown safety. No executable or registered
test is added; the Release baseline remains exactly 109 tests.

The implementation has an exact six-file allowlist and includes compiled gates.
Validation covers focused and full Release tests, Release install, standalone
and packaged C/C++ consumers, exact-SCM Conan creation, current and pinned-legacy citations, cross-document
consistency, completed translations, locked `Maximum word distance` / spin-box-
owned `0..1000` and `Maximum articles per dictionary` / spin-box-owned
`1..100000`, completed P8-FT behavior, Shared-Library and GUI Boundary, Phase
terminology, successor neutrality, exactly 109 registered Release tests,
`git diff --check`, synchronized refs/remotes, and clean worktrees. Index
readiness/status/progress/rebuild/failure reporting/background lifecycle and
full-text Preferences remain blocked without a separately authoritative Core
lifecycle/policy resolution. Other parity remains unranked. Completion unlocks
only a fresh independent bounded readiness audit; no successor is selected,
ranked, recommended, or named.

### P8-FT-59 full-text error-count translation acceptance

P8-FT-59 is complete from synchronized migrated revision
`471ba2a7db8491aa486951506389101caa8cb255` and clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It accepts focused translation of
P8-FT-27's existing private `Errors: %1` status.

Production retains dialog-owned `tr("Errors: %1")`. Focused acceptance extends
only the existing private dialog test and proves exact
`goldendict::app::FullTextSearchDialog` context and source replacement,
authoritative single- and multi-digit decimal interpolation, English fallback
before installation and after scoped cleanup, sole direct-child label identity,
and unchanged P8-FT-27 predicates, coexistence, stale/cancelled/detached/
replaced-service/teardown safety, and raw-detail suppression. No executable or
registered test is added; the Release baseline remains exactly 109 tests.

The Shared-Library and GUI Boundary governs. Completed translations
`Full-text search`, both `Search` uses, `Results may be incomplete.`, `No
matches`, `Full-text search failed`, `Some dictionaries could not be searched`,
and `No matches in searched dictionaries` remain exact. The locked `Maximum
word distance` / spin-box-owned `0..1000` and `Maximum articles per dictionary`
/ spin-box-owned `1..100000` policies remain unchanged. Index readiness/status/
progress/rebuild/failure reporting/background lifecycle and full-text
Preferences remain blocked without a separately authoritative Core lifecycle/
policy resolution. Other translation, accessibility, styling, navigation,
excerpt, diacritics, result presentation, adapter/index-format, and unrelated
parity remain independent and unranked. Completion unlocks only a fresh
independent bounded readiness audit; no successor is selected, ranked,
recommended, or named.

Validation uses the focused Release dialog test, a fresh Release configure and
build, exactly 109 registered tests and 109/109 passing, Release install,
standalone installed C/C++ consumers, clean committed exact-SCM Conan creation
with packaged consumers, the exact five-file allowlist, `git diff --check`,
synchronized refs/remotes, and clean worktrees.

### P8-FT-60 exact-result navigation contract acceptance

The documentation-only audit is pinned to synchronized migrated revision
`4cca1e81e1167222d067e475a4053088cf99ba38` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects only
P8-FT-60, the prerequisite Core/facade/navigation contract for an accepted
result's dictionary ID and opaque `document_id`. The stable identifier is the
next unused persisted ordinal after P8-FT-59; the abandoned unpersisted
accepted-result-count translation-test recommendation reserves nothing.

Acceptance requires complete-diff review, current and legacy citations, cross-
document consistency, the Shared-Library and GUI Boundary, the intentional
desktop-facade C++ ABI change, unchanged headless service/runtime-source/C ABI,
and all index-format/dependency/build/
catalog/locale-loader/executable/registration boundaries, completed P8-FT
identities, predicates, lifecycle, coexistence, and privacy guarantees, Phase
terminology, prerequisite-only and no-successor wording, exactly 109 registered
Release tests, synchronized refs/remotes, clean worktrees, and
`git diff --check`.

Current focused evidence is `main_window.cpp:6394-6431`, which explicitly
supplies but does not target the result dictionary/document identity while
pinning scope, history, main-query selection, and search handoff. Pinned legacy
evidence is `fulltextsearch.cc:596-609` and `mainwindow.cc:3001-3013`.

P8-FT-60 adds focused Core tests for bounded exact
target validation/resolution and atomic invalid or unresolved failure; facade
tests for navigation identity, current-tab behavior, history, and session
round-trip/replay. Direct MainWindow/dialog activation coverage is excluded by
the approved facade-only prerequisite; group, authoritative accepted scope,
main-query selection, and the existing article-search handoff remain unchanged.
Existing test registrations retain
the exactly 109-test registration baseline; adding cases to existing
executables must not add registrations.

Completed translations `Full-text search`, both `Search` uses, `Results may be
incomplete.`, `No matches`, `Full-text search failed`, `Some dictionaries
could not be searched`, `No matches in searched dictionaries`, and
`Errors: %1` remain exact. `Maximum word distance` remains spin-box-owned at
`0..1000`, and `Maximum articles per dictionary` remains spin-box-owned at
`1..100000`. Index readiness/status/progress/rebuild/failure reporting/
background lifecycle and full-text Preferences remain blocked without
separately authoritative Core lifecycle/policy resolution. Navigation
activation, translation acceptance, accessibility, styling, highlighting
and excerpts, diacritics, presentation, adapters, and other independent parity
gaps remain excluded and unranked. No successor is selected or ranked. This
completed prerequisite unlocks only its dependency boundary.

### P8-FT-61 exact-result activation connection acceptance (complete)

The implementation starts from synchronized migrated revision
`0394b031031c265c7799386996bcbda22e5b0a3b` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects only
P8-FT-61, the private GUI connection from an accepted full-text result to the
`ExactArticleTarget` and `TabNavigationState` contract completed by P8-FT-60.
P8-FT-60's focused Core resolution, validation, atomic mutation, navigation-
identity, history, and session tests remain the foundation; no duplicate Core
contract or new interface is required.

Current focused evidence is `full_text_search_dialog_test.cpp:2299-2394` and
`2685-2764` for exact accepted result/scope/context activation delivery,
`main_window.cpp:6394-6520` for the existing current-tab/group/scope/main-query/
article-search success and atomic failure smoke, and
`application_service_test.cpp:2375-2438` plus
`article_tabs_test.cpp:105-153` for P8-FT-60 exact-target resolution and
failure atomicity. Pinned legacy evidence remains
`fulltextsearch.cc:596-609` and `mainwindow.cc:3001-3013`.

Focused implementation coverage must prove that a generation-current accepted
row copies its dictionary ID and opaque `document_id` unchanged into
`navigation.exact_target`; that successful current-tab activation records that
exact navigation in facade state and exported session; and that group,
authoritative accepted scope and order, headword/title, main-query text/cursor/
selection, lookup dispatch, and accepted-query article-search handoff remain
unchanged. Widgets must not parse IDs or call a backend resolver directly.

The same existing GUI smoke covers invalid target, unavailable dictionary,
missing/stale document, and existing tab-operation failure without tab,
history/session, request, or article-search mutation, while retaining exactly
`Unable to update article state` and exposing no raw detail. Cases are added to
existing test executables only; no test registration is added and the Release
baseline remains exactly 109 tests.

Implementation acceptance requires the exact five-file allowlist comprising
`apps/goldendict/src/main_window.cpp` and these four governing documents,
complete-diff
review, current and legacy citations, cross-document consistency, Phase
terminology, the Shared-Library and GUI Boundary, completed P8-FT-60 foundation,
locked strings/captions/ranges and completed behavior, unchanged public/
installed/headless/service/runtime-source/C API/dependency/index-format/build
boundaries, no-successor wording, synchronized refs, clean worktrees, and
`git diff --check`. Compiled gates are intentionally skipped for this
documentation-only audit. Highlighting/excerpts, ignore-diacritics semantics,
translations, and every successor remain excluded and unranked.

### P8-FT-62 match-centered excerpt contract prerequisite acceptance

The completed implementation started from synchronized migrated HEAD,
upstream, and live remote revision
`7dda841276274a1f21d8c7905ff48698395d63c2` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
It selects only P8-FT-62, the Shared-Library prerequisite for a bounded valid-
UTF-8 match-centered excerpt and explicit document-relative excerpt origin.
The implemented DTO field is final-member
`std::size_t FullTextResult::excerpt_byte_offset`, with a zero default so
existing shorter aggregate initializers retain their meaning.
P8-FT-60 remains the completed exact-target facade/navigation contract;
P8-FT-61 remains the completed private exact-result activation connection.

Current acceptance evidence is `dictionary_service.h:164-190` for the match
and result DTO, `full_text_index.cc:314-410` for original-text range mapping and
the unsafe first-4096-byte excerpt, `full_text_response_model.cpp:15-46` and
`full_text_search_dialog.cpp:300-348` for by-value projection and generation
ownership, and `main_window.cpp:5996-6040,7970-8110` for successful exact-load
literal search handoff. Pinned legacy evidence is
`fulltextsearch.cc:596-609`, `mainwindow.cc:3001-3013`, and
`articleview.cc:2569-2728`; legacy rematches rendered page text and supplies no
competing excerpt-origin contract.

Focused Core cases in the existing full-text index test cover ASCII
and multibyte matches near the start, middle and end of documents; exact
document-relative offset/length/text correspondence; deterministic excerpt
origin and content; valid UTF-8 start and end boundaries; the exact
`kMaximumFullTextExcerptBytes` maximum; matches adjacent to combining marks;
pattern-mode matches spanning multibyte text; and a match longer than the
excerpt bound. The over-bound case must follow the
documented prefix-from-match-offset policy and never emit malformed text or an
invalid range. Equal-length fitting candidates must prove balanced-context
selection and the earlier-origin tie break. Tests assert UTF-8 code-point
boundaries; they do not impose a Core grapheme-cluster contract. The shared
index path supplies this behavior to every accepted built-in
format without adapter-specific match or excerpt logic.

Existing model/dialog cases must prove that the accepted result preserves
excerpt, origin and ordered matches exactly by value. Pending replacement,
cancellation, stale or duplicate completion, controller detachment, teardown,
and rejected or failed exact activation must not expose, revive or overwrite
accepted presentation data. Until a separate presentation leaf is approved,
the model continues to expose only its established roles and the dialog renders
no excerpt.

Implementation acceptance keeps match ranges document-relative and derives any
excerpt-relative range only through checked subtraction from the authoritative
origin. Widgets treat the excerpt as plain text with normal Qt role escaping;
they do not parse dictionaries, construct indexes, reinterpret backend
matching, map ranges into the DOM, or expose raw backend details. Literal
WebEngine search remains the separately completed article-search handoff and is
not accepted as equivalent to backend highlighting.

Cases are added only to existing test executables; no executable or test
registration is added and the Release registration baseline remains exactly
109 tests. Implementation verification uses the focused Release tests,
full 109-test Release suite, Release install, standalone installed C/C++
consumers, and exact-SCM packaged consumers. The installed C++ consumer reads
the final zero-defaulted origin and checks containment by checked subtraction;
the additive member preserves shorter aggregate initializers at source level
but intentionally changes the DTO layout/ABI and requires a rebuilt exact-SCM
package. The C consumer remains unchanged.

Audit acceptance requires the exact four-document allowlist, complete-diff
review, cross-document consistency, current and pinned-legacy citations, Phase
terminology, the Shared-Library and GUI Boundary, explicit installed C++ DTO
impact, unchanged headless/runtime-source/C API/configuration/persistence/index-
format/adapter/dependency/build/catalog/locale/executable/registration
boundaries, locked P8-FT strings/captions/ranges/privacy/lifecycle behavior,
distinct completed P8-FT-60/P8-FT-61 contracts, exactly 109 registered tests,
no-successor wording, synchronized refs, clean worktrees, and
`git diff --check`. Result rendering, exact-article highlighting, index
lifecycle and Preferences, and all other full-text and unrelated parity remain
excluded and unranked. No successor after
P8-FT-62 is selected, ranked, recommended, or named. Completion unlocks only
the dependency boundary established by this match-centered excerpt/origin
contract.

The locked control policies checked by the audit remain `Maximum word
distance` with spin-box-owned `0..1000` and `Maximum articles per dictionary`
with spin-box-owned `1..100000`.

### P8-FT-63 accepted-query article-highlighting context prerequisite acceptance

P8-FT-63 is complete.

The implementation starts from synchronized migrated HEAD, upstream, and live
remote revision `97f2269a0cee85ae96b6c634d1967116a476e7e9` and clean pinned
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It completes only
P8-FT-63: retain
the complete accepted highlighting-relevant query policy through the private
activation and article-load handoff. The result list remains headword-only with
its established dictionary-name tooltip; no excerpt or row-presentation test
changes are permitted.

Focused cases in the existing dialog and MainWindow/full-text smokes prove
exact by-value retention and delivery of query text, mode, match-
case, ignore-word-order, maximum-word-distance, and the already-carried ignore-
diacritics value. They also prove that later widget edits, replacement,
cancellation, stale or duplicate completion, controller detachment, rejected
or failed exact activation, tab/view replacement, and teardown cannot revive,
overwrite, or dispatch older context. Existing successful exact activation,
generation-current load gating, literal first-match selection, status, and
forward/backward article-search behavior remain unchanged.

Acceptance explicitly does not treat P8-FT-62 indexed UTF-8 byte ranges as DOM
ranges. It adds no rendered-page extraction, Core rematcher, JavaScript, DOM
mapping, highlighting, ignore-diacritics consumption, new status text, or
Previous/Next behavior. Pinned legacy evidence remains
`fulltextsearch.cc:596-609` and `articleview.cc:2569-2799`; current evidence is
`full_text_search_dialog.h:24-35,106-122`,
`full_text_search_dialog.cpp:280-348`, and
`main_window.cpp:5996-6043,7976-8110`.

Cases stay in existing test executables, so the Release registration baseline
remains exactly 109 tests. Public/installed interfaces, Core DTOs, headless and
runtime-source contracts, C API, configuration, persistence, index format,
adapters, dependencies, build, catalogs, translations, generated files, and
executables remain unchanged. Validation requires the exact nine-file
allowlist, complete-diff review, cross-document consistency, current and legacy
citations, Phase terminology, Shared-Library and GUI Boundary, locked strings/
captions/ranges and completed P8-FT-60 through P8-FT-62 behavior, focused and
full Release tests, install and consumer checks, exact-SCM package creation,
synchronized refs, clean worktrees, and `git diff --check`. No successor after
P8-FT-63 is selected or ranked; completion unlocks only its private accepted-
query article-highlighting context dependency boundary.

### P8-FT-64 rendered-page text extraction transport prerequisite acceptance (complete)

The fresh bounded documentation-only audit starts from synchronized migrated
HEAD, upstream, and live remote
`6f473bf7ffc3d256342a585ab19313fe0b52a003` and clean pinned legacy
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects only P8-FT-64,
the private asynchronous rendered-page plain-text extraction transport required
after the completed P8-FT-60 through P8-FT-63 chain.

Focused cases in existing GUI test executables prove that
successful current exact-result load extracts the rendered plain text and
delivers it only while the accepted search generation, lookup presentation
generation, tab ID, and `ArticleView` remain current. They must reject delivery
after replacement, cancellation, failed activation or page load, newer lookup
or search work, tab/view replacement, navigation, and teardown. The tests must
also prove that extraction alone does not invoke matching, alter the DOM, change
literal `findText`, update highlighting/search status, select a match, navigate,
or consume ignore-diacritics.

Acceptance must not interpret P8-FT-62 indexed-document byte offsets, match
text, or excerpts as rendered-page or DOM coordinates. Widgets must not
reimplement wildcard, regular-expression, whole-word, word-order, word-distance,
case, or normalization semantics. Current evidence is
`main_window.cpp:7632-7644,7976-8110`; pinned legacy evidence is
`articleview.cc:2569-2799`. The Core matching-plan API, installed ABI shape,
rematching, DOM/literal application, highlight-all behavior, first selection,
Previous/Next behavior, and status wording remain outside this acceptance and
unranked.

The implementation keeps cases in existing executables, so the
Release registration baseline remains exactly 109 tests. Public/installed and C
interfaces, Core DTOs, index format, adapters, dependencies, build,
configuration, persistence, catalogs, translations, generated files,
executables, locked strings/captions/ranges, and completed P8-FT behavior remain
unchanged. Focused acceptance covers exact by-value accepted-generation and
rendered-text retention plus rejection after accepted-query, lookup/search,
navigation, tab/view/page, facade, cancellation, failure, and teardown
invalidation. No successor after P8-FT-64 is selected or named; completion
unlocks only generation-safe rendered-page text extraction.

### P8-FT-65 rendered-text matching-plan facade prerequisite acceptance (complete)

Implementation starts from migrated/local/upstream/live-remote
`1dce706344bd33254bedeb1e13d9b6eb5fa8c8c4` and legacy
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It completes only P8-FT-65 and the
GET-approved installed `DesktopFacade` operation; the private stateless Core
matcher must also serve indexed full-text search so matching semantics have one
owner.
Acceptance derives from current `full_text_index_test.cpp:69-319`, installed
surface coverage in `core_api_test.cpp:1-193` and
`test_package/headless_api_test.cpp:650-735`, Widgets transport at
`main_window.cpp:8312-8356`, and pinned legacy
`articleview.cc:2569-2788`.

Focused Core acceptance covers every query mode, match-case,
word-order and maximum-distance combination accepted by existing full-text
validation; Unicode normalization and valid UTF-8 boundary mapping; wildcard
and regular-expression validity; multiple and repeated literals; adjacent
matches; deterministic leftmost-first non-overlap; zero-length rejection; and
exact equality between every returned literal and its rendered-text byte range.
No-match must be successful and empty. P8-FT-62 indexed offsets must not
participate.

Boundary acceptance must cover empty query, invalid UTF-8 and oversized
rendered text/query, non-positive timeout, excessive distance and incompatible
pattern constraints as invalid requests; malformed patterns as their own failure; and
cancellation, deadline, resource-limit and contained internal failures with no
partial plan. Cancellation/deadline checks must be exercised during
normalization, pattern matching and result collection. Installed API tests must
pin the new request/result defaults and `DesktopFacade` vtable operation while
preserving `DictionaryService` and C API surfaces.

Installed API coverage must pin the 16 MiB
`kMaximumRenderedTextMatchPlanBytes`, request/result/range/error defaults,
five-second default request timeout, null default cancellation, and the const
`DesktopFacade::BuildRenderedTextMatchPlan` vtable operation. Empty rendered
text must be covered as successful no-match.

Later Widgets acceptance may consume the plan only after the existing accepted-
query, lookup, search, navigation, tab, view and page identities all remain
current. P8-FT-65 uses existing Core/facade/API test targets and does not test
or specify
DOM/JavaScript application, highlight-all, first selection, Previous/Next UI,
status wording or ignore-diacritics behavior. Headword-only rows, dictionary-
name tooltips, locked strings/captions/ranges/translations and exactly 109
registered Release tests remain unchanged. The installed C++ consumer covers
the new facade contract while the C consumer remains unchanged. No successor
after P8-FT-65 is selected or named; completion unlocks only its
`DesktopFacade` matching-plan dependency boundary.

### P8-FT-66 private match-plan worker/controller prerequisite acceptance (complete)

Implementation starts from clean synchronized
migrated/local/upstream/live-remote revision
`7596259baab285526438af205df4172032401f62` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. P8-FT-66 supersedes P8-FT-65's
historical no-successor closure. GET's Option B selects only P8-FT-66:
cancellable worker execution of the synchronous P8-FT-65 facade
matcher with by-value request input and GUI-thread delivery.
Acceptance is grounded in current `full_text_request_controller.cpp:21-188`
and `main_window.cpp:8320-8364`; pinned legacy
`articleview.cc:2569-2791` defines presentation behavior outside this leaf.

Focused acceptance in existing targets must prove request isolation, monotonic
replacement, worker-thread invocation, queued GUI-thread completion, explicit
cancellation of running and superseded pending work, and safe stop before
facade replacement or teardown. Successful empty and nonempty plans and every
typed P8-FT-65 failure must be delivered without reinterpretation. Cancelled
work must not complete as current.

Main-window acceptance must independently invalidate work generation,
accepted-query generation or exact query policy, lookup presentation
generation, article-search generation, navigation generation, tab, view and
page and prove each stale completion is discarded. It must also cover
replacement activation and article search, lookup/navigation invalidation,
tab/view/page replacement, tab close, facade detachment, teardown and duplicate
completion. Accepted output remains inert private state.

Acceptance must prove no `findText`, DOM/JavaScript application, highlighting,
selection, Previous/Next navigation, status mutation or ignore-diacritics
consumption. No installed API, Core DTO, C API, index, configuration,
dependency, catalog, translation, executable or test-registration gate changes;
the Release baseline remains exactly 109 tests. Compiled gates are omitted for
this four-document-only audit. No successor after P8-FT-66 is selected, ranked,
recommended or named. Completion unlocks only generation-safe private match-plan
availability.

P8-FT-66 is complete. The existing `full_text_request_controller_test` covers
by-value isolation, worker and GUI thread boundaries, typed delivery,
replacement, pending/running cancellation, detach, facade replacement and
teardown. The existing full-text dialog smoke independently covers every
MainWindow identity gate, duplicate rejection and absence of visible effects.
No test was registered; the Release baseline remains exactly 109.

### P8-FT-67 private CSS Custom Highlight plan application acceptance (completed)

The completed implementation starts from the selected clean synchronized
migrated/local/upstream/live-remote revision
`c8bfcd77e01a243e3b565ebc818151c2255a0a2c` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. P8-FT-67 supersedes P8-FT-66's
historical no-successor closure and selects only private CSS Custom Highlight
application of the accepted ordered plan. Acceptance is grounded in current
`main_window.cpp:8479-8695`, `main_window.h:499-523`, the existing private
ApplicationWorld execution at `article_view.cpp:70-107`, the Qt 6.11.1 pin at
`conanfile.py:51-58`, and pinned legacy `articleview.cc:2569-2791`.

Focused offscreen WebEngine coverage in the existing full-text application
smoke must prove the Qt5-parity application contract: repeated literals and
case-sensitive/case-insensitive grouping highlight every DOM occurrence of each
unique supplied literal; distinct literals are highlighted simultaneously;
Unicode and a match spanning adjacent text nodes map without changing DOM
structure; the range mapped from the first ordered plan item is selected and
scrolled into view; and private state records current position zero with the
complete plan order intact. Empty plans clear
the private highlight and state. The test must also prove system mark styling,
no wrapper insertion or text-node normalization, and unchanged article links,
markup and ordinary find-in-page behavior.

Coverage also pins token ownership: delayed generation A success/failure and
token-scoped cleanup after generation B publication leave B's highlight,
stylesheet, selection and ordered position intact. Capability-probe failure,
rendered-text/DOM or literal mismatch, JavaScript
failure and partial construction must leave no private registry entry,
selection or applied state. Existing generation/query-policy, lookup, search,
navigation, tab, view and page identity gates must be rechecked at asynchronous
completion; replacement activation/search/navigation, tab close, page
replacement, facade detachment and teardown must remove an applied highlight
and reject stale callbacks. Coverage must prove that application neither
reinterprets the Core plan nor consumes `ignore_diacritics`.

P8-FT-67 changes only existing GUI smoke coverage and registers no test. It
changes no installed/Core/C API or DTO, index, configuration, dependency,
catalog, translation or executable. Headword-only result rows, the exact
dictionary-name tooltip, exact activation, status wording and exactly 109
registered Release tests remain unchanged. Previous/Next commands are not
tested or specified by this leaf. Compiled gates are omitted for this
four-document-only audit. No successor after P8-FT-67 is selected, ranked,
recommended or named. Completion will unlock only generation-bound ordered
applied-range state.

### P8-FT-68 private ordered applied-range navigation command prerequisite acceptance (completed)

The completed implementation was selected by clean synchronized revision
`67dd57c26f1f7b7af1021ffb1041947ddb0c2f20` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It supersedes P8-FT-67's
historical no-successor closure and selects only P8-FT-68. Acceptance is
grounded in current `article_view.cpp:90-256` and `main_window.h:499-523`, plus
pinned legacy `articleview.cc:2703-2704,2730-2788` for one-step non-wrapping
navigation, boundary availability and match-position semantics.

Focused coverage in the existing GUI smoke target proves forward and
backward one-range movement, atomic selection/scroll/position update, initial
position zero, first and last boundaries, a one-range plan, empty state, token
mismatch, stale ownership and lifecycle invalidation. A boundary-unavailable
command must return the unchanged current snapshot with no DOM, selection,
scroll or position effect. Missing, empty, stale and token-mismatched state must
be rejected without effects. The private callback must distinguish accepted
current state from rejection and report token, zero-based position, ordered
count, `can_previous` and `can_next` equivalents.

Acceptance proves no controls or button bindings, status mutation,
translations, F3/shortcut behavior, `findText` calls or ignore-diacritics
consumption. It must preserve P8-FT-67 highlighting and first selection,
ordinary find-in-page, exact activation, headword-only rows and dictionary
tooltips. No installed/Core/C interface or DTO, index, configuration,
dependency, catalog, translation, executable or test-registration gate changes;
exactly 109 registered Release tests remain unchanged. No successor after
P8-FT-68 is selected, ranked, recommended or named. Completion unlocks only
private full-text navigation UI/status binding.

### P8-FT-69 private per-article full-text navigation row binding acceptance (completed)

The completed implementation was selected by clean synchronized revision
`35b2ffb94d3b819b7fe6242585fc5fa0729906b9` and unchanged clean pinned
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` supersedes P8-FT-68's
historical no-successor closure and completed only P8-FT-69. Acceptance is
grounded in pinned legacy `articleview.ui:58-100` for the dedicated per-article
row below the web view and exact source captions `&Previous` and `&Next`, and
`articleview.cc:220-230,2688-2706,2730-2788` for translated
`%1 of %2 matches`, one-based status, initial enablement, boundaries and
one-step non-wrapping navigation. Current P8-FT-68 supplies the accepted typed
token, zero-based position, ordered count, `can_previous` and `can_next`.

Focused coverage in the existing GUI smoke target proves a distinct row
owned by every `ArticleView`, below its web content and separate from the
ordinary find toolbar. Initial, single-range, interior, first-boundary and
last-boundary snapshots must produce exact captions, `position + 1` status and
button state solely from an accepted identity-current snapshot. Forward and
backward completions update only after the complete identity recheck; rejected,
stale, token-mismatched, detached and teardown callbacks leave the current UI
unchanged. Tab switching reveals only the active tab's retained current row;
replacement, page/load/view invalidation, tab close and teardown hide and
clear the affected row.

Coverage also proves that the ordinary find controls, state, status,
shortcuts and `findText` path are unchanged and never reused. F3 remains the
exclusive migrated Dictionaries shortcut, and no F3/Shift+F3 full-text binding
is added. The leaf does not consume `ignore_diacritics` or change any
installed/Core/C interface or DTO, index, configuration, dependency, catalog,
activation, headword-only row or dictionary-tooltip contract. It registers no
test and preserves exactly 109 registered Release tests. No successor after
P8-FT-69 is selected, ranked, recommended or named.

### P8-FT-70 ICU normalized matching and origin-map acceptance (complete)

Acceptance was implemented from synchronized selected revision
`c91dfc628bea5382c2dc10182e848561c919305e` and clean pinned legacy
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It completes only the Core
normalization and origin-map prerequisite. Implementation evidence is
`full_text_matcher.cc`, `full_text_matcher.h`, `full_text_index.cc`,
`desktop_facade.cc` and
`main_window.cpp:9040-9078`, plus pinned legacy
`fulltextsearch.cc:596-609` and `articleview.cc:133-190,2569-2648`.

Focused cases in existing Core executables apply the identical query/source
pipeline: NFD; ICU full case fold only when `match_case` is false; NFD again;
remove all `Mn`, `Mc` and `Me` only when `ignore_diacritics` is true; then NFC.
The four policy combinations cover precomposed and decomposed equivalents,
canonical mark reordering, combined case-sensitive/ignore-diacritics positive
matches and wrong-case rejection, and ignore-disabled preservation.

Origin-map cases prove spans survive every expansion and contraction. They
cover ICU case-fold expansion such as `ß` to `ss`, folding that emits marks,
canonical contraction/equivalence, repeated normalized units sharing one
original cluster, attached `Mn`/`Mc`/`Me`, leading and unattached Marks, and
supplementary-plane scalars. Every result is the minimal contiguous complete-
original-cluster range covering the normalized units touched even when a
boundary falls inside an expansion. Byte offset, byte length and literal must
identify the exact complete valid-UTF-8 original slice.

Repeated normalized units with one origin yield no duplicate or zero-length
occurrence. Accepted ranges advance past every normalized unit whose origin
overlaps the accepted range; empty, backward, duplicate and overlapping
candidates are skipped with forward cursor progress. Coverage proves
leftmost-first original-range non-overlap for adjacent and repeated matches,
all four query modes, every valid word-order and distance combination,
unchanged rejection of invalid pattern-mode combinations, and agreement
between private matcher and indexed full-text behavior.

Existing cases continue to cover malformed patterns, cancellation, deadlines,
resource bounds and `full-text-v1` serialization. P8-FT-70 changes no public
layout or ABI, C API, DTO, rendered request, Widgets path, configuration,
dependency, catalog, translation, executable or test registration. Activation,
headword-only rows, exact dictionary tooltips, ordinary find and exactly 109
registered Release tests remain unchanged. GET's intentional ICU divergence
from Qt5 custom folding and trailing `Mark_NonSpacing` behavior is explicit.
Rendered request and Widgets consumption remain unresolved. No successor is
selected, ranked, recommended or named; completion unlocks only the Core
normalized matching/origin-map dependency boundary.

### P8-FT-71 rendered-text ignore-diacritics acceptance (completed)

P8-FT-71 was implemented from synchronized migrated revision
`14b78a90dd5a37dfa4a3381aeebf7a559eb9ae5d` with clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Evidence is
`desktop_facade.h:169-178`, `desktop_facade.cc:130-164`,
`main_window.h:455-522`, `main_window.cpp:9063-9344`,
`rendered_text_match_plan_controller.cpp:24-215`,
`article_view.cpp:88-205`, and pinned legacy
`fulltextsearch.cc:596-609` plus `articleview.cc:133-190,2569-2648`. The
completed leaf transports and consumes only the accepted flag.

Focused Core coverage in the existing application-service target
set the new default-false request member and prove all four independent
`match_case`/`ignore_diacritics` policies, canonical equivalents, and exact
complete original UTF-8 byte offsets, lengths and literals. Existing malformed
pattern, invalid request, cancellation, deadline and resource-bound behavior
must remain unchanged.

Existing controller coverage proves that `ignore_diacritics` survives the
request's by-value asynchronous copy and cannot be revived or overwritten by
replacement, cancellation, stale completion or consumer detachment. Existing
GUI smoke coverage proves true and false accepted-policy propagation,
request-owned stale-policy rejection, exact DOM application, highlight-all,
first selection and unchanged P8-FT-69 Previous/Next and status snapshots.
Tab/view/page/generation isolation, failure clearing and ordinary find-in-page
remain required.

No new executable or test registration is permitted; the Release baseline
remains exactly 109. No C API, facade signature/vtable, configuration,
dependency, index serialization, catalog, translation or JavaScript-interface
test change is required. The necessary installed request DTO source/layout
change must be explicit. Documentation validation requires current and legacy
citation checks, cross-document terminology review, the four-file allowlist,
`git diff --check`, ref equality and clean migrated and pinned worktrees. No
further successor is selected, ranked, recommended or named.

### Phase 8 P8-FT-72 Core full-text index lifecycle contract prerequisite (completed)

P8-FT-72 was implemented from migrated revision
`0b84e6dc2ab627c613c483a931b995f6c0554191` and pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Core owns
full-text index policy and lifecycle coordination; Widgets may edit policy,
issue rebuild/cancel intents and consume immutable snapshots only; adapters
report capability/source revision and execute bounded cancellable work.

`full_text_index_test` uses a deterministic fake format-work port to cover
policy defaults/equality, rebuild/cancel intent identity, immutable generation-
and dictionary-identified snapshots, capability/opaque-source-revision
reporting, bounded request transport, cancellation observation and contained
explicit/exception failures. Changed generation and dictionary identities are
tested independently. No executable or registration was added, preserving
exactly 109 Release registrations.

The tests require no coordinator execution, persistence application,
Preferences or Widgets, visible progress/status, a real format adapter, index
serialization or a complete rebuild. They must not encode progress percentage,
queue/concurrency, legacy two-pass ordering, retry or failure-presentation
policy. The delivery gate is the focused existing Core target, Linux
Release configure/build and 109/109 CTest, install and packaged consumers, and
exact-SCM package creation. Documentation validation requires citation,
changed-section, registration-count, ref/worktree and `git diff --check`
checks. No successor is selected or ranked.

### Phase 8 P8-FT-73 private Core lifecycle coordinator (completed)

The documentation-only readiness audit at migrated revision
`fbc50b18fb183f69c34b524db869140a3760da25` and pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8` selected one private Core
coordinator state machine. Its completed implementation test boundary remains
the existing `full_text_index_test` registration with a deterministic fake
P8-FT-72 format-work port.

Focused coverage proves requested-to-working-to-terminal transitions;
independent generation and dictionary identity; capability rejection and
not-indexed state; exact source-revision capture; bounded request forwarding;
current success, cancellation and contained failure; cancellation idempotence;
replacement during work; and suppression of stale success, cancellation,
exception and failure completions. Snapshot assertions consume immutable
values, and the fakes verify cancellation through the existing token. They also
pin equal or older generation rejection, cancellation before, during and after
work, repeated cancellation, exact bounds and deadline forwarding, contained
explicit, standard and unknown failures, and the absence of implicit work,
retry or progress behavior.

Tests must not require a real adapter, persistence/configuration application,
facade or Widgets transport, automatic scheduling, queue/concurrency, legacy
two-pass ordering, retry, progress, visible failure behavior, serialization or
the complete rebuild workflow. No new executable or registration was added,
so the Release baseline remains exactly 109. This audit requires citation and
terminology review, the four-file allowlist, `git diff --check`, exact
registration counting, reference equality and clean migrated and legacy
worktrees. P8-FT-73 is complete, and no dependency beyond it is selected or
named.

### Phase 8 P8-FT-74 article-count policy-ingress acceptance (completed)

The completed implementation from migrated base revision
`a5e013d8164550f4757c6d4a58949cb575624989` and pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8` covers one implementation
boundary: correct maximum-dictionary terminology and project the three
persisted lifecycle inputs by value into private Core policy. Evidence is
current `application.h:268-278`,
`configuration.cc:141,189,219-220,746-750,1542,1557-1559`,
`legacy_configuration.cc:403,451,471-472`, and pinned legacy
`config.hh:156-181`, `preferences.ui:1371-1395`,
`preferences.cc:360-382,490-575` and `stardict.cc:202-206`.

The existing `application_service_test` acceptance boundary proves default
zero, current misnamed-key canonical read/write round trip, legacy XML
migration, acceptance of `10000000`, rejection of `10000001`, and independence
from query `full_text_maximum_articles_per_dictionary`. The existing
`full_text_index_test` boundary proves the corrected
`maximum_dictionary_articles` default and equality plus exact by-value
projection of enablement, article count and raw disabled-format text. Mutating
the source preferences after projection must not alter the policy value.

The installed preference member becomes
`full_text_maximum_dictionary_articles`; its unchanged type, order and layout
do not remove the explicitly documented C++ source compatibility rename. The
serialized current key `full_text_maximum_dictionary_megabytes` remains the
canonical compatibility spelling, and Qt5 import continues to read
`fullTextSearch.maxDictionarySize`. Tests must not introduce a byte limit,
second field, alias member or dual interpretation.

No coordinator generation, request, port call, lifecycle transition,
cancellation, scheduling, adapter, composition, facade/Widgets transport, UI,
serialization or complete rebuild behavior is in scope. No executable or
registration is added, so the Release baseline remains exactly 109.
Implementation delivery uses the two focused existing targets, Linux Release
configure/build, 109/109 CTest, installed/package consumers and exact-SCM
package creation. Delivery includes citation and terminology review,
the four-file allowlist, `git diff --check`, registration counting, reference
equality and clean migrated and legacy worktrees. P8-FT-74 is complete. No next
dependency is selected.

### Phase 8 P8-FT-75 registration metadata and policy-eligibility acceptance

The completed P8-FT-75 implementation is grounded at migrated base revision
`18023f3aebae9ad610fa1b9afcb505dc946b7a1a` and pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Evidence is current
`dictionary_service.cc:450-464,687-1036`, the authoritative reader article-
count assignments in the twelve textual dictionaries,
`full_text_index_lifecycle.h:19-180`,
`full_text_index_lifecycle.cc:15-246`, and pinned legacy per-format
`setFTSParameters` implementations such as `stardict.cc:202-206`.

Extend only `full_text_index_test`. Registration coverage accepts each exact
canonical ASCII identifier independently: `AARD`, `BGL`, `DICTD`, `DSL`,
`MDICT`, `SDICT`, `SLOB`, `STARDICT`, `XDXF`, `ZIM`, `EPWING` and `GLS`. It
rejects empty, unknown ASCII, differently cased, embedded-NUL and representative
non-ASCII identifiers. Each invalid case is atomic before duplicate lookup or
port probing: test both a new dictionary ID and an invalid request reusing an
existing ID, proving no insertion, replacement, snapshot change, cancellation
or identity/generation mutation. Also prove null-port, empty-ID and duplicate-
ID rejection plus copied metadata immunity to caller mutation.

Pure-predicate coverage proves enabled and disabled policy; exact and mixed-
case matches; prefix and suffix partial-substring matches; ordinary non-match;
whitespace and delimiter non-normalization; non-ASCII surroundings; and an
embedded NUL in disabled text. The length-aware helper folds only ASCII
`A`-`Z`, compares all other bytes unchanged and is tested directly without
depending on or changing process-global locale. Article coverage proves zero
unlimited, below and equal limits eligible, and above limit excluded.

Coordinator coverage distinguishes technical `kUnavailable` from
`kPolicyExcluded`. A supported excluded generation is accepted, capability
true and revision-empty; it replaces and cancels older work but performs no
source-revision or bounded-work call. Exact cancellation is accepted and
idempotent without changing excluded state, and execution is rejected.
Complete every stale-result outcome after replacement and prove none can
overwrite the excluded snapshot, then submit a newer eligible generation and
prove normal requested work resumes. Eligible source-revision exceptions
remain `kFailed`. Generation-zero not-indexed/unavailable/failed probing and
all P8-FT-72/73/74 identity, monotonicity, bounds, cancellation and failure
cases remain unchanged.

No executable or registration is added, so the Release baseline remains
exactly 109. No real adapter, composition wiring, automatic persisted-policy
apply/restart, facade/Widgets transport, UI, scheduling, progress, retry,
two-pass ordering, serialization or complete rebuild is tested or selected.
Installed/public C++, facade, C, DTO and configuration ABI, dependencies,
`full-text-v1`, ordinary find, Dictionaries-only F3 and translations remain
locked. Documentation validation requires current and legacy citation review,
the four-file allowlist, terminology review, `git diff --check`, exact
registration counting, reference equality and clean worktrees. P8-FT-75 is
complete. No successor is selected or named.

### Phase 8 P8-FT-76 immutable index-publication acceptance (completed)

The completed implementation is grounded at synchronized migrated base revision
`c32aa1217bb9934b4b8ede1e1623a12c7a1777b3` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It extends
only the existing `full_text_index_test` registration with the private Core
immutable snapshot-publication contract; it adds no executable or registration,
so the Release baseline remains exactly 109.

Focused coverage proves initial absence, successful non-null publication,
exact acquired pointer identity, replacement visibility and continued validity
of a prior snapshot retained by an in-flight reader. Deterministic concurrent
coverage shows that readers observe either the complete old index or the
complete new index, never partial or destroyed state. Rejected null publication
leaves the current snapshot unchanged. The holder uses C++17 atomic shared-
pointer operations; tests do not require lock-free behavior. Tests exercise
only the narrow acquisition and
publication abstraction, not its synchronization mechanics, and prove that it
owns no building, lifecycle, generation or scheduling responsibility.

Off-side construction acceptance proves that failure, cancellation, deadline
expiry, resource-bound rejection and stale-generation outcomes perform no
publication. Format-specific bounded
incremental traversal, a real adapter port and the exact generation-authorized
handoff remain excluded until a bridge is independently selected. Tests must
not add composition, persisted-policy apply/restart, scheduling, progress,
facade/Widgets transport, UI or serialization behavior, and must preserve every
P8-FT-72 through P8-FT-75 identity, policy and lifecycle case.
No speculative interface or pattern receives acceptance coverage.

Delivery includes the exact eight-file implementation allowlist,
`git diff --check`, ten consecutive focused Release runs, exactly 109
registrations and 109/109 passing, Release install, standalone and packaged C
and C++ consumers, exact-SCM package creation, local/upstream/live-remote
equality and clean migrated and pinned legacy worktrees. P8-FT-76 is complete.
No successor is selected or named.

### Phase 8 P8-FT-77 bounded AARD traversal acceptance (completed)

The completed implementation is grounded at migrated base revision
`a4393dcb1fd3bf4cd13938dddc909ef96af26135` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It extends
only existing AARD reader and dictionary test registrations; no executable or
test is registered, so the Release baseline remains exactly 109.

Reader coverage proves deterministic current record order, first-record
deduplication, exact record/article ordinals, headword and payload, and one
checkpoint before every record inspection including duplicates. A checkpoint
throw before a record emits nothing for that record or later records; a visitor
throw emits nothing later and propagates unchanged. The visitor observes a
non-owning article only for the callback duration, and the reader retains no
visitor result or traversal state.

Dictionary coverage preserves the current fixture results: exact
`aard-index:<record_ordinal>:<article_ordinal>` IDs,
assembled plain text, generated-index state, full-text results, document
resolution and error translation remain unchanged. The tests add no lifecycle
port, request-bound enforcement, registration, generation authorization or
snapshot publication expectation. Existing P8-FT-72 through P8-FT-76 cases
remain locked; cross-format traversal, policy restart, scheduling, progress,
facade/UI transport, serialization and two-pass ordering remain excluded.

Delivery validation uses the exact eight-file implementation allowlist,
`git diff --check`, ten consecutive focused Release runs, exactly 109 Release
registrations and 109/109 passing, Release install, standalone and packaged C
and C++ consumers, exact-SCM package creation, local/upstream/live-remote
equality and clean migrated and pinned legacy worktrees. P8-FT-77 is complete.
No successor is selected or named.

### Phase 8 P8-FT-78 generation-authorized snapshot-handoff acceptance (completed)

The completed implementation is grounded at migrated base revision
`5c58b1ead60aece993bd41d49ce763ad67940a47` and clean pinned legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It implements only the
private generation-authorized handoff needed before a real AARD format-work
port.

The implementation extends only the existing `full_text_index_test`
registration. A deterministic port returns an unpublished immutable candidate
and a registered holder exposes it only after the coordinator revalidates the
exact current dictionary/generation and uncancelled state. Coverage must prove
that successful publication precedes `kCurrent`, a retained old reader remains
valid while later readers acquire the complete replacement, and exactly one
current candidate is published.

Replacement and cancellation races must prove that stale successful work
cannot publish or overwrite the newer snapshot. Cancelled, failed, exceptional,
deadline-expired, over-budget, identity-mismatched and completed-with-null
results must leave the holder unchanged; completed-with-null becomes contained
failure and never `kCurrent`. Existing P8-FT-72 through P8-FT-77 lifecycle,
identity, policy, bounds, cancellation, exception, immutable-publication and
AARD traversal cases remain unchanged.

No AARD bridge, catalog/composition wiring, automatic persisted-policy apply
or restart, scheduler, progress, facade/UI transport, serialization, public or
installed interface, dependency, executable or registration is added. The
implementation gate is the focused existing Release test, full 109-test
Release suite, install and packaged consumers, exact-SCM creation and clean ref
equality. Delivery uses the exact seven-file allowlist, `git diff --check`, ten
focused Release repeats, exactly 109 Release registrations, install and
standalone and packaged C/C++ consumers. P8-FT-78 is complete. No successor is
selected or named.

### Phase 8 P8-FT-79 AARD format-work bridge acceptance (complete)

The audit is grounded at synchronized migrated revision
`7a07e41b7ad0e9613a93129bd55c5cf598e06166` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects one private AARD
format-work bridge, authorization-safe prepared-artifact prerequisite and
catalog registration; no successor is selected.

Extend only the existing `aard_dictionary_test`, `full_text_index_test` and
`application_service_test` registrations. Focused cases must prove exact
canonical `AARD` metadata and authoritative article count, capability with and
without an index destination, deterministic sole-archive revision and captured-
revision mismatch rejection. Candidate coverage pins created, reused, stale-
rebuilt and corrupt-rebuilt immutable indexes, unchanged first-record ownership,
assembled text and `aard-index:<record>:<article>` IDs.

Bounded-work cases exercise zero and exceeded document, per-document-byte and
corpus-byte limits, overflow-safe totals, cancellation and deadline at traversal
checkpoints, source revision drift, and escaped reader, assembly, generated-
index and full-text-index failures. Every unsuccessful result carries no
candidate and leaves the holder and canonical artifact unchanged. Candidate
preparation is non-persisting; only coordinator-authorized finalization may
replace the canonical artifact. The port must never publish.

Holder and composition cases prove construction-time seeding, retained old
readers during replacement, complete-call snapshot retention for search and
resolution, availability/state projection, shared dictionary/holder/port
lifetime and exact coordinator registration. Successful work becomes visible
and `kCurrent` only through the P8-FT-78 generation/cancellation revalidation;
replacement and cancellation races cannot publish stale candidates.

No new executable or test registration is added, preserving exactly 109
Release tests. The implementation gate is the focused existing Release tests,
full 109/109 Release CTest, Release install, standalone and packaged C/C++
consumers, exact-SCM package creation, `git diff --check`, allowlist and
ref/worktree checks. Documentation review requires the four P8-FT-79 sections,
citation and terminology checks, `git diff --check`, exactly 109 registrations,
ref equality and clean migrated/pinned worktrees.

P8-FT-72 through P8-FT-78, public/installed boundaries, canonical IDs,
`kPolicyExcluded`, article/work bounds, ICU divergence, ordinary find/F3,
UI/translations, serialization and dependencies remain locked. P8-FT-79 is
complete. No successor beyond it is selected or named.

### Phase 8 P8-FT-80 persisted-policy application acceptance (completed)

The fresh audit is grounded at synchronized migrated revision
`d79180de54bc19076ff3eae4743cf5de40a40e18` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly one private
Core leaf: apply the persisted full-text policy to all registered lifecycle
entries after discovery. No successor is selected.

Completed acceptance extends only the existing `full_text_index_test` and
`application_service_test` registrations. Coordinator cases prove that
one application assigns a strictly newer generation to each registered entry,
uses the existing canonical capability and policy-eligibility rules, captures
an eligible source revision, cancels a superseded generation and produces only
`kUnavailable`, `kPolicyExcluded`, `kFailed` or `kWorkRequested`. Cover enabled
and disabled policy, disabled format matching, maximum-article inclusion and
exclusion, capable and incapable ports, source-revision failure, multiple and
zero registrations, repeated application and monotonic per-entry identities.

Race cases hold requested or working generations while a later policy is
applied and prove the superseded cancellation is observable. Late stale,
cancelled, failed, excluded or unavailable completion cannot finalize a
prepared artifact, mutate the canonical index, publish the holder or reach
`kCurrent`. Existing cooperative cancellation, generation revalidation and all
P8-FT-79 AARD publication-safety cases remain unchanged.

Application-service coverage constructs eligible and excluded AARD
fixtures, projects the persisted preferences after discovery, and observes the
matching requested or policy-excluded lifecycle state without executing work.
The empty-discovery case remains successful. The leaf adds no scheduler,
thread, restart reconciliation, progress/status projection, facade/UI
transport, format port, executable or test registration.

The documentation gate reviews exactly the four P8-FT-80 sections and validates
every current and pinned citation, terminology, `git diff --check`, the bounded
implementation/test allowlist, exactly 109 Release registrations, ref equality
and clean migrated/pinned worktrees. P8-FT-72 through P8-FT-79,
public/installed APIs, `full-text-v1`, canonical IDs, `kPolicyExcluded`,
article/work bounds, ICU divergence, ordinary find/F3, UI/translations,
serialization and dependencies
remain locked. Startup/restart reconciliation, scheduling/submission,
progress/status visibility, other real ports and facade/UI transport remain
unselected and unranked. No successor beyond P8-FT-80 is selected or named.

### Phase 8 P8-FT-81 startup artifact reconciliation acceptance (complete)

The fresh audit was grounded at synchronized migrated revision
`73f9e6cd976379e5f16a4c7eb5deb4e1f965ad80` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly one private
Core leaf: reconcile an exact policy-eligible `kWorkRequested` generation with
an immutable startup artifact already validated against its captured source
revision. No successor is selected.

Acceptance extends only the existing `full_text_index_test`,
`aard_dictionary_test` and `application_service_test` registrations.
Coordinator cases prove that
immutable startup evidence is accepted only for the exact current requested
identity when capability, policy eligibility, source revision, holder snapshot
and uncancelled state all match. Acceptance changes that same generation to
`kCurrent`, retains exact snapshot pointer identity, invokes no work port,
prepares or finalizes no update, publishes no snapshot, rewrites no artifact and
allocates no generation. Repeating acceptance is an idempotent no-op; zero and
multiple registrations are deterministic. A snapshot successfully rebuilt from
a stale or corrupt on-disk artifact is accepted without another write during
reconciliation.

Negative cases cover absent snapshot or evidence, a startup build that
published no snapshot, stale identity, source-revision or snapshot mismatch,
corrupt or otherwise unverifiable evidence, cancellation and replacement before
reconciliation, plus unavailable, `kPolicyExcluded`, failed, current and other
non-requested states. Each remains unchanged and stale evidence cannot later
persist, publish or become current. AARD cases must prove its construction-time
current/reused/rebuilt-stale/rebuilt-corrupt startup snapshot supplies exact
evidence without another traversal or canonical-file write. Application-service
startup must reconcile eligible AARD to `kCurrent`,
leave excluded AARD `kPolicyExcluded`, and keep empty discovery successful.

The completed implementation leaf adds no scheduler, thread, queue, retry,
progress/status projection, additional format port, facade/UI transport,
executable or test registration. Its delivery gate uses the three focused
existing Release tests, full 109/109 Release CTest, Release install, standalone
and packaged C/C++ consumers, exact-SCM package creation and clean ref equality.

The documentation gate reviews exactly the four P8-FT-81 sections and validates
every current and pinned citation, terminology, `git diff --check`, the exact
four-file allowlist, exactly 109 Release registrations, ref equality and clean
migrated/pinned worktrees. P8-FT-72 through P8-FT-80, public/installed APIs,
`full-text-v1`, canonical IDs, `kPolicyExcluded`, article/work bounds, ICU
divergence, ordinary find/F3, UI/translations, serialization and dependencies
remain locked. No successor beyond P8-FT-81 is selected or named.

### Phase 8 P8-FT-82 bounded work-request projection acceptance (complete)

The fresh audit was grounded at synchronized migrated revision
`333bdbbca0812c8289bdc3194d66cd17300ecbee` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly one private Core
leaf: validate immutable execution bounds and project the request for the exact
current eligible `kWorkRequested` generation. No successor is selected.

Acceptance extends only the existing `full_text_index_test` registration.
Positive cases prove exact nonzero maximum-document, per-document-byte and
corpus-byte values and a future absolute deadline reach the projected request.
The request's identity, policy, captured source revision and cancellation must
come from the coordinator's exact generation rather than caller input.
Projection leaves the generation `kWorkRequested`, invokes no format port,
prepares or finalizes no update, writes no artifact, publishes no snapshot and
allocates no generation. Repeated projection is deterministic and
side-effect-free; zero and multiple registrations remain isolated.

Negative cases cover each zero resource bound, the current multiplication-
representability restriction, corpus/document incoherence, an expired
deadline, unknown dictionary, stale or
mismatched identity,
replacement and cancellation, plus unavailable, `kPolicyExcluded`, failed,
working, current and every other non-requested state. Each produces no request
and no observable mutation. A successful projected request must remain
executable through the existing bounded-work seam with exact bounds while all
existing cancellation, revision, Prepare/Finalize, stale-completion,
persistence and snapshot-publication assertions continue to pass.

No new executable or test is registered, so the Release baseline remains
exactly 109. The implementation gate is the focused existing Release
test, full 109/109 Release CTest, Release install, standalone and packaged C/C++
consumers, exact-SCM package creation, and clean ref equality. P8-FT-82 adds no
submission, scheduler, dispatcher/executor ownership, thread, queue,
shutdown/join, retry, progress/status, additional format port, facade/UI
transport, public/installed API or dependency.

The documentation gate reviews exactly the four P8-FT-82 sections and validates
every current and pinned citation, terminology, `git diff --check`, the exact
seven-file implementation/test/documentation allowlist, exactly 109 Release
registrations, ref equality and clean migrated/pinned worktrees. P8-FT-72
through P8-FT-81, `full-text-v1`, canonical
IDs, `kPolicyExcluded`, article/work bounds, ICU divergence, ordinary find/F3,
UI/translations, stale/artifact/snapshot safety, serialization and dependencies
remain locked. P8-FT-82 is complete, and no successor is selected or named.

### Phase 8 P8-FT-83 deterministic work-discovery acceptance (complete)

The implementation is grounded at synchronized migrated revision
`93590fa656b06d31bdd3c92bc477f72fdbb5256f` and clean pinned legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It completes exactly one private
Core leaf: side-effect-free discovery of actionable `kWorkRequested`
identities in canonical dictionary-ID order through
`DiscoverRequestedWork() const`.

Acceptance extends only the existing `full_text_index_test` registration.
Positive cases cover zero registrations, no actionable entries, one entry and
multiple entries registered out of dictionary-ID order. Discovery must return
only each accepted current generation's exact identity in canonical order when
the entry remains `kWorkRequested`, format-capable, policy-eligible,
uncancelled and backed by a cancellation token. Repeated calls with unchanged
state return the same vector and leave registrations isolated.

Negative cases exclude initial `kNotIndexed`, unavailable,
`kPolicyExcluded`, working, current, cancelled and failed entries, superseded
generations, newly ineligible policy and cancellation. Discovery invokes no
format work, projects no bounds, claims no state, accesses no artifact,
prepares or finalizes no update, writes no canonical file, publishes no holder
snapshot and allocates no generation. A discovered identity must project
successfully through P8-FT-82 with valid bounds, while replacement or
cancellation between discovery and projection must make that same identity
fail safely.

No new executable or test is registered, so the Release baseline remains
exactly 109. The implementation gate is the focused existing Release
test, full 109/109 Release CTest, Release install, standalone and packaged C/C++
consumers, exact-SCM package creation and clean ref equality. P8-FT-83 adds no
executor/dispatcher ownership, submission, thread, queue, concurrency limit,
shutdown/cancel/join orchestration, retry, progress/status, two-pass ordering,
format port, facade/UI transport, public/installed API or dependency.

The documentation gate reviews exactly the four P8-FT-83 sections and
validates every current and pinned citation, terminology, `git diff --check`,
the exact seven-file implementation/test/documentation allowlist, exactly 109
Release registrations, ref equality and clean migrated/pinned worktrees.
P8-FT-72 through P8-FT-82,
`full-text-v1`, canonical IDs, `kPolicyExcluded`, article/work bounds, ICU
divergence, ordinary find/F3, UI/translations, stale/artifact/snapshot safety,
serialization and dependencies remain locked. P8-FT-83 is complete. No
successor beyond P8-FT-83 is selected or named.

## P8-FT-84 Private Serial Executor Acceptance (Complete)

P8-FT-84 remains within the existing `full_text_index_test` registration. Its
controllable fake ports use condition-based gates and add no timing sleeps,
executable or registration. Focused acceptance proves:

- exactly one port call can be active and actionable identities execute in the
  P8-FT-83 canonical dictionary-ID order;
- submitted immutable bounds reach work only through P8-FT-82 projection, and
  `ExecuteBoundedWork` remains the sole successful claim;
- concurrent submissions coalesce into one pending sweep without duplicate
  queues or retry, while work requested after a discovery snapshot waits for a
  later explicit submission;
- stale, replaced, cancelled, expired, duplicate and already-working
  identities cannot invoke unintended work or weaken publication safety;
- one port failure does not escape the worker or trigger retry and does not
  prevent later identities in the accepted sweep from being considered;
- shutdown rejects new submission, discards an unclaimed pending sweep,
  cancels the exact active identity and joins before coordinator or fake-port
  teardown; and
- repeated shutdown and destruction are safe and leave no worker accessing
  released lifecycle, port, cancellation, prepared-update or snapshot state.

The implementation gate is repeated focused Release execution, fresh Release
configure/build, 109/109 CTest, install, standalone and packaged C/C++
consumers, exact-SCM Conan creation, `git diff --check`, the exact eight-file
allowlist, synchronized refs and clean worktrees. P8-FT-72 through P8-FT-83,
public/installed APIs, dependencies, `full-text-v1`, persistence/snapshot
safety and UI/find/F3 remain locked. P8-FT-84 is complete. At its completion,
no successor beyond it was selected or named.

## P8-FT-85 Overflow-Safe Bounds-Coherence Acceptance (Selected)

P8-FT-85 remains within the existing `full_text_index_test` registration and
changes only P8-FT-82 arithmetic validation. For positive `D`, `B` and `C`, the
test accepts exactly mathematical `C <= D * B` by quotient/remainder comparison
without evaluating the product. It rejects `D < C / B` and equality with a
nonzero `C % B`; all other positive cases are coherent.

Focused implementation acceptance must prove:

- every zero bound and expired deadline remains rejected;
- representable equality and quotient boundaries with zero and nonzero
  remainders behave exactly as specified;
- one-below, equal and one-above coherence cases are covered where each value
  is representable;
- coherent mathematical products larger than `SIZE_MAX`, including
  `D == B == C == SIZE_MAX`, are accepted and forwarded unchanged;
- synthetic 32- and 64-bit-width values exercise the same arithmetic rule
  independently of the build host; and
- projection remains deterministic and side-effect-free, retains authoritative
  lifecycle fields, and invokes no port or executor work.

The future implementation gate is the focused existing Release test, fresh
Release configure/build, 109/109 CTest and the established install/package/API
checks. This documentation leaf runs no compiled test. It adds no production
bounds provider, composition or submission, executor behavior, lifecycle
transition, artifact/snapshot/persistence change, public API, dependency, UI
or registration. P8-FT-72 through P8-FT-84 remain locked, exactly 109
registrations remain required, implementation of this private correction is
the only dependency unlocked, and no successor beyond P8-FT-85 is selected.

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
. build/Release/generators/conanbuild.sh
. build/Release/generators/conanrun.sh
cmake --fresh --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release --output-on-failure
cmake --install build/Release
```

That workflow uses the default `install_mode=library` and verifies the SDK
install boundary. It is not a self-contained application deployment. For a
clean-environment runtime smoke, configure a separate runtime-mode build:

```sh
conan install . --build=missing -s build_type=Release \
  -pr:h=profiles/qt-webengine -pr:b=default \
  -o '&:install_mode=runtime'
. build/Release/generators/conanbuild.sh
. build/Release/generators/conanrun.sh
cmake --fresh --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release -R goldendict_installed_runtime_smoke \
  --output-on-failure
```

Run the full workflow after changes to CMake, Conan, modules, applications,
tests, install behavior, or dependency configuration. For
documentation-only changes, a full build is not required. If the full workflow
is skipped, mention why in the final response or pull request notes.

When install verification includes a runtime-dependency self-contained smoke
test, use `install_mode=runtime` and leave
`install_runtime_dependencies=auto`. Run the installed wrapper from a clean
environment; on Linux, keep only the normal system command path with
`PATH=/usr/bin:/bin`. On Windows, clear `Path`. Do not use a library-mode
install for this check: it intentionally omits third-party runtime deployment.

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
