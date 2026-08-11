# Architecture

This document describes GoldenDict's Tiger-based repository structure, project
layout rules, and design rationale. `AGENTS.md` stays short and links here for
details.

## Project Purpose

GoldenDict is a cross-platform dictionary lookup application. The migration
uses Tiger's reusable CMake and Conan 2 structure on Linux, Windows, and macOS.

Tiger owns module and application conventions, Conan dependency integration,
installation layout, and generated project configuration files. GoldenDict
owns product identity and behavior. See [migration.md](migration.md).

## Supported Platforms And Toolchains

Supported operating systems:

- Linux.
- Windows.
- macOS.

Supported compiler families:

- MSVC 2019 or newer.
- GCC 9.1 or newer.
- Clang/LLVM 8.0 or newer.

For now, treat macOS compiler support under the Clang/LLVM policy instead of
maintaining a separate AppleClang minimum version.

## Repository Structure

- `apps/`: application targets.
- `apps/<app_name>/resources/`: optional application resource directory for
  runtime files such as configs, images, and other assets. `TigerApp.cmake`
  copies its contents to the app binary directory after build when resource
  copying is enabled.
- `modules/`: reusable project modules.
- `cmake/`: shared CMake logic, templates, checks, and packaging helpers.
- `cmake_finders/`: project CMake finder modules for Conan dependencies.
- `conan/recipes/`: narrowly scoped local recipes for dependencies unavailable
  from configured Conan remotes.
- `profiles/`: project Conan profiles for dependency build requirements.
- `test_package/`: Conan package verification project.
- `CMakeLists.txt`: root CMake entry point.
- `conanfile.py`: Conan recipe for configuring, building, and packaging.
- `VERSION`: project version source.

See [coding-style.md](coding-style.md) for concrete module, application,
proto, and generated-file layout rules.

## Module Design

Tiger separates public shared-library modules from runnable applications.
GoldenDict initially uses one product library, `goldendict_core`, while
`apps/goldendict` contains presentation and the composition root only.

The planned dependency spine is:

```text
apps/goldendict GUI ----------> desktop application facade --+
                                                             |
future dictionary service ---> headless dictionary API ------+-> goldendict_core
                                                             |
future transport adapter -----> transport-neutral DTOs ------+

goldendict_core contains private application/domain, dictionary-format,
article, configuration, and infrastructure components.
```

The internal components preserve dependency inversion and focused tests
without creating a public ABI for each layer or dictionary format. The
headless API supports discovery, indexing, lookup, article/resource retrieval,
cancellation, and lifecycle without Qt Widgets, Qt Gui, or Qt WebEngine. It
does not choose HTTP, gRPC, JSON, or another future service transport.

Consumers inject a `CoreConfiguration` containing dictionary roots and the
generated-index directory. `LoadConfiguration` treats a missing file as a
clean profile, while `SaveConfiguration` persists the same bounded,
versioned schema. `CreateDictionaryService` is the headless composition entry
point and `CreateDesktopFacade` is the presentation-facing entry point. Both
compose private format adapters inside `goldendict_core`; neither exposes a
StarDict type. Synchronous lookup is available for simple headless consumers,
and `StartLookup` returns an owned request with explicit completion and
cancellation whose lifetime does not depend on the service object.

Lookup normalization is a private foundation concern. Backends compare a
canonical Unicode form that applies compatibility normalization, full case
folding, diacritic removal, and whitespace/punctuation folding. Public results
retain the dictionary's original headword and expose the canonical form only as
transport-neutral match metadata, so GUI and future AI-service adapters share
identical lookup semantics.

The same headless lookup request supports bounded prefix matching through
`MatchMode::kPrefix`. Prefix ranking is core behavior: canonical exact matches
come first, followed by shorter canonical candidates with deterministic scores.
Adapters may present these entries as type-ahead results, but neither the GUI
nor a future AI transport reimplements folding, ranking, limits, cancellation,
or dictionary traversal.

Legacy text encoding is also a private foundation concern. Format adapters use
one bounded, strict UTF-8 conversion primitive rather than Qt GUI-era codec
objects or format-local conversion loops. The primitive supports both decoding
dictionary payloads and encoding query text, rejects substitution on malformed
or unrepresentable data, and keeps ICU types out of public headers.

`main.cpp` may wire a future optional integration module into the core
extension contracts; other GUI code must not include adapter headers.

Another product DLL is justified only by a real deployment boundary such as
optional dependencies, platform isolation, or plugin loading. Network, audio,
and desktop integration are evaluated against that rule when their phases
begin instead of being pre-split speculatively.

Phase 2 retains Tiger's base module as build infrastructure and the minimal
GoldenDict application shell. Phase 4 introduces `goldendict_core`.

## AI Dictionary Service Compatibility

The future service is an AI retrieval surface, not a remote copy of the GUI.
Its adapter may use MCP, HTTP, gRPC, or another protocol, but those transports
map onto the same headless core contract.

The contract is designed around:

- dictionary catalog and capability discovery;
- exact, suggested, and later full-text lookup with explicit dictionary and
  language filters;
- bounded single or batch requests with cancellation, deadlines, pagination,
  and result limits;
- structured results containing the requested and normalized headword, match
  kind or score, stable dictionary identity, dictionary metadata, language
  metadata, article text, optional sanitized HTML, and typed resource
  references;
- provenance sufficient for an AI client to identify and cite the dictionary
  source and edition; and
- deterministic errors and partial-result reporting without hidden desktop
  state.

The installed API publishes hard limits for query text, filter counts, filter
size, and result count. The core rejects malformed UTF-8 and embedded NUL
bytes at the service boundary before dispatching work to a format adapter;
requested result counts above the published maximum are deterministically
clamped.

Dictionary content is untrusted data. The core and service adapter must not
execute embedded active content or interpret article text as instructions.
Transport adapters apply authentication, authorization, rate limits, request
budgets, logging, and protocol serialization outside `goldendict_core`.

See [project-design-rules.md](project-design-rules.md) for project design rules
and design-boundary rationale.
