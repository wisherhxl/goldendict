# GoldenDict Porting Map

## Purpose And Baseline

This map classifies the pinned legacy GoldenDict source at commit
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It controls migration ownership,
dependency direction, Qt 6 work, test obligations, and implementation order.
It is not permission to copy the legacy tree wholesale.

The baseline contains 1,135 tracked files:

- 319 root files, including 273 C, C++, Objective-C++, and header files;
- 37 root UI, resource, style, manifest, and platform metadata files;
- 380 product assets under `flags/`, `icons/`, `locale/`, `help/`, and
  `redist/`;
- 436 bundled or platform payload files under `maclibs/`, `winlibs/`,
  `opencc/`, `mouseover_win32/`, `qtsingleapplication/`, `generators/`, and
  `nsis/`.

The legacy tree has no material automated application or backend test suite.
Every migrated capability therefore needs a new focused test or an explicit
manual parity check.

## Ownership Rules

- Product logic belongs behind the public `goldendict_core` Tiger module,
  which builds as a DLL/shared library and exposes narrow desktop and headless
  service interfaces.
- `apps/goldendict` contains GUI presentation and the composition root only.
  Widgets display state and forward user intent; they do not implement product
  use cases or infrastructure.
- Foundation, dictionary, format, article, configuration, and application
  responsibilities remain independently tested internal components of
  `goldendict_core`; source-level separation does not require a binary module.
- Concrete backends depend on dictionary abstractions and shared primitives.
  Application and GUI code never depend on a concrete backend.
- A future dictionary-service process must reuse the same discovery, indexing,
  lookup, article, and resource behavior without linking Qt Widgets, Qt Gui, or
  Qt WebEngine. It is intended for AI lookup, so results preserve structured
  content, dictionary/language metadata, stable provenance, match information,
  and bounded resource references instead of requiring HTML scraping. Its
  eventual transport stays outside the core API.
- Add a separate module only for an independent consumer, optional deployment
  lifecycle, distinct platform/license/dependency boundary, or required plugin
  ABI. Do not create one DLL per layer, format, or legacy file.
- Platform behavior stays behind narrow adapters. Linux is implemented first;
  Windows and macOS source remains mapped but is not imported into active Linux
  targets prematurely.
- Bundled legacy binaries and headers are inventory evidence, not dependency
  sources. External libraries must be resolved through Conan and their current
  licenses recorded before use.

## Target Module And Application Layout

The paths below are ownership boundaries, not a requirement to create every
directory before code needs it.

- `modules/core/`: the Tiger module that produces the `goldendict_core`
  shared-library boundary and contains internal
  foundation, dictionary, local-format, article, configuration, and
  application components.
- `modules/core/include/goldendict/core/`: narrow installed desktop
  and headless APIs, transport-neutral DTOs, and genuine extension contracts.
- `modules/core/src/`: private implementations organized by
  responsibility, including `foundation/`, `dictionary/`, `formats/`,
  `article/`, and `application/`.
- Later optional integration modules are added only when their deployment or
  dependency boundary is demonstrated; network, audio, and platform work are
  not pre-split speculatively.
- `apps/goldendict/src/`: `main.cpp` composition plus GUI widgets, view models,
  and Qt WebEngine presentation adapters only.
- `apps/goldendict/resources/`: styles, icons, translations, help, desktop
  metadata, and other runtime assets after provenance review.

The public module keeps implementation and concrete-format headers out of the
installed surface.

## Dependency Direction

```text
apps/goldendict presentation -----> desktop facade ---+
future dictionary service --------> headless API -----+-> goldendict_core
future HTTP/gRPC/etc. adapter -----> neutral DTOs -----+

Within goldendict_core:

public APIs -> private application
                          /          |          \
                         v           v           v
                  dictionary      article   configuration
                      ^              |
                      |              +------> dictionary
                      |
               format adapters

All private components -> shared foundation/infrastructure
Core composition ------> application + format adapters
```

Forbidden edges include application or presentation code importing a concrete
format, a format adapter calling UI code, shared infrastructure knowing a
dictionary format, article assembly depending on a browser widget, or
cross-platform code directly invoking X11/Win32/macOS APIs. Core public headers
must not expose GUI or transport types. `main.cpp` may import only a future
optional integration module's narrow composition API.

