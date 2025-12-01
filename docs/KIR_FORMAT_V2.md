# Kryon IR Binary Format Specification v2.0

**Version:** 2.0
**Status:** Stable
**Date:** 2024-11-30

## Overview

The Kryon IR (KIR) binary format is a compact, efficient serialization format for UI component trees with full support for styling, layout, animations, and reactive state. This document specifies the v2.0 format which adds comprehensive property serialization and reactive manifest support.

## Design Goals

1. **Compactness** - Minimal overhead for fast transmission and storage
2. **Completeness** - Serialize all component state including styles, layout, animations
3. **Versioning** - Forward/backward compatibility through version negotiation
4. **Validation** - CRC32 checksumming for corruption detection
5. **Extensibility** - Optional sections via header flags
6. **Hot Reload** - Preserve reactive state across reloads

## File Structure

```
┌─────────────────────────────────┐
│ File Header                     │
├─────────────────────────────────┤
│ Component Tree                  │
│  ├─ Root Component              │
│  ├─ Child Components (recursive)│
│  └─ Styles, Layout, Events      │
├─────────────────────────────────┤
│ Reactive Manifest (optional)    │
│  ├─ Variable Descriptors        │
│  ├─ Component Bindings          │
│  ├─ Conditionals                │
│  └─ For Loops                   │
├─────────────────────────────────┤
│ CRC32 Checksum                  │
└─────────────────────────────────┘
```

## Data Types

### Primitive Types

| Type     | Size    | Description                    |
|----------|---------|--------------------------------|
| uint8    | 1 byte  | Unsigned 8-bit integer         |
| uint16   | 2 bytes | Unsigned 16-bit integer        |
| uint32   | 4 bytes | Unsigned 32-bit integer        |
| float32  | 4 bytes | IEEE 754 single precision      |
| float64  | 8 bytes | IEEE 754 double precision      |
| bool     | 1 byte  | Boolean (0=false, 1=true)      |
| cstring  | variable| Length-prefixed UTF-8 string   |

### String Encoding

Strings are encoded as:
```
┌────────────┬──────────────────┐
│ Length     │ UTF-8 Data       │
│ (uint32)   │ (n bytes)        │
└────────────┴──────────────────┘
```

- Length includes null terminator
- Empty strings: length=1, data='\0'
- NULL pointers: length=0

## File Header

```c
struct KIRHeader {
    uint32_t magic;           // 0x4B495232 ('KIR2')
    uint8_t  version_major;   // 2
    uint8_t  version_minor;   // 0
    uint8_t  flags;           // Feature flags
    uint8_t  reserved;        // Future use
    uint32_t endianness;      // 0x01020304
};
```

### Header Flags

| Bit | Flag                          | Description                    |
|-----|-------------------------------|--------------------------------|
| 0   | IR_HEADER_FLAG_HAS_MANIFEST   | Reactive manifest present      |
| 1   | IR_HEADER_FLAG_COMPRESSED     | Data is compressed (reserved)  |
| 2-7 | Reserved                      | Must be 0                      |

### Endianness Check

The `endianness` field contains `0x01020304`. Readers check if bytes appear as `01 02 03 04` (big-endian) or `04 03 02 01` (little-endian) to determine byte order conversion needs.

## Component Serialization

### Component Header

```c
struct ComponentHeader {
    uint32_t id;                    // Unique component ID
    uint8_t  type;                  // IRComponentType enum
    uint8_t  has_style;             // 1 if style data follows
    uint8_t  has_layout;            // 1 if layout data follows
    uint8_t  has_events;            // 1 if events follow
    uint32_t child_count;           // Number of children
};
```

Followed by:
- Text content (cstring) - if text-bearing component
- Custom data (cstring) - if present
- Style data - if `has_style=1`
- Layout data - if `has_layout=1`
- Event data - if `has_events=1`
- Child components (recursive) - `child_count` times

## Style Serialization

### IRStyle Structure

```c
struct IRStyle {
    IRDimension width, height;
    IRDimension min_width, min_height;
    IRDimension max_width, max_height;
    IRColor     background;
    IRBorder    border;
    IRSpacing   margin;
    IRSpacing   padding;
    IRTypography font;
    bool        visible;
    uint32_t    z_index;
    uint8_t     position_mode;
    float       absolute_x, absolute_y;

    // Extended properties (v2.0)
    IRAnimation animations[8];
    uint8_t     animation_count;
    IRTransition transitions[4];
    uint8_t     transition_count;
    IRPseudoStyle pseudo_styles[4];  // hover, active, focus, disabled
    IRBreakpoint breakpoints[4];
    uint8_t     breakpoint_count;
    IRFilter    filters[4];
    uint8_t     filter_count;
    IRShadow    shadows[4];
    uint8_t     shadow_count;
    IRTextEffect text_effects;
};
```

### IRDimension

```c
struct IRDimension {
    uint8_t type;     // 0=px, 1=percent, 2=auto, 3=flex
    float   value;    // Numeric value (unused for auto)
};
```

### IRColor

