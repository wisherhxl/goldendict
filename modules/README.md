# Tiger Modules

This folder contains Tiger C++ modules. Each first-level subdirectory with a
`CMakeLists.txt` is scanned as one module.

## Add a Module

Create this layout:

```text
modules/<module_name>/
  CMakeLists.txt
  include/<project_include_namespace>/<public headers>
  src/<source files>
```

Minimal `CMakeLists.txt`:

```cmake
set(the_description "short module description")

ti_define_module(<module_name>)
```

With dependencies:

```cmake
set(the_description "generated protobuf module")

ti_define_module(proto
  MODULES <base_module>
  EXTRAS ${protobuf_link}
)
```

## `ti_define_module`

```cmake
ti_define_module(<module_name>
  [INTERNAL]
  [EXCLUDE_CUDA]
  [[REQUIRED|OPTIONAL] [PUBLIC|PRIVATE] MODULES <Tiger modules>]
  [[REQUIRED|OPTIONAL] [PUBLIC|PRIVATE] EXTRAS <external dependencies>]
  [OPTIONAL_MODULES <optional Tiger modules>]
  [OPTIONAL_EXTRAS <optional external dependencies>]
  [WRAP <wrapper names>]
)
```

`REQUIRED` and `PUBLIC` are the defaults. Use short module names in `MODULES`;
the build system adds the project namespace prefix internally:

```cmake
ti_define_module(my_feature MODULES base proto)
```

Use `EXTRAS` for external CMake targets, libraries, or paths:

```cmake
ti_define_module(my_feature
  MODULES base
  EXTRAS Boost::boost SomeVendor::sdk
)
```

Use `PRIVATE` for dependencies needed only to build the module, and `PUBLIC`
for dependencies that appear in installed headers or exported target usage
requirements:

```cmake
ti_define_module(my_feature
  MODULES base
  PUBLIC EXTRAS fmt::fmt
  PRIVATE EXTRAS SomeVendor::implementation
  OPTIONAL PRIVATE MODULES debug_tools
)
```

`OPTIONAL_MODULES` and `OPTIONAL_EXTRAS` are kept as backward-compatible
spellings for `OPTIONAL PUBLIC MODULES` and `OPTIONAL PUBLIC EXTRAS`.

`WRAP` lists wrappers or binding modules that should expose the current module.
It does not make the current module depend on the wrapper.

## Agent Checklist

When adding a new module:

1. Create `modules/<module_name>/CMakeLists.txt`.
2. Put public headers under `modules/<module_name>/include/<project_include_namespace>/`.
3. Put implementation files under `modules/<module_name>/src/`.
4. Call `ti_define_module(<module_name> ...)`.
5. Put Tiger module dependencies in `MODULES` using short names.
6. Put third-party dependencies in `EXTRAS`.
7. Mark dependencies `PRIVATE` unless they are part of installed public headers
   or exported target usage requirements.
