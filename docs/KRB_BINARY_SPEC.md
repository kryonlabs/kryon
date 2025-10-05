# KRB Binary Format Specification v0.1
## Smart Hybrid System with Enhanced Theming

## Overview
KRB (Kryon Binary) v0.1 is designed for the Smart Hybrid System, supporting CSS-like styling, widget-based layout, style inheritance, and enhanced theming with variable groups.

## File Structure

```
[Header - 128 bytes]
[String Table]
[Style Definition Table]
[Theme Variable Table]
[Widget Definition Table]
[Widget Instances]
[Script Section]
[Resource Section]
[Checksum - 4 bytes]
```

## Header Format (128 bytes)

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0x00 | 4 | Magic | u32 | `0x4B52594E` ("KRYN") |
| 0x04 | 2 | Version Major | u16 | Format version major (0) |
| 0x06 | 2 | Version Minor | u16 | Format version minor (1) |
| 0x08 | 2 | Version Patch | u16 | Format version patch (0) |
| 0x0A | 2 | Flags | u16 | File flags (see below) |
| 0x0C | 4 | Style Definition Count | u32 | Number of style definitions |
| 0x10 | 4 | Theme Variable Count | u32 | Number of theme variable groups |
| 0x14 | 4 | Widget Definition Count | u32 | Number of widget types |
| 0x18 | 4 | Widget Instance Count | u32 | Number of widget instances |
| 0x1C | 4 | Property Count | u32 | Total properties across all widgets |
| 0x20 | 4 | String Table Size | u32 | String table size in bytes |
| 0x24 | 4 | Data Size | u32 | Total file data size |
| 0x28 | 4 | Checksum | u32 | CRC32 checksum |
| 0x2C | 1 | Compression | u8 | Compression type |
| 0x2D | 4 | Uncompressed Size | u32 | Original size if compressed |
| 0x31 | 4 | Style Definition Offset | u32 | Offset to style definitions |
| 0x35 | 4 | Theme Variable Offset | u32 | Offset to theme variables |
| 0x39 | 4 | Widget Definition Offset | u32 | Offset to widget definitions |
| 0x3D | 4 | Widget Instance Offset | u32 | Offset to widget instances |
| 0x41 | 4 | Script Offset | u32 | Offset to script section |
| 0x45 | 4 | Resource Offset | u32 | Offset to resource section |
| 0x49 | 55 | Reserved | bytes | Reserved for future use |

### Header Flags
- `0x0001` - Has styles
- `0x0002` - Has theme variables
- `0x0004` - Has embedded scripts
- `0x0008` - Is compressed
- `0x0010` - Has debug information
- `0x0020` - Has resources section
- `0x0040` - Has source mapping
- `0x0080` - Has style inheritance
- `0x0100` - Has responsive properties
- `0x0200` - Supports theme switching

## Style Definition Table

### Style Definition Structure

```
[Style ID: u32]
[Style Name String ID: u16]
[Parent Style ID: u32]             # 0 = no parent, for inheritance
[Property Count: u16]
[Property Definitions]
[Style Flags: u16]
[Reserved: 4 bytes]
```

### Style Property Definition

```
[Property Name String ID: u16]
[Property Type: u8]
[Value Type: u8]
[Value Size: u16]
[Value Data]
```

### CSS-Like Property Types

| Property Type | ID | Description |
|---------------|----|----|
| Layout | 0x01 | width, height, padding, margin |
| Visual | 0x02 | background, color, border, shadow |
| Typography | 0x03 | fontSize, fontWeight, fontFamily |
| Transform | 0x04 | transition, animation, transform |
| Interaction | 0x05 | cursor, userSelect, pointerEvents |

### Built-in Style Property IDs

#### Meta & System Properties (0x0000-0x00FF)
| Property | ID | Type | Description |
|----------|----|----|-------------|
| id | 0x0001 | String | Element identifier |
| class | 0x0002 | String | CSS class name |
| style | 0x0003 | String | Style reference |
| theme | 0x0004 | String | Theme reference |
| extends | 0x0005 | String | Style inheritance |
| title | 0x0006 | String | Element title |
| version | 0x0007 | String | Version info |

