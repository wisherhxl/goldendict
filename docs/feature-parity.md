# GoldenDict Linux Feature-Parity Matrix

## Status Vocabulary

- `done`: implemented and verified in the Qt 6 migration.
- `mapped`: legacy behavior and ownership are identified, but code is not yet
  migrated.
- `slice`: required by the Phase 4 first-usable vertical slice.
- `later`: required for the formal Linux release but intentionally sequenced
  after the vertical slice.
- `post-linux`: restored after the Linux acceptance gate.

## Build, Packaging, And Product Identity

| Capability | Status | Target gate | Verification |
| --- | --- | --- | --- |
| GoldenDict 1.6.0 identity | done | Phase 2 | CMake and Conan metadata |
| Qt 6.11.1 with Qt WebEngine | done | Phase 2 | clean Conan package build |
| Linux Release build/install/TGZ | done | Phase 2 | direct build and CPack |
| Conan consumer package | done | Phase 2 | `conan create` and `test_package` |
| GPL product / MIT Tiger boundary | done | Phase 2 | installed/package notices |
| Public `goldendict_core` library boundary | slice | Phase 4 | shared build, install, and exported CMake target |
| GUI depends only on application facade | slice | Phase 4 | startup and replacement atomically compose local, online, and shell-free external runtime sources behind the facade; include-boundary and focused application tests |
| AI-ready headless dictionary API | slice | Phase 4 | structured, bounded, provenance-bearing non-GUI consumer |
| Debug, runtime bundle, DEB/RPM, CI | mapped | Phase 9 | clean release matrix |

## Core Lookup Path

| Capability | Status | Target gate | Verification |
| --- | --- | --- | --- |
| Minimal configuration load/persist | slice | Phase 4 | clean-profile round-trip |
| Legacy configuration migration | slice | Phase 8 | bounded one-shot XML import covers paths, groups, portable preferences, exact tab-opening preferences, opaque 64 KiB main-window geometry, all P4-supported online/external sources, exact portable/Linux/Windows/macOS legacy profile selection, and platform-specific history/favorites companion discovery; independent current precedence, deterministic no-fallback selection, per-destination atomic persistence, strict representability, secret exclusion, and untouched legacy sources are pinned; the pinned legacy application has no persisted article-session format; legacy version-1 opaque main-window state is intentionally excluded because no authentic pinned artifact or exact Qt runtime compatibility contract exists, and Windows-specific normal/maximized rectangle/window-mode migration is likewise unsupported by evidence; current Qt 6 state versions 2 through 7 are bounded and transactionally supported with rollback and semantic defaults, while the backed shell hierarchy and complete canonical File/View/Edit/Search/History/Favorites/Help menu bar are legacy-compatible |
| Dictionary path discovery | slice | Phase 4 | temporary-directory tests |
| Dictionary identity and index lifecycle | slice | Phase 4 | deterministic index tests |
| Async word/article/resource requests | slice | Phase 4 | completion/cancel/error tests |
| Exact headword lookup | slice | Phase 4 | generated StarDict fixture |
| Ranked Unicode-folded prefix lookup | slice | Phase 5 | bounded prefix ranking and installed-consumer tests |
| Unicode case/diacritic/punctuation folding | slice | Phase 5 | data-driven text and folded-lookup tests |
| Bounded legacy text encoding conversion | slice | Phase 5 | strict Latin-1, UTF-16, GB18030, and EUC-JP fixtures |
| Headword suggestions | slice | Phase 5 | bounded ranked suggestion fixtures and installed-consumer tests |
| Morphology and transliteration | later | Phase 5 | per-language fixtures |
| Full-text search | slice | Phase 5/6/8 | Installed bounded query and all twelve private adapters are accepted; P8-FT-1 through P8-FT-13 complete the cancellable request path, persisted query modes and bounds, capability/filter projection, modeless dialog, response model/synchronization, visible ordered result list, and private activation intent. P8-FT-14 preserves absent, authoritative-empty, and ordered nonempty lookup scope through navigation identity, history, session restoration, replay, and participation refresh. P8-FT-15 privately binds the exact submitted scope to the accepted response and delivers result and immutable scope by value. P8-FT-16 privately connects activation to existing scoped current-tab navigation with exact headword and accepted scope. P8-FT-17 presents the accepted retained-result count. P8-FT-18 is complete: the private response model exposes each valid row's exact UTF-8 dictionary name through `Qt::ToolTipRole`, preserves independent names for duplicate-headword rows, and suppresses empty names without metadata fallback. P8-FT-19 is complete: the same private model exposes each valid row's exact UTF-8 headword through `Qt::EditRole`, identical to `Qt::DisplayRole`. P8-FT-20 is complete: the private result list has at most one current and selected row, never auto-selects or steals focus for an accepted response, clears current selection atomically for replacement, never retains it into the replacement even at the same row, and cannot restore it from stale or cancelled completion. P8-FT-21 is complete: a private Widgets delegate derives direction for each displayed headword, paints right-to-left text with left elision and all other text with right elision, preserves explicit no-elision, and changes no model, ordering, tooltip, count, selection, focus, activation, or response behavior. P8-FT-22 is complete: one private Widgets status reads `Results may be incomplete.` exactly for a generation-current accepted response whose authoritative `partial` flag is true; initial and replacement states and complete responses hide it, stale/cancelled/detached completions cannot alter it, and no raw error detail is exposed. P8-FT-23 is complete: one private Widgets status reads `No matches` exactly for a generation-current accepted response with zero retained results, `partial == false`, and no errors; initial and replacement states and nonempty, partial, or error-containing responses hide it, stale/cancelled/detached completions cannot alter it, and the partial status remains independent. P8-FT-24 is complete: one private Widgets status reads `Full-text search failed` exactly for a generation-current accepted response with zero retained results, `partial == false`, and one or more errors; initial and replacement states and conclusive-empty, nonempty, or partial responses hide it, stale/cancelled/detached completions cannot alter it, and no dictionary ID, error code, backend message, or raw detail is exposed. P8-FT-25 is complete: one private Widgets status reads `Some dictionaries could not be searched` exactly for a generation-current accepted response with one or more retained results and one or more errors; it hides for initial/replacement, result-free, and error-free states, does not infer or alter authoritative partiality, may coexist with the partial status only when `partial == true`, and exposes no error details. P8-FT-26 is complete: one private Widgets status reads `No matches in searched dictionaries` exactly for a generation-current accepted response with zero retained results and authoritative `partial == true`; initial/replacement, complete, and nonempty states hide it, it coexists with the partial status without changing authoritative partiality, and it exposes no error details. P8-FT-27 is complete: one private Widgets status reads `Errors: %1` with the accepted response's authoritative decimal error count exactly when one or more errors are present; initial/replacement and accepted error-free states hide it, it may coexist with existing response statuses without changing their predicates or partiality, and it exposes no error details. P8-FT-28 is complete: the private dialog binds each generation's exact submitted query text and authoritative `ignore_diacritics` value to the generation-current accepted response and delivers that context by value with the exact result and immutable dictionary scope; replacement clears it, later query edits cannot change it, and stale/cancelled/lifecycle completions cannot revive or overwrite it. P8-FT-29 is complete: successful full-text activation and navigation acceptance privately replace the target tab's article-search query and status with the exact accepted UTF-8 query, and only the corresponding generation-current nonempty lookup page may dispatch that text through Qt WebEngine's existing literal find operation; tab/view/generation isolation prevents stale lookup, load, or find callbacks from affecting another presentation. P8-FT-30 is complete: every generation-current accepted response produces exactly one private Widgets completion beep, while initial, pending, cancelled, stale, duplicate, detached, replacement-submission, and teardown paths remain silent. P8-FT-31 is complete: the existing Cancel control remains available while idle and dismisses the modeless dialog through its established destruction path, while activation during an active generation cancels only that work, restores idle state, and leaves the dialog open. P8-FT-32 is complete: one independently optional, opaque full-text dialog geometry value round-trips through current configuration and exact legacy migration with a 64 KiB decoded bound and atomic malformed/duplicate/oversized rejection; Core does not interpret Qt geometry. P8-FT-33 is complete: Widgets restores a nonempty value once at new-dialog creation, preserves default geometry on absence or Qt rejection, and captures exact geometry on idle Cancel or window-manager close for the composition root's established atomic save; active cancellation and unrelated lifecycle paths do not persist it. P8-FT-34 is complete: every new private Widgets dialog has the pinned legacy minimum of 430 by 450 logical pixels, and direct resize or restored geometry cannot leave it below either bound without changing P8-FT-33 behavior above the minimum. P8-FT-35 is complete: absent or Qt-rejected geometry initializes every new private Widgets dialog at the pinned legacy 492 by 593 logical pixels; valid restoration still wins and minimum clamping remains unchanged. P8-FT-36 is complete: one private `Help` button and one dialog-scoped F1 action emit the same argument-free private help-request intent exactly once per activation without changing search, response, selection, geometry, or lifecycle state. The delivered `ignore_diacritics` value remains unconsumed because Qt WebEngine exposes no matching policy. Ignore-diacritics semantics and legacy regular-expression equivalence; exact `document_id` navigation and source targeting; columns/icons/additional metadata roles and other decoration; match ranges/excerpt display; Preferences/index policy and persistence; index readiness/visibility/status/progress/background lifecycle/rebuild/failure UI; adapters/index formats and legacy `_FTS`; dependencies/build work; and unrelated parity remain separately decomposed and unranked. Evidence is pinned legacy `fulltextsearch.ui:5-18,250-255` and `fulltextsearch.cc:300-309,676-680`, the existing private dialog and focused test, and P8-FT-33/P8-FT-34's Widgets-owned geometry contracts. Help content/destination/transport, maximum size, aspect ratio, screen/topology normalization, placement fallback, DPI policy beyond Qt logical sizing, and other dialog state remain unselected and unranked. P8-FT-37 is complete: `fullTextSearchButton` remains the explicit default and is exactly non-auto-default across construction and request-state transitions without changing Search, Cancel, or Help behavior; no successor after P8-FT-37 is selected or ranked. |

