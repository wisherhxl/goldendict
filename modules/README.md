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
  [MODULES <required Tiger modules>]
  [EXTRAS <required external dependencies>]
  [OPTIONAL_MODULES <optional Tiger modules>]
  [OPTIONAL_EXTRAS <optional external dependencies>]
  [WRAP <wrapper names>]
)
```

Use short module names in `MODULES` and `OPTIONAL_MODULES`:

```cmake
ti_define_module(my_feature MODULES base proto)
```

Use `EXTRAS` and `OPTIONAL_EXTRAS` for external CMake targets, libraries, or
paths:

```cmake
ti_define_module(my_feature
  MODULES base
  EXTRAS Boost::boost SomeVendor::sdk
)
```

`WRAP` lists wrappers or binding modules that should expose the current module.
It does not make the current module depend on the wrapper.

## Agent Checklist

When adding a new module:

1. Create `modules/<module_name>/CMakeLists.txt`.
2. Put public headers under `modules/<module_name>/include/<project_include_namespace>/`.
3. Put implementation files under `modules/<module_name>/src/`.
4. Call `ti_define_module(<module_name> ...)`.
5. Put Tiger module dependencies in `MODULES` or `OPTIONAL_MODULES` using short names.
6. Put third-party dependencies in `EXTRAS` or `OPTIONAL_EXTRAS`.