#### Layout Properties (0x0100-0x01FF)
| Property | ID | Type | Description |
|----------|----|----|-------------|
| width | 0x0100 | Dimension | Element width |
| height | 0x0101 | Dimension | Element height |
| minWidth | 0x0102 | Dimension | Minimum width |
| maxWidth | 0x0103 | Dimension | Maximum width |
| minHeight | 0x0104 | Dimension | Minimum height |
| maxHeight | 0x0105 | Dimension | Maximum height |
| padding | 0x0106 | Spacing | Inner spacing |
| margin | 0x0107 | Spacing | Outer spacing |
| aspectRatio | 0x0109 | Number | Aspect ratio |
| flex | 0x010A | Number | Flex value |
| gap | 0x010B | Dimension | Gap spacing |
| posX | 0x010C | Float | X position |
| posY | 0x010D | Float | Y position |
| paddingTop | 0x0110 | Dimension | Top padding |
| paddingRight | 0x0111 | Dimension | Right padding |
| paddingBottom | 0x0112 | Dimension | Bottom padding |
| paddingLeft | 0x0113 | Dimension | Left padding |
| marginTop | 0x0114 | Dimension | Top margin |
| marginRight | 0x0115 | Dimension | Right margin |
| marginBottom | 0x0116 | Dimension | Bottom margin |
| marginLeft | 0x0117 | Dimension | Left margin |

#### Visual Properties (0x0200-0x02FF)
| Property | ID | Type | Description |
|----------|----|----|-------------|
| background | 0x0200 | Color/Gradient | Background color/gradient |
| color | 0x0201 | Color | Text color |
| border | 0x0202 | String | Border definition |
| borderRadius | 0x0203 | Number | Corner radius |
| boxShadow | 0x0204 | String | Drop shadow |
| opacity | 0x0205 | Number | Transparency |
| borderColor | 0x0206 | Color | Border color |
| borderWidth | 0x0207 | Number | Border width |
| visible | 0x0208 | Boolean | Visibility |

#### Typography Properties (0x0300-0x03FF)
| Property | ID | Type | Description |
|----------|----|----|-------------|
| fontSize | 0x0300 | Number | Font size |
| fontWeight | 0x0301 | Number/String | Font weight |
| fontFamily | 0x0302 | String | Font family |
| lineHeight | 0x0303 | Number | Line height |
| textAlign | 0x0304 | String | Text alignment |

#### Widget-Specific Properties (0x0600-0x06FF)
| Property | ID | Type | Description |
|----------|----|----|-------------|
| src | 0x0600 | String | Image source |
| value | 0x0601 | String | Input value |
| child | 0x0602 | Reference | Single child |
| children | 0x0603 | Array | Multiple children |
| placeholder | 0x0604 | String | Input placeholder |
| objectFit | 0x0605 | String | Image fit mode |
| mainAxis | 0x0606 | String | Main axis alignment |
| crossAxis | 0x0607 | String | Cross axis alignment |
| direction | 0x0608 | String | Flex direction |
| wrap | 0x0609 | String | Flex wrap |
| align | 0x060A | String | Align items |
| justify | 0x060B | String | Justify content |
| contentAlignment | 0x060E | String | Generic content alignment |
| text | 0x060F | String | Text content for Text widget |

## Theme Variable Table

### Theme Variable Group Structure

```
[Theme Group ID: u32]
[Theme Group Name String ID: u16]  # "colors", "spacing", "typography"
[Variable Count: u16]
[Variables]
[Theme Flags: u16]
```

### Theme Variable Definition

```
[Variable Name String ID: u16]     # "primary", "secondary", "sm", "md"
[Variable Type: u8]
[Value Type: u8]
[Value Size: u16]
[Value Data]
```

### Theme Variable Types

| Type | ID | Description |
|------|----|-------------|
| Color | 0x01 | Color values |
| Spacing | 0x02 | Spacing values |
| Typography | 0x03 | Typography values |
| Radius | 0x04 | Border radius values |
| Shadow | 0x05 | Shadow definitions |
| Custom | 0x06 | Custom theme variables |

### Theme Switching Support

```
[Active Theme Name String ID: u16] # "light", "dark", etc.
[Theme Count: u8]
[Theme Definitions]
```

### Theme Definition

```
[Theme Name String ID: u16]        # "light", "dark"
[Variable Override Count: u16]
[Variable Overrides]
```

### Variable Override

```
[Variable Group String ID: u16]    # "colors"
[Variable Name String ID: u16]     # "background"
[New Value Type: u8]
[New Value Size: u16]
[New Value Data]
```

## Widget Definition Table

### Widget Definition Structure

```
[Widget Type ID: u32]
[Widget Name String ID: u16]
[Property Definition Count: u16]
[Property Definitions]
[Default Property Values]
[Widget Flags: u32]
[Reserved: 8 bytes]
```

### Built-in Widget Type IDs

#### Layout Widgets (0x0000-0x00FF)  
| Widget | ID | Description |
|--------|----|-------------|
| Widget | 0x0000 | Base widget (abstract) |
| Column | 0x0001 | Vertical layout |
| Row | 0x0002 | Horizontal layout |
| Center | 0x0003 | Center child |
| Container | 0x0004 | Basic container |
| Flex | 0x0005 | Flexible layout |
| Spacer | 0x0006 | Flexible space |

