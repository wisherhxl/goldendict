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