## Local Dictionary Formats

| Format/source | Status | Batch | Required verification |
| --- | --- | --- | --- |
| StarDict | slice | vertical slice | discover/index/search/article/resource accepted; P6-FT-1 full-text adapter accepted with primary-only ingestion, assembled plain text, private lifecycle, and installed-service coverage |
| Dictd | slice | text batch | strict `.index` + bounded plain/dictzip data, folded lookup, suggestions, and corruption tests; P6-FT-4 private full-text support accepted with byte-range deduplication, metadata exclusion, assembled plain text, two-source lifecycle, and mixed-service coverage |
| SDict | slice | text batch | bounded `.dct` parsing, plain/zlib/bzip2 fields, folded search/suggestions, sanitized HTML/link conversion, and corruption tests accepted; P6-FT-2 private full-text support accepted with distinct-offset ingestion, assembled plain text, private lifecycle, and mixed-service coverage |
| XDXF | slice | text batch | bounded XML/compressed XML, folded lookup/suggestions, sanitized markup/links, and confined directory resources accepted; P6-FT-3 private full-text support accepted with per-article ingestion, first-key provenance, alias deduplication, inert assembled plain text, and single-source lifecycle |
| GLS | slice | text batch | bounded UTF-8/UTF-16 and compressed text, metadata, aliases, folded lookup/suggestions, sanitized HTML, and confined directory resources; P6-FT-5 private full-text support is accepted with per-article deduplication, first-record provenance, inert assembled text, and single-source lifecycle; resource ZIP remains later |
| ABBYY Lingvo DSL | slice | text batch | bounded plain/compressed decoding, directives, optional/tilde headwords, common markup/links, and confined directory resources; P6-FT-7 private full-text support is accepted with per-article deduplication, first-expanded-record provenance, inert text, and sole `.dsl`/`.dsl.dz` revision; abbreviations, resource ZIP, and nested cards remain |
| Babylon BGL | slice | binary batch | bounded signature/gzip/block parsing, metadata and common code pages, aliases, folded lookup/suggestions, sanitized HTML, and embedded resources; P6-FT-8 private full-text support is accepted with one document per referenced article ordinal, first-retained-record ownership, inert text, and a sole-`.bgl` revision; advanced control records remain |
| MDict MDX/MDD | slice | binary batch | bounded 2.x headers/key/record tables, plain/zlib blocks, strict text decoding, styles, redirects, folded lookup/suggestions, sanitized HTML, and companion MDD resources; P6-FT-11 accepts terminal ownership and the complete ordered MDX/MDD revision, and P6-FT-12 accepts private full-text support with terminal deduplication, exact five-component provenance, inert text, and complete MDX/MDD lifecycle; encryption, LZO, 1.x, and later parity remain |
| Aard | slice | binary batch | bounded 32/64-bit archive indexes, zlib/bzip2/raw articles, metadata, folded lookup/suggestions, redirects, and sanitized article links; P6-FT-6 private full-text support is accepted with unique-article ingestion, first-record ownership, inert assembled text, and a sole-`.aar` source revision; multi-volume aggregation, icons, and later parity remain |
| ZIM | slice | binary batch | bounded header/directory/cluster parsing, consecutive split files, raw/zlib/bzip2 clusters, 32/64-bit blob offsets, metadata, redirects, folded lookup/suggestions, and resources; P6-FT-10 private full-text support is accepted with terminal-entry deduplication, first-source ownership, exact five-component provenance, inert text, and complete split-volume revision; LZMA2/Zstd, complex link rewriting, and icons remain |
| SLOB | slice | binary batch | bounded header/reference/item/bin parsing, declared text encoding, metadata tags, aliases, raw/zlib/bzip2 stores, folded lookup/suggestions, sanitized articles, and resources; P6-FT-9 private full-text adapter is complete with retained-textual ordinals, first-reference item/bin ownership, exact provenance, inert text, a sole-`.slob` revision, and nine-format dispatch; LZMA2, advanced conversion, and icons remain |
| EPWING | slice | specialized batch | bounded `CATALOGS`/subbook/index parsing, Latin-1 and JIS X 0208 text, folded lookup/suggestions, internal references, and confined subbook resources; P6-FT-13 accepts headword-independent `(text-file-ordinal, page, offset)` ownership and the complete ordered source revision; P6-FT-14 accepts one document per retained physical article, first-record ownership, exact five-component provenance, inert assembled text, complete lifecycle, and twelfth-format dispatch; no successor is selected or ranked; compressed HONMON, mixed GB2312, gaiji/media rendering, grouped indexes, and later parity remain |
| LSA audio archive | slice | specialized batch | bounded `.lsa`/`.dat` index and UTF-16 name parsing, folded lookup/suggestions, safe HTML5 audio references, and sample-range Vorbis-to-WAV retrieval; icons and large-file streaming remain |
| ZIP sound packs | slice | specialized batch | bounded recursive `.zips` discovery, classic central/local header validation, UTF-8/CP437 names, stored/deflated members with CRC verification, folded lookup/suggestions, sanitized HTML5 playback, and typed audio retrieval; ZIP64, encrypted/split archives, icons, and large-file streaming remain |
| Sound directories | slice | specialized batch | persistent explicit path/name configuration, bounded recursive regular-file indexing without symlink traversal, folded lookup/suggestions, sanitized HTML5 playback, and confined typed audio retrieval; icons and large-file streaming remain |

All rows above are required for the first formal Linux release. Batch names do
not authorize dropping a legacy format.

## Articles, Browser, And Networking

