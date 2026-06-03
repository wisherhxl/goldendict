# AGENTS.md

Guidance for AI coding agents and human contributors working in this repository.

## Project Purpose

This project is an easy template for C++ projects that can be built across
platforms, including Linux, Windows, and macOS.

Officially supported operating systems:

- Linux.
- Windows.
- macOS.

Officially supported compiler families:

- MSVC 2019 or newer.
- GCC 9.1 or newer.
- Clang/LLVM 8.0 or newer.

For now, treat macOS compiler support under the Clang/LLVM policy instead of
maintaining a separate AppleClang minimum version.

## Current Project Facts

- Build system: CMake.
- Package manager: Conan 2.
- License: MIT, as recorded in `LICENSE`.
- Official project/template name: `Tiger`.
- Official CMake project namespace: `ti`.
- Conan package name: `tiger`.
- CMake project name: `Tiger`.
- Version is stored in `VERSION` and must use `X.Y.Z` format.

## Repository Map

- `apps/`: application targets.
- `apps/<app_name>/resources/`: optional application resource directory for
  runtime files such as configs, images, and other assets. `TigerApp.cmake`
  copies its contents to the app binary directory after build when resource
  copying is enabled.
- `modules/`: reusable project modules.
- `protos/`: protobuf definitions.
- `cmake/`: shared CMake logic, templates, checks, and packaging helpers.
- `cmake_finders/`: project CMake finder modules for Conan dependencies.
- `test_package/`: Conan package verification project.
- `CMakeLists.txt`: root CMake entry point.
- `conanfile.py`: Conan recipe for configuring, building, and packaging.
- `VERSION`: project version source.

## Working Rules

- Keep changes scoped to the requested task.
- Preserve existing user changes. Do not revert unrelated files.
- Prefer existing project patterns over introducing new structure.
- Update documentation when changing project behavior, build steps, layout, or
  contributor workflow.
- Avoid guessing project policy. If a decision is not documented here or in the
  repository, ask before encoding it as a rule.
- Do not commit generated build output.
- Before starting a new task, fetch the remote and make sure the local base
  branch is up to date.
- If the local branch has unpushed commits, uncommitted changes, or diverges
  from the remote, inspect the state and resolve it before creating a feature
  branch.
- In interactive collaboration, do not commit, push, or create pull requests
  automatically.
- If the user explicitly asks to commit, push, or create a pull request, do it
  without asking for another confirmation.
- For delegated implementation work, such as when the user asks an agent to
  make a plan and implement it, use the full branch, commit, push, and pull
  request flow when the work is ready.
- Create a local feature branch for delegated implementation work before
  committing.
- Do not push directly to `main` or `master` unless explicitly requested.
- Name task branches as `<type>/<short-kebab-case-description>`.
- Use these branch types:
  - `feature/` for new user-visible or template functionality;
  - `fix/` for bug fixes;
  - `docs/` for documentation-only changes;
  - `test/` for test-only changes;
  - `opt/` for optimization work;
  - `chore/` for maintenance, tooling, dependency, or cleanup work.
- Do not upload feature branches to a remote unless opening a pull request or
  explicitly requested.
- A feature branch may be deleted after its pull request is merged into `main`
  or `master`.
- Keep commit scope focused.
- Use Conventional Commits for commit messages:
  `<type>(optional-scope): <summary>`.
- Use these commit types: `feature`, `fix`, `docs`, `test`, `opt`, and `chore`.
- Use the same Conventional Commit format for pull request titles.
- Use this pull request description template:

```markdown
## Summary

- 

## Changes

- 

## Verification

- 

## Notes

- 
```

## Project Design Rules

Agents must preserve explicit project design rules before optimizing for local
fixes. If a requested change appears to conflict with a design rule, stop and
explain the conflict before editing.

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
- If the inferred rule seems important, recommend adding it to `AGENTS.md` or
  project docs.

