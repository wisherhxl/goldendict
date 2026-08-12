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
conan export conan/recipes/python-html5lib
conan install . --build=missing \
  -pr:h=profiles/qt-webengine -pr:b=default
. build/Debug/generators/conanbuild.sh
. build/Debug/generators/conanrun.sh
cmake --preset conan-debug
cmake --build --preset conan-debug
```

## Linux Release Build

```sh
conan export conan/recipes/python-html5lib
conan install . --build=missing -s build_type=Release \
  -pr:h=profiles/qt-webengine -pr:b=default
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
exports/configs. In `cmd.exe`, quote scoped Conan options with double quotes,
for example `-o "&:install_mode=runtime"`.

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

GoldenDict requires Conan's `qt/6.11.1` package with the `qtwebengine` option
and its required `qtdeclarative`, `qtshadertools`, and `qtwebchannel` features.
Linux WebEngine also requires Qt's `with_dbus` option. The application links Qt
Core, Gui, Widgets, Network, WebChannel, WebEngineCore, and WebEngineWidgets
through `TigerFindQt.cmake`.

Qt WebEngine's source configure requires the Python `html5lib` module, but
Conan Center does not currently provide it. Export the focused local recipe at
`conan/recipes/python-html5lib/` before dependency resolution and use
`profiles/qt-webengine`. The profile adds that package as a tool requirement
only for `qt/*`, placing `html5lib` and its Python dependencies on `PYTHONPATH`
inside Qt's isolated Conan build environment. This keeps the prerequisite
Conan-resolved without modifying the official Qt recipe or the host Python
installation. The profile also sets `NINJAFLAGS=-j1` because Qt WebEngine
launches a nested Chromium Ninja build that does not inherit Conan's outer
`tools.build:jobs` limit. Keep this limit on memory-constrained Linux builders;
raising it can make concurrent Chromium compiler processes exhaust RAM and
swap.

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
as `Qt6::QXcbIntegrationPlugin`. `minimal` is useful for headless or diagnostic
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
  project `shared` option controls GoldenDict targets; dependency options such as
  `qt/*:shared` belong in Conan profiles or command-line overrides.

## Dependency Policy

- Dependencies are managed through Conan 2 in `conanfile.py`.
- Do not add or upgrade dependencies without confirming the reason and impact
  first.
- If an installed public header or exported CMake target usage requirement
  exposes a Conan dependency, declare it with the recipe's `_public_requires()`
  helper. Public requirements use Conan's transitive header and library traits
  so consumers that require only the `goldendict` package still
  receive the needed dependency CMake config files.
- Module declarations own dependency visibility. In `ti_define_module(...)`,
  mark build-only dependencies with `PRIVATE`; dependencies are `PUBLIC` by
  default and must stay public when they appear in installed headers or
  exported target usage requirements.
- If an exported public CMake target links an external target, register the
  external target's package-find metadata in the dependency finder or project
  CMake code with `ti_register_external_dependency(...)`, or use
  `ti_add_thirdparty_status(... PACKAGE ... CONFIG ... COMPONENTS ...)`. This
  lets Tiger's installed config generate `find_dependency(...)` calls without
  hardcoding third-party package names in config generation.
- Use plain `self.requires()` for dependencies that are only needed to build or
  run GoldenDict itself and are not part of installed library usage requirements.
- After changing dependencies, run `conan install`, configure, build, and the
  relevant tests.
- Keep dependencies minimal.
- Prefer optional integration when a feature is optional.
- Prefer a Conan CMakeDeps package config when it exports the required CMake
  targets. No project finder is needed in that case.
- When a dependency does not provide a usable package config, add its CMake
  finder under `cmake_finders/`. CMake finders should come from
  `https://github.com/wisherhxl/tiger_finder.git`. If the required finder is
  not available there, do not create one locally; ask the `tiger_finder`
  repository maintainer to create it.

ICU 74.2 is a direct private `goldendict_core` implementation dependency for
Unicode lookup folding and strict legacy text-encoding conversion. Conan
already resolves the same ICU revision through Qt, so the explicit requirement
does not add a second runtime implementation. Its CMakeDeps config exports
`ICU::uc`; ICU types and linkage do not appear in the installed public headers
or exported target usage requirements.

