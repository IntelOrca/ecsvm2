# ecsvm MVP contract

## Goal

Ship a narrow, runnable vertical slice that proves the runtime architecture before the language frontend and package loader are added.

## First milestone

The first milestone is **specification-first**, but it ends with a buildable runtime core. The milestone includes:

1. Stable draft docs for the `.ecs` language subset, runtime model, and package/project format.
2. A repository skeleton for runtime development, examples, and tooling.
3. A C99 runtime core with:
   - entity creation
   - component registration
   - contiguous component storage only
   - system registration and ordered execution
   - per-system allocation/log hooks
   - blob and string handles
   - built-in `core.hierarchy`

## Explicitly deferred

These are out of scope for the first milestone:

- alternate component storage backends
- parsing and semantic analysis of `.ecs`
- managed-system bytecode / IR execution
- project-folder loading
- `.ecsar` loading
- transpiling managed systems to C
- hot reload
- optimized blob allocator buckets
- the full accessor/property syntax surface

## Acceptance criteria

The first milestone is complete when:

1. The docs define the current contract and what is deferred.
2. The runtime library builds cleanly as C99.
3. The CLI can run a self-test that exercises runtime registration and system execution.
4. The example project layout matches the documented future source-project format.

