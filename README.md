# Tiger Template

Tiger is a C++ project template built around CMake and Conan 2. It is intended
to provide a reusable starting point for cross-platform C++ projects on Linux,
Windows, and macOS.

The template includes conventions for modules, applications, protobuf
generation, Conan dependency integration, installation layout, and generated
project configuration files.

## Supported Toolchains

Supported operating systems:

- Linux
- Windows
- macOS

Supported compiler families:

- MSVC 2019 or newer
- GCC 9.1 or newer
- Clang/LLVM 8.0 or newer

macOS compiler support is currently treated under the Clang/LLVM policy.

## Prerequisites

- CMake
- Conan 2
- A supported C/C++ compiler available in the current shell

On Windows, run the commands from a Visual Studio Developer PowerShell or
another shell where MSVC is available to CMake.

## Build

Install dependencies first. Conan generates the CMake user presets used by the
configure and build steps. After `conan install`, activate the generated Conan
build and runtime environment scripts before configuring so CMake and
Conan-provided tools use the expected paths and runtime libraries.

For interactive development, the repository includes convenience starters that
prepare the Conan environment and open the project in VS Code. They require the
VS Code `code` command to be available in `PATH`.

Windows:

```sh
start_code.bat
start_code.bat release
start_code.bat r
```

Linux:

```sh
./start_code.sh
./start_code.sh release
./start_code.sh r
```

Without arguments, the starters use Debug. Pass `debug`, `dbg`, or `d` for
Debug, and `release`, `rel`, or `r` for Release. The platform-specific
implementations live under `scripts/windows/` and `scripts/linux/`.

Windows Debug build:

```sh
conan install . --build=missing
build\generators\conanbuild.bat
build\generators\conanrun.bat
cmake --preset conan-default
cmake --build --preset conan-debug
```

Windows Release build:

```sh
conan install . --build=missing -s build_type=Release
build\generators\conanbuild.bat
build\generators\conanrun.bat
cmake --preset conan-default
cmake --build --preset conan-release
```

Linux Debug build:

```sh
conan install . --build=missing
. build/Debug/generators/conanbuild.sh
. build/Debug/generators/conanrun.sh
cmake --preset conan-debug
cmake --build --preset conan-debug
```

Linux Release build:

```sh
conan install . --build=missing -s build_type=Release
. build/Release/generators/conanbuild.sh
. build/Release/generators/conanrun.sh
cmake --preset conan-release
cmake --build --preset conan-release
```

Use `cmake --list-presets` after `conan install` to confirm the generated
preset names for the current platform and profile.

Qt linkage is selected by Conan profiles or CLI options, not hardcoded by the
template:

```sh
conan install . --build=missing -o "qt/*:shared=True"
conan install . --build=missing -o "qt/*:shared=False"
```

`ti_add_qt_app` handles the matching Qt runtime behavior. Shared Qt builds use
the platform deployment/install logic; static Qt builds generate the required
platform plugin import for normal Qt apps. Advanced users can override the
static platform plugin with `-o "&:qt_linux_platform_plugin=minimal"` or disable
template-managed imports with `-o "&:qt_linux_platform_plugin=off"`.

With the default Windows profiles, Conan generates:

- configure preset: `conan-default`
- Debug build preset: `conan-debug`
- Release build preset: `conan-release`
- Debug test preset: `conan-debug`
- Release test preset: `conan-release`

With the default Linux profiles, Conan generates:

- Debug configure preset: `conan-debug`
- Release configure preset: `conan-release`
- Debug build preset: `conan-debug`
- Release build preset: `conan-release`
- Debug test preset: `conan-debug`
- Release test preset: `conan-release`

## Test

Run the test preset that matches the build configuration:

```sh
ctest --preset conan-debug
ctest --preset conan-release
```

Tests are built by default. Disable them explicitly with `-DBUILD_TESTS=OFF`
when configuring if a project does not need local test targets.

## Install

Install the Windows Release build:

```sh
cmake --install build --config Release
```

The default Windows local install destination is:

```text
build/install
```

Install the Linux Release build:

```sh
cmake --install build/Release
```

The default Linux local install destination is:

```text
build/Release/install
```

### Deployable Runtime Dependencies

By default, install copies this project's build outputs only. For deployable
install output that also includes runtime shared-library dependencies needed by
the project binaries, enable the Conan option.

Windows Release deployable install:

```sh
conan install . --build=missing -s build_type=Release -o "&:install_runtime_dependencies=True"
build\generators\conanbuild.bat
build\generators\conanrun.bat
cmake --fresh --preset conan-default
cmake --build --preset conan-release
cmake --install build --config Release
```

Linux Release deployable install:

```sh
conan install . --build=missing -s build_type=Release -o '&:install_runtime_dependencies=True'
. build/Release/generators/conanbuild.sh
. build/Release/generators/conanrun.sh
cmake --fresh --preset conan-release
cmake --build --preset conan-release
cmake --install build/Release
```