| Capability | Status | Target gate | Verification |
| --- | --- | --- | --- |
| Backend-independent article assembly | slice | Phase 4 | exact HTML assertions |
| Multi-dictionary result composition | slice | Phase 7 | bounded composition and escaping tests |
| Internal article/resource URL model | slice | Phase 4 | scheme routing tests |
| Qt WebEngine rendering of local article | slice | Phase 4 | documented rendering smoke |
| Embedded images/styles/resources | slice | Phase 4 | generated fixture resources |
| Navigation, links, search, zoom | slice | Phase 7/8 | WebEngine integration tests |
| Context menus, copy, print, save | slice | Phase 7/8 | bounded safe link/selection/image menu, accepted 20-entry per-dictionary article navigation with results-pane overflow, native/system print dialog and preview, retained HTML/PDF export, and offscreen checks; resource saving remains |
| Inspector/DevTools integration | later | Phase 7 | manual parity check |
| Bounded HTTP transport | slice | Phase 7 | deterministic local HTTP server |
| Proxy and authentication | slice | Phase 7 | explicit HTTP proxy and scoped Basic credentials, including cross-origin redirect isolation, verified by local origin/proxy fixtures; system proxy discovery and interactive credential storage remain |
| MediaWiki sources | slice | Phase 7/8 | bounded JSON adapter, ordered current/legacy persistence, and enabled runtime catalog composition through the core extension contract; application wiring remains |
| Arbitrary websites | slice | Phase 7/8 | bounded `%GDWORD%` adapter, ordered current/non-iframe legacy persistence, and enabled runtime catalog composition; legacy encodings, iframe behavior, resources, and application wiring remain |
| Forvo | slice | Phase 7/8 | bounded pronunciation/audio adapter, credential-free persistence, and ordered runtime language composition with explicit in-memory credentials and redacted missing-secret diagnostics; UI composition and playback remain |
| DICT servers | slice | Phase 7/8 | bounded RFC 2229 adapter, ordered persistence, and runtime definition/suggestion composition; authentication and UI composition remain |
| External programs | slice | Phase 7/8 | shell-free adapter plus ordered current and absolute non-audio legacy persistence for plain-text, HTML, and prefix-match programs; environment/process-tree isolation, audio execution, and UI composition remain |

## Audio And Speech

| Capability | Status | Target gate | Verification |
| --- | --- | --- | --- |
| Article audio-link routing | blocked | Phase 7/9 | playback/service ownership contract, then local media fixtures |
| Qt Multimedia playback | mapped | Phase 9 | supported codec smoke |
| FFmpeg/libao playback | mapped | Phase 9 | optional feature tests |
| External audio player | mapped | Phase 9 | controlled process test |
| Linux speech/TTS path | mapped | Phase 9 | adapter/manual checks |
| Windows SAPI and macOS speech | post-linux | Phase 10 | platform-native tests |

## User Interface And User-Owned State

| Capability | Status | Target gate | Verification |
| --- | --- | --- | --- |
| Minimal application window | done | Phase 2 | smoke test |
| Minimal lookup and article view | slice | Phase 4 | vertical-slice workflow |
| Main tabs and navigation | slice | Phase 8 | bounded lifecycle and current-format persisted session, visible retained WebEngine tabs, configurable append/after-current placement and default activation with explicit overrides, positional or symmetric runtime-only MRU keyboard traversal without persisted reordering, deterministic close behavior, synchronized state, facade-backed navigation, a legacy-compatible active-tab results-navigation pane and accepted bounded per-dictionary article-context navigation backed by ordered lookup identities, and a cancellable per-tab prefix-suggestion pane backed only by the existing bounded suggestion service; the pinned legacy application has no persisted article-session format to migrate |
| Dictionary/group controls | slice | Phase 8 | config round-trip, ordered catalog-resolved lookup/suggestion/article filtering, visible all/configured-group selector, bounded ordered group/member/metadata/muting editor, and a legacy-compatible ephemeral per-group `dictionaryBar` whose visible checks filter lookup and suggestions without configuration mutation |
| Preferences and source editor | slice | Phase 8 | source editing and the backed General controls for tab placement/activation, hide-single-tab, MRU traversal, ESC hiding with child/modal precedence, article double-click translation and single-click selection through an isolated bounded WebEngine boundary, history, Favorites deletion, article collapsing, input limits, diacritics, synonyms, DSL optional parts, dictionary context-menu capacity with zero-to-results-pane handoff, credential-free manual HTTP CONNECT proxying for every configured HTTP and raw-TCP DICT source, and the pinned Qt Network cache size/clear-on-exit policy are accepted through atomic complete-candidate coverage; Preferences as a whole remains open and no next Preferences leaf is independently ready; full-text persistence exists but its enablement, format exclusions, and size policy remain unselected behind index-lifecycle decisions and are explicitly outside P8-FT-1; languages/appearance, tray/autostart, hotkeys/scan, audio, unsupported proxy modes, and updates retain explicit Phase 7/9/10 prerequisites; Qt Network exclusively owns the managed HTTP/HTTPS cache while WebEngine cache, cookies, and storage remain outside the controls; delayed history/Favorites saves, WebKit plugins, optional cross-site weakening, unapproved header suppression, Windows scan technologies, and non-dialog legacy fields are intentionally excluded with documented evidence |
| History | slice | Phase 8 | bounded UTF-8/group-aware current store, legacy line-format migration, current-state precedence, atomic persistence, visible group selection with missing-group fallback, group-preserving lookup recording and activation, filtering, clear, import, export, and a reusable pane |
| Favorites | slice | Phase 8 | bounded hierarchical current store, strict legacy XML migration, current-state precedence, atomic persistence, reusable tree pane, root/selected-folder add, nested folder creation, item/folder rename, sibling reordering, arbitrary entry/subtree cross-folder move, move-to-root and removal, re-lookup, and bounded legacy-compatible XML import/export |
| Dictionary info/headword browser | slice | Phase 8 | facade-backed catalog identity, redacted source, language, count, and selectable plain-text description display for metadata-bearing migrated formats; copy-source/open-containing-folder actions, bounded per-dictionary prefix, wildcard, and regular-expression filtering, lookup activation, atomic complete single-dictionary headword export through bounded enumeration, and fixture-backed UI smokes; R3a.1-R3a.3 provide authenticated bounded full-list enumeration for StarDict, 11 article formats, LSA, sound directories, and ZIP sounds, and R3b streams that enumeration to a legacy-compatible UTF-8 text file |
| Scan popup | mapped | Phase 9 | X11 and Wayland behavior |
| Global hotkeys | mapped | Phase 9 | X11 and Wayland behavior |
| Clipboard/selection lookup | mapped | Phase 9 | X11 and Wayland behavior |
| Tray integration | mapped | Phase 9 | desktop smoke test |

Legacy configuration, groups, history, and favorites must not be silently
discarded. Migration may normalize representation only with tested read/upgrade
behavior and a recoverable failure path.

The completed P8-FT-36 implementation supersedes the Full-text search row's
historical no-successor closure and delivers the private
full-text Help activation intent. Pinned legacy
`fulltextsearch.ui:250-255` and `fulltextsearch.cc:300-309,676-680` require one
`Help` button and one dialog-scoped F1 action to produce the same help request;
pre-P8-FT-36 Widgets had neither. The completed leaf adds only private
`fullTextHelpButton`, `fullTextHelpAction`, and argument-free
`HelpRequested()` intent. Help content,
destination/transport, composition-root consumption, Help-menu changes,
Core/public or installed contracts, dependencies, and all other parity remain
excluded and unranked. The Release registration baseline remains exactly 109
tests, and installed C and C++ consumers remain unchanged and source-compatible.
The completed P8-FT-37 implementation preserves the private Search button's
legacy-authenticated explicit-default and non-auto-default policy. Pinned
legacy `fulltextsearch.ui:204-214` sets `default == true` and
`autoDefault == false`; current Widgets now preserves both properties, and the
focused dialog test pins their stability across construction and request-state
transitions plus Search, Cancel, and Help regressions. The leaf is limited to
`fullTextSearchButton` and its existing focused dialog test. Tab order, focus
transfer, broader keyboard dispatch, other buttons, public/Core or installed
contracts, and all unrelated parity remain excluded and unranked. The Release
registration baseline remains exactly 109 tests, and installed C and C++
consumers remain unchanged and source-compatible. No successor after P8-FT-37
is selected or ranked.