Concrete project design rules should be added here one by one during design
work. Do not invent rules just to fill this section.

## Coding Style

- Use C++17 for new modules.
- Use Google C++ Style Guide for C++ coding style and naming conventions.
- Use the repository `.clang-format` for C and C++ formatting.
- Keep generated files out of source edits unless the task explicitly requires
  regenerating them.
- Generated files and directories are managed by the CMake workflow. Do not edit
  generated files directly; update templates, CMake logic, or source inputs
  instead.
- Generated source files must use fixed suffixes: `*.tp.h` and `*.tp.cc` for
  Tiger platform-generated files, and `*.pb.h` and `*.pb.cc` for
  protobuf-generated files.
- Generated-file cleanup logic relies on those suffixes.
- A module named `foo` lives under `modules/foo/`.
- Public headers for module `foo` live under
  `modules/foo/include/<TI_INTERNAL_NAME>/foo/`.
- Implementation files for module `foo` live under `modules/foo/src/`.
- Proto files live under `protos/<TI_INTERNAL_NAME>/`.
- An application named `app_name` lives under `apps/app_name/`.

## Build Workflow

Known prerequisites from the repository:

- CMake.
- Conan 2.
- A supported C/C++ compiler available in the current shell.

On Windows, use a Visual Studio Developer PowerShell or another shell where
MSVC is available to CMake.

After `conan install`, activate the generated Conan build and runtime
environment scripts before configuring so CMake and Conan-provided tools use
the expected paths and runtime libraries.

Keep dependency linkage profile-owned. Do not hardcode Qt shared/static policy
in `conanfile.py`; use Conan profiles or CLI options such as
`-o "qt/*:shared=True"` or `-o "qt/*:shared=False"`. `ti_add_qt_app` owns normal
Qt app deployment/import behavior: shared Qt uses runtime deployment, while
static Qt links the selected platform plugin and generates the required
`Q_IMPORT_PLUGIN(...)` source. Static platform plugin options are advanced
overrides: `qt_linux_platform_plugin` and `qt_windows_platform_plugin`; use
`off` only when the app handles plugin imports itself.

Official local Windows Debug build workflow:

```sh
conan install . --build=missing
build\generators\conanbuild.bat
build\generators\conanrun.bat
cmake --preset conan-default
cmake --build --preset conan-debug
```

Official local Windows Release build workflow:

```sh
conan install . --build=missing -s build_type=Release
build\generators\conanbuild.bat
build\generators\conanrun.bat
cmake --preset conan-default
cmake --build --preset conan-release
```

Official local Linux Debug build workflow:

```sh
conan install . --build=missing
. build/Debug/generators/conanbuild.sh
. build/Debug/generators/conanrun.sh
cmake --preset conan-debug
cmake --build --preset conan-debug
```

Official local Linux Release build workflow:

```sh
conan install . --build=missing -s build_type=Release
. build/Release/generators/conanbuild.sh
. build/Release/generators/conanrun.sh
cmake --preset conan-release
cmake --build --preset conan-release
```

Official local Windows Release install workflow:

```sh
cmake --install build --config Release
```

The verified default Windows install destination is `build/install`.

Official local Linux Release install workflow:

```sh
cmake --install build/Release
```

The verified default Linux Release install destination is
`build/Release/install`.

Use `-o '&:install_runtime_dependencies=True'` when preparing deployable
install/package output that should include runtime shared-library dependencies
for project binaries. Leave the option off for the default project-only install
layout.

Conan profiles or command-line options own dependency shared/static linkage.
Do not hardcode dependency linkage in `conanfile.py` unless the project requires
one mode. Runtime deployment logic must support shared, static, and optional
dependencies without requiring per-app workarounds.

Packaging through CMake/CPack is under development. Do not treat package output
as stable until the package workflow is verified and documented.

Official local test workflow:

```sh
ctest --preset conan-debug
ctest --preset conan-release
```

