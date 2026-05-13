# `.ecs` language draft

## Scope of this draft

This document defines the **initial language subset** and intentionally leaves advanced syntax for later revisions.

## File model

- Source files use the `.ecs` extension.
- A project is a folder containing a TOML manifest plus one or more `.ecs` source files.
- Source files may declare namespaces, structs, components, constants, functions, and systems.

## Namespaces

Two forms are allowed in source:

```ecs
[core.transform]
component transform {
    position: vec3;
}
```

```ecs
namespace app {
    component hover {
        radius: vec2;
    }
}
```

For the MVP language frontend, both forms normalize to a fully-qualified symbol name such as `core.transform` or `app.hover`.

## Initial declarations

### Struct

```ecs
struct vec2 {
    x: f32;
    y: f32;
}
```

- Struct fields are stored inline.
- Recursive structs are not allowed in the MVP subset.

### Component

```ecs
component velocity {
    x: f32;
    y: f32;
}
```

- Components are plain data in the MVP subset.
- Each component resolves to one runtime component registration.
- Storage is contiguous only in the first runtime implementation.

### Constant

```ecs
const GRAVITY: f32 = 9.8;
```

### Function

```ecs
fn add(a: i32, b: i32): i32 {
    return a + b;
}
```

### System

```ecs
system Gravity {
    // body
}
```

Systems may take **zero or one** parameter.

- If present, the single parameter must be `core.Entity`.
- Components for that entity are read via index syntax:

```ecs
system MoveBall(ball: core.Entity) {
    let transform: core.transform = ball[core.transform];
    let velocity: app.velocity = ball[app.velocity];
}
```

- Components on other entities are retrieved through core lookup helpers (for example `getFirstComponent<T>()`):

```ecs
system MoveBall(ball: core.Entity) {
    let window: core.Window = getFirstComponent<core.Window>();
}
```

## MVP type set

The initial type set is:

- `bool`
- `i32`
- `u32`
- `f32`
- `entity`
- `blob`
- `string`
- user-defined `struct`

## Deferred syntax

The following features are valid design targets but not part of the first implementation:

- computed component properties
- `get` / `set` accessors
- shorthand accessor bodies
- the full iteration/filter sugar for systems
- native interop declarations in `.ecs`
