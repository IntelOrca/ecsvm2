# ecsvm

`ecsvm` is an Entity Component System Virtual Machine written in C99.

This repository currently contains:
- the MVP contract and draft specs in `docs/`
- a bootstrap project layout for the runtime, tools, and examples
- a buildable runtime core with entities, components, systems, blobs/strings, and the built-in `core.*` components
- SDL3-backed native systems for windowing and shape rendering
- a native C pong demo in `src/pong.c`

## Build

```sh
./tools/build.sh
./build/ecsvm --self-test
./build/ecsvm --pong
```

If SDL3 is installed in a non-standard prefix, set `SDL3_PREFIX` before building.

## Layout

- `docs/` - language, runtime, package, and MVP specifications
- `include/ecsvm/` - public C API
- `src/` - runtime implementation and CLI entry point
- `src/pong.c` - native pong demo using only the C runtime APIs
- `examples/basic/` - golden-path source project skeleton
- `tools/` - bootstrap build helper

## Current scope

The current implementation covers phases 1-3 of the plan and adds a native SDL3 demo path:
- define the MVP boundaries
- create the initial project skeleton
- implement the runtime core
- exercise the runtime with a pong game built through native C systems and components

Project-folder loading, `.ecsar` loading, the `.ecs` frontend, managed-system execution, and hot reload remain later phases.