The completed P8-FT-38 implementation supersedes that historical closure.
Pinned legacy
`fulltextsearch.ui:274-285` defines the consecutive named forward tab sequence
from the query field through results, word-distance controls, mode,
article-limit controls, Match Case, Search, and Cancel; current Widgets has no
equivalent dialog-level order. Widgets now establishes that exact private
connection once, and the focused dialog test pins it across construction and
request transitions. The leaf leaves initial/transferred
focus, focus policies, endpoints/wraparound, omitted-control placement, key
dispatch, public/Core and installed contracts, and all unrelated parity
excluded and unranked. The Release registration baseline remains exactly 109
tests, and standalone installed C and C++ consumers remain unchanged and
source-compatible. No successor after P8-FT-38 is selected or ranked.

The completed P8-FT-39 implementation supersedes that historical closure and
preserves only the private result-count label's explicit minimum height.
Pinned legacy `fulltextsearch.ui:103-115` requires 21 logical pixels, while
current `fullTextArticlesFoundLabel` now has the equivalent minimum. The
completed leaf keeps that minimum stable across construction and
request/lifecycle transitions
without changing count text, response/progress behavior, layout, dialog
geometry, public/Core or installed contracts, dependencies, or test
registration. The Release baseline remains exactly 109 tests, and standalone
installed C and C++ consumers remain unchanged and source-compatible. No
successor after P8-FT-39 is selected or ranked.

The completed P8-FT-40 implementation supersedes that historical closure and
preserves only the private full-text search progress bar's explicit centered alignment. Pinned
legacy `fulltextsearch.ui:117-128` requires `Qt::AlignCenter`, while current
`fullTextSearchProgress` now establishes its indeterminate range with the
equivalent explicit alignment. The completed leaf keeps the alignment stable
across construction and request/lifecycle transitions without changing range,
visibility, workflow, result/count/status presentation, layout, geometry,
public/Core or installed contracts, dependencies, or test registration.
Progress text/format, value/range policy, orientation, style/animation,
indexing progress, and unrelated parity remain excluded and unranked. The
Release baseline remains exactly 109 tests, and standalone installed C and C++
consumers remain unchanged and source-compatible. No successor after P8-FT-40
is selected or ranked.

The completed P8-FT-41 implementation supersedes that historical closure and
restores only the private result-count/progress horizontal row. Pinned legacy
`fulltextsearch.ui:102-129` places `articlesFoundLabel` first and
`searchProgressBar` second in one `QHBoxLayout`, while current mapped private
widgets are separate vertical-layout items with response-status widgets between
them. The completed leaf restores only that relationship after P8-FT-17,
P8-FT-39, and P8-FT-40, keeping both widgets unique direct dialog children and
stable across construction and request/lifecycle transitions. Count text and
minimum height, progress alignment/range/visibility, status order/behavior,
workflow, geometry, focus, public/Core and installed contracts, dependencies,
and test registration remain unchanged. Spacing, margins, stretch factors,
size policies, widths, broader layout redesign, indexing UI, and unrelated
parity remain excluded and unranked. The Release baseline remains exactly 109
tests, and standalone installed C and C++ consumers remain unchanged and
source-compatible. No successor after P8-FT-41 is selected or ranked.

P8-FT-42 supersedes that historical closure and completes only the
private button-row spacer sequence. Pinned legacy
`fulltextsearch.ui:190-270` places expanding horizontal spacers before Search,
between Search and Cancel, between Cancel and Help, and after Help, while the
current mapped private row had only its outer stretches. The completed leaf
restores only the two missing inter-button spacers after P8-FT-36 through
P8-FT-38, keeping the exact spacer/Search/spacer/Cancel/spacer/Help/spacer item
sequence stable across construction and request/lifecycle transitions. Button
identity, text, order and parentage, Search default policy, tab chain, Help
intent, Cancel lifecycle, workflow, geometry, public/Core and installed
contracts, dependencies, and test registration remain unchanged. Exact spacer
size hints, stretch factors, margins, layout spacing, button sizing or
reordering, broader layout/style work, indexing UI, and unrelated parity remain
excluded and unranked. The Release baseline remains exactly 109 tests, and
standalone installed C and C++ consumers remain unchanged and source-
compatible. No successor after P8-FT-42 is selected or ranked.

P8-FT-43 supersedes that historical closure and completes only the private
full-text Search group-box boundary. Pinned legacy
`fulltextsearch.ui:23-96` places the query field and search-option controls in
one `QGroupBox` titled exactly `Search`, while current
`full_text_search_dialog.cpp:65-70` adds the mapped unique private
`FullTextQueryComposer` directly to the dialog layout. The completed leaf adds
only that titled intermediate private Widgets container after P8-FT-1 through
P8-FT-42, keeping the composer relationship stable across construction and
request/lifecycle transitions. Query values, labels, ordering, enablement,
composition semantics, focus/tab behavior, workflow, result/status/button
layout, geometry, public/Core and installed contracts, dependencies, and test
registration remain unchanged. Group-box margins, spacing, size policy,
alignment, styling, checkability, flatness, mnemonic policy, broader composer
layout, indexing UI, and unrelated parity remain excluded and unranked. The
Release baseline remains exactly 109 tests; exact-SCM creation uses:

```sh
conan create . --build=missing \
  -pr:h=profiles/qt-webengine -pr:b=default \
  -s:h build_type=Release
```

Standalone installed C and C++ consumers remain unchanged and source-
compatible. No successor after P8-FT-43 is selected or ranked.

P8-FT-44 supersedes that historical closure and completes only the private
full-text ignore-words-order label parity leaf. Pinned legacy
`fulltextsearch.ui:83-89` gives the mapped checkbox the exact translatable text
`Ignore words order`, while before P8-FT-44
`full_text_query_composer.cpp:89-92` used `Ignore word order`. The completed
leaf changes only the unique private
`fullTextIgnoreWordOrder` checkbox text after P8-FT-1 through P8-FT-43,
preserving its identity, parentage, checked/enabled state, ordering, focus/tab
behavior, mode-dependent behavior, query composition, workflow, responses,
geometry, public/Core and installed contracts, dependencies, and test
registration. The label stays stable across relevant control and
request/lifecycle transitions. Layout, mnemonic policy, translation-catalog
work, other labels, grammar modernization, indexing UI/lifecycle, Preferences,
adapters/index formats, dependencies/builds, HTTP GET policy, and unrelated
parity remain excluded and unranked. The Release baseline remains exactly 109
tests; exact-SCM creation uses:

```sh
conan create . --build=missing \
  -pr:h=profiles/qt-webengine -pr:b=default \
  -s:h build_type=Release
```

Standalone installed C and C++ consumers remain unchanged and source-
compatible. P8-FT-44 is complete. No successor after P8-FT-44 is selected or
ranked.

P8-FT-45 supersedes that historical closure and completes only the private
full-text query-mode label parity leaf. Pinned legacy
`fulltextsearch.ui:41-53` associates exact translatable text `Mode:` with the
unique search-mode selector, while before P8-FT-45
`full_text_query_composer.cpp:60-74,131-134` maps that selector to the unique
private `fullTextQueryMode` combo box but labeled its form row `Mode`. The
completed leaf changes only that associated label after P8-FT-1 through
P8-FT-44, preserving the association, selector identity, values, ordering, state,
focus/tab behavior, persistence, query composition, request/response lifecycle,
geometry, public/Core and installed contracts, dependencies, and test
registration. The exact label stays stable across relevant control and
request/lifecycle transitions. Layout restructuring, any other label, mnemonic
policy, translation-catalog work, indexing lifecycle/UI, Preferences,
adapters/index formats, dependencies/builds, HTTP GET policy, and unrelated
parity remain excluded and unranked. The Release baseline remains exactly 109
tests; exact-SCM creation uses:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Standalone installed C and C++ consumers remain unchanged and source-
compatible. P8-FT-45 is complete. No successor after P8-FT-45 is selected or
ranked.