## Source And Resource Classification

### Application Foundation

Legacy evidence includes `main.*`, `parsecmdline.*`, `config.*`, `gddebug.*`,
`categorized_logging.hh`, `termination.*`, `delegate.*`, `mutex.*`, `sptr.hh`,
`ex.hh`, `cpp_features.hh`, `atomic_rename.*`, `processwrapper.*`, and
`qtsingleapplication/`.

Target ownership is split between private foundation/application components in
`goldendict_core` and the presentation shell. Configuration persistence
belongs to the core application component, not GUI widgets.
Replace the bundled
QtSingleApplication copy with a maintained Conan-resolved or small
project-owned solution only after single-instance and command forwarding
semantics are captured in tests.

Primary Qt 6 work: modern message handling, command-line parsing, atomic and
threading APIs, signal/slot syntax, paths, and removal of `qt4x5.hh` shims.

### Dictionary Contract And Request Lifecycle

Legacy evidence includes `dictionary.*`, `loaddictionaries.*`, `initializing.*`,
`instances.*`, `wordfinder.*`, `wordlist.*`, `wildcard.*`, and the
`Dictionary::Request`, `WordSearchRequest`, `DataRequest`, and
`Dictionary::Class` hierarchy.

Target ownership is the private dictionary component of `goldendict_core`.
Preserve asynchronous completion, cancellation, incremental updates, errors,
headword lookup, article retrieval, resource retrieval, dictionary identity,
properties, and deferred initialization. Modernize ownership and
synchronization deliberately; do not mechanically translate raw pointers or
`QAtomicInt` usage. Remove the legacy assumption that public requests are
created and destroyed only on the GUI thread; document the headless execution
and callback context explicitly. Requests used by an AI service must support
explicit dictionary/language filters, result limits, deadlines, cancellation,
and deterministic partial-result/error reporting without reading desktop
selection or active-group state.

### Shared Index, Storage, File, And Text Primitives

Legacy evidence includes `btreeidx.*`, `chunkedstorage.*`, `dictzip.*`,
`indexedzip.*`, `zipfile.*`, `splitfile.*`, `decompress.*`, `file.*`,
`filetype.*`, `fsencoding.*`, `ufile.*`, `utf8.*`, `wstring*`, `folding.*`,
`langcoder.*`, `language.*`, `htmlescape.*`, and `x64.*`.

Target ownership begins in the private foundation and dictionary components of
`goldendict_core`. Keep format-specific helpers private to their adapter; move
a primitive into shared infrastructure only when its contract is
format-independent. Generated index formats may change, but original
dictionary files must remain directly usable.

Primary Qt 6 work: replace `QRegExp`, move non-UTF codecs to Qt Core5Compat or
a Conan-resolved codec library with an explicit exit plan, audit integer/file
offset width, and preserve cancellation and corruption handling.

### Local Dictionary And Archive Backends

The authoritative legacy discovery path is `loaddictionaries.cc`.

- StarDict: `stardict.*`; `.ifo`, `.idx`, `.dict`, and compressed data.
- Babylon: `bgl.*`, `bgl_babylon.*`; `.bgl`.
- ABBYY Lingvo DSL: `dsl.*`, `dsl_details.*`; `.dsl`, `.dsl.dz`, and resource
  directories.
- Dictd: `dictdfiles.*`; `.index`, `.dict`, and `.dict.dz` families.
- XDXF: `xdxf.*`, `xdxf2html.*`; `.xdxf` and `.xdxf.dz`.
- SDict: `sdict.*`; `.dct`.
- Aard: `aard.*`; `.aar`.
- MDict: `mdx.*`, `mdictparser.*`, `ripemd.*`; `.mdx` plus resource companions.
- GLS: `gls.*`; `.gls` and `.gls.dz`.
- ZIM: `zim.*`; `.zim` and split `.zimaa` families.
- SLOB: `slob.*`; `.slob`.
- EPWING: `epwing.*`, `epwing_book.*`, `epwing_charmap.*`; `catalogs`-based
  books.
