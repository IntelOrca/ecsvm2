# ECS binary format

## Header
- magic: char[5] - Always ECSVM
- version: char[3] - Start with 0x00,0x00,0x01 meaning 0.0.1
- type_reference_offset: uint64
- field_reference_offset: uint64
- struct_definition_offset: uint64
- field_definition_offset: uint64
- attribute_offset: uint64
- blob_offset: uint64
- type_reference_count: uint32
- field_reference_count: uint32
- struct_definition_count: uint32
- field_definition_count: uint32
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
- field_start: uint32 (field reference id)
- field_count: uint32
- attribute_start: uint32 (attribute reference id)
- attribute_count: uint32

# Field Definitions
- field_id: uint32 (field reference id)
- attribute_start: uint32
- attribute_count: uint32

# Attributes
- type_id: uint32 (type reference id)
- data: uint32 (blob id)

## Blobs
- offset: uint64
- length: uint64
