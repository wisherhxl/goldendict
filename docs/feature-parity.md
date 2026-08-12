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
| Legacy configuration migration | mapped | Phase 8 | pinned legacy config fixtures |
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
| XDXF | mapped | text batch | XML/compressed XML/resources |
| GLS | mapped | text batch | text/compressed text/resources |
| ABBYY Lingvo DSL | mapped | text batch | directives, markup, resource directory |
| Babylon BGL | mapped | binary batch | compression, metadata, resources |
| MDict MDX/MDD | mapped | binary batch | encryption/compression, styles, resources |
| Aard | mapped | binary batch | archive lookup and article links |
| ZIM | mapped | binary batch | split files, compression, resources |
| SLOB | mapped | binary batch | codec/container/resources |
| EPWING | mapped | specialized batch | catalog, encoding, references, resources |
| LSA audio archive | mapped | specialized batch | index and audio retrieval |
| ZIP sound packs | mapped | specialized batch | index and audio retrieval |
| Sound directories | mapped | specialized batch | recursive discovery and audio retrieval |

All rows above are required for the first formal Linux release. Batch names do
not authorize dropping a legacy format.

## Articles, Browser, And Networking

| Capability | Status | Target gate | Verification |
| --- | --- | --- | --- |
| Backend-independent article assembly | slice | Phase 4 | exact HTML assertions |
| Internal article/resource URL model | slice | Phase 4 | scheme routing tests |
| Qt WebEngine rendering of local article | slice | Phase 4 | documented rendering smoke |
| Embedded images/styles/resources | slice | Phase 4 | generated fixture resources |
| Navigation, links, search, zoom | mapped | Phase 7/8 | WebEngine integration tests |
| Context menus, copy, print, save | mapped | Phase 7/8 | manual and automated checks |
| Inspector/DevTools integration | later | Phase 7 | manual parity check |
| Proxy and authentication | mapped | Phase 7 | local HTTP proxy fixture |
| MediaWiki sources | mapped | Phase 7 | deterministic local HTTP server |
| Arbitrary websites | mapped | Phase 7 | encoding/link-rewrite fixtures |
| Forvo | mapped | Phase 7 | mocked API and audio response |
| DICT servers | mapped | Phase 7 | local protocol server |
| External programs | mapped | Phase 7 | controlled helper executable |

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
| History | mapped | Phase 8 | legacy-data migration |
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
| Core styles for vertical slice | slice | Phase 4 | rendered fixture article |
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
