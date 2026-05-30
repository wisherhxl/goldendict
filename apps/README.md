# Tiger Applications

This folder contains executable applications. Each first-level subdirectory with
a `CMakeLists.txt` is scanned as one app.

There are two app macros:

```cmake
ti_add_app(<app_name> ...)
ti_add_qt_app(<app_name> ...)
```

Use `ti_add_app` for a normal console or non-Qt executable. Use
`ti_add_qt_app` for a Qt GUI executable.

## Add a Non-Qt App

Create this layout:

```text
apps/<app_name>/
  CMakeLists.txt
  src/main.cpp
```

Minimal `CMakeLists.txt`:

```cmake
ti_add_app(my_tool
  MODULES base proto
)
```

With external dependencies:

```cmake
ti_add_app(my_tool
  MODULES base proto
  EXTRAS Boost::boost SomeVendor::sdk
)
```

## Add a Qt App

Create this layout:

```text
apps/<app_name>/
  CMakeLists.txt
  src/main.cpp
  src/<optional .ui files>
  resources/<optional files>
  resources/res.qrc
```

Example `CMakeLists.txt`:

```cmake
ti_add_qt_app(z_example
  MODULES base proto
  EXTRAS ${qt_link}
)
```

`ti_add_qt_app` enables Qt-specific handling, including `.ui` files,
translations, optional `resources/res.qrc`, and a Windows GUI executable target.

## App Dependencies

Use short Tiger module names in `MODULES`:

```cmake
MODULES base proto
```

Use module names exactly as they appear under `modules/`.

Use `EXTRAS` for external CMake targets or libraries:

```cmake
EXTRAS ${qt_link} Boost::boost
```

## Agent Checklist

When adding a new app:

1. Create `apps/<app_name>/CMakeLists.txt`.
2. Put source files under `apps/<app_name>/src/`.
3. Use `ti_add_app` for non-Qt apps.
4. Use `ti_add_qt_app` for Qt apps.
5. Put Tiger module dependencies in `MODULES` using short names.
6. Put third-party dependencies in `EXTRAS`.
7. Add optional runtime resources under `resources/`; they are copied beside the executable.
