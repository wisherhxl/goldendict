# Build

This document contains Tiger's build environment, dependency, CMake, Conan,
install, and packaging guidance.

## Prerequisites

Known prerequisites from the repository:

- CMake.
- Conan 2.
- A supported C/C++ compiler available in the current shell.

On Windows, use a Visual Studio Developer PowerShell or another shell where
MSVC is available to CMake.

After `conan install`, activate the generated Conan build and runtime
environment scripts before configuring so CMake and Conan-provided tools use
the expected paths and runtime libraries.

## Windows Debug Build

```sh
conan install . --build=missing
build\generators\conanbuild.bat
build\generators\conanrun.bat
cmake --preset conan-default
cmake --build --preset conan-debug
```

## Windows Release Build

```sh
conan install . --build=missing -s build_type=Release
build\generators\conanbuild.bat
build\generators\conanrun.bat
cmake --preset conan-default
cmake --build --preset conan-release
```

## Linux Debug Build

```sh
conan install . --build=missing
. build/Debug/generators/conanbuild.sh
. build/Debug/generators/conanrun.sh
cmake --preset conan-debug
cmake --build --preset conan-debug
```

## Linux Release Build

```sh
conan install . --build=missing -s build_type=Release
. build/Release/generators/conanbuild.sh
. build/Release/generators/conanrun.sh
cmake --preset conan-release
cmake --build --preset conan-release
```

## Install

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

Use `-o '&:install_mode=runtime'` when preparing deployable install/package
output that should contain only runtime files. Runtime mode installs the project
executables, runtime shared libraries/plugins/resources needed by those
executables, and package metadata such as the license/readme. It skips
development files such as headers, static/import archives, and CMake package
exports/configs.

Use `-o '&:install_mode=library'` for the SDK/library install layout. Library
mode installs runtime files plus development files such as headers, libraries,
and CMake package metadata.

`install_runtime_dependencies` controls whether third-party runtime shared
libraries/plugins are copied into the install prefix. Its default is `auto`:

- `install_mode=library` + `install_runtime_dependencies=auto` resolves to
  runtime dependencies off.
- `install_mode=runtime` + `install_runtime_dependencies=auto` resolves to
  runtime dependencies on.

Users can still force the dependency deployment behavior with
`-o '&:install_runtime_dependencies=True'` or
`-o '&:install_runtime_dependencies=False'`.

## CMake Presets

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

## Qt And Linkage

Keep dependency linkage profile-owned. Do not hardcode Qt shared/static policy
in `conanfile.py`; use Conan profiles or CLI options such as
`-o "qt/*:shared=True"` or `-o "qt/*:shared=False"`.

`ti_add_qt_app` owns normal Qt app deployment/import behavior: shared Qt uses
runtime deployment, while static Qt links the selected platform plugin and
generates the required `Q_IMPORT_PLUGIN(...)` source.

Static platform plugin options are advanced overrides:
`qt_linux_platform_plugin` and `qt_windows_platform_plugin`. The selected
plugin must be exported as a CMake target by the resolved Qt package. For
example, Linux `auto` selects `xcb`, which Conan's Qt package commonly exports
as `Qt5::QXcbIntegrationPlugin`. `minimal` is useful for headless or diagnostic
Qt tools, but only works when the Qt package exports the corresponding static
plugin target. Use `off` only when the app handles plugin imports itself.

Conan profiles or command-line options own dependency shared/static linkage.
Do not hardcode dependency linkage in `conanfile.py` unless the project requires
one mode. Runtime deployment logic must support shared, static, and optional
dependencies without requiring per-app workarounds.

Library linkage policy:

- Support both shared and static library builds.
- Use shared libraries as the default and primary path.
- Treat project linkage and dependency linkage as separate policies. The
  project `shared` option controls Tiger targets; dependency options such as
  `qt/*:shared` belong in Conan profiles or command-line overrides.

## Dependency Policy

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

## Packaging

Packaging through CMake/CPack is under development. Do not treat package output
as stable until the package workflow is verified and documented.
