# Coding Style

This document contains Tiger's coding, formatting, naming, and code
organization rules.

## C++ Style

- Use C++17 for new modules.
- Use Google C++ Style Guide for C++ coding style and naming conventions.
- Use the repository `.clang-format` for C and C++ formatting.

## Generated Files

- Keep generated files out of source edits unless the task explicitly requires
  regenerating them.
- Generated files and directories are managed by the CMake workflow. Do not edit
  generated files directly; update templates, CMake logic, or source inputs
  instead.
- Generated source files must use fixed suffixes: `*.tp.h` and `*.tp.cc` for
  Tiger platform-generated files, and `*.pb.h` and `*.pb.cc` for
  protobuf-generated files.
- Generated-file cleanup logic relies on those suffixes.

## Module Layout

- A module named `foo` lives under `modules/foo/`.
- Public headers for module `foo` live under
  `modules/foo/include/<TI_INTERNAL_NAME>/foo/`.
- Implementation files for module `foo` live under `modules/foo/src/`.

## Proto Layout

- Proto files live under `protos/<TI_INTERNAL_NAME>/`.

## Application Layout

- An application named `app_name` lives under `apps/app_name/`.
- `apps/<app_name>/resources/` is the optional application resource directory
  for runtime files such as configs, images, and other assets. `TigerApp.cmake`
  copies its contents to the app binary directory after build when resource
  copying is enabled.
