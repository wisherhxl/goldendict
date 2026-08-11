# Phase 4 First-Usable Definition

## Meaning

“First usable” is an internal vertical-slice milestone, not the first formal
Linux release. It proves the migration architecture through one real local
dictionary workflow while the remaining parity work continues incrementally.

The approved representative backend is StarDict.

## Required User Path

On Linux, a user can:

1. start the Qt 6 application;
2. point it at a directory containing the generated StarDict fixture;
3. discover the fixture without editing source or build files;
4. build or rebuild the application-owned index;
5. enter an exact headword;
6. receive the result through the real asynchronous dictionary request path;
7. assemble the article as HTML through the real article layer;
8. load embedded fixture resources through a typed internal URL path; and
9. render the article in the Qt WebEngine view.

The flow must have no runtime dependency on the legacy worktree.

## Included Engineering Scope

- A public `goldendict_core` shared library containing separately tested
  foundation, dictionary, StarDict, article, configuration, and application
  components behind separate narrow headless and desktop interfaces.
- A transport-neutral headless API for discovery, indexing, lookup,
  article/resource retrieval, cancellation, and lifecycle. It has no Qt
  Widgets, Qt Gui, Qt WebEngine, GUI-thread, HTTP, or gRPC dependency.
- A structured lookup result with requested and normalized headword, stable
  dictionary identity and provenance, match information, language metadata,
  plain article text or sanitized markup, and typed resource references. The
  API does not require an AI client to scrape rendered GUI HTML.
- A presentation-only GUI that consumes the `goldendict_core` application
  facade without exposing the concrete backend to widgets.
- Minimal configuration schema for dictionary paths and index location.
- Safe handling of an absent legacy configuration and a documented boundary
  for later legacy-config migration.
- Dictionary interface and request lifecycle for discovery, indexing, exact
  word search, article data, and resource data.
- Cancellation, completion, and error propagation needed by the slice.
- Only the shared file/index/text primitives required by StarDict.
- StarDict `.ifo`, `.idx`, and `.dict` handling required by the generated
  fixture. Compressed `.dict.dz` becomes part of the same backend gate before
  the StarDict backend is declared complete, even if the first UI demo starts
  with uncompressed data.
- Backend-independent article assembly.
- Qt WebEngine integration through an article-view boundary, not direct
  backend-to-widget calls.
- Minimal lookup controls and article display; no voluntary visual redesign.

## Generated Fixture

Add a tiny project-generated StarDict fixture containing at least:

- two distinct headwords;
- UTF-8 non-ASCII text;
- basic formatted article content;
- an internal cross-reference or link;
- one embedded resource path used by the rendered article; and
- a deliberately missing headword case.

The fixture directory includes provenance and license documentation. Prefer a
small deterministic generator so binary offsets can be reviewed and regenerated
instead of hand-editing opaque files.

## Automated Acceptance

- Fixture discovery finds exactly the intended dictionary.
- A first run creates a usable index; a second run reuses it.
- A stale or corrupt generated index is rejected and rebuilt safely.
- Exact lookup returns the expected headword and article.
- Missing lookup finishes successfully with no false result.
- Article and resource requests complete with the expected data.
- Cancellation reaches a finished state without a use-after-free or hang.
- Malformed/truncated fixture variants produce stable errors and no crash.
- Article HTML tests run without Qt WebEngine.
- A focused application test exercises the non-visual orchestration boundary.
- A headless consumer completes the fixture workflow using only the installed
  core API and a non-GUI event loop.
- The headless result identifies the fixture dictionary and language, enforces
  the request's result limit, and returns article content as inert data with no
  active-content execution.
- Component tests and include-boundary checks prove that the GUI does not
  depend on concrete backend headers and that concrete formats remain private.
- The shared-library build, install, and exported CMake target expose only the
  intended core facade to a consumer without leaking private dependencies.
- Existing Release configure, build, install, package, and Conan
  `test_package` checks remain green.

Run sanitizer-assisted focused tests when practical for parser and request
lifetime code. Sanitizers supplement, not replace, normal Release verification.

## Rendering Acceptance

Document one Linux Qt WebEngine smoke procedure that verifies:

- the expected article text appears;
- styles and the embedded fixture resource load;
- an internal dictionary link is intercepted rather than sent to the public
  network;
- external navigation is handled according to the application policy; and
- closing or replacing the lookup does not leave a hung request.

Automate browser-facing checks where Qt WebEngine provides a stable callback;
retain a short manual checklist for visual behavior that cannot be asserted
reliably.

## Explicitly Deferred From The Slice

- All other local dictionary formats.
- Suggestions, morphology, transliteration, and full-text search beyond what
  the exact lookup path strictly requires.
- Online dictionaries, arbitrary websites, Forvo, DICT servers, proxy/auth,
  and external programs.
- Complete tabs, groups, preferences, history, favorites, dictionary panes,
  and source-management UI.
- Audio playback, speech, scan popup, global hotkeys, clipboard/selection
  monitoring, tray integration, printing, complete translations, and help.
- Windows and macOS restoration.

Deferred means sequenced later, not removed from the formal Linux parity gate.

## Implementation Increments

1. [ ] Commit the generated StarDict fixture and parser-facing tests. The
   initial uncompressed exact-lookup fixture is complete; formatted content,
   links, and resources remain.
2. [x] Establish the narrow `goldendict_core` headless API and desktop facade,
   internal component boundaries, and dependency tests.
3. [x] Port the smallest dictionary request contract and shared primitives
   needed by the fixture tests.
4. [ ] Implement StarDict discovery, indexing, search, article, and resource
   behavior as a private format adapter.
5. [ ] Add browser-independent article assembly and typed resource URLs as a
   private core component.
6. [ ] Implement configuration, catalog, and lookup orchestration behind the
   core application facade.
7. [ ] Add presentation-only lookup controls and article view, then pass the
   WebEngine checks.
8. [ ] Add the installed headless consumer and run the full Release,
   `goldendict_core` install/export, package, and Conan consumer gate.

Each increment should build and test independently. Mechanical source import,
Qt 6 adaptation, and behavior changes should remain reviewable as separate
commits whenever practical.

## Exit Gate

Phase 4 is complete only when the required user path and automated acceptance
pass from a clean checkout, the fixture provenance is reviewable, the legacy
worktree remains unchanged, and the deferred list is still represented in the
feature-parity matrix.
