# Protobuf Files

This folder contains `.proto` files used to generate C++ protobuf sources during
CMake configure.

## Add a Proto File

Put proto files under the project internal-name folder:

```text
protos/<project_internal_name>/<group>/<name>.proto
```

Example:

```text
protos/<project_internal_name>/common/message.proto
protos/<project_internal_name>/service/request.proto
```

Then run CMake configure. The build system uses `protos/` as the protobuf
import root, but only collects `.proto` files from `protos/<project_internal_name>/`
for generation.

## Generated Output

Generated files keep the same directory layout as the source `.proto` files.

For this input:

```text
protos/<project_internal_name>/common/message.proto
```

the generated files are:

```text
modules/<protobuf_module>/include/<project_internal_name>/common/message.pb.h
modules/<protobuf_module>/src/<project_internal_name>/common/message.pb.cc
```

`<protobuf_module>` is the module selected for generated protobuf code. In this
template it is usually `proto`.

## Directory Strategy

The generator preserves paths relative to `protos/`. Because only files under
`protos/<project_internal_name>/` are collected, place project-owned proto files
inside that folder.

Prefer this:

```text
protos/<project_internal_name>/common/message.proto
protos/<project_internal_name>/service/request.proto
```

Avoid this:

```text
protos/message.proto
protos/request.proto
```

Grouping files under the project internal name keeps generated headers under a
matching include path and avoids collisions with other proto groups.

## Imports

Use import paths relative to `protos/`.

Example:

```proto
import "<project_internal_name>/common/message.proto";
```

This matches the recommended folder layout and keeps imports stable when files
are generated into the protobuf module.

## Agent Checklist

When adding proto files:

1. Create files under `protos/<project_internal_name>/`.
2. Group related files into subfolders such as `common`, `service`, or `model`.
3. Use imports relative to `protos/`.
4. Do not edit generated `.pb.h` or `.pb.cc` files by hand.
5. Run CMake configure to regenerate protobuf output.
6. Include generated headers using the same path as the proto layout, for example `#include "<project_internal_name>/common/message.pb.h"`.