P8-FT-46 supersedes that historical closure and completes only the private
full-text query-field label parity leaf. Pinned legacy
`fulltextsearch.ui:28-31` places the unique query `QLineEdit` directly in the
Search group without a label, while before P8-FT-46
`full_text_query_composer.cpp:57-58,131-133` mapped it to the unique private
`fullTextQueryText` line edit but added a `QFormLayout` label with text `Query`.
The completed leaf changes only that form row to unlabeled full-width placement
after P8-FT-1 through P8-FT-45, preserving field identity, parentage, value,
ordering, focus/tab behavior, query composition, request/response lifecycle,
geometry, public/Core and installed contracts, dependencies, and test
registration. The absent association and unique field stay stable across
construction, text mutation, mode/option, composition, and request/lifecycle
transitions. Any second label or behavior, broader layout restructuring,
spacing, margins, mnemonic policy, translation-catalog work, indexing
lifecycle/UI, Preferences, adapters/index formats, dependencies/builds, HTTP
GET policy, and unrelated parity remain excluded and unranked. Focused
acceptance extends only `full_text_query_composer_test`; no executable or
registered test is added. The Release baseline remains exactly 109 tests;
exact-SCM creation uses:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Full Release CTest, Release install, packaged consumers, and standalone
installed C and C++ consumer gates remain unchanged. P8-FT-46 is complete. No
successor after P8-FT-46 is selected or ranked.

P8-FT-47 supersedes that historical closure and completes only the private
full-text wildcard mode-text parity leaf. Pinned legacy
`fulltextsearch.cc:232-236` gives the third item of the unique search-mode
selector exact translatable text `Wildcards` and maps it to the wildcard mode,
while before P8-FT-47 `full_text_query_composer.cpp:60-74` mapped the same third item of
the unique private `fullTextQueryMode` selector to
`FullTextSearchMode::kWildcard` but displayed `Wildcard`. The completed leaf changes
only that item's displayed text to `Wildcards` after P8-FT-1 through P8-FT-46,
preserving selector identity and parentage, four-item count and order, enum
data, selected index, persistence, query composition, option enablement,
focus/tab behavior, request/response lifecycle, geometry, public/Core and
installed contracts, dependencies, and test registration. The exact text and
wildcard mapping stay stable across construction, mode/option, composition,
and request/lifecycle transitions. The regular-expression item, any other
label or behavior, captions and bounds, layout, mnemonic or translation-
catalog work, indexing lifecycle/UI, Preferences, adapters/index formats,
dependencies/builds, HTTP GET policy, and unrelated parity remain excluded and
unranked. Focused acceptance extends only `full_text_query_composer_test`; no
executable or registered test is added. The Release baseline remains exactly
109 tests; exact-SCM creation uses:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Full Release CTest, Release install, packaged consumers, and standalone
installed C and C++ consumer gates remain unchanged. P8-FT-47 is complete. No
successor after P8-FT-47 is selected or ranked.

P8-FT-48 supersedes that historical closure and completes only the private
full-text regular-expression mode-text parity leaf. Pinned legacy
`fulltextsearch.cc:232-236` gives the fourth item of the unique search-mode
selector exact translatable text `RegExp` and maps it to regular-expression
mode, while before P8-FT-48 `full_text_query_composer.cpp:60-75` mapped the
same fourth item of the unique private `fullTextQueryMode` selector to
`FullTextSearchMode::kRegularExpression` but displayed `Regular expression`.
The completed leaf changes only that item's displayed text to `RegExp`
after P8-FT-1 through P8-FT-47, preserving selector identity and parentage,
four-item count and order, enum data, selected index, persistence, query
composition, option enablement, focus/tab behavior, request/response lifecycle,
geometry, public/Core and installed contracts, dependencies, and test
registration. The exact text and regular-expression mapping must stay stable
across construction, mode/option, composition, and request/lifecycle
transitions. The wildcard item, any other label or behavior, captions and
bounds, layout, mnemonic or translation-catalog work, indexing lifecycle/UI,
Preferences, adapters/index formats, dependencies/builds, HTTP GET policy, and
unrelated parity remain excluded and unranked. Completed focused acceptance
extends only `full_text_query_composer_test`; no executable or registered test
is added. The Release baseline remains exactly 109 tests; exact-SCM creation
uses:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Full Release CTest, Release install, packaged consumers, and standalone
installed C and C++ consumer gates remain unchanged. P8-FT-48 is complete. No
successor after P8-FT-48 is selected or ranked.

P8-FT-49 supersedes that historical closure and completes only the private
full-text ignore-options horizontal-row parity leaf. Pinned legacy
`fulltextsearch.ui:75-99` places exactly `checkBoxIgnoreWordOrder` followed by
`checkBoxIgnoreDiacritics` in one horizontal layout, while before P8-FT-49
`full_text_query_composer.cpp:84-93,137-141` already owns the corresponding
private controls and mappings but adds them as separate vertical items in the
opposite order. The completed leaf reuses both existing controls in one
private horizontal row, word-order first and diacritics second, after P8-FT-1
through P8-FT-48. It preserves widget identity, parentage, object names, text,
state, mode behavior, query composition, focus/tab and request/response
lifecycle, geometry, public/Core and installed contracts, dependencies, and
test registration. No widget is recreated and no behavior is added.

Match-case/grid relocation, distance/article rows, captions and bounds,
spacing/margins/stretch/alignment, any third widget or broader layout work,
mnemonic or translation-catalog policy, indexing lifecycle/UI, Preferences,
adapters/index formats, dependencies/builds, HTTP GET policy, and unrelated
parity remain excluded and unranked. The conflicting legacy and migrated
caption bounds require a separate explicit product/Core decision. Focused
acceptance extends only `full_text_query_composer_test` to prove exact
two-item membership and order plus unchanged identity, state, mode transitions,
and repeated composition; no executable or registered test is added. The
Release baseline remains exactly 109 tests; exact-SCM creation uses:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Full Release CTest, Release install, packaged consumers, and standalone
installed C and C++ consumer gates remain unchanged. P8-FT-49 is complete. No
successor after P8-FT-49 is selected or ranked.

P8-FT-50 supersedes that historical closure and is complete. It restores the
pinned legacy coupled search-options grid using only the
existing private composer controls. After the existing full-width unlabeled
query field, one `QGridLayout` contains word-distance toggle/spin box/mode row
at coordinates `(0,0)`, `(0,1)`, and `(0,2)`, with the last cell holding the
existing `Mode:` label then selector in a two-item horizontal layout. Its
second row contains article-limit toggle/spin box/`Match case` at `(1,0)`,
`(1,1)`, and `(1,2)`. The completed word-order/diacritics row follows the grid.
The seven participating widgets remain direct children of the composer, whose
existing parentage inside the `Search` group is unchanged.

This is layout parity, not semantic rollback. Existing captions, mode texts
and data, object names, identity, state and enablement, explicit focus/tab
chain, query composition, persistence, request/response lifecycle, and all
P8-FT-1 through P8-FT-49 behavior remain unchanged. Word distance retains
`0..1000`; articles per dictionary retains `1..100000`. Legacy Qt `0..99`
defaults and range-bearing legacy captions are not restored or synthesized.
Indexing lifecycle/status UI, Preferences, public/Core/config and installed
contracts, adapters/index formats, dependencies/builds, layout styling policy,
new controls or behavior, HTTP GET policy, and unrelated parity are excluded
and unranked.

Focused acceptance extends only `full_text_query_composer_test` for the
unique grid, exact coordinates/order/parentage, unchanged controls, bounds,
transitions, and repeated composition. Existing dialog coverage retains Search
group, focus/tab, request, response, geometry, and lifecycle ownership. Add no
executable or registered test; the Release baseline remains exactly 109.
Full Release CTest, Release install, packaged consumers, unchanged standalone
installed C and C++ consumers, and clean committed exact-SCM creation remain
required:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Stop on ref/worktree or legacy drift, ambiguity, architectural conflict,
failed validation, another required file, or scope expansion. No successor
after P8-FT-50 is selected or ranked.