- LSA audio archives: `lsa.*`; `.lsa` and `.dat`.
- ZIP audio packs and directories: `zipsounds.*`, `sounddir.*`; `.zips` and
  configured sound directories.

Local backends are private adapters inside `goldendict_core`. They depend only
on dictionary contracts and shared infrastructure, register through one
internal factory mechanism, retain their upstream copyright notices, and
receive legal generated or redistributable fixtures before enablement. A
format becomes a separate module only if a later plugin or optional deployment
requirement independently justifies that boundary.

### Morphology, Transliteration, And Language Support

Legacy evidence includes `hunspell.*`, `transliteration.*`, `romaji.*`,
`russiantranslit.*`, `german.*`, `greektranslit.*`,
`belarusiantranslit.*`, `chinese.*`, `chineseconversion.*`, `country.*`,
`language.*`, and the generated folding tables.

Target ownership is focused private dictionary-support components in
`goldendict_core`. Hunspell and OpenCC are Conan dependencies. Verify case and
diacritic folding, language identification, morphology weighting, and each
enabled transliteration with data-driven tests.

### Article Construction And Resource Handling

Legacy evidence includes `article_maker.*`, `article_netmgr.*`, `audiolink.*`,
`webmultimediadownload.*`, `externalviewer.*`, `tiff.*`, and per-backend HTML
transformation code.

Target ownership is the private article component of `goldendict_core` plus
narrow network and audio extension contracts only when needed. Article
generation must be testable without Qt WebEngine. Define typed internal URLs
and resource requests before restoring browser behavior.

### WebKit UI To Qt WebEngine

Legacy evidence includes `articleview.*`, `articlewebview.*`,
`articleinspector.*`, and WebKit calls in `mainwindow.*`, `main.*`, and
`article_netmgr.*`.

This is a redesign boundary. `QWebView`, `QWebPage`, `QWebFrame`,
`QWebElement`, `QWebInspector`, `QWebSettings`, link delegation, synchronous
JavaScript, frame traversal, security-origin whitelists, and context-menu hit
testing do not have mechanical one-to-one replacements. Use Qt WebEngine page,
profile, URL-scheme handler, request interceptor, JavaScript callbacks, and
DevTools APIs behind an article-view interface.

### Online And External Sources

Legacy evidence includes `mediawiki.*`, `website.*`, `forvo.*`,
`dictserver.*`, `programs.*`, and configuration in `sources.*`.

Target ownership begins as private external-source adapters in
`goldendict_core`. Split an optional integration module only if its dependency
or deployment lifecycle warrants it. These remain a separately gated Phase 7
workstream. Tests use local deterministic HTTP/DICT fixtures; no automated test
depends on a public service or API key.

### Audio And Speech

Legacy evidence includes `audioplayer*`, `ffmpegaudio.*`,
`multimediaaudioplayer.*`, `externalaudioplayer.*`, `voiceengines.*`,
`speechclient*`, `speechhlp.*`, `texttospeechsource.*`, and `sapi.hh`.

Target ownership begins behind private audio service interfaces in
`goldendict_core`. A separate audio integration module is justified when
optional codec/runtime dependencies require independent deployment. Qt
Multimedia, FFmpeg, Vorbis/Ogg, libao, external-player, and native speech
choices remain separate capabilities. Do not let an audio backend enter
dictionary parsing or article generation directly.

### Full UI And User-Owned State

Legacy evidence includes `mainwindow.*`, all 19 root `.ui` files,
`maintabwidget.*`, `translatebox.*`, `dictionarybar.*`, `groupcombobox.*`,
`groups*`, `preferences.*`, `sources.*`, `history*`, `favoritespanewidget.*`,
`dictheadwords.*`, `dictinfo.*`, `fulltextsearch.*`, `searchpanewidget.hh`,
`scanpopup.*`, `scanflag.*`, and supporting widgets.

Target ownership is split: user-state models and use cases belong to the
private application component of `goldendict_core`; widgets and view models
belong to `apps/goldendict`. Preserve legacy configuration, groups, history,
and favorites through a compatible migration path. UI layout and interactions
are parity work, not an opportunity for redesign.

