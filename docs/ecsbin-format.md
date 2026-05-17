# ECS binary format

## Header
- magic: char[5] - Always ECSVM
- version: char[3] - Current format is 0x00,0x00,0x02 meaning 0.0.2
- type_reference_offset: uint64
- field_reference_offset: uint64
- struct_definition_offset: uint64
- field_definition_offset: uint64
- function_reference_offset: uint64
- parameter_offset: uint64
- attribute_offset: uint64
- blob_offset: uint64
- type_reference_count: uint32
- field_reference_count: uint32
- struct_definition_count: uint32
- field_definition_count: uint32
- function_reference_count: uint32
- parameter_count: uint32
- attribute_count: uint32
- blob_count: uint32

## Type References
- namespace: uint32 (blob id)
- name: uint32 (blob id)

## Field References
- name: uint32 (blob id)
- type_id: uint32 (type reference id)

## Struct Definitions
- type_id: uint32 (type reference id)
- flags: uint32
  - bit 0 (`0x1`) marks a component struct
- field_start: uint32 (field reference id)
- field_count: uint32
- attribute_start: uint32 (attribute reference id)
- attribute_count: uint32

## Field Definitions
- field_id: uint32 (field reference id)
- attribute_start: uint32
- attribute_count: uint32

## Function References
- namespace: uint32 (blob id)
- name: uint32 (blob id)
- parameter_start: uint32 (parameter id)
- parameter_count: uint32
- attribute_start: uint32 (attribute id)
- attribute_count: uint32
- body_blob_id: uint32 (blob id, `0` when the function is declaration-only)

Function attributes always start with the return type attribute. The attribute payload is currently empty.

## Parameters
- name: uint32 (blob id)
- type_id: uint32 (type reference id)
- attribute_start: uint32
- attribute_count: uint32
- default_value_blob_id: uint32 (blob id, `0` when no default value is present)

# Attributes
- type_id: uint32 (type reference id)
- data: uint32 (blob id)

## Blobs
- offset: uint64
- length: uint64

Blob table entries are followed by the raw blob payload region. Each blob `offset` is relative to the start of that payload region.

Function bodies are stored in blobs as serialized AST payloads.

## AST blobs

Function bodies use AST blobs with this layout:

- version: uint32 - Must be `2`
- node_count: uint32
- nodes: `ecsvm_ecsbin_ast_node_t[node_count]`

The root node is always stored at index `0`. Child and sibling links use `0` as the sentinel for “no child/sibling”, so linked node indices start at `1` and never refer to the root node.

### AST nodes

- kind: uint32
- first_child: uint32
- last_child: uint32
- next_sibling: uint32
- token_kind: uint32
- value_kind: uint32
- value: uint32

### AST node kinds

- `1` - root
- `2` - block
- `3` - parenthesized group
- `4` - bracketed group
- `5` - token

### AST value kinds

- `0` - none
- `1` - blob id
- `2` - type reference id
- `3` - field reference id
- `4` - function reference id
- `5` - parameter id

### Token kinds

Token nodes use the same token enum as the parser. The supported token kinds are:

- punctuation: `(` `)` `{` `}` `[` `]` `:` `;` `.` `,` `=` `!` `+` `-` `*` `/` `%` `<` `>` `&` `|` `^` `~`
- keywords: `import` `namespace` `struct` `component` `attribute` `system` `const` `fn` `if` `else` `let` `return` `true` `false` `null`
- identifiers, numbers, and strings are stored as blobs or references through `value_kind`

### Value resolution

- `blob id` values point at raw blob payloads
- `type reference id` values resolve to type names
- `field reference id` values resolve to field names
- `function reference id` values resolve to function names
- `parameter id` values resolve to parameter names