The post-P8-FT-50 word-distance caption policy audit is closed without a code
leaf. GET selected Option A: preserve the exact visible private label
`Maximum word distance`; labels describe settings, while numeric controls own
and expose bounds. The existing spin box remains `0..1000`, and its range is
not embedded in translatable caption text. Current production code and focused
tests already meet this policy, so no artificial source or test change is
authorized.

This four-document-only closure preserves all P8-FT-1 through P8-FT-50
behavior, the exactly 109-test Release baseline, public/Core/config and
installed contracts, dependencies, and install/package/standalone C and C++
consumer gates. Future exact-SCM verification retains:

```sh
conan create . --build=missing -pr:h=profiles/qt-webengine -pr:b=default -s:h build_type=Release
```

Compiled checks are skipped for this documentation-only audit. Indexing
lifecycle/status UI, the articles-per-dictionary caption, other text or
accessibility policy, styling/layout, translation work, and unrelated parity
remain excluded and unranked. Stop on drift, ambiguity, design conflict,
another required file, failed validation, or scope expansion. No successor is
selected or ranked; the next boundary is a fresh post-policy readiness audit.

The fresh post-policy audit selects exactly one smallest independently ready
prerequisite, P8-FT-51: an explicit product decision for the
articles-per-dictionary caption. Pinned legacy `fulltextsearch.cc:249-256`
synthesizes `Max articles per dictionary (%1-%2):` and assigns legacy numeric
bounds. Current `full_text_query_composer.cpp:109-125` and its focused test
instead establish exact visible text `Maximum articles per dictionary` and
sole spin-box ownership/exposure of `1..100000`.

P8-FT-51 does not infer that the completed word-distance decision governs this
separate caption. GET must choose between retaining the exact current label and
sole numeric-control ownership, which creates no implementation leaf, or a
range-bearing caption using the current `1..100000` bounds, which may authorize
a later private Widgets leaf. Restoring legacy Qt `0..99` defaults is excluded
as incompatible with the current bounded query contract. This audit chooses no
outcome and authorizes no source or test change.

Index readiness/status/background lifecycle and full-text Preferences
enablement, type exclusions and dictionary-size policy remain blocked on
separate Core lifecycle or policy work. Exact-document navigation,
match/excerpt presentation, ignore-diacritics consumption, accessibility,
styling/layout, translation work, public/Core/config/index-format/dependency or
installed-surface changes, and unrelated parity remain excluded and unranked.

This four-document-only selection preserves P8-FT-1 through P8-FT-50, the
word-distance policy closure, and the exactly 109-test Release baseline.
Compiled checks are omitted. Stop on ref/worktree or legacy drift, further
ambiguity, architectural conflict, another required file, failed validation,
or scope expansion. No successor after P8-FT-51 is selected or ranked.

P8-FT-51 is closed by GET's Option A without a code leaf. Preserve exact
visible private label `Maximum articles per dictionary`; its spin box solely
owns and exposes `1..100000`, and bounds are not synthesized into translatable
caption text. The previously locked `Maximum word distance` label and sole
spin-box ownership of `0..1000` remain unchanged.

Current `full_text_query_composer.cpp:95-125` and focused composer tests already
prove both labels, ranges, identity, enablement, transitions, and repeated
composition. Pinned legacy `fulltextsearch.cc:249-256` is retained as
historical range-bearing-caption evidence only. No artificial source, test,
executable, registration, public/Core/config/index-format/dependency, build,
installed-surface, or Release-baseline change is authorized.

This four-document-only closure omits compiled checks and preserves the
exactly 109-test Release baseline. Stop on drift, ambiguity, design conflict,
another required file, failed validation, or scope expansion. No successor is
selected or ranked; the next boundary is a fresh independent bounded full-text
readiness audit.

P8-FT-52 is complete from synchronized migrated base
`b25cee8fd95381ecd16f733107f7d201d5068eeb`: the private full-text dialog
window title now resolves through its dialog translation context while
preserving exact source text and default visible text `Full-text search`.
Pinned legacy `fulltextsearch.cc:219-223` remains the translation-ownership
evidence.

The implementation changes only `full_text_search_dialog.cpp` and its existing
focused test. Construction coverage proves an exact-context test translator's
replacement and the exact English fallback without a translator. It adds no
catalog, locale loader, executable, registration, public/Core/config/index-
format/dependency, build, composition-root, or installed-surface change. The
Release baseline remains exactly 109 tests, and both locked caption/spin-box
range policies remain unchanged.

Index readiness/status/progress/background lifecycle remains blocked because
state is private per backend and indexing occurs during dictionary loading.
Full-text Preferences enablement, type exclusions and size policy therefore
remain blocked on separate Core lifecycle/policy work. Other translation,
accessibility and styling surfaces, exact-document navigation, match/excerpt
presentation, ignore-diacritics consumption, adapters/index formats, and
unrelated parity remain separate and unranked.

Delivery requires the focused and full 109-test Linux Release suite, fresh
Release configure/build and install, standalone installed C/C++ consumers,
clean committed exact-SCM Conan creation with packaged consumers, repository
validation, and clean synchronized refs/worktrees. No successor after P8-FT-52
is selected or ranked.