bzip2 1.0.8 is a direct private `goldendict_core` implementation dependency
for SDict's legacy per-field compression mode. It is declared explicitly even
though Qt's graph may already contain bzip2 transitively. The Conan CMakeDeps
config exports `BZip2::BZip2`; bzip2 types and linkage do not appear in public
headers or exported target usage requirements. bzip2 uses its permissive
BSD-style license and is covered by the package dependency/license inventory.

Expat 2.7.5 is a direct private `goldendict_core` implementation dependency for
bounded XDXF stream parsing on every supported platform. Conan already resolves
the same Expat revision through Qt, so the explicit requirement does not add a
second runtime implementation. Its CMakeDeps config exports `expat::expat`;
Expat types and linkage do not appear in installed public headers or exported
target usage requirements. Expat uses the MIT license and is covered by the
package dependency/license inventory.

Vorbis 1.3.7 (and its Ogg dependency) is a direct private `goldendict_core`
implementation dependency for bounded LSA sample-range decoding. The private
audio component owns the codec calls and returns transport-neutral WAV bytes;
Vorbis/Ogg types and linkage do not appear in installed public headers or
exported target usage requirements. Both libraries use BSD-style licenses and
are covered by the package dependency/license inventory.

## Packaging

Packages are produced through CMake/CPack from the current CMake install rules.
CPack does not define a separate layout; it packages the same files that
`cmake --install` would install for the active configuration and install mode.

The default binary package generator is platform-specific:

- Linux: `TGZ`;
- Windows: `ZIP`.

Linux distro packages are selected explicitly with CPack, not by changing the
default package target:

```sh
cpack -G DEB --config build/Release/CPackConfig.cmake
cpack -G RPM --config build/Release/CPackConfig.cmake
```

`DEB` and `RPM` packages install under `/opt/goldendict` and use `goldendict`
as the package name so package-manager upgrades replace
the previous version in place.

Official Linux Release package workflow:

```sh
conan export conan/recipes/python-html5lib
conan install . --build=missing -s build_type=Release \
  -pr:h=profiles/qt-webengine -pr:b=default
. build/Release/generators/conanbuild.sh
. build/Release/generators/conanrun.sh
cmake --fresh --preset conan-release
cmake --build --preset conan-release
cmake --build --preset conan-release --target package
```

The package is written to the Release build directory with a name like:

```text
build/Release/goldendict-1.6.0-linux-x86_64-library.tar.gz
```

Official Windows Release package workflow:

```sh
conan install . --build=missing -s build_type=Release
build\generators\conanbuild.bat
build\generators\conanrun.bat
cmake --fresh --preset conan-default
cmake --build --preset conan-release
cmake --build --preset conan-release --target package
```

The package is written to the build directory with a name like:

```text
build/goldendict-1.6.0-windows-amd64-library.zip
```

Runtime-mode packages use the same platform and generator naming with a
`runtime` suffix, for example:

```text
build/goldendict-1.6.0-windows-amd64-runtime.zip
```

The archive uses Tiger's Conan install layout, so the installed GoldenDict
CMake package config lives under `lib/cmake` inside the extracted Linux
archive. Point consumers at that directory, along with dependency package
config paths, when using `find_package(GoldenDict CONFIG REQUIRED)`:

```sh
cmake -S <consumer-source> -B <consumer-build> \
  -DCMAKE_PREFIX_PATH="<extract-root>/goldendict-1.6.0-linux-x86_64-library/lib/cmake;<dependency-prefixes>"
```

The package target is enabled by default. Disable it with
`-DTIGER_ENABLE_CPACK=OFF` when configuring.

Linux `TGZ`, Windows `ZIP`, and Linux `DEB` package output are currently
verified. `RPM` package metadata is configured, but full RPM install and upgrade
testing should run in an RPM-native environment.

If CMake reports duplicate presets after previous package or test-package work,
inspect the ignored generated `CMakeUserPresets.json`. It may include stale
Conan preset files from another build tree that define the same preset names as
the active root build.
