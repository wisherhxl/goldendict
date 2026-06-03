# Project Design Rules

This document is the canonical place for Tiger's explicit project design rules.
Agents must preserve these rules before optimizing for local fixes. If a
requested change appears to conflict with a design rule, stop and explain the
conflict before editing.

Explicit rules override inferred patterns. Existing code is evidence of design
intent, but it is not the only source of truth.

When solving a problem:

- Read the relevant design rules first when they exist.
- State which rule governs the change.
- Implement the fix in a way that reinforces the rule.
- If no explicit rule exists, infer one from the nearest relevant code first,
  then widen to related code when the change affects shared infrastructure or
  public behavior.
- Always state the inferred rule's scope.
- If the inferred rule seems important, recommend adding it to this document or
  another project doc.

Concrete project design rules should be added here one by one during design
work. Do not invent rules just to fill this section.

## Conan/CMake Responsibility Boundary

`conanfile.py` owns package-manager policy:

- package identity;
- settings and options;
- dependency requirements and dependency version constraints;
- dependency option validation;
- Conan generators and tool requirements;
- Conan layout;
- the package lifecycle that invokes configure, build, install, and package
  steps.

CMake owns build-system behavior:

- project structure;
- targets, modules, and applications;
- generated files;
- compiler and linker behavior;
- install layout;
- exported CMake package config;
- runtime deployment and plugin import behavior;
- platform-specific build mechanics.

Keep the boundary narrow and explicit: Conan resolves external dependency
policy and passes stable build facts into CMake; CMake consumes those facts to
build, install, and export Tiger.

Do not move target, source layout, compiler feature, generated-source, install,
or app/module behavior into `conanfile.py`.

Do not make CMake choose Conan dependency versions, hardcode Conan profile
policy, mutate Conan dependency options, or duplicate package-manager
resolution logic.

This split keeps Tiger usable both as a Conan package and as a CMake project
after `conan install`, without hiding target and install behavior inside the
recipe. It also prevents duplicate policy: for example, Qt shared/static
selection belongs to Conan profiles or options, while Qt app deployment/import
behavior belongs to CMake.