P8-FT-53 is complete from synchronized migrated base
`339d1dd6e8b3540923153628497af23b6fa7208b` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`: the private
full-text Search group-box title now resolves through the exact
`goldendict::app::FullTextSearchDialog` context while preserving source and
default visible text `Search`.

Pinned legacy `fulltextsearch.ui:23-27` marks the title as translatable;
`full_text_search_dialog.cpp` now uses dialog-owned `tr()`. Completed P8-FT-52
supplies the proven private translation context. The implementation is limited
to `full_text_search_dialog.cpp` and `full_text_search_dialog_test.cpp`;
focused coverage proves exact-context replacement, English fallback, sole
direct-child group identity/title, and scoped translator cleanup. It adds no catalog, locale
loader, executable, registration, public/Core/config/index-format/dependency,
build, composition-root, ABI, or installed-surface change.

The Shared-Library and GUI Boundary remains intact, the Release baseline stays
exactly 109 tests, and exact `Maximum word distance` / control-owned `0..1000`
and `Maximum articles per dictionary` / control-owned `1..100000` policies
remain locked. Index lifecycle/readiness/status/progress and full-text
Preferences remain blocked on separate Core lifecycle/policy work. Translation
catalogs and other strings, accessibility, styling/layout, exact-document
navigation, match/excerpt presentation, ignore-diacritics consumption,
adapters/index formats, and unrelated parity remain separate and unranked.

Delivery requires the focused and full 109-test Linux Release suite, fresh
Release dependency install/configure/build and install, standalone installed C
and C++ consumers, clean committed exact-SCM Conan creation with packaged
consumers, exact six-file repository validation, `git diff --check`, and clean
synchronized refs/worktrees. No successor after P8-FT-53 is selected or ranked.

P8-FT-54 is complete from clean synchronized migrated base
`8dcf3d87fe4b25a916c864da56c307f5c78de24b` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The existing
private status `Results may be incomplete.` now resolves through the exact
`goldendict::app::FullTextSearchDialog` context. P8-FT-22 supplies the exact
text and generation-safe visibility contract; current
`full_text_search_dialog.cpp` now uses dialog-owned `tr()`, and P8-FT-52/
P8-FT-53 supply dialog-owned translation and focused-test precedents. Pinned
legacy full-text code and UI contain no equivalent status and therefore no
conflicting wording or context contract.

The implementation is limited to `full_text_search_dialog.cpp` and its focused
dialog test. It changes only construction of the existing label to dialog-owned
`tr("Results may be incomplete.")`; coverage must prove exact-context
replacement, English fallback, unchanged identity/text and P8-FT-22 visibility
predicates, and scoped translator cleanup. It adds no catalog, locale loader,
executable, registration, public/Core/config/index-format/dependency/build/
composition-root, ABI, or installed-surface change.

The translated `Full-text search` and `Search` titles and both locked concise
caption/control-range policies remain unchanged: `Maximum word distance` with
`0..1000`, and `Maximum articles per dictionary` with `1..100000`. Other
response strings/catalog readiness, accessibility, styling/layout,
exact-document navigation, match/excerpt presentation, ignore-diacritics
consumption, index formats, and unrelated parity remain unselected and
unranked. Index lifecycle/readiness/status/progress and full-text Preferences
remain blocked on separate Core lifecycle/policy work.

Delivery requires the focused and full 109-test Linux Release suite, fresh
Release dependency install/configure/build and install, standalone installed C
and C++ consumers, clean committed exact-SCM Conan creation with packaged
consumers, exact six-file repository validation, and clean synchronized
refs/worktrees. The exactly 109-test baseline remains unchanged. Completion
unlocks only a fresh independent bounded full-text readiness audit; no
successor is selected or ranked.

P8-FT-55 is complete from clean synchronized migrated base
`175a92926b1046798b51b9757a28ed156555c0aa` and unchanged clean read-only legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Only the existing private
`No matches` empty-response status moved into the exact
`goldendict::app::FullTextSearchDialog` translation context. P8-FT-23 retains
the text, identity, and conclusive-empty lifecycle predicates; P8-FT-52 through
P8-FT-54 and the focused dialog test establish the translation and test
precedents. Pinned legacy has no equivalent status or conflicting contract.

The implementation is limited to `full_text_search_dialog.cpp` and its focused
test, using dialog-owned `tr("No matches")` and proving exact-context
replacement, English fallback, unchanged identity/text and P8-FT-23 visibility,
stale/cancelled/detached safety, and scoped translator cleanup. No catalog,
locale loader, executable, registration, public/Core/config/index-format/
dependency/build/composition-root, ABI, installed-surface, or test-baseline
change is authorized. The exactly 109-test Release baseline remains unchanged.

Completed translations `Full-text search`, `Search`, and
`Results may be incomplete.` remain exact. The locked caption/control policies
remain `Maximum word distance` with `0..1000` and
`Maximum articles per dictionary` with `1..100000`. Other response strings and
catalog readiness, accessibility, styling/layout, exact-document navigation,
match/excerpt presentation, ignore-diacritics consumption, adapters/index
formats, and unrelated parity remain independent and unranked. Index readiness,
status, progress, rebuild/failure UI, background lifecycle, and full-text
Preferences remain blocked on a separate Core lifecycle/policy boundary. The
Release baseline remains exactly 109 tests. No successor after P8-FT-55 is
selected or ranked; completion unlocks only a fresh independent bounded full-
text readiness audit.

P8-FT-56 is complete from clean synchronized migrated revision
`7bc3fbeee4af637e25dff8656ce7d22406d8ea2d` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The existing
private `fullTextFailureResponseStatus` retains exact source and fallback text
`Full-text search failed` and all completed P8-FT-24 lifecycle predicates; the
leaf moves its construction from `QStringLiteral` to the established
`goldendict::app::FullTextSearchDialog` translation context.

P8-FT-52 through P8-FT-55 provide the dialog-owned translation and scoped-test
precedent, and pinned legacy contains no equivalent terminal-failure status.
The implementation is restricted to the private dialog source and its existing
focused test. Exact-context replacement, English fallback, identity, lifecycle,
stale/cancelled/detached safety, and cleanup are covered. It adds no catalog,
locale loader, executable,
registration, public/Core/configuration/index-format/dependency/build/
composition-root, ABI, installed-interface, or test-baseline change. The
Shared-Library and GUI Boundary governs; no architecture or product decision is
required.

Completed translations `Full-text search`, `Search`,
`Results may be incomplete.`, and `No matches` remain exact. The locked
caption/control policies remain `Maximum word distance` with `0..1000` and
`Maximum articles per dictionary` with `1..100000`. All other visible and
private full-text gaps remain independent and unranked. Index readiness,
status, progress, rebuild/failure reporting, background lifecycle, and
full-text Preferences remain blocked on a separately evidenced Core lifecycle/
policy boundary. The exactly 109-test Release baseline is preserved. P8-FT-56
completion unlocks only a fresh independent bounded full-text readiness audit.

P8-FT-57 is complete. It was the sole leaf selected by the fresh independent
bounded post-P8-FT-56 audit from clean synchronized migrated revision
`491d85500d27df280c19d4a62a2adc9e14d55a33` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It translates only
the existing private mixed-result status through
`goldendict::app::FullTextSearchDialog`, retaining exact source and fallback
text `Some dictionaries could not be searched`.

P8-FT-25 already owns `fullTextMixedResultResponseStatus`, its exact text, and
its generation-current result-plus-error visibility and partial-status
coexistence predicates. P8-FT-57 replaces the former `QStringLiteral`
construction with dialog-owned `tr`; P8-FT-52 through P8-FT-56 establish the
exact dialog-owned translation context and scoped-translator pattern. Pinned legacy contains no
equivalent mixed-result status or conflicting contract, so no architecture or
product choice remains.

The completed P8-FT-57 implementation changes exactly
`apps/goldendict/src/full_text_search_dialog.cpp` and
`apps/goldendict/tests/full_text_search_dialog_test.cpp`. It is limited to
dialog-owned `tr("Some dictionaries could not be searched")` and focused
coverage for exact-context replacement, English fallback, stable identity and
text, unchanged visibility and lifecycle safety, and translator cleanup. The
Shared-Library and GUI Boundary applies. Catalogs, locale loading, executables,
registrations, public/Core/configuration/index-format/dependency/build/
composition-root/ABI/installed interfaces, and the exactly 109-test Release
baseline remain unchanged.

Completed translations `Full-text search`, `Search`,
`Results may be incomplete.`, `No matches`, and `Full-text search failed`
remain exact. The locked caption/control policies remain `Maximum word
distance` with `0..1000` and `Maximum articles per dictionary` with
`1..100000`. Index readiness/status/progress/rebuild/failure reporting/
background lifecycle and full-text Preferences remain blocked on separately
evidenced Core lifecycle/policy work. Exact-document navigation,
result/match/excerpt presentation, ignore-diacritics consumption,
adapters/index formats, accessibility, styling/layout, catalogs, and unrelated
parity remain independent and unranked. P8-FT-57 completion unlocks only a
fresh independent bounded readiness audit; no successor is selected, ranked,
recommended, or named.

The fresh independent bounded post-P8-FT-57 audit is pinned to clean
synchronized migrated revision
`58612007652ac24f08fc0bd8e2a4fb2b59839366` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. P8-FT-58 moves the
existing private partial-empty status into the established
`goldendict::app::FullTextSearchDialog` translation context.

P8-FT-26 retains the unique `fullTextPartialEmptyResponseStatus`, exact source
and fallback text `No matches in searched dictionaries`, authoritative-partial
zero-result visibility, coexistence with `Results may be incomplete.`,
generation safety, and raw-detail suppression. The dialog now constructs that
status with dialog-owned `tr()`. P8-FT-52 through P8-FT-57 provide the exact context and
focused scoped-translator precedent. Pinned legacy full-text source and UI have
no equivalent status or conflicting contract.

The completed implementation is limited to the private dialog and its existing
focused test and uses dialog-owned
`tr("No matches in searched dictionaries")`; focused coverage proves exact-context
replacement, English fallback, stable identity/text, unchanged P8-FT-26
predicates and lifecycle safety, and translator cleanup. The Shared-Library and
GUI Boundary applies. Catalogs, locale loading, executables, registrations,
public/Core/configuration/index-format/dependency/build/composition-root/ABI/
installed interfaces, and exactly 109 registered Release tests remain
unchanged.

Completed translations `Full-text search`, both `Search` uses,
`Results may be incomplete.`, `No matches`, `Full-text search failed`, and
`Some dictionaries could not be searched` remain exact. Locked policies remain
`Maximum word distance` with spin-box-owned `0..1000` and
`Maximum articles per dictionary` with spin-box-owned `1..100000`. Index
readiness/status/progress/rebuild/failure reporting/background lifecycle and
full-text Preferences remain blocked because the Core lifecycle/policy boundary
has no separately authoritative resolution. Other full-text and unrelated
parity gaps remain independent and unranked. Completion unlocks only a fresh
independent bounded readiness audit; no successor is selected, ranked,
recommended, or named.

P8-FT-59 is complete from synchronized migrated revision
`471ba2a7db8491aa486951506389101caa8cb255` and unchanged clean read-only legacy
revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It accepts focused translation
of the existing private error-count status `Errors: %1`.

P8-FT-27 retains the unique status identity, authoritative decimal count,
visibility and coexistence predicates, lifecycle safety, and raw-detail
suppression. Production retains dialog-owned `tr("Errors: %1")`. The existing
private dialog test proves exact-context and exact-source replacement,
authoritative single- and multi-digit decimal interpolation, English fallback
before installation and after scoped cleanup, sole direct-child label identity,
and unchanged predicates, coexistence, lifecycle safety, and raw-detail
suppression. Pinned legacy has no equivalent aggregate status or conflicting
contract. No source behavior or registered test changes.

The Shared-Library and GUI Boundary applies. Catalogs, locale loading,
executables, registrations, public/Core/configuration/index-format/dependency/
build/composition-root/ABI/installed interfaces, and the exactly 109-test
Release baseline remain unchanged. All completed translations, including `No
matches in searched dictionaries`, and both locked caption/range policies
remain exact. Index lifecycle reporting and full-text Preferences remain
blocked on a separately authoritative Core lifecycle/policy resolution. Other
translation, accessibility, styling, navigation, excerpt, diacritics, result
presentation, adapter/index-format, and unrelated parity remain independent
and unranked. Completion unlocks only a fresh independent bounded readiness
audit; no successor is selected, ranked, recommended, or named.

### P8-FT-60 exact-result navigation contract prerequisite (complete)

The bounded audit is pinned to synchronized migrated revision
`4cca1e81e1167222d067e475a4053088cf99ba38` and clean read-only legacy revision
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. The approved priority selects
exactly P8-FT-60: a narrow Core/facade/navigation prerequisite for consuming an
accepted result's dictionary identity and opaque `document_id`. Repository
history persistently ends at P8-FT-59 and contains no P8-FT-60; an abandoned,
unpersisted accepted-result-count translation-test proposal owns no ordinal.

Current `dictionary_service.h:183-190` preserves dictionary identity,
`document_id`, excerpt, and matches, but `main_window.cpp:5996-6040`
deliberately performs only a scoped headword lookup. The installed lookup
service has no document selector, and `article_tab_session.cc:38-66` forbids
the existing internal-link article fields for lookup navigation. Pinned legacy
`fulltextsearch.cc:596-609` and `mainwindow.cc:3001-3013` similarly activate a
headword across aggregated dictionary IDs, not an exact document. Widgets
therefore cannot implement exact targeting without inventing backend-specific
lookup in violation of the Shared-Library and GUI Boundary.

P8-FT-60 defines Core-owned target validation/resolution behind
`DesktopFacade` and distinct facade/tab
navigation identity while preserving current-tab, group and authoritative
scope behavior, history/session replay, main-query selection, and the existing
article-search handoff. Invalid or unresolved targets must leave navigation
and history unchanged. Well-formed stale IDs map to missing document. The
approved narrow change intentionally extends the installed desktop C++ ABI;
the headless service, runtime-source contract, and C ABI remain unchanged.

Completed translations `Full-text search`, both `Search` uses, `Results may be
incomplete.`, `No matches`, `Full-text search failed`, `Some dictionaries
could not be searched`, `No matches in searched dictionaries`, and
`Errors: %1` remain exact. `Maximum word distance` remains spin-box-owned at
`0..1000`, and `Maximum articles per dictionary` remains spin-box-owned at
`1..100000`. All completed P8-FT behavior and exactly 109 registered Release
tests remain unchanged.

The implementation adds backward-compatible session persistence, private
resolution for twelve accepted built-ins, focused existing-test cases, and
packaged C++ consumer coverage. Index-format, dependency, build, catalog/
locale-loader, executable, and registration boundaries remain unchanged.
Direct exact-result activation, highlighting
and excerpts, ignore-diacritics consumption, translation acceptance, index
lifecycle and Preferences, presentation, adapters, and unrelated parity remain
excluded and unranked. No successor after P8-FT-60 is selected or ranked. The
completed prerequisite unlocks only its dependency boundary.

### P8-FT-61 exact-result activation connection (complete)

The implementation starts from synchronized migrated revision
`0394b031031c265c7799386996bcbda22e5b0a3b` and unchanged clean read-only
legacy revision `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It selects exactly
P8-FT-61, the smallest dependent GUI leaf that projects the accepted result's
authoritative dictionary ID and opaque `document_id` into the
`TabNavigationState::exact_target` contract completed by P8-FT-60. P8-FT-60
remains the Core/facade validation, resolution, navigation-identity, history,
and session foundation; P8-FT-61 does not extend it.

