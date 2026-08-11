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

`stardict_reader_test` generates a deterministic uncompressed StarDict fixture
at runtime. It verifies metadata, duplicate and UTF-8 headwords, exact and
missing lookup, and stable error categories for invalid metadata, truncated
indexes, missing companion files, and article ranges outside dictionary data.
It also verifies generated-index creation and reuse, source-stamp invalidation,
checksum corruption recovery, temporary-file cleanup, and rejection of a
directory used as an index-file target.

`stardict_dictionary_test` verifies the private backend contract and StarDict
adapter: identity and provenance, bounded exact results, cancellation,
deadlines, translated format errors, raw formatted article preservation, and
typed resources. Resource checks cover legacy delimiters, missing files,
traversal and absolute paths, symlink escapes, oversized data, and
cancellation.

`stardict_discovery_test` verifies recursive and explicit-file discovery,
stable deduplication, unrelated-file filtering, and partial results when a
configured dictionary root is missing.

`article_assembler_test` verifies browser-independent plain-text and HTML
assembly, a strict formatting allowlist, active-content and event-attribute
removal, inert malformed-markup fallback, bounded document size, and canonical
typed lookup and resource URLs. It also verifies that unsafe resource paths and
non-internal navigation are not emitted into rendered HTML.

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