#### Content Widgets (0x0400-0x04FF)
| Widget | ID | Description |
|--------|----|-------------|
| Text | 0x0400 | Text display |
| Button | 0x0401 | Interactive button |
| Image | 0x0402 | Image display |
| Input | 0x0403 | Text input |

#### Application Widgets (0x1000+)
| Widget | ID | Description |
|--------|----|-------------|
| App | 0x1000 | Application root |
| Modal | 0x1001 | Modal dialog |
| Form | 0x1002 | Form container |
| Grid | 0x1003 | Grid layout |
| List | 0x1004 | List container |

#### Custom Widgets (0x1000+)
Reserved for user-defined widgets

## Widget Instance Structure

```
[Instance ID: u32]
[Widget Type ID: u32]
[Parent Instance ID: u32]
[Style Reference ID: u32]          # 0 = no style
[Property Count: u16]
[Child Count: u16]
[Event Handler Count: u16]
[Widget Flags: u32]
[Properties]
[Child Instance IDs]
[Event Handlers]
```

## Property System

### Property Structure

```
[Property ID: u16]
[Value Type: u8]
[Flags: u8]
[Theme Variable Reference: u16]    # 0 = no reference
[Responsive Breakpoints: u8]       # Number of breakpoints
[Value Size: u32]
[Value Data]
[Responsive Values]                # If breakpoints > 0
```

### Value Types

| Type | Code | Description |
|------|------|-------------|
| Null | 0x00 | No value |
| Boolean | 0x01 | true/false |
| Integer | 0x02 | Signed 64-bit |
| Float | 0x03 | Double precision |
| String | 0x04 | String table index |
| Color | 0x05 | RGBA color |
| ThemeVariable | 0x06 | Theme variable reference |
| Dimension | 0x07 | Size with units |
| Spacing | 0x08 | TRBL spacing |
| Array | 0x09 | Array of values |
| Object | 0x0A | Key-value pairs |
| StyleReference | 0x0B | Style definition reference |

### Theme Variable Reference

```
[Theme Group String ID: u16]       # "colors", "spacing"
[Variable Name String ID: u16]     # "primary", "md"
```

### Responsive Values

```
[Breakpoint Name String ID: u16]   # "mobile", "tablet", "desktop"
[Value Type: u8]
[Value Size: u16]
[Value Data]
```

## Style Inheritance

### Style Inheritance Chain

```
[Child Style ID: u32]
[Parent Style ID: u32]
[Override Property Count: u16]
[Override Properties]
```

### Override Property

```
[Property ID: u16]
[Override Value Type: u8]
[Override Value Size: u16]
[Override Value Data]
```

## Event System

### Event Handler

```
[Event Type String ID: u16]        # "onClick", "onChange"
[Handler Function String ID: u16]  # Function name
[Handler Type: u8]                 # 0=built-in, 1=script
[Handler Flags: u8]
```

## Script Section

### Script Definition

```
[Script ID: u32]
[Script Language String ID: u16]   # Reserved for future scripting engines (empty when unused)
[Function Count: u16]
[Functions]
```

### Function Definition

```
[Function Name String ID: u16]
[Parameter Count: u8]
[Parameter Names]
[Code Size: u32]
[Code Data]
```

## Resource Section

### Resource Definition

```
[Resource ID: u32]
[Resource Type: u8]                # 0=image, 1=font, 2=audio, 3=video
[Resource Name String ID: u16]
[MIME Type String ID: u16]
[Data Size: u32]
[Data]
```

## Performance Optimizations

### Style Deduplication

```
[Unique Style Hash: u32]
[Reference Count: u16]
[Style Definition]
```

### Theme Variable Caching

```
[Computed Value Cache Size: u16]
[Cached Values]
```

### Widget Instance Pooling

```
[Pool ID: u16]
[Widget Type ID: u32]
[Pool Size: u16]
[Pool Flags: u16]
```

## Cross-Platform Considerations

### Platform-Specific Overrides

```
[Platform ID: u8]                  # 0=web, 1=native, 2=mobile
[Override Count: u16]
[Property Overrides]
```

### Platform Property Override

```
[Property ID: u16]
[Override Value Type: u8]
[Override Value Size: u16]
[Override Value Data]
```

## Checksum & Validation

- **Algorithm**: CRC32
- **Coverage**: All file data except checksum field
- **Validation**: Header validation, style inheritance validation, theme variable validation
- **Location**: End of file (4 bytes)

This binary format supports the Smart Hybrid System with CSS-like styling, widget-based layout, style inheritance, and enhanced theming with organized variable groups.