This sets `TIGER_INSTALL_RUNTIME_DEPENDENCIES=ON` in the generated CMake
toolchain, allowing install rules to copy runtime dependencies such as
Qt/protobuf shared libraries and Qt plugins when applicable.

Choose shared or static third-party dependencies in your Conan profile or with
Conan command-line options; deployable installs handle the selected linkage
mode.

## Package

Packages are produced through CMake/CPack from the same install rules used by
`cmake --install`. The default binary package generator is `TGZ` on Linux and
`ZIP` on Windows.

```sh
conan install . --build=missing -s build_type=Release
. build/Release/generators/conanbuild.sh
. build/Release/generators/conanrun.sh
cmake --fresh --preset conan-release
cmake --build --preset conan-release
cmake --build --preset conan-release --target package
```

The generated archive is written to the Release build directory. Package names
include the active install mode, for example
`build/Release/${TI_INTERNAL_NAME}-2.3.3-linux-x86_64-library.tar.gz` or
`build/Release/${TI_INTERNAL_NAME}-2.3.3-linux-x86_64-runtime.tar.gz`.

Linux `TGZ`, Windows `ZIP`, and explicit Linux `DEB` packages are currently
verified. See [docs/build.md](docs/build.md) for details.

## Project Layout

- `apps/`: application targets.
- `apps/<app_name>/resources/`: optional runtime resources copied beside the
  app binary after build.
- `modules/`: reusable C++ modules.
- `protos/`: protobuf definitions.
- `cmake/`: shared Tiger CMake logic, templates, checks, and packaging helpers.
- `cmake_finders/`: project CMake finder modules for Conan dependencies.
- `test_package/`: Conan package verification project.
- `CMakeLists.txt`: root project configuration.
- `conanfile.py`: Conan recipe.
- `VERSION`: project version source.
- `AGENTS.md`: contributor and coding-agent workflow rules.

## Add Modules, Apps, And Protos

Detailed guides live next to each project area:

- [apps/README.md](apps/README.md)
- [modules/README.md](modules/README.md)
- [protos/README.md](protos/README.md)

General layout rules:

- A module named `foo` lives under `modules/foo/`.
- Public headers for module `foo` live under
  `modules/foo/include/<project_internal_name>/foo/`.
- Implementation files for module `foo` live under `modules/foo/src/`.
- Applications live under `apps/<app_name>/`.
- Project-owned proto files live under `protos/<project_internal_name>/`.

## Happy Path Example

The current happy path example is `apps/z_example/`.

It demonstrates:

- a Qt application target;
- linking Tiger modules with `MODULES tiger proto`;
- linking external dependencies with `EXTRAS ${qt_link}`;
- protobuf generation from `protos/tiger/test.proto`;
- generated protobuf headers included as `tiger/test.pb.h`;
- QTest coverage for the generated protobuf message;
- Qt translation source handling through `apps/z_example/lang/zh_cn.ts`.

Qt translation release targets require Qt LinguistTools. The example still
builds when LinguistTools are unavailable, but translation files are not
generated in that case.

Build the Release workflow, then run the generated `z_example` executable from
the build output directory for your platform and generator.

## Dependencies

Dependencies are managed with Conan 2 in `conanfile.py`.

Dependencies that appear in installed public headers or exported CMake target
usage requirements must be declared as public Conan requirements in the recipe.
For example, the generated protobuf module installs `.pb.h` headers and exports
targets linked to Protobuf, so the recipe marks Protobuf as public. This lets a
consumer that only requires the `${TI_INTERNAL_NAME}` package get the needed
Conan-generated dependency config files automatically.

Public external CMake targets also need package-find metadata so Tiger's
installed config can call `find_dependency(...)` before importing exported
targets. Register that metadata in the dependency finder or project CMake code
with `ti_register_external_dependency(...)`, or via `ti_add_thirdparty_status`
using its `PACKAGE`, `CONFIG`, and `COMPONENTS` arguments.

When adding a Conan dependency, add the matching CMake finder under
`cmake_finders/`. Finder modules should come from:

```text
https://github.com/wisherhxl/tiger_finder.git
```

If a required finder does not exist there, request it from the `tiger_finder`
maintainer instead of creating a local replacement.

## Generated Files

The CMake workflow generates project configuration files, base module files, and
protobuf C++ output when needed. Do not edit generated files directly. Update
the source templates, CMake logic, or source inputs instead.

Generated source files use fixed suffixes:

- `*.tp.h` and `*.tp.cc` for Tiger platform-generated files.
- `*.pb.h` and `*.pb.cc` for protobuf-generated files.

The generated-file cleanup logic relies on these suffixes.

## Contributor Workflow

See [AGENTS.md](AGENTS.md) for branch, commit, pull request, verification, and
coding-agent rules.