```c
struct IRColor {
    uint8_t type;     // 0=solid, 1=transparent, 2=gradient, 3=var
    union {
        struct { uint8_t r, g, b, a; };  // Solid color
        uint16_t var_id;                 // Style variable reference
        IRGradient* gradient;            // Gradient pointer
    } data;
};
```

**Serialization:**
- Type byte
- If type=SOLID: 4 bytes (r, g, b, a)
- If type=VAR_REF: 2 bytes (var_id)
- If type=GRADIENT: serialize gradient structure

### IRGradient

```c
struct IRGradient {
    uint8_t  type;         // 0=linear, 1=radial, 2=conic
    uint8_t  stop_count;   // Number of gradient stops
    IRGradientStop stops[8];
    float    angle;        // For linear (degrees)
    float    center_x, center_y;  // For radial/conic (0.0-1.0)
};

struct IRGradientStop {
    uint8_t r, g, b, a;
    float   position;      // 0.0-1.0
};
```

### IRAnimation

```c
struct IRAnimation {
    char     name[32];
    uint8_t  property;         // Animated property
    float    duration;         // Seconds
    float    delay;            // Seconds
    uint8_t  easing;           // Easing function
    uint8_t  iteration_count;  // 0=infinite
    bool     alternate;        // Reverse on alternate iterations
    IRKeyframe keyframes[16];
    uint8_t  keyframe_count;
};

struct IRKeyframe {
    float position;       // 0.0-1.0
    float value;          // Property value at this keyframe
    uint8_t easing;       // Easing to next keyframe
};
```

### IRFilter

```c
struct IRFilter {
    uint8_t type;    // blur, brightness, contrast, grayscale, etc.
    float   amount;  // Filter-specific parameter
};
```

### IRShadow

```c
struct IRShadow {
    uint8_t type;              // box, text, inner
    float   offset_x, offset_y;
    float   blur_radius;
    float   spread_radius;
    IRColor color;
    bool    inset;
};
```

## Layout Serialization

### IRLayout Structure

```c
struct IRLayout {
    IRDimension min_width, min_height;
    IRDimension max_width, max_height;
    IRFlexbox   flex;
    IRSpacing   margin;
    IRSpacing   padding;
    IRAlignment align_items;
    IRAlignment align_content;
};

struct IRFlexbox {
    bool     wrap;
    uint32_t gap;
    uint8_t  main_axis;       // IRAlignment
    uint8_t  cross_axis;      // IRAlignment
    uint8_t  justify_content; // IRAlignment
    uint8_t  grow;            // 0-255
    uint8_t  shrink;          // 0-255
    uint8_t  direction;       // 0=column, 1=row
};
```

## Event Serialization

```c
struct IREvent {
    uint8_t  type;        // click, hover, focus, blur, key, scroll
    char     logic_id[64];
    char     handler_data[256];
    IREvent* next;        // Linked list
};
```

Events are serialized as a linked list:
1. Event count (uint32)
2. For each event:
   - Type (uint8)
   - Logic ID (cstring)
   - Handler data (cstring)

## Reactive Manifest

The reactive manifest preserves application state for hot reload.

### Manifest Header

```c
struct ManifestHeader {
    uint32_t format_version;     // Manifest format version
    uint32_t variable_count;
    uint32_t binding_count;
    uint32_t conditional_count;
    uint32_t for_loop_count;
};
```

### Variable Descriptor

```c
struct IRReactiveVarDescriptor {
    uint32_t id;
    char*    name;
    uint8_t  type;               // int, float, string, bool, custom
    IRReactiveValue value;
    uint32_t version;            // Change tracking
    char*    source_location;    // Optional debug info
    uint32_t binding_count;      // Components using this var
};

union IRReactiveValue {
    int32_t  as_int;
    double   as_float;
    char*    as_string;
    bool     as_bool;
    void*    as_custom;
};
```

**Serialization:**
- ID (uint32)
- Name (cstring)
- Type (uint8)
- Version (uint32)
- Value (type-dependent):
  - INT: 4 bytes
  - FLOAT: 8 bytes (double)
  - STRING: cstring
  - BOOL: 1 byte
  - CUSTOM: skip (not serialized)
- Source location (cstring)
- Binding count (uint32)

### Component Binding

```c
struct IRReactiveBinding {
    uint32_t component_id;       // Target component
    uint32_t reactive_var_id;    // Source variable
    uint8_t  binding_type;       // text, conditional, attribute, for_loop
    char*    expression;         // Optional expression
    char*    update_code;        // Optional update logic
};
```

### Reactive Conditional

```c
struct IRReactiveConditional {
    uint32_t component_id;
    char*    condition;          // Condition expression
    bool     last_eval_result;
    bool     suspended;          // For hot reload state preservation
    uint32_t* dependent_var_ids;
    uint32_t dependent_var_count;
};
```

### Reactive For Loop

```c
struct IRReactiveForLoop {
    uint32_t parent_component_id;
    char*    collection_expr;
    uint32_t collection_var_id;
    char*    item_template;
    uint32_t* child_component_ids;
    uint32_t child_count;
};
```

## Validation & Integrity

### CRC32 Checksum

A CRC32 checksum is appended to the end of the file:
```
┌────────────────┐
│ CRC32 value    │
│ (uint32)       │
└────────────────┘
```

