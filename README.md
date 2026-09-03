# GoldenDict

GoldenDict is a feature-rich dictionary lookup application. This worktree is
migrating the legacy product to Qt 6 while adopting Tiger's CMake, Conan,
module, test, install, and package structure.

Phase 2 provides the GoldenDict 1.6.0 project/package identity and a minimal
Qt 6 application window using Qt WebEngine. Legacy dictionary functionality is
not yet present. The exact source baselines and ownership boundary are recorded
in [docs/migration.md](docs/migration.md).

## Prerequisites

- Conan 2
- A supported C/C++ compiler

CMake and all application dependencies, including Qt 6.11.1 and Qt WebEngine,
are resolved by Conan. On Linux, Conan Center's approved system wrappers may
report host package requirements for Xorg, OpenGL, and keyboard configuration.

## Linux Release Build

```sh
conan export conan/recipes/python-html5lib
conan install . --build=missing -s build_type=Release \
  -pr:h=profiles/qt-webengine -pr:b=default
. build/Release/generators/conanbuild.sh
. build/Release/generators/conanrun.sh
cmake --fresh --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release --output-on-failure
```

Run the build-tree application through the repository launcher so it inherits
the matching Conan runtime environment:

```sh
./run_with_conan.sh --build-type Release -- \
  build/Release/bin/goldendict
```

On Windows, use `run_with_conan.ps1` from PowerShell or `run_with_conan.bat`
from `cmd.exe` with the same arguments. A runtime-mode install or package is
the supported environment-independent launch path.

## Install And Package

```sh
cmake --install build/Release
cmake --build --preset conan-release --target package
```

The default Linux install prefix is `build/Release/install`. See
[docs/build.md](docs/build.md) for linkage options, runtime installs, and
packaging details.

## Repository Layout

- `apps/goldendict/`: GoldenDict application target.
- `modules/`: reusable project modules.
- `cmake/`: reusable Tiger CMake infrastructure.
- `cmake_finders/`: dependency discovery integration.
- `docs/`: architecture, build, testing, workflow, and migration guidance.

## License

The distributed GoldenDict application is licensed under GPL-3.0-or-later.
Reusable Tiger infrastructure retains its MIT license. See
[LICENSES/README.md](LICENSES/README.md) for the component boundary and third-
party inventory policy.
