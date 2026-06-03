# AGENTS.md

High-signal guidance for AI coding agents and human contributors working in
this repository. Keep this file short; move detailed procedures into `docs/`.

## Project Overview

Tiger is a C++ project template built around CMake and Conan 2. It supports
Linux, Windows, and macOS.

Supported compiler families:

- MSVC 2019 or newer.
- GCC 9.1 or newer.
- Clang/LLVM 8.0 or newer.

For now, treat macOS compiler support under the Clang/LLVM policy instead of
maintaining a separate AppleClang minimum version.

Current project facts:

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
- `modules/`: reusable project modules.
- `protos/`: protobuf definitions.
- `cmake/`: shared CMake logic, templates, checks, and packaging helpers.
- `cmake_finders/`: project CMake finder modules for Conan dependencies.
- `test_package/`: Conan package verification project.
- `CMakeLists.txt`: root CMake entry point.
- `conanfile.py`: Conan recipe for configuring, building, and packaging.
- `VERSION`: project version source.

See [docs/architecture.md](docs/architecture.md) for detailed structure and
design rationale.

## Must-Follow Rules

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
- For non-trivial implementation, architecture, build, dependency, workflow, or
  design-rule changes, discuss the approach and get an approved plan before
  editing.
- Do not push directly to `main` or `master` unless explicitly requested.
- In interactive collaboration, do not commit, push, or create pull requests
  automatically.
- If the user explicitly asks to commit, push, or create a pull request, do it
  without asking for another confirmation.

See [docs/agent-workflow.md](docs/agent-workflow.md) for branch, commit, pull
request, review, and documentation workflow details.

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
- If the inferred rule seems important, recommend adding it to
  `docs/project-design-rules.md` or another focused project doc.

Read [docs/project-design-rules.md](docs/project-design-rules.md) for the
canonical project design rules.

## Coding Rules

- Use C++17 for new modules.
- Use Google C++ Style Guide for C++ coding style and naming conventions.
- Use the repository `.clang-format` for C and C++ formatting.
- Keep generated files out of source edits unless the task explicitly requires
  regenerating them.
- Do not edit generated files directly; update templates, CMake logic, or
  source inputs instead.

See [docs/coding-style.md](docs/coding-style.md) for naming, formatting,
generated-file, module, app, and proto layout rules.

## Build Commands

Known prerequisites:

- CMake.
- Conan 2.
- A supported C/C++ compiler available in the current shell.

Linux Debug quick path:

```sh
conan install . --build=missing
. build/Debug/generators/conanbuild.sh
. build/Debug/generators/conanrun.sh
cmake --preset conan-debug
cmake --build --preset conan-debug
```

Windows Debug quick path:

```sh
conan install . --build=missing
build\generators\conanbuild.bat
build\generators\conanrun.bat
cmake --preset conan-default
cmake --build --preset conan-debug
```

See [docs/build.md](docs/build.md) for Release workflows, install commands,
Conan/CMake presets, dependency policy, Qt linkage, runtime dependency
packaging, and troubleshooting.

## Test And Verification

Official local test workflow:

```sh
ctest --preset conan-debug
ctest --preset conan-release
```

Before considering a change complete, run the smallest relevant verification
command documented by the project. Prefer Release tests unless the change is
Debug-specific or Release cannot be built locally. If verification is skipped,
mention why in the final response or pull request notes.

See [docs/testing.md](docs/testing.md) for full verification workflows, test
strategy, QTest policy, and pre-PR verification expectations.

## Documentation

- `README.md` should serve project users.
- `AGENTS.md` should stay a short entry point for rules agents must always
  know before editing the repository.
- Detailed contributor workflow belongs in
  [docs/agent-workflow.md](docs/agent-workflow.md).
- Detailed build guidance belongs in [docs/build.md](docs/build.md).
- Detailed testing guidance belongs in [docs/testing.md](docs/testing.md).
- Detailed coding and layout guidance belongs in
  [docs/coding-style.md](docs/coding-style.md).
- Project design rules belong in
  [docs/project-design-rules.md](docs/project-design-rules.md).
- Architecture and design rationale belong in
  [docs/architecture.md](docs/architecture.md).