Current `full_text_search_dialog.h:24-35` and
`full_text_search_dialog.cpp:337-348` retain and deliver exact accepted result,
scope, and query context by value. Current `main_window.cpp:5996-6040` owns the
activation command and preserves current-tab, group, accepted scope, main-query
selection, lookup, and article-search handoff behavior, but omits the exact
target. Current `desktop_facade.h:35-85,139-190`,
`desktop_facade.cc:156-205`, and
`application_service_test.cpp:2375-2438` prove the completed atomic facade
contract. Pinned legacy `fulltextsearch.cc:596-609` and
`mainwindow.cc:3001-3013` target only a headword and aggregated dictionary IDs.

Implementation is confined to private MainWindow projection and focused
existing GUI smoke cases. MainWindow copies, without parsing, the result
dictionary ID and `document_id` into `ExactArticleTarget` before the single
`DesktopFacade::OpenArticleTab` call. Any invalid/unresolved target or other tab
operation failure retains the existing `Unable to update article state` policy
and mutates no tab, history/session, lookup request, or article-search state.
Success preserves the authoritative accepted scope and all completed current-
tab/group/query/history/session and article-search semantics.

The Shared-Library and GUI Boundary, completed P8-FT behavior and exact strings,
`Maximum word distance` with spin-box-owned `0..1000`, `Maximum articles per
dictionary` with spin-box-owned `1..100000`, and exactly 109 registered Release
tests remain unchanged. Public/installed APIs and ABI, configuration, headless
service/runtime-source/C API, dependencies, adapters/index formats, build,
catalogs, locale loading, highlighting/excerpts, ignore-diacritics semantics,
translations, and unrelated parity remain excluded and unranked. No successor
after P8-FT-61 is selected or ranked. Completion unlocks only the dependency
boundary established by this connection.

## Resources And Platform Integration

| Capability | Status | Target gate | Verification |
| --- | --- | --- | --- |
| Core styles for vertical slice | slice | Phase 4/7 | responsive shell and sanitizer tests |
| Complete styles and icons | mapped | Phase 8/9 | visual parity checklist |
| Translations | mapped | Phase 9 | Linguist build and locale smoke |
| Help | mapped | Phase 9 | installed resource checks |
| Desktop icon/metainfo/launcher | mapped | Phase 9 | package install checks |
| X11 integration | mapped | Phase 9 | X11 session checks |
| Wayland behavior | mapped | Phase 9 | native/fallback matrix |
| Windows/macOS packaging and native hooks | post-linux | Phase 10 | platform CI/installers |

## Formal Linux Release Gate

The first formal Linux release requires all `slice`, `mapped`, and `later`
Linux rows above to pass. Only `post-linux` rows may remain incomplete. Any
intentional behavior difference from legacy commit `3d93dd66` requires a
documented rationale and explicit review.
