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
| GUI depends only on application facade | slice | Phase 4 | include-boundary and focused application tests |
| AI-ready headless dictionary API | slice | Phase 4 | structured, bounded, provenance-bearing non-GUI consumer |
| Debug, runtime bundle, DEB/RPM, CI | mapped | Phase 9 | clean release matrix |

## Core Lookup Path

| Capability | Status | Target gate | Verification |
| --- | --- | --- | --- |
| Minimal configuration load/persist | slice | Phase 4 | clean-profile round-trip |
| Legacy configuration migration | slice | Phase 8 | bounded XML import of dictionary and sound-directory paths, current-config precedence, atomic new-format persistence, and untouched legacy source fixtures; groups, sources, preferences, and platform-specific legacy location discovery remain |
| Dictionary path discovery | slice | Phase 4 | temporary-directory tests |
| Dictionary identity and index lifecycle | slice | Phase 4 | deterministic index tests |
| Async word/article/resource requests | slice | Phase 4 | completion/cancel/error tests |
| Exact headword lookup | slice | Phase 4 | generated StarDict fixture |
| Ranked Unicode-folded prefix lookup | slice | Phase 5 | bounded prefix ranking and installed-consumer tests |
| Unicode case/diacritic/punctuation folding | slice | Phase 5 | data-driven text and folded-lookup tests |
| Bounded legacy text encoding conversion | slice | Phase 5 | strict Latin-1, UTF-16, GB18030, and EUC-JP fixtures |
| Headword suggestions | slice | Phase 5 | bounded ranked suggestion fixtures and installed-consumer tests |
| Morphology and transliteration | later | Phase 5 | per-language fixtures |
| Full-text search | later | Phase 5/8 | index and UI workflow tests |

## Local Dictionary Formats

| Format/source | Status | Batch | Required verification |
| --- | --- | --- | --- |
| StarDict | slice | vertical slice | discover/index/search/article/resource |
| Dictd | slice | text batch | strict `.index` + bounded plain/dictzip data, folded lookup, suggestions, and corruption tests |
| SDict | slice | text batch | bounded `.dct` parsing, plain/zlib/bzip2 fields, folded search/suggestions, sanitized HTML/link conversion, and corruption tests |
| XDXF | slice | text batch | bounded XML/compressed XML, folded lookup/suggestions, sanitized markup/links, and confined directory resources; resource ZIP and full-text parity remain |
| GLS | slice | text batch | bounded UTF-8/UTF-16 and compressed text, metadata, aliases, folded lookup/suggestions, sanitized HTML, and confined directory resources; resource ZIP and full-text parity remain |
| ABBYY Lingvo DSL | slice | text batch | bounded plain/compressed decoding, directives, optional/tilde headwords, common markup/links, and confined directory resources; abbreviations, resource ZIP, nested cards, and full-text parity remain |
| Babylon BGL | slice | binary batch | bounded signature/gzip/block parsing, metadata and common code pages, aliases, folded lookup/suggestions, sanitized HTML, and embedded resources; advanced control records and full-text parity remain |
| MDict MDX/MDD | slice | binary batch | bounded 2.x headers/key/record tables, plain/zlib blocks, strict text decoding, styles, redirects, folded lookup/suggestions, sanitized HTML, and companion MDD resources; encryption, LZO, 1.x, and full-text parity remain |
| Aard | slice | binary batch | bounded 32/64-bit archive indexes, zlib/bzip2/raw articles, metadata, folded lookup/suggestions, redirects, and sanitized article links; multi-volume aggregation, icons, and full-text parity remain |
| ZIM | slice | binary batch | bounded header/directory/cluster parsing, consecutive split files, raw/zlib/bzip2 clusters, 32/64-bit blob offsets, metadata, redirects, folded lookup/suggestions, and resources; LZMA2/Zstd, complex link rewriting, icons, and full-text parity remain |
| SLOB | slice | binary batch | bounded header/reference/item/bin parsing, declared text encoding, metadata tags, aliases, raw/zlib/bzip2 stores, folded lookup/suggestions, sanitized articles, and resources; LZMA2, advanced conversion, icons, and full-text parity remain |
| EPWING | slice | specialized batch | bounded `CATALOGS`/subbook/index parsing, Latin-1 and JIS X 0208 text, folded lookup/suggestions, internal references, and confined subbook resources; compressed HONMON, mixed GB2312, gaiji/media rendering, grouped indexes, and full-text parity remain |
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
| Context menus, copy, print, save | slice | Phase 7/8 | manual and automated checks |
| Inspector/DevTools integration | later | Phase 7 | manual parity check |
| Bounded HTTP transport | slice | Phase 7 | deterministic local HTTP server |
| Proxy and authentication | slice | Phase 7 | explicit HTTP proxy and scoped Basic credentials, including cross-origin redirect isolation, verified by local origin/proxy fixtures; system proxy discovery and interactive credential storage remain |
| MediaWiki sources | slice | Phase 7 | bounded JSON API suggestions/articles with encoded queries, duplicate suppression, transport policy reuse, and deterministic local HTTP fixture; source persistence and UI composition remain |
| Arbitrary websites | slice | Phase 7 | bounded `%GDWORD%` URL templates, strict UTF-8/Latin-1/Windows-1252 response decoding, and quoted/unquoted relative `href`/`src` resolution verified by local fixtures; persistence, legacy query encodings, resources, and UI composition remain |
| Forvo | slice | Phase 7 | bounded, strictly encoded XML API lookup with validated pronunciation metadata and bounded typed audio retrieval, verified by a deterministic local API/audio fixture; source persistence, UI composition, and playback remain |
| DICT servers | slice | Phase 7 | bounded RFC 2229 TCP handshake, MIME negotiation, quoted `MATCH`/`DEFINE`, dot-stuffed definition parsing, timeout/cancellation, and deterministic local protocol fixture; authentication, persistence, and UI composition remain |
| External programs | slice | Phase 7 | shell-free absolute executable plus argument arrays, bounded `%GDWORD%` substitution or stdin, Unicode decoding, timeout/cancellation, output limits, and deterministic controlled-helper tests; environment/process-tree isolation, persistence, audio execution, and UI composition remain |

## Audio And Speech

| Capability | Status | Target gate | Verification |
| --- | --- | --- | --- |
| Article audio-link routing | mapped | Phase 7/9 | local media fixtures |
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
| Main tabs and navigation | mapped | Phase 8 | interaction tests |
| Dictionary/group controls | mapped | Phase 8 | config round-trip |
| Preferences and source editor | mapped | Phase 8 | settings migration tests |
| History | slice | Phase 8 | bounded UTF-8/group-aware current store, legacy line-format migration, current-state precedence, atomic persistence, and untouched source fixtures; application recording, history pane, filtering, and clear/export UI remain |
| Favorites | mapped | Phase 8 | legacy-data migration |
| Dictionary info/headword browser | mapped | Phase 8 | fixture-backed UI checks |
| Scan popup | mapped | Phase 9 | X11 and Wayland behavior |
| Global hotkeys | mapped | Phase 9 | X11 and Wayland behavior |
| Clipboard/selection lookup | mapped | Phase 9 | X11 and Wayland behavior |
| Tray integration | mapped | Phase 9 | desktop smoke test |

Legacy configuration, groups, history, and favorites must not be silently
discarded. Migration may normalize representation only with tested read/upgrade
behavior and a recoverable failure path.

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
