# Project and package format

## Canonical source project format

The first canonical application format is a source project folder:

```text
project/
  project.toml
  src/
    *.ecs
```

`project.toml` is the project manifest and must contain:

- `name`
- `version`
- `entry`

Example:

```toml
name = "basic"
version = "0.1.0"
entry = "src/systems.ecs"
```

## `.ecsar` direction

`.ecsar` is reserved as the packaged application format. The first spec draft defines the container at a high level:

- file magic: `ECSA`
- format version
- manifest block
- one or more payload sections

The concrete binary layout is deferred until the language frontend and IR are implemented. Until then, the source project folder is the canonical authoring format.

## Intermediate representation

Managed `.ecs` code will eventually compile to an intermediate representation that is either:

1. stored directly in a package payload, or
2. loaded from project build output

That IR is not implemented in the current phase, but the package contract already reserves space for it.

