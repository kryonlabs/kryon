# KRB Binary Format Specification v1.0

## Overview
KRB (Kryon Binary) is a compact, efficient binary format for storing UI definitions compiled from KRY source files.

## File Structure

```
[Header - 64 bytes]
[String Table]
[Elements Array]
[Metadata Section]
[Script Section]
[Resource Section]
[Checksum - 4 bytes]
```

## Header Format (64 bytes)

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0x00 | 4 | Magic | u32 | `0x4B52594E` ("KRYN") |
| 0x04 | 2 | Version Major | u16 | Format version major (1) |
| 0x06 | 2 | Version Minor | u16 | Format version minor (0) |
| 0x08 | 2 | Version Patch | u16 | Format version patch (0) |
| 0x0A | 2 | Flags | u16 | File flags (see below) |
| 0x0C | 4 | Element Count | u32 | Number of elements |
| 0x10 | 4 | Property Count | u32 | Total properties across all elements |
| 0x14 | 4 | String Table Size | u32 | String table size in bytes |
| 0x18 | 4 | Data Size | u32 | Total file data size |
| 0x1C | 4 | Checksum | u32 | CRC32 checksum |
| 0x20 | 1 | Compression | u8 | Compression type |
| 0x21 | 4 | Uncompressed Size | u32 | Original size if compressed |
| 0x25 | 4 | Metadata Offset | u32 | Offset to metadata section |
| 0x29 | 4 | Script Offset | u32 | Offset to script section |
| 0x2D | 4 | Resource Offset | u32 | Offset to resource section |
| 0x31 | 15 | Reserved | bytes | Reserved for future use |

### Header Flags
- `0x0001` - Has metadata section
- `0x0002` - Has embedded scripts
- `0x0004` - Is compressed
- `0x0008` - Has debug information
- `0x0010` - Has resources section
- `0x0020` - Has source mapping
- `0x0040` - Has component definitions
- `0x0080` - Has style definitions

### Compression Types
- `0x00` - None
- `0x01` - Zlib
- `0x02` - LZ4
- `0x03` - Brotli

## String Table

```
[String Count: u32]
[String 0: Length(u16) + UTF-8 Data]
[String 1: Length(u16) + UTF-8 Data]
...
```

All strings in the file are stored in the string table and referenced by index.

## Element Structure

```
[Element Header - 24 bytes]
[Properties Array]
[Child IDs Array]
[Event Handlers Array]
```

### Element Header

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0x00 | 4 | ID | u32 | Unique element ID |
| 0x04 | 1 | Type | u8 | Element type code |
| 0x05 | 2 | Name String ID | u16 | String table index for element name |
| 0x07 | 4 | Parent ID | u32 | Parent element ID (0 = root) |
| 0x0B | 2 | Property Count | u16 | Number of properties |
| 0x0D | 2 | Child Count | u16 | Number of children |
| 0x0F | 2 | Handler Count | u16 | Number of event handlers |
| 0x11 | 4 | Flags | u32 | Element flags |
| 0x15 | 3 | Reserved | bytes | Reserved |

### Element Type Codes

| Type | Code | Description |
|------|------|-------------|
| App | 0x00 | Application root |
| Container | 0x01 | Layout container |
| Text | 0x02 | Text element |
| EmbedView | 0x03 | Platform embed |
| Button | 0x10 | Button |
| Input | 0x11 | Text input |
| Link | 0x12 | Hyperlink |
| Image | 0x20 | Image |
| Video | 0x21 | Video player |
| Audio | 0x22 | Audio player |
| Canvas | 0x23 | Drawing canvas |
| Svg | 0x24 | SVG graphics |
| WebView | 0x25 | Web view |
| Select | 0x30 | Dropdown |
| Slider | 0x31 | Range slider |
| Checkbox | 0x33 | Checkbox |
| Radio | 0x34 | Radio button |
| TextArea | 0x35 | Multi-line input |
| FileInput | 0x36 | File picker |
| DatePicker | 0x37 | Date picker |
| ColorPicker | 0x38 | Color picker |
| Form | 0x40 | Form container |
| List | 0x41 | List container |
| ListItem | 0x42 | List item |
| Header | 0x43 | Header section |
| Footer | 0x44 | Footer section |
| Nav | 0x45 | Navigation |
| Section | 0x46 | Section |
| Article | 0x47 | Article |
| Aside | 0x48 | Aside |
| Table | 0x50 | Table |
| TableRow | 0x51 | Table row |
| TableCell | 0x52 | Table cell |
| TableHeader | 0x53 | Table header |
| ScrollView | 0x60 | Scrollable area |
| Grid | 0x61 | Grid layout |
| Stack | 0x62 | Stack layout |
| Spacer | 0x63 | Flexible space |

