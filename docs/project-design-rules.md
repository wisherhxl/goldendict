# Project Design Rules

This document is the canonical place for Tiger's explicit project design rules.
Agents must preserve these rules before optimizing for local fixes. If a
requested change appears to conflict with a design rule, stop and explain the
conflict before editing.

Explicit rules override inferred patterns. Existing code is evidence of design
intent, but it is not the only source of truth.

When solving a problem:

- Read the relevant design rules first when they exist.
- State which rule governs the change.
- Implement the fix in a way that reinforces the rule.
- If no explicit rule exists, infer one from the nearest relevant code first,
  then widen to related code when the change affects shared infrastructure or
  public behavior.
- Always state the inferred rule's scope.
- If the inferred rule seems important, recommend adding it to this document or
  another project doc.

Concrete project design rules should be added here one by one during design
work. Do not invent rules just to fill this section.

## Documentation Language And Migration Terminology

All repository documentation must be written in English. The numbered
migration plan uses `Phase` consistently. `Stage` is not a second hierarchy or
an alias for those phases and must not be introduced into migration documents.

## Conan/CMake Responsibility Boundary

`conanfile.py` owns package-manager policy:

- package identity;
- settings and options;
- dependency requirements and dependency version constraints;
- dependency option validation;
- Conan generators and tool requirements;
- Conan layout;
- the package lifecycle that invokes configure, build, install, and package
  steps.

CMake owns build-system behavior:

- project structure;
- targets, modules, and applications;
- generated files;
- compiler and linker behavior;
- install layout;
- exported CMake package config;
- runtime deployment and plugin import behavior;
- platform-specific build mechanics.

Keep the boundary narrow and explicit: Conan resolves external dependency
policy and passes stable build facts into CMake; CMake consumes those facts to
build, install, and export Tiger.

Do not move target, source layout, compiler feature, generated-source, install,
or app/module behavior into `conanfile.py`.

Do not make CMake choose Conan dependency versions, hardcode Conan profile
policy, mutate Conan dependency options, or duplicate package-manager
resolution logic.

This split keeps Tiger usable both as a Conan package and as a CMake project
after `conan install`, without hiding target and install behavior inside the
recipe. It also prevents duplicate policy: for example, Qt shared/static
selection belongs to Conan profiles or options, while Qt app deployment/import
behavior belongs to CMake.

## Module Dependency Visibility

CMake module declarations own dependency visibility. A module dependency is
public only when it is part of installed public headers or exported CMake target
usage requirements; otherwise mark it private in `ti_define_module(...)`.

External dependency registration, such as `ti_register_external_dependency(...)`,
must only describe how an external target is found by consumers. It must not be
used as the source of truth for whether a module dependency is public or
private. The same external target may be public for one module and private for
another; generated package config files should emit dependency discovery only
when at least one exported module exposes that external target publicly.

## Shared-Library And GUI Boundary

GoldenDict product logic must be separated from the GUI through a Tiger public
module built as a shared library when `BUILD_SHARED_LIBS=ON`.

- `apps/goldendict` is the presentation executable and composition root. GUI
  classes display state, collect user input, and translate view events into
  application commands. They do not parse dictionaries, build indexes,
  persist configuration, implement lookup workflows, assemble articles, or
  contain backend-specific behavior.
- The initial `goldendict_core` module owns domain contracts, application use
  cases, local dictionary formats, article assembly, configuration, and shared
  primitives. These remain separate internal components, but do not create
  separate binary interfaces without an independent deployment boundary.
- `goldendict_core` exports separate narrow interfaces for headless dictionary
  operations and desktop-application orchestration, plus transport-neutral
  DTOs and extension contracts that an external adapter genuinely needs.
  Concrete formats and implementation details remain private.
- The core public surface must not depend on Qt Widgets, Qt Gui, Qt WebEngine,
  a GUI thread, or a network transport protocol. A future dictionary-service
  executable must be able to discover, index, query, and retrieve resources
  through the same library using a headless event loop.
- Asynchronous operations expose explicit completion, cancellation, errors,
  and ownership. Configuration paths, storage, and policies are injected or
  passed as data rather than read from GUI globals.
- Headless results are structured and bounded. They preserve stable dictionary
  identity, source/edition provenance, language metadata, match information,
  plain text or sanitized markup, and typed resource references so an AI
  client can filter and cite results without scraping GUI HTML.
- Dictionary text, markup, and resources are untrusted payloads. Core code does
  not execute active content or treat retrieved text as commands. A future
  service adapter owns authentication, authorization, rate limits, request
  budgets, logging, and transport serialization.
- High-level core components depend on abstractions rather than concrete
  backends. Built-in format composition stays private to `goldendict_core`.
  The executable's `main.cpp` may reference only a future optional module's
  narrow composition API; presentation classes may not.
- Add another shared-library module only when it has an independent consumer,
  optional deployment lifecycle, distinct platform/license/dependency
  boundary, or a required plugin ABI. A source-level responsibility alone is
  not sufficient. Do not create one module per layer, format, or legacy file,
  and do not introduce pass-through libraries with no independent contract.
- Public and private module dependencies must be declared accurately in
  `ti_define_module(...)`. Exposing a dependency through installed headers or
  exported usage requirements makes it public; implementation-only
  dependencies remain private.
- GoldenDict-managed HTTP/HTTPS response caching belongs exclusively to the
  Qt Network adapter. Core may persist transport-neutral cache policy, but the
  network module owns the application-lifetime manager, disk-cache instance,
  injected cache path, and request quiescence. Widgets and WebEngine do not
  own or reinterpret that policy, and the owned cache directory must not be
  shared by multiple disk-cache instances or combined with WebEngine data.

This rule applies to all migrated GoldenDict product behavior. It implements
single responsibility, interface segregation, and dependency inversion while
keeping the GUI replaceable and backend code independently testable.

The Phase 5 full-text boundary evolves the installed `DictionaryService` with
only a transport-neutral bounded query and structured result/error DTOs.
Document ingestion, generated-index paths and formats, lifecycle state,
reference corpora, and concrete adapters remain private to `goldendict_core`;
private search dependencies do not enter the exported dependency graph. The
virtual-interface addition is an intentional ABI change identified by Conan's
exact SCM and package revisions.
P6-FT-8 is complete. BGL now participates through the same private boundary
with referenced-article deduplication, first-retained-record ownership, inert
assembled text, and a sole-container source revision. No successor is selected.
