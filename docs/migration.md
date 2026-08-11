# GoldenDict Qt 6 Migration

## Provenance

This migration combines two read-only source baselines:

- GoldenDict product source: commit
  `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` from the legacy GoldenDict
  repository.
- Tiger project template: commit
  `2b36f6bdd52ff2fe81572893b311b6b33752c075` from the Tiger repository.

The GoldenDict baseline defines product identity and behavior. The Tiger
baseline supplies the CMake, Conan, module, test, install, and packaging
structure. Reusable Tiger implementation names, including `Tiger*.cmake` and
`ti_*` commands, remain unchanged unless a product-facing interface requires a
GoldenDict name.

## Governing Rules

- GoldenDict remains the product. Its behavior, identity, and migrated source
  remain licensed under GPLv3 or later.
- Tiger Platform remains separately licensed under its existing MIT license.
  Preserve its copyright and license notices instead of relicensing it as
  GoldenDict product code.
- The distributed GoldenDict application must satisfy GPLv3-or-later terms;
  the permissively licensed Tiger Platform source remains independently
  available under MIT.
- Tiger owns the project, dependency, build, test, install, and package
  structure. GoldenDict product behavior is migrated into that structure.
- Runtime entry points, GUI, Qt WebEngine integration, application resources,
  translations, and product orchestration belong in `apps/goldendict`.
- Add a library under `modules/` only when it is genuinely reusable, has a
  clear interface, and can be tested independently. Do not create modules only
  to mirror the legacy file layout.
- Linux with Qt 6 is the first supported target. Keep platform dependencies
  behind narrow boundaries so Windows and macOS can be restored later.
- Conan resolves all external dependencies. Official Conan `*/system`
  packages are allowed for Linux display and graphics ABI integration.
- Compilation alone is not a migration gate. Every migrated capability needs
  focused automated tests or a documented manual parity check.
- Keep the legacy GoldenDict worktree clean and unchanged as the source and
  behavior reference.
- Freeze migration scope at GoldenDict commit `3d93dd66` until Linux behavior
  parity is complete. Do not continuously chase later legacy `master` changes;
  reconcile them as a separately reviewed update after the pinned baseline
  passes the Linux acceptance gate.

## Optimized Migration Plan

### Stage 1 — Baseline, Isolation, And License Boundary

- Pin the GoldenDict and Tiger source commits.
- Keep the legacy GoldenDict worktree clean and create the Tiger-based
  migration worktree.
- Inventory legacy build options, bundled code, generated files, external
  dependencies, and platform-specific code.
- Establish the repository license layout before importing legacy source:
  GoldenDict product code under GPLv3-or-later and Tiger Platform under MIT,
  with component notices preserved.

Gate: both source baselines and worktrees are independently verifiable, and
the product/component license boundary is documented and represented in the
repository.

Status: complete. The source baselines and worktree isolation are recorded,
and the repository represents the product/component boundary through the root
GPL license, `LICENSES/README.md`, preserved Tiger MIT notices, SPDX headers on
new product entry points, and package metadata.

### Stage 2 — Reproducible Tiger And Qt 6 Skeleton

- Apply GoldenDict product and package identity without renaming reusable Tiger
  internals.
- Remove unused template applications, protobuf examples, and dependencies.
- Resolve Qt 6.11.1 and Qt WebEngine through Conan.
- Build a minimal `apps/goldendict` Qt Widgets application backed by Qt
  WebEngine.
- Keep memory-constrained WebEngine build policy in the Conan profile rather
  than relying on operator-only command-line knowledge.
- Capture the exact clean Git commit in Conan export metadata so package source
  builds reproduce the migration revision instead of cloning a default branch.
- Verify a clean Conan install, configure, build, focused tests, install, and
  initial package behavior.

Gate: a clean Linux environment can reproduce the minimal GoldenDict Qt 6
skeleton, and Qt WebEngine is demonstrably enabled.

Status: implementation and direct Release verification complete. Conan
`test_package` verification remains deferred until an approved commit makes
the SCM revision exportable.

### Stage 3 — Porting Map And First-Usable Definition

Classify every legacy source and resource by:

- ownership in `apps/goldendict` or a justified reusable module;
- dependencies and direction of dependency;
- required Qt 6 API changes;
- operating-system coupling;
- source license and notices;
- available tests or required fixtures;
- priority for the first usable Linux release.

Produce a dependency graph, feature-parity matrix, and prioritized backend
batches. StarDict is the approved representative dictionary format for the
Stage 4 vertical slice. Batching controls migration and verification order; it
does not reduce the formal Linux release scope. The first formal Linux release
must restore every dictionary format supported by the pinned legacy
GoldenDict baseline.

Gate: the ownership/dependency map, first-usable feature set, representative
backend, fixtures, and deferred-feature list are reviewed and approved before
broad source movement.

### Stage 4 — Minimal Vertical Slice

Migrate the smallest complete user path:

1. application configuration;
2. dictionary abstraction and asynchronous request path;
3. one representative dictionary backend;
4. dictionary discovery and indexing;
5. headword lookup and article HTML generation;
6. Qt WebEngine rendering in a minimal application UI.

Keep scope deliberately narrow. This stage validates dependency direction,
the WebEngine bridge, and the end-to-end migration strategy before expanding
the foundation or format count.

