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
