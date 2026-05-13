# ecsvm

`ecsvm` is an Entity Component System Virtual Machine written in C99.

This repository currently contains:
- the MVP contract and draft specs in `docs/`
- a bootstrap project layout for the runtime, tools, and examples
- a buildable runtime core with entities, components, systems, blobs/strings, and the built-in `core.*` components
- optional SDL3-backed native systems for windowing and shape rendering
- a native C pong demo in `src/pong.c` when built with SDL3 enabled
- a source-project frontend that builds `.ecs` component declarations into `.ecsbin` plus generated `types.h`

## Build

```sh
make
./build/ecsvm --self-test
./build/ecsvm --pong
./build/ecsvm build examples/pong
./build/ecsvm run examples/pong
./build/ecsvm decompile examples/pong/out/pong.ecsbin
./build/ecsvm inspect examples/pong/out/pong.ecsbin
```

Build without SDL3 by setting `ECSVM_ENABLE_SDL3=0`:

```sh
make ECSVM_ENABLE_SDL3=0
./build/ecsvm --self-test
```

When SDL3 is enabled, `--pong` and `run examples/pong` require SDL3 to be installed. If SDL3 is installed in a non-standard prefix, set `SDL3_PREFIX` before building.

## Layout

- `docs/` - language, runtime, package, and MVP specifications
- `include/ecsvm/` - public C API
- `src/` - runtime implementation and CLI entry point
- `src/pong.c` - native pong demo using only the C runtime APIs
- `Makefile` - Linux build entry point
- `examples/basic/` - golden-path source project skeleton

## Current scope

The current implementation covers phases 1-4 of the plan and adds a native SDL3 demo path:
- define the MVP boundaries
- create the initial project skeleton
- implement the runtime core
- add a first `.ecs` frontend slice for component compilation and `.ecsbin` loading
- exercise the runtime with a pong game built through native C systems and components

Managed-system execution, `.ecsar` loading, and hot reload remain later phases.
