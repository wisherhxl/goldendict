# Architecture

This document describes Tiger's repository structure, project layout rules, and
design rationale. `AGENTS.md` stays short and links here for details.

## Project Purpose

Tiger is a C++ project template built around CMake and Conan 2. It is intended
to provide a reusable starting point for cross-platform C++ projects on Linux,
Windows, and macOS.

The template includes conventions for modules, applications, protobuf
generation, Conan dependency integration, installation layout, and generated
project configuration files.

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
- `protos/`: protobuf definitions.
- `cmake/`: shared CMake logic, templates, checks, and packaging helpers.
- `cmake_finders/`: project CMake finder modules for Conan dependencies.
- `test_package/`: Conan package verification project.
- `CMakeLists.txt`: root CMake entry point.
- `conanfile.py`: Conan recipe for configuring, building, and packaging.
- `VERSION`: project version source.

See [coding-style.md](coding-style.md) for concrete module, application,
proto, and generated-file layout rules.

## Module Design

Tiger separates reusable code, generated protobuf code, and runnable
applications. Reusable project behavior belongs in `modules/`; applications in
`apps/` should consume modules instead of becoming shared infrastructure
themselves. Protobuf definitions live under `protos/` so generated code can be
managed consistently by the CMake workflow.

## Protobuf Support

- The template should fully support protobuf-based projects.
- Protobuf usage is optional; projects without `.proto` files must remain
  supported.

See [project-design-rules.md](project-design-rules.md) for project design rules
and design-boundary rationale.