Tests are built by default. Disable them explicitly with `-DBUILD_TESTS=OFF`
only when a task does not need local test targets.

Use `ctest --preset conan-debug` after a Debug build and
`ctest --preset conan-release` after a Release build. Before considering a
change complete, prefer Release tests unless the change is Debug-specific or
Release cannot be built locally.

`conan install` generates `CMakeUserPresets.json`, which includes the
Conan-generated preset files. Use `cmake --list-presets` after `conan install`
to confirm the available preset names for the current platform and profile.

With the current Windows Debug and Release profiles, Conan generates:

- configure preset: `conan-default`;
- Debug build preset: `conan-debug`;
- Debug test preset: `conan-debug`;
- Release build preset: `conan-release`;
- Release test preset: `conan-release`.

With the current Linux Debug and Release profiles, Conan generates:

- Debug configure preset: `conan-debug`;
- Release configure preset: `conan-release`;
- Debug build preset: `conan-debug`;
- Debug test preset: `conan-debug`;
- Release build preset: `conan-release`;
- Release test preset: `conan-release`.

Library linkage policy:

- Support both shared and static library builds.
- Use shared libraries as the default and primary path.
- Treat project linkage and dependency linkage as separate policies. The
  project `shared` option controls Tiger targets; dependency options such as
  `qt/*:shared` belong in Conan profiles or command-line overrides.

## Verification Workflow

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
conan install . --build=missing -s build_type=Release
cmake --fresh --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release --output-on-failure
cmake --install build/Release
```

Run the full workflow after changes to CMake, Conan, modules, applications,
protobuf generation, tests, install behavior, or dependency configuration. For
documentation-only changes, a full build is not required. If the full workflow
is skipped, mention why in the final response or pull request notes.

Test framework policy:

- Use QTest for sample and project tests.
- Qt is expected as a dependency for all tests.

Expected verification areas:

- configure;
- build;
- test, if tests exist;
- package verification through `test_package/`, if applicable;
- formatting, if formatting is changed.

Pre-PR checklist:

- Keep the change focused on the requested task and avoid unrelated refactors.
- Run the relevant Debug or Release build workflow, with Release preferred
  before completion.
- Run the relevant `ctest` preset when tests exist.
- Run install verification when install or package behavior changes.
- Run `conan install` after dependency changes.
- Update `README.md` or `AGENTS.md` when behavior or workflow changes.
- Do not commit generated build output.
- Review `git diff` and `git status` before committing or opening a pull
  request.
- Mention unverified areas or known limitations in the pull request `Notes`.

## Dependency Policy

Dependency change policy:

- Dependencies are managed through Conan 2 in `conanfile.py`.
- Do not add or upgrade dependencies without confirming the reason and impact
  first.
- After changing dependencies, run `conan install`, configure, build, and the
  relevant tests.
- Keep dependencies minimal.
- Prefer optional integration when a feature is optional.
- When adding a dependency in `conanfile.py`, add its CMake finder under
  `cmake_finders/`.
- CMake finders should come from
  `https://github.com/wisherhxl/tiger_finder.git`.
- If the required finder is not available there, do not create one locally.
  Ask the `tiger_finder` repository maintainer to create it.

Protobuf policy:

- The template should fully support protobuf-based projects.
- Protobuf usage is optional; projects without `.proto` files must remain
  supported.

## Documentation Policy

- `README.md` should serve project users.
- `AGENTS.md` should serve contributors and coding agents.
- Build instructions should be tested before being presented as the main path.
- Platform-specific notes should identify the affected platform explicitly.
- Update `README.md` when a change affects prerequisites, configure/build/test/
  install commands, module/app/proto creation, template consumption, public
  behavior, or examples.
- Update `AGENTS.md` when a change affects contribution workflow, branch,
  commit, pull request, or review rules, coding style, layout policy,
  verification expectations, dependency management, or instructions agents must
  follow while editing the repository.
- If a change affects both users and contributors, update both files.