### Element Flags
- `0x00000001` - Is hidden
- `0x00000002` - Is absolute positioned
- `0x00000004` - Has event handlers
- `0x00000008` - Has animations
- `0x00000010` - Has conditional rendering
- `0x00000020` - Is component instance
- `0x00000040` - Has slots
- `0x00000080` - Has lifecycle hooks

## Property Structure

```
[Property Header - 8 bytes]
[Property Value - varies]
```

### Property Header

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0x00 | 2 | Property ID | u16 | Property type ID |
| 0x02 | 1 | Value Type | u8 | Value data type |
| 0x03 | 1 | Flags | u8 | Property flags |
| 0x04 | 4 | Value Size | u32 | Value size in bytes |

### Property Type IDs

#### Visual Properties (0x01-0x1F)
| ID | Property | Type |
|----|----------|------|
| 0x01 | backgroundColor | Color |
| 0x02 | textColor | Color |
| 0x03 | borderColor | Color |
| 0x04 | borderWidth | Float |
| 0x05 | borderRadius | Float |
| 0x06 | opacity | Float |
| 0x07 | visibility | Boolean |
| 0x08 | cursor | String |
| 0x09 | boxShadow | String |
| 0x0A | filter | String |
| 0x0B | backdropFilter | String |
| 0x0C | transform | String |
| 0x0D | transition | String |
| 0x0E | animation | String |
| 0x0F | gradient | String |

#### Text Properties (0x20-0x3F)
| ID | Property | Type |
|----|----------|------|
| 0x20 | textContent | String |
| 0x21 | fontSize | Float |
| 0x22 | fontWeight | Integer |
| 0x23 | fontFamily | String |
| 0x24 | fontStyle | String |
| 0x25 | textAlign | String |
| 0x26 | textDecoration | String |
| 0x27 | textTransform | String |
| 0x28 | lineHeight | Float |
| 0x29 | letterSpacing | Float |
| 0x2A | wordSpacing | Float |
| 0x2B | whiteSpace | String |
| 0x2C | textOverflow | String |
| 0x2D | wordBreak | String |

#### Layout Properties (0x40-0x5F)
| ID | Property | Type |
|----|----------|------|
| 0x40 | width | Dimension |
| 0x41 | height | Dimension |
| 0x42 | minWidth | Dimension |
| 0x43 | maxWidth | Dimension |
| 0x44 | minHeight | Dimension |
| 0x45 | maxHeight | Dimension |
| 0x46 | display | String |
| 0x47 | position | String |
| 0x48 | left | Dimension |
| 0x49 | top | Dimension |
| 0x4A | right | Dimension |
| 0x4B | bottom | Dimension |
| 0x4C | zIndex | Integer |
| 0x4D | overflow | String |
| 0x4E | overflowX | String |
| 0x4F | overflowY | String |

#### Flexbox Properties (0x60-0x7F)
| ID | Property | Type |
|----|----------|------|
| 0x60 | flexDirection | String |
| 0x61 | flexWrap | String |
| 0x62 | justifyContent | String |
| 0x63 | alignItems | String |
| 0x64 | alignContent | String |
| 0x65 | alignSelf | String |
| 0x66 | flexGrow | Float |
| 0x67 | flexShrink | Float |
| 0x68 | flexBasis | Dimension |
| 0x69 | gap | Float |
| 0x6A | rowGap | Float |
| 0x6B | columnGap | Float |

#### Spacing Properties (0x80-0x9F)
| ID | Property | Type |
|----|----------|------|
| 0x80 | padding | Spacing |
| 0x81 | paddingTop | Float |
| 0x82 | paddingRight | Float |
| 0x83 | paddingBottom | Float |
| 0x84 | paddingLeft | Float |
| 0x85 | margin | Spacing |
| 0x86 | marginTop | Float |
| 0x87 | marginRight | Float |
| 0x88 | marginBottom | Float |
| 0x89 | marginLeft | Float |

#### Interactive Properties (0xA0-0xBF)
| ID | Property | Type |
|----|----------|------|
| 0xA0 | id | String |
| 0xA1 | class | String |
| 0xA2 | style | String |
| 0xA3 | disabled | Boolean |
| 0xA4 | enabled | Boolean |
| 0xA5 | tooltip | String |
| 0xA6 | pointerEvents | String |
| 0xA7 | userSelect | String |

