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

# Field Definitions
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