Gate: a fixture dictionary can be discovered, indexed, searched, and rendered
through the real Qt 6 application path with automated checks for non-visual
behavior and a documented rendering smoke test.

### Stage 5 — Non-UI Foundation Hardening

- Port common utilities, configuration, logging, and error handling.
- Harden dictionary interfaces and asynchronous request ownership.
- Port compression, persistent storage, indexing, text folding,
  transliteration, and encoding behavior in dependency order.
- Replace obsolete Qt APIs deliberately, including regular expressions,
  codecs, containers, and signal/slot usage where required.
- Promote code into `modules/` only when the Stage 3 ownership map and actual
  reuse justify it.

Gate: focused tests pass without requiring the GUI, and the Stage 4 vertical
slice remains functional.

### Stage 6 — Dictionary Backends In Priority Batches

- Port backends in approved core, common optional, and deferred/high-cost
  batches.
- Give every backend small legal or generated fixtures.
- Verify discovery, indexing, headword enumeration, lookup, embedded resource
  loading, corruption handling, cancellation, and useful diagnostics.
- Keep optional backends behind explicit CMake/Conan feature checks.
- Do not enable a backend by default until its gate passes.

Gate per backend: fixture-based build and lookup parity. Internal milestones
may contain only the formats already migrated, but the first formal Linux
release is blocked until every format supported by the pinned legacy baseline
passes its gate. Original dictionary files must remain directly usable without
format conversion; implementation-generated indexes may be rebuilt when
binary compatibility cannot be preserved safely.

### Stage 7 — Articles, WebEngine, And Networking

Run three separately gated workstreams:

- article construction and transformation;
- Qt WebEngine navigation, JavaScript, resources, link handling, printing,
  zoom, search, and external URL bridge;
- resource networking and online dictionaries using Qt 6 APIs.

Stabilize local article rendering and embedded resources before enabling
online dictionary workflows. Treat WebKit-to-WebEngine behavior as a redesign,
not a mechanical API rename.

Gate: representative local articles render correctly, links and resources
work, and approved online dictionary scenarios pass without weakening the
local rendering gate.

### Stage 8 — Complete Application UI

- Port the main window, tabs, dictionary and group controls, preferences,
  history, favorites, inspector, and scan popup.
- Preserve the pinned legacy GoldenDict UI layout and interaction behavior.
  This migration does not include a voluntary UI redesign; make only the
  smallest behavior or layout changes required by Qt 6, Qt WebEngine, or Linux
  platform compatibility, and document those differences.
- Keep UI code dependent on application or module interfaces rather than
  concrete backend internals.
- Provide a compatible migration path for legacy configuration, dictionary
  groups, history, and favorites. Do not silently discard these user-owned
  states. Implementation-generated dictionary indexes may be rebuilt
  automatically instead of preserving their binary representation.

Gate: the Linux application can load, index, search, and render the approved
dictionary set through the intended user workflows.

### Stage 9 — Linux Integration And Release Quality

- Complete audio, clipboard and selection monitoring, global hotkeys, scan
  behavior, desktop files, icons, MIME integration, paths, process launching,
  translations, and resources.
- Document X11 and Wayland behavior separately where they differ.
- Finalize Conan profiles/locks, CI, install and uninstall behavior, CPack,
  formatter/static checks, dependency and license inventory, feature-parity
  matrix, known issues, and intentionally deferred features.

Packaging and clean-build checks run throughout the migration; this stage is
the final Linux acceptance gate, not the first time packaging is attempted.

Gate: clean clone/configure/build succeeds; the application launches;
representative dictionaries index and search; articles and embedded resources
render; settings persist; packages install and uninstall; automated tests
pass; and the build has no dependency on the legacy worktree.

### Stage 10 — Windows And macOS Restoration

After Linux acceptance:

- define platform Conan profiles and CI;
- implement the isolated platform adapters;
- solve Qt WebEngine deployment, native integration, audio, hotkeys, scanning,
  and installers;
- apply the same build, test, install, package, and workflow gates per
  platform.

This is a post-Linux roadmap and does not block the active Linux delivery.

The first formal Linux release is a full Linux behavior-parity milestone, not
a reduced-format or reduced-feature edition. In addition to every legacy
dictionary format, it must restore the pinned legacy baseline's major Linux
user workflows, including online dictionaries, audio, clipboard/selection
lookup, scan popup behavior, and global hotkeys. Earlier internal milestones
may be narrower only to make migration and verification incremental.

## Continuous Execution Rules

- Keep changes small and organized by one migration concern.
- Separate mechanical import, build wiring, Qt 6 API changes, and behavioral
  changes whenever practical.
- Run the smallest relevant tests after each increment and a clean build,
  install, and package gate at each stage boundary.
- Preserve source notices and update the dependency/license inventory whenever
  code or a dependency enters the migration tree.
- Report stage start, meaningful checkpoints, blockers or failed durable jobs,
  and stage completion. Long-running jobs must have a durable exit wake or an
  equivalent reconciliation mechanism.
- Do not modify legacy `master`, push, open a pull request, or publish artifacts
  without explicit authorization.

## Phase 2 Baseline

Phase 2 establishes the `GoldenDict` CMake project and `goldendict` Conan
package/application at version 1.6.0. This starts the Qt 6 and Tiger-based
GoldenDict version line. It provides a minimal Qt 6 application
window backed by Qt WebEngine. Product features from the legacy source are not
yet migrated.