#### Element-Specific Properties (0xC0-0xFF)
| ID | Property | Type |
|----|----------|------|
| 0xC0 | windowTitle | String |
| 0xC1 | windowWidth | Integer |
| 0xC2 | windowHeight | Integer |
| 0xC3 | windowResizable | Boolean |
| 0xC4 | src | String |
| 0xC5 | alt | String |
| 0xC6 | href | String |
| 0xC7 | target | String |
| 0xC8 | placeholder | String |
| 0xC9 | value | String |
| 0xCA | type | String |
| 0xCB | checked | Boolean |
| 0xCC | options | Array |
| 0xCD | selected | String |
| 0xCE | autoplay | Boolean |
| 0xCF | controls | Boolean |

### Value Types
| Type | Code | Size | Description |
|------|------|------|-------------|
| Null | 0x00 | 0 | No value |
| Boolean | 0x01 | 1 | true/false |
| Integer | 0x02 | 8 | Signed 64-bit |
| Float | 0x03 | 8 | Double precision |
| String | 0x04 | varies | String table index (u16) |
| Color | 0x05 | 4 | RGBA (0xRRGGBBAA) |
| Dimension | 0x06 | 5 | Type(u8) + Value(f32) |
| Spacing | 0x07 | 16 | TRBL floats |
| Position | 0x08 | 8 | X(f32) + Y(f32) |
| Size | 0x09 | 8 | W(f32) + H(f32) |
| Array | 0x0A | varies | Count(u32) + Items |
| Object | 0x0B | varies | Count(u32) + KV pairs |
| Reference | 0x0C | 4 | Element ID |
| Expression | 0x0D | varies | Expression data |
| Function | 0x0E | 2 | Function string ID |

### Dimension Type Codes
| Type | Code | Description |
|------|------|-------------|
| Auto | 0x00 | Automatic |
| Pixels | 0x01 | Absolute pixels |
| Percent | 0x02 | Percentage |
| MinContent | 0x03 | Minimum content |
| MaxContent | 0x04 | Maximum content |
| Rem | 0x05 | Root em units |
| Em | 0x06 | Relative em units |
| Vw | 0x07 | Viewport width |
| Vh | 0x08 | Viewport height |

## Event Handler Structure

```
[Event Type: u8]
[Handler String ID: u16]
[Flags: u8]
```

### Event Type Codes
| Type | Code | Description |
|------|------|-------------|
| onClick | 0x01 | Click event |
| onDoubleClick | 0x02 | Double click |
| onMouseEnter | 0x03 | Mouse enter |
| onMouseLeave | 0x04 | Mouse leave |
| onMouseMove | 0x05 | Mouse move |
| onMouseDown | 0x06 | Mouse down |
| onMouseUp | 0x07 | Mouse up |
| onFocus | 0x08 | Focus gained |
| onBlur | 0x09 | Focus lost |
| onChange | 0x0A | Value changed |
| onInput | 0x0B | Input event |
| onKeyDown | 0x0C | Key down |
| onKeyUp | 0x0D | Key up |
| onKeyPress | 0x0E | Key press |
| onScroll | 0x0F | Scroll event |
| onLoad | 0x10 | Load complete |
| onError | 0x11 | Error occurred |

## Metadata Section

```
[Section Size: u32]
[JSON Data: UTF-8]
```

Contains JSON-encoded metadata:
- App configuration
- Component definitions
- Style definitions
- Script references
- Resource manifest

## Script Section

```
[Script Count: u32]
[Script 0: Language(u8) + NameID(u16) + Size(u32) + Code]
[Script 1: Language(u8) + NameID(u16) + Size(u32) + Code]
...
```

### Language Codes
| Language | Code |
|----------|------|
| Lua | 0x01 |
| JavaScript | 0x02 |
| Python | 0x03 |
| Wren | 0x04 |

## Resource Section

```
[Resource Count: u32]
[Resource 0: Type(u8) + PathID(u16) + Size(u32) + Data]
[Resource 1: Type(u8) + PathID(u16) + Size(u32) + Data]
...
```

### Resource Types
| Type | Code | Description |
|------|------|-------------|
| Image | 0x01 | Embedded image |
| Font | 0x02 | Font data |
| Audio | 0x03 | Audio file |
| Video | 0x04 | Video file |
| Data | 0x05 | Generic data |

## Checksum
- Algorithm: CRC32
- Coverage: All file data except checksum field itself
- Location: End of file (4 bytes)