The checksum is computed over all data from the start of the file up to (but not including) the checksum itself.

### Validation Levels

1. **Format Validation**
   - Magic number check
   - Version compatibility
   - Endianness verification
   - CRC32 checksum

2. **Structure Validation**
   - Component tree integrity
   - Parent-child relationships
   - Style/layout consistency
   - No circular references

3. **Semantic Validation**
   - Valid enum values
   - Dimension constraints
   - Color ranges (0-255)
   - Animation timing > 0

## Version Compatibility

### Reading Older Versions

- v2.0 readers can read v1.x files by:
  - Detecting version from header
  - Using legacy deserialization path
  - Default values for missing v2.0 properties

### Upgrading Files

Files can be upgraded by:
1. Deserialize with legacy reader
2. Fill missing v2.0 properties with defaults
3. Serialize with v2.0 writer

## Performance Considerations

### Optimization Flags

The dirty tracking system allows incremental updates:
- `dirty_flags` field tracks modified properties
- Only dirty components need re-serialization
- Efficient for hot reload scenarios

### Memory Layout

Structures are designed for:
- Cache-friendly access patterns
- Minimal padding/alignment waste
- Sequential component tree traversal

## Example Binary Layout

```
Offset | Data                    | Description
-------|-------------------------|---------------------------
0x00   | 4B 49 52 32            | Magic 'KIR2'
0x04   | 02 00                  | Version 2.0
0x06   | 01                     | Flags (has manifest)
0x07   | 00                     | Reserved
0x08   | 01 02 03 04            | Endianness check
       |                        |
0x0C   | 00 00 00 01            | Component ID = 1
0x10   | 00                     | Type = CONTAINER
0x11   | 01                     | Has style
0x12   | 01                     | Has layout
0x13   | 00                     | No events
0x14   | 00 00 00 02            | 2 children
       |                        |
0x18   | [Style data...]        |
0x??   | [Layout data...]       |
0x??   | [Child 1 component...] |
0x??   | [Child 2 component...] |
       |                        |
0x??   | [Manifest header...]   |
0x??   | [Variable 1...]        |
0x??   | [Variable 2...]        |
0x??   | [Bindings...]          |
       |                        |
0x??   | XX XX XX XX            | CRC32 checksum
```

## Error Handling

### Graceful Degradation

When encountering corrupt or unsupported data:
1. Log warning with specific issue
2. Use safe defaults for corrupt properties
3. Continue deserialization if possible
4. Return partial tree if critical data is intact

### Error Codes

- `IR_ERROR_INVALID_MAGIC` - Not a KIR file
- `IR_ERROR_VERSION_MISMATCH` - Unsupported version
- `IR_ERROR_CORRUPT_DATA` - CRC32 failure
- `IR_ERROR_TRUNCATED` - Unexpected EOF
- `IR_ERROR_INVALID_STRUCTURE` - Malformed tree

## Security Considerations

1. **Input Validation**
   - Bounds checking on all array accesses
   - String length limits
   - Component tree depth limits (prevent stack overflow)

2. **Resource Limits**
   - Maximum file size: 100MB
   - Maximum component count: 1,000,000
   - Maximum tree depth: 1000
   - Maximum string length: 1MB

3. **Memory Safety**
   - No buffer overflows in deserialization
   - Proper cleanup on error paths
   - No memory leaks

## Tools

### CLI Commands

```bash
# Validate IR format
kryon validate file.kir

# Inspect IR metadata
kryon inspect-ir file.kir

# Detailed analysis
kryon inspect-detailed file.kir --tree

# Compare two files
kryon diff old.kir new.kir

# Show component tree
kryon tree file.kir --max-depth=5
```

### Programmatic Access

```nim
# Load IR file
let buffer = ir_buffer_create_from_file("app.kir")
var manifest: ptr IRReactiveManifest
let root = ir_deserialize_binary_with_manifest(buffer, addr manifest)

# Validate
if ir_validate_binary_format(buffer):
  echo "Valid IR file"

# Save with manifest
discard ir_write_binary_file_with_manifest(root, manifest, "app.kir")
```

## Future Extensions

### Reserved for v2.1+

- Compression (zstd, lz4)
- Incremental updates (delta encoding)
- External resource references
- Multi-document format
- Signature/encryption support

## References

- **IR Builder API**: `ir/ir_builder.h`
- **Serialization Implementation**: `ir/ir_serialization.c`
- **Validation**: `ir/ir_validation.c`
- **CLI Tools**: `cli/compile.nim`, `cli/inspect.nim`

## Changelog

### v2.0 (2024-11-30)

- ✨ Added comprehensive style property serialization
- ✨ Added layout properties (flexbox, alignment, spacing)
- ✨ Added animations and transitions
- ✨ Added pseudo-class styles
- ✨ Added filters and shadows
- ✨ Added responsive breakpoints
- ✨ Added reactive manifest for state preservation
- ✨ Added CRC32 checksumming
- ✨ Added graceful degradation on errors

### v1.0 (Initial)

- Basic component tree serialization
- Simple style support
- Text content
- Parent-child relationships
