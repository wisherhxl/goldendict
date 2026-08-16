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