### Linux And Later Platform Integration

Linux evidence includes X11 use in `hotkeywrapper.*`, `keyboardstate.*`,
`mouseover.*`, `scanpopup.*`, `broken_xrecord.*`, `fixx11h.h`, tray behavior,
selection monitoring, and desktop metadata under `redist/`.

Windows evidence includes `mouseover_win32/`, `wordbyauto.*`, `hotkeys.*`,
`speechclient_win.*`, `x64.*`, manifests, `winlibs/`, and `nsis/`.

macOS evidence includes `*.mm`, `macmouseover.*`, `machotkeywrapper.mm`,
`speechclient_mac.mm`, `myInfo.plist`, and `maclibs/`.

Linux code enters a private platform adapter behind core application
interfaces. Split a platform integration module only if independent deployment
or system dependencies justify it. Document X11 and Wayland differences.
Windows/macOS sources remain mapped for Phase 10. Bundled
`winlibs/` and `maclibs/` content will not be copied; dependencies come from
Conan.

### Product Assets, Localization, Help, And Packaging

The 380 tracked product assets under `flags/`, `icons/`, `locale/`, `help/`,
and `redist/`, plus root CSS and QRC files, remain product resources. Preserve
relative lookup behavior, translation contexts, attribution, and install
locations. Import assets only with known provenance and only when their owning
feature enters the migration.

## External Dependency Map

Legacy direct dependencies include Qt Core, Gui, Widgets, XML, Network, SVG,
SQL, WebKit, PrintSupport, Help, Multimedia, and X11Extras; zlib, bzip2, LZO,
Hunspell, Vorbis/Ogg, FFmpeg, libao, iconv, X11/XTest, TIFF, EPWING/libeb,
OpenCC, xz/lzma, and zstd.

Migration policy:

- Qt WebKit becomes Qt WebEngine and its bridge is redesigned.
- All enabled third-party dependencies are declared through Conan.
- Optional features use explicit Conan options and CMake feature gates.
- No dependency is added merely because it existed in the old qmake file.
- Each dependency addition records purpose, linkage, license, platform scope,
  and the first backend or feature that requires it.

## Backend Migration Batches

1. Vertical slice: StarDict only. It proves discovery, index generation,
   lookup, article HTML, resource routing, and Qt WebEngine rendering.
2. Text-oriented local formats: Dictd, SDict, XDXF, GLS, and DSL. This batch
   maximizes reuse of the validated text/index/article path.
3. Complex binary/container formats: Babylon BGL, MDict, Aard, ZIM, and SLOB.
   Each is isolated because decompression, encryption/container parsing, and
   embedded resources differ materially.
4. Specialized and audio collections: EPWING, LSA, ZIP sound packs, and sound
   directories.
5. Non-file dictionary providers: morphology/transliteration, MediaWiki,
   websites, Forvo, DICT servers, external programs, and TTS. These follow
   their Phase 5 or Phase 7 gates rather than blocking the local-format batches.

Batching controls implementation order only. The first formal Linux release is
blocked until every format and major Linux workflow from the pinned baseline
passes its acceptance gate.

## Fixture And Verification Policy

- Prefer generated minimal dictionaries with intentionally tiny content.
- A checked-in fixture needs a `README` stating format, generator or source,
  copyright, license, and which behaviors it covers.
- Do not check in downloaded dictionaries without explicit redistribution
  permission.
- Each backend covers discovery, valid indexing, headword enumeration, exact
  lookup, resource lookup when applicable, cancellation, missing/truncated
  input, corrupt index recovery, stable diagnostics, and index rebuild.
- Original dictionary data remains unchanged. Tests may assert that generated
  indexes are replaceable implementation artifacts.
- Network providers use local servers and deterministic responses.
- Rendering has non-visual HTML/resource assertions plus a documented Qt
  WebEngine smoke test.

## Phase 3 Review Gate

Phase 3 is ready for approval when this map, `feature-parity.md`, and
`first-usable.md` agree on ownership, dependency direction, backend order,
fixtures, deferred behavior, and the Phase 4 acceptance gate. Broad source
movement starts only after that review.
