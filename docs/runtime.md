# Runtime model

## Core objects

The runtime owns these primary tables:

1. **Entity table** - ordered list of entity IDs created by the engine.
2. **Component registry** - metadata for every registered component.
3. **Component stores** - per-component instance tables.
4. **System registry** - ordered list of systems to execute.
5. **Blob table** - managed memory blocks for `blob` and `string`.

## Entities

- Entity IDs are 32-bit unsigned integers.
- `0` is reserved as `ECSVM_INVALID_ENTITY`.
- The runtime issues IDs monotonically.

## Components

Each component registry entry contains:

- fully-qualified name
- element size in bytes
- preferred storage mode

## Storage mode

Only **contiguous** storage is implemented in the first runtime:

- one contiguous array of entity IDs
- one contiguous byte buffer storing component payloads in the same order
- lookup is linear for now

Future storage backends may be added behind the same component API.

## Systems

Systems are registered in engine order. Each system executes with a context containing:

- a pointer to the engine
- its index in the system list
- its registered name
- an API table with `alloc`, `free`, `log`, and `userdata`

The runtime supports managed function execution from `.ecsbin` AST payloads for a small core language subset.
Functions marked with the `core.System` attribute are registered as runtime systems.
Native-C callback systems remain supported alongside managed systems.

## Blobs and strings

- `blob` and `string` are opaque handles backed by the blob table.
- A string is a blob marked with string semantics and stored as a null-terminated byte sequence.
- The initial implementation uses a simple slot table and heap allocation. Bucketed allocators are deferred.

## Built-in component

The engine reserves `core.hierarchy` as a built-in component with this layout:

- `parent`
- `first_child`
- `next_sibling`
- `prev_sibling`

All fields are `entity` IDs.

The current native runtime also defines:

- `core.transform`
  - `position: vec2`
  - `scale: vec2`
  - `rotation: f32`
- `core.graphics.shape`
  - `color: vec4`
  - `kind: i32`

`core.graphics.shape` is rendered using the transform data from `core.transform`.

## Native built-in systems

The first native built-in systems are SDL3-backed and implemented one system per source file:

1. `core.window` - creates the SDL window/renderer pair, pumps events, updates the managed `core.ui.Window` and `core.input.InputMonitor` components, and performs the `closing` handshake.
2. `core.renderer` - renders `core.graphics.shape` entities using `core.transform`.

The pong example uses these systems from managed `.ecs` execution.
