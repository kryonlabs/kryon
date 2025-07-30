# KRB Binary Format Specification

This document provides the complete specification for Kryon's binary KRB format, including every hex value mapping, data types, and binary structure.

> **🔗 Related Documentation**: 
> - [KRYON_REFERENCE.md](KRYON_REFERENCE.md) - Element inheritance and relationships
> - [KRB_TO_HTML_MAPPING.md](KRB_TO_HTML_MAPPING.md) - HTML conversion mappings

## Table of Contents
- [Binary Format Overview](#binary-format-overview)
- [KRY to KRB Compilation Process](#kry-to-krb-compilation-process)
- [Element Type Mappings](#element-type-mappings)
- [Property Type Mappings](#property-type-mappings)
- [Directive Type Mappings](#directive-type-mappings)
- [Advanced Feature Encodings](#advanced-feature-encodings)
- [Data Type Specifications](#data-type-specifications)
- [Binary Structure](#binary-structure)
- [Encoding Rules](#encoding-rules)
- [Implementation Guidelines](#implementation-guidelines)

## Binary Format Overview

KRB (Kryon Binary) is a compact binary format for storing Kryon UI descriptions. Each element and property is identified by a unique hex value for efficient storage and parsing.

### File Header
```
Magic Number: 0x4B524259 ("KRBY")
Version: 4 bytes (major.minor.patch.build)
Flags: 4 bytes (compression, encryption, etc.)
Element Count: 4 bytes
Total Size: 4 bytes
```

### Data Organization
```
[Header][Element_1][Element_2]...[Element_N]
```

Each element contains:
```
[Element_Type:1][Property_Count:2][Properties...][Child_Count:2][Children...]
```

## KRY to KRB Compilation Process

This section details how KRY source code is compiled into KRB binary format.

### Compilation Pipeline

```
KRY Source → Lexing → Parsing → AST → Optimization → KRB Binary
```

1. **Lexing**: Convert text to tokens
2. **Parsing**: Build Abstract Syntax Tree (AST)
3. **Optimization**: Resolve aliases, apply inheritance, constant folding
4. **Binary Generation**: Convert AST to KRB format

### Example Compilation

#### Input KRY Source
```kry
App {
    title: "My Application"
    windowWidth: 1200
    windowHeight: 800
    backgroundColor: "#f5f5f5"
    
    Container {
        padding: 20
        gap: 16
        flexDirection: "column"
        
        Text {
            text: "Welcome to Kryon"
            fontSize: 24
            fontWeight: 700
            textColor: "#333"
        }
        
        Button {
            text: "Click Me"
            backgroundColor: "#007bff"
            textColor: "white"
            padding: 12
            borderRadius: 6
            onClick: "handleClick()"
        }
    }
}
```

#### Step 1: AST Generation
```rust
Element {
    element_type: App(0x00),
    properties: [
        Property { prop_type: 0xA2, value: String("My Application") },  // windowTitle
        Property { prop_type: 0xA0, value: Int(1200) },                // windowWidth  
        Property { prop_type: 0xA1, value: Int(800) },                 // windowHeight
        Property { prop_type: 0x01, value: Color("#f5f5f5") },         // backgroundColor
    ],
    children: [
        Element {
            element_type: Container(0x01),
            properties: [
                Property { prop_type: 0x70, value: Dimension(20px) },     // padding
                Property { prop_type: 0x23, value: Float(16) },           // gap
                Property { prop_type: 0x40, value: String("column") },    // flexDirection
            ],
            children: [
                Element {
                    element_type: Text(0x02),
                    properties: [
                        Property { prop_type: 0x10, value: String("Welcome to Kryon") }, // textContent
                        Property { prop_type: 0x11, value: Float(24) },                  // fontSize
                        Property { prop_type: 0x12, value: Int(700) },                   // fontWeight
                        Property { prop_type: 0x02, value: Color("#333") },              // textColor
                    ],
                    children: []
                },
                Element {
                    element_type: Button(0x10),
                    properties: [
                        Property { prop_type: 0x10, value: String("Click Me") },         // textContent
                        Property { prop_type: 0x01, value: Color("#007bff") },           // backgroundColor
                        Property { prop_type: 0x02, value: Color("white") },             // textColor
                        Property { prop_type: 0x70, value: Dimension(12px) },            // padding
                        Property { prop_type: 0x05, value: Float(6) },                   // borderRadius
                    ],
                    scripts: [
                        Script { trigger: 0x00, code: "handleClick()" }  // onClick
                    ],
                    children: []
                }
            ]
        }
    ]
}
```

#### Step 2: Binary Encoding Process

**File Header (16 bytes)**
```
4B 52 42 59  // Magic "KRBY" 
01 00 00 00  // Version 1.0.0.0
00 00 00 00  // Flags (no compression)
01 00 00 00  // Element count: 1 (root App)
XX XX XX XX  // Total file size (calculated)
```

**Root App Element**
```
00           // Element type: App (0x00)
04 00        // Property count: 4
```

**App Properties**
```
// windowTitle: "My Application"
A2           // Property type: windowTitle (0xA2)
01           // Value type: String
0E 00        // String length: 14
4D 79 20 41 70 70 6C 69 63 61 74 69 6F 6E  // "My Application"

// windowWidth: 1200
A0           // Property type: windowWidth (0xA0)
02           // Value type: Int
B0 04 00 00  // Int value: 1200 (little endian)

// windowHeight: 800
A1           // Property type: windowHeight (0xA1)
02           // Value type: Int
20 03 00 00  // Int value: 800 (little endian)

// backgroundColor: "#f5f5f5"
01           // Property type: backgroundColor (0x01)
05           // Value type: Color
03           // Color type: Named Color
07 00        // String length: 7
23 66 35 66 35 66 35  // "#f5f5f5"
```

**App Children**
```
01 00        // Child count: 1
```

**Container Element**
```
01           // Element type: Container (0x01)
03 00        // Property count: 3

// padding: 20px
70           // Property type: padding (0x70)
06           // Value type: Dimension
00           // Dimension type: Pixels
00 00 A0 41  // Float value: 20.0

// gap: 16
23           // Property type: gap (0x23)
03           // Value type: Float
00 00 80 41  // Float value: 16.0

// flexDirection: "column"
40           // Property type: flexDirection (0x40)
01           // Value type: String
06 00        // String length: 6
63 6F 6C 75 6D 6E  // "column"

02 00        // Child count: 2
```

**Text Element**
```
02           // Element type: Text (0x02)
04 00        // Property count: 4

// textContent: "Welcome to Kryon"
10           // Property type: textContent (0x10)
01           // Value type: String
10 00        // String length: 16
57 65 6C 63 6F 6D 65 20 74 6F 20 4B 72 79 6F 6E  // "Welcome to Kryon"

// fontSize: 24
11           // Property type: fontSize (0x11)
03           // Value type: Float
00 00 C0 41  // Float value: 24.0

// fontWeight: 700
12           // Property type: fontWeight (0x12)
02           // Value type: Int
BC 02 00 00  // Int value: 700

// textColor: "#333"
02           // Property type: textColor (0x02)
05           // Value type: Color
03           // Color type: Named Color
04 00        // String length: 4
23 33 33 33  // "#333"

00 00        // Child count: 0
```

**Button Element**
```
10           // Element type: Button (0x10)
05 00        // Property count: 5

// textContent: "Click Me"
10           // Property type: textContent (0x10)
01           // Value type: String
08 00        // String length: 8
43 6C 69 63 6B 20 4D 65  // "Click Me"

// backgroundColor: "#007bff"
01           // Property type: backgroundColor (0x01)
05           // Value type: Color
03           // Color type: Named Color
07 00        // String length: 7
23 30 30 37 62 66 66  // "#007bff"

// textColor: "white"
02           // Property type: textColor (0x02)
05           // Value type: Color
03           // Color type: Named Color
05 00        // String length: 5
77 68 69 74 65  // "white"

// padding: 12px
70           // Property type: padding (0x70)
06           // Value type: Dimension
00           // Dimension type: Pixels
00 00 40 41  // Float value: 12.0

// borderRadius: 6
05           // Property type: borderRadius (0x05)
03           // Value type: Float
00 00 C0 40  // Float value: 6.0

00 00        // Child count: 0

01           // Script count: 1
00           // Script trigger: onClick (0x00)
0C 00        // Script code length: 12
68 61 6E 64 6C 65 43 6C 69 63 6B 28 29  // "handleClick()"
```

### Compilation Phases in Detail

#### Phase 1: Lexical Analysis
```rust
enum Token {
    Identifier(String),      // App, Container, Text, etc.
    String(String),         // "My Application"
    Number(f32),           // 1200, 24.0
    Symbol(char),          // {, }, :, ,
    Keyword(String),       // @if, @for, etc.
}
```

**Tokenization Example:**
```kry
Container { padding: 20 }
```
→
```rust
[
    Token::Identifier("Container"),
    Token::Symbol('{'),
    Token::Identifier("padding"),
    Token::Symbol(':'),
    Token::Number(20.0),
    Token::Symbol('}')
]
```

#### Phase 2: Syntax Analysis
```rust
struct Parser {
    tokens: Vec<Token>,
    position: usize,
}

impl Parser {
    fn parse_element(&mut self) -> Element {
        let element_name = self.expect_identifier();
        self.expect_symbol('{');
        
        let mut properties = Vec::new();
        let mut children = Vec::new();
        
        while !self.is_symbol('}') {
            if self.is_element_name() {
                children.push(self.parse_element());
            } else {
                properties.push(self.parse_property());
            }
        }
        
        Element { element_name, properties, children }
    }
}
```

#### Phase 3: Alias Resolution
```rust
fn resolve_aliases(element: &mut Element) {
    for property in &mut element.properties {
        property.name = match property.name.as_str() {
            "bg" => "backgroundColor".to_string(),
            "color" => "textColor".to_string(),
            "size" => "fontSize".to_string(),
            "title" => "windowTitle".to_string(),
            _ => property.name.clone()
        };
    }
    
    for child in &mut element.children {
        resolve_aliases(child);
    }
}
```

#### Phase 4: Property Validation
```rust
fn validate_properties(element: &Element) -> Result<(), CompileError> {
    let valid_properties = get_valid_properties_for_element(&element.element_type);
    
    for property in &element.properties {
        if !valid_properties.contains(&property.name) {
            return Err(CompileError::InvalidProperty {
                element: element.element_type.clone(),
                property: property.name.clone()
            });
        }
        
        validate_property_value(&property.name, &property.value)?;
    }
    
    Ok(())
}
```

#### Phase 5: Binary Generation
```rust
struct KrbWriter {
    buffer: Vec<u8>,
}

impl KrbWriter {
    fn write_element(&mut self, element: &Element) {
        // Write element type
        let element_hex = element_type_to_hex(&element.element_type);
        self.write_u8(element_hex);
        
        // Write property count
        self.write_u16(element.properties.len() as u16);
        
        // Write properties
        for property in &element.properties {
            self.write_property(property);
        }
        
        // Write child count
        self.write_u16(element.children.len() as u16);
        
        // Write children
        for child in &element.children {
            self.write_element(child);
        }
    }
    
    fn write_property(&mut self, property: &Property) {
        let property_hex = property_name_to_hex(&property.name);
        self.write_u8(property_hex);
        self.write_property_value(&property.value);
    }
}
```

### File Size Calculation

For the example above:
- **Header**: 16 bytes
- **App Element**: 1 + 2 + (properties) + 2 + (children)
- **Properties**: 4 properties × ~10 bytes average = ~40 bytes
- **Container Element**: 1 + 2 + (properties) + 2 + (children)
- **Text Element**: 1 + 2 + (properties) + 2 = ~35 bytes
- **Button Element**: 1 + 2 + (properties) + 2 + (script) = ~50 bytes

**Total estimated size**: ~150 bytes (compared to ~450 bytes of source KRY text)

### Compilation Optimizations

#### Constant Folding
```kry
Container {
    width: 100 + 50    // Compiled to: width: 150
}
```

#### Dead Code Elimination
```kry
@const_if(false) {
    Text { text: "Never shown" }    // Removed during compilation
}
```

#### Property Deduplication
```kry
Container {
    backgroundColor: "red"
    bg: "blue"                      // Overrides previous, only "blue" stored
}
```

This compilation process ensures that KRY source code is efficiently converted to a compact, optimized KRB binary format while preserving all functionality and maintaining cross-platform compatibility.

## Element Type Mappings

Complete mapping of hex values to element types.

### Core Elements (0x00-0x0F)
| Hex | Element | Description | Binary Encoding |
|-----|---------|-------------|-----------------|
| `0x00` | App | Application root | `00` |
| `0x01` | Container | Layout container | `01` |
| `0x02` | Text | Text display | `02` |
| `0x03` | EmbedView | Platform-specific embed | `03` |
| `0x04` | *Reserved* | Future core element | `04` |
| `0x05` | *Reserved* | Future core element | `05` |
| `0x06` | *Reserved* | Future core element | `06` |
| `0x07` | *Reserved* | Future core element | `07` |
| `0x08` | *Reserved* | Future core element | `08` |
| `0x09` | *Reserved* | Future core element | `09` |
| `0x0A` | *Reserved* | Future core element | `0A` |
| `0x0B` | *Reserved* | Future core element | `0B` |
| `0x0C` | *Reserved* | Future core element | `0C` |
| `0x0D` | *Reserved* | Future core element | `0D` |
| `0x0E` | *Reserved* | Future core element | `0E` |
| `0x0F` | *Reserved* | Future core element | `0F` |

### Interactive Elements (0x10-0x1F)
| Hex | Element | Description | Binary Encoding |
|-----|---------|-------------|-----------------|
| `0x10` | Button | Interactive button | `10` |
| `0x11` | Input | User input field | `11` |
| `0x12` | Link | Clickable link | `12` |
| `0x13` | *Reserved* | Future interactive | `13` |
| `0x14` | *Reserved* | Future interactive | `14` |
| `0x15` | *Reserved* | Future interactive | `15` |
| `0x16` | *Reserved* | Future interactive | `16` |
| `0x17` | *Reserved* | Future interactive | `17` |
| `0x18` | *Reserved* | Future interactive | `18` |
| `0x19` | *Reserved* | Future interactive | `19` |
| `0x1A` | *Reserved* | Future interactive | `1A` |
| `0x1B` | *Reserved* | Future interactive | `1B` |
| `0x1C` | *Reserved* | Future interactive | `1C` |
| `0x1D` | *Reserved* | Future interactive | `1D` |
| `0x1E` | *Reserved* | Future interactive | `1E` |
| `0x1F` | *Reserved* | Future interactive | `1F` |

### Media Elements (0x20-0x2F)
| Hex | Element | Description | Binary Encoding |
|-----|---------|-------------|-----------------|
| `0x20` | Image | Image display | `20` |
| `0x21` | Video | Video playback | `21` |
| `0x22` | Canvas | Drawing canvas | `22` |
| `0x23` | Svg | SVG graphics | `23` |
| `0x24` | *Reserved* | Future media | `24` |
| `0x25` | *Reserved* | Future media | `25` |
| `0x26` | *Reserved* | Future media | `26` |
| `0x27` | *Reserved* | Future media | `27` |
| `0x28` | *Reserved* | Future media | `28` |
| `0x29` | *Reserved* | Future media | `29` |
| `0x2A` | *Reserved* | Future media | `2A` |
| `0x2B` | *Reserved* | Future media | `2B` |
| `0x2C` | *Reserved* | Future media | `2C` |
| `0x2D` | *Reserved* | Future media | `2D` |
| `0x2E` | *Reserved* | Future media | `2E` |
| `0x2F` | *Reserved* | Future media | `2F` |

### Form Controls (0x30-0x3F)
| Hex | Element | Description | Binary Encoding |
|-----|---------|-------------|-----------------|
| `0x30` | Select | Dropdown selection | `30` |
| `0x31` | Slider | Range control | `31` |
| `0x32` | ProgressBar | Progress indicator | `32` |
| `0x33` | Checkbox | Boolean input | `33` |
| `0x34` | RadioGroup | Radio selection | `34` |
| `0x35` | Toggle | Switch control | `35` |
| `0x36` | DatePicker | Date selection | `36` |
| `0x37` | FilePicker | File selection | `37` |
| `0x38` | *Reserved* | Future form control | `38` |
| `0x39` | *Reserved* | Future form control | `39` |
| `0x3A` | *Reserved* | Future form control | `3A` |
| `0x3B` | *Reserved* | Future form control | `3B` |
| `0x3C` | *Reserved* | Future form control | `3C` |
| `0x3D` | *Reserved* | Future form control | `3D` |
| `0x3E` | *Reserved* | Future form control | `3E` |
| `0x3F` | *Reserved* | Future form control | `3F` |

### Semantic Elements (0x40-0x4F)
| Hex | Element | Description | Binary Encoding |
|-----|---------|-------------|-----------------|
| `0x40` | Form | Form container | `40` |
| `0x41` | List | List container | `41` |
| `0x42` | ListItem | List item | `42` |
| `0x43` | *Reserved* | Future semantic | `43` |
| `0x44` | *Reserved* | Future semantic | `44` |
| `0x45` | *Reserved* | Future semantic | `45` |
| `0x46` | *Reserved* | Future semantic | `46` |
| `0x47` | *Reserved* | Future semantic | `47` |
| `0x48` | *Reserved* | Future semantic | `48` |
| `0x49` | *Reserved* | Future semantic | `49` |
| `0x4A` | *Reserved* | Future semantic | `4A` |
| `0x4B` | *Reserved* | Future semantic | `4B` |
| `0x4C` | *Reserved* | Future semantic | `4C` |
| `0x4D` | *Reserved* | Future semantic | `4D` |
| `0x4E` | *Reserved* | Future semantic | `4E` |
| `0x4F` | *Reserved* | Future semantic | `4F` |

### Table Elements (0x50-0x5F)
| Hex | Element | Description | Binary Encoding |
|-----|---------|-------------|-----------------|
| `0x50` | Table | Table container | `50` |
| `0x51` | TableRow | Table row | `51` |
| `0x52` | TableCell | Table data cell | `52` |
| `0x53` | TableHeader | Table header cell | `53` |
| `0x54` | *Reserved* | Future table element | `54` |
| `0x55` | *Reserved* | Future table element | `55` |
| `0x56` | *Reserved* | Future table element | `56` |
| `0x57` | *Reserved* | Future table element | `57` |
| `0x58` | *Reserved* | Future table element | `58` |
| `0x59` | *Reserved* | Future table element | `59` |
| `0x5A` | *Reserved* | Future table element | `5A` |
| `0x5B` | *Reserved* | Future table element | `5B` |
| `0x5C` | *Reserved* | Future table element | `5C` |
| `0x5D` | *Reserved* | Future table element | `5D` |
| `0x5E` | *Reserved* | Future table element | `5E` |
| `0x5F` | *Reserved* | Future table element | `5F` |

### Custom Elements (0x60-0xFF)
| Range | Purpose | Description |
|-------|---------|-------------|
| `0x60-0x6F` | Reserved | Future built-in elements |
| `0x70-0x7F` | Reserved | Future built-in elements |
| `0x80-0x8F` | Reserved | Future built-in elements |
| `0x90-0x9F` | Reserved | Future built-in elements |
| `0xA0-0xAF` | Reserved | Future built-in elements |
| `0xB0-0xBF` | Reserved | Future built-in elements |
| `0xC0-0xCF` | Reserved | Future built-in elements |
| `0xD0-0xDF` | Reserved | Future built-in elements |
| `0xE0-0xEF` | User-defined | Custom user elements |
| `0xF0-0xFF` | Extensions | Plugin/extension elements |

## Property Type Mappings

Complete mapping of hex values to property types.

### Visual Properties (0x01-0x0F)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0x01` | backgroundColor | backgroundColor | Color | No |
| `0x02` | textColor | textColor | Color | Yes |
| `0x03` | borderColor | borderColor | Color | No |
| `0x04` | borderWidth | borderWidth | Float | No |
| `0x05` | borderRadius | borderRadius | Float | No |
| `0x06` | opacity | opacity | Float | Yes |
| `0x07` | visibility | visibility | Bool | Yes |
| `0x08` | cursor | cursor | String | Yes |
| `0x09` | boxShadow | boxShadow | String | No |
| `0x0A` | imageSource | imageSource | String | No |
| `0x0B` | *Reserved* | - | - | - |
| `0x0C` | *Reserved* | - | - | - |
| `0x0D` | *Reserved* | - | - | - |
| `0x0E` | *Reserved* | - | - | - |
| `0x0F` | *Reserved* | - | - | - |

### Text Properties (0x10-0x1F)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0x10` | textContent | textContent | String | No |
| `0x11` | fontSize | fontSize | Float | Yes |
| `0x12` | fontWeight | fontWeight | Int | Yes |
| `0x13` | textAlignment | textAlignment | String | Yes |
| `0x14` | fontFamily | fontFamily | String | Yes |
| `0x15` | whiteSpace | whiteSpace | String | Yes |
| `0x16` | listStyleType | listStyleType | String | Yes |
| `0x17` | *Reserved* | - | - | - |
| `0x18` | *Reserved* | - | - | - |
| `0x19` | *Reserved* | - | - | - |
| `0x1A` | *Reserved* | - | - | - |
| `0x1B` | *Reserved* | - | - | - |
| `0x1C` | *Reserved* | - | - | - |
| `0x1D` | *Reserved* | - | - | - |
| `0x1E` | *Reserved* | - | - | - |
| `0x1F` | *Reserved* | - | - | - |

### Layout Properties (0x20-0x2F)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0x20` | width | width | Dimension | No |
| `0x21` | height | height | Dimension | No |
| `0x22` | layoutFlags | layoutFlags | Flags | No |
| `0x23` | gap | gap | Float | No |
| `0x24` | zIndex | zIndex | Int | No |
| `0x25` | display | display | String | No |
| `0x26` | position | position | String | No |
| `0x27` | transform | transform | Transform | No |
| `0x28` | styleId | styleId | String | No |
| `0x29` | *Reserved* | - | - | - |
| `0x2A` | *Reserved* | - | - | - |
| `0x2B` | *Reserved* | - | - | - |
| `0x2C` | *Reserved* | - | - | - |
| `0x2D` | *Reserved* | - | - | - |
| `0x2E` | *Reserved* | - | - | - |
| `0x2F` | *Reserved* | - | - | - |

### Size Constraints (0x30-0x3F)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0x30` | minWidth | minWidth | Dimension | No |
| `0x31` | minHeight | minHeight | Dimension | No |
| `0x32` | maxWidth | maxWidth | Dimension | No |
| `0x33` | maxHeight | maxHeight | Dimension | No |
| `0x34` | *Reserved* | - | - | - |
| `0x35` | *Reserved* | - | - | - |
| `0x36` | *Reserved* | - | - | - |
| `0x37` | *Reserved* | - | - | - |
| `0x38` | *Reserved* | - | - | - |
| `0x39` | *Reserved* | - | - | - |
| `0x3A` | *Reserved* | - | - | - |
| `0x3B` | *Reserved* | - | - | - |
| `0x3C` | *Reserved* | - | - | - |
| `0x3D` | *Reserved* | - | - | - |
| `0x3E` | *Reserved* | - | - | - |
| `0x3F` | *Reserved* | - | - | - |

### Flexbox Properties (0x40-0x4F)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0x40` | flexDirection | flexDirection | String | No |
| `0x41` | flexWrap | flexWrap | String | No |
| `0x42` | flexGrow | flexGrow | Float | No |
| `0x43` | flexShrink | flexShrink | Float | No |
| `0x44` | flexBasis | flexBasis | Dimension | No |
| `0x45` | alignItems | alignItems | String | No |
| `0x46` | alignContent | alignContent | String | No |
| `0x47` | alignSelf | alignSelf | String | No |
| `0x48` | justifyContent | justifyContent | String | No |
| `0x49` | justifyItems | justifyItems | String | No |
| `0x4A` | justifySelf | justifySelf | String | No |
| `0x4B` | *Reserved* | - | - | - |
| `0x4C` | *Reserved* | - | - | - |
| `0x4D` | *Reserved* | - | - | - |
| `0x4E` | *Reserved* | - | - | - |
| `0x4F` | *Reserved* | - | - | - |

### Position Properties (0x50-0x5F)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0x50` | left | left | Dimension | No |
| `0x51` | top | top | Dimension | No |
| `0x52` | right | right | Dimension | No |
| `0x53` | bottom | bottom | Dimension | No |
| `0x54` | *Reserved* | - | - | - |
| `0x55` | *Reserved* | - | - | - |
| `0x56` | *Reserved* | - | - | - |
| `0x57` | *Reserved* | - | - | - |
| `0x58` | *Reserved* | - | - | - |
| `0x59` | *Reserved* | - | - | - |
| `0x5A` | *Reserved* | - | - | - |
| `0x5B` | *Reserved* | - | - | - |
| `0x5C` | *Reserved* | - | - | - |
| `0x5D` | *Reserved* | - | - | - |
| `0x5E` | *Reserved* | - | - | - |
| `0x5F` | *Reserved* | - | - | - |

### Grid Properties (0x60-0x6F)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0x60` | gridTemplateColumns | gridTemplateColumns | String | No |
| `0x61` | gridTemplateRows | gridTemplateRows | String | No |
| `0x62` | gridTemplateAreas | gridTemplateAreas | String | No |
| `0x63` | gridAutoColumns | gridAutoColumns | String | No |
| `0x64` | gridAutoRows | gridAutoRows | String | No |
| `0x65` | gridAutoFlow | gridAutoFlow | String | No |
| `0x66` | gridArea | gridArea | String | No |
| `0x67` | gridColumn | gridColumn | String | No |
| `0x68` | gridRow | gridRow | String | No |
| `0x69` | gridColumnStart | gridColumnStart | Int | No |
| `0x6A` | gridColumnEnd | gridColumnEnd | Int | No |
| `0x6B` | gridRowStart | gridRowStart | Int | No |
| `0x6C` | gridRowEnd | gridRowEnd | Int | No |
| `0x6D` | gridGap | gridGap | Dimension | No |
| `0x6E` | gridColumnGap | gridColumnGap | Dimension | No |
| `0x6F` | gridRowGap | gridRowGap | Dimension | No |

### Spacing Properties (0x70-0x7F)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0x70` | padding | padding | Dimension | No |
| `0x71` | paddingTop | paddingTop | Dimension | No |
| `0x72` | paddingRight | paddingRight | Dimension | No |
| `0x73` | paddingBottom | paddingBottom | Dimension | No |
| `0x74` | paddingLeft | paddingLeft | Dimension | No |
| `0x75` | margin | margin | Dimension | No |
| `0x76` | marginTop | marginTop | Dimension | No |
| `0x77` | marginRight | marginRight | Dimension | No |
| `0x78` | marginBottom | marginBottom | Dimension | No |
| `0x79` | marginLeft | marginLeft | Dimension | No |
| `0x7A` | *Reserved* | - | - | - |
| `0x7B` | *Reserved* | - | - | - |
| `0x7C` | *Reserved* | - | - | - |
| `0x7D` | *Reserved* | - | - | - |
| `0x7E` | *Reserved* | - | - | - |
| `0x7F` | *Reserved* | - | - | - |

### Border Properties (0x80-0x8F)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0x80` | borderTopWidth | borderTopWidth | Float | No |
| `0x81` | borderRightWidth | borderRightWidth | Float | No |
| `0x82` | borderBottomWidth | borderBottomWidth | Float | No |
| `0x83` | borderLeftWidth | borderLeftWidth | Float | No |
| `0x84` | borderTopColor | borderTopColor | Color | No |
| `0x85` | borderRightColor | borderRightColor | Color | No |
| `0x86` | borderBottomColor | borderBottomColor | Color | No |
| `0x87` | borderLeftColor | borderLeftColor | Color | No |
| `0x88` | borderTopLeftRadius | borderTopLeftRadius | Float | No |
| `0x89` | borderTopRightRadius | borderTopRightRadius | Float | No |
| `0x8A` | borderBottomRightRadius | borderBottomRightRadius | Float | No |
| `0x8B` | borderBottomLeftRadius | borderBottomLeftRadius | Float | No |
| `0x8C` | *Reserved* | - | - | - |
| `0x8D` | *Reserved* | - | - | - |
| `0x8E` | *Reserved* | - | - | - |
| `0x8F` | *Reserved* | - | - | - |

### Box Model Properties (0x90-0x9F)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0x90` | boxSizing | boxSizing | String | No |
| `0x91` | outline | outline | String | No |
| `0x92` | outlineColor | outlineColor | Color | No |
| `0x93` | outlineWidth | outlineWidth | Float | No |
| `0x94` | outlineOffset | outlineOffset | Float | No |
| `0x95` | overflow | overflow | String | No |
| `0x96` | overflowX | overflowX | String | No |
| `0x97` | overflowY | overflowY | String | No |
| `0x98` | *Reserved* | - | - | - |
| `0x99` | *Reserved* | - | - | - |
| `0x9A` | *Reserved* | - | - | - |
| `0x9B` | *Reserved* | - | - | - |
| `0x9C` | *Reserved* | - | - | - |
| `0x9D` | *Reserved* | - | - | - |
| `0x9E` | *Reserved* | - | - | - |
| `0x9F` | *Reserved* | - | - | - |

### Window Properties (0xA0-0xAF)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0xA0` | windowWidth | windowWidth | Int | No |
| `0xA1` | windowHeight | windowHeight | Int | No |
| `0xA2` | windowTitle | windowTitle | String | No |
| `0xA3` | windowResizable | windowResizable | Bool | No |
| `0xA4` | windowFullscreen | windowFullscreen | Bool | No |
| `0xA5` | windowVsync | windowVsync | Bool | No |
| `0xA6` | windowTargetFps | windowTargetFps | Int | No |
| `0xA7` | windowAntialiasing | windowAntialiasing | Int | No |
| `0xA8` | windowIcon | windowIcon | String | No |
| `0xA9` | *Reserved* | - | - | - |
| `0xAA` | *Reserved* | - | - | - |
| `0xAB` | *Reserved* | - | - | - |
| `0xAC` | *Reserved* | - | - | - |
| `0xAD` | *Reserved* | - | - | - |
| `0xAE` | *Reserved* | - | - | - |
| `0xAF` | *Reserved* | - | - | - |

### Transform Properties (0xB0-0xBF)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0xB0` | scale | scale | Float | No |
| `0xB1` | scaleX | scaleX | Float | No |
| `0xB2` | scaleY | scaleY | Float | No |
| `0xB3` | scaleZ | scaleZ | Float | No |
| `0xB4` | translateX | translateX | Float | No |
| `0xB5` | translateY | translateY | Float | No |
| `0xB6` | translateZ | translateZ | Float | No |
| `0xB7` | rotate | rotate | String | No |
| `0xB8` | rotateX | rotateX | String | No |
| `0xB9` | rotateY | rotateY | String | No |
| `0xBA` | rotateZ | rotateZ | String | No |
| `0xBB` | skewX | skewX | String | No |
| `0xBC` | skewY | skewY | String | No |
| `0xBD` | perspective | perspective | Float | No |
| `0xBE` | matrix | matrix | Array | No |
| `0xBF` | *Reserved* | - | - | - |

### Semantic Properties (0xC0-0xCF)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0xC0` | semanticRole | semanticRole | String | No |
| `0xC1` | headingLevel | headingLevel | Int | No |
| `0xC2` | listType | listType | String | No |
| `0xC3` | listItemRole | listItemRole | String | No |
| `0xC4` | tableSection | tableSection | String | No |
| `0xC5` | interactiveType | interactiveType | String | No |
| `0xC6` | mediaType | mediaType | String | No |
| `0xC7` | embedType | embedType | String | No |
| `0xC8` | inputType | inputType | String | No |
| `0xC9` | *Reserved* | - | - | - |
| `0xCA` | *Reserved* | - | - | - |
| `0xCB` | *Reserved* | - | - | - |
| `0xCC` | *Reserved* | - | - | - |
| `0xCD` | *Reserved* | - | - | - |
| `0xCE` | *Reserved* | - | - | - |
| `0xCF` | *Reserved* | - | - | - |

### Text Formatting Properties (0xD0-0xDF)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0xD0` | fontStyle | fontStyle | String | Yes |
| `0xD1` | textDecoration | textDecoration | String | Yes |
| `0xD2` | verticalAlign | verticalAlign | String | No |
| `0xD3` | lineHeight | lineHeight | Float | Yes |
| `0xD4` | letterSpacing | letterSpacing | Float | Yes |
| `0xD5` | wordSpacing | wordSpacing | Float | Yes |
| `0xD6` | textIndent | textIndent | Dimension | Yes |
| `0xD7` | textTransform | textTransform | String | Yes |
| `0xD8` | textShadow | textShadow | String | Yes |
| `0xD9` | wordWrap | wordWrap | String | Yes |
| `0xDA` | textOverflow | textOverflow | String | No |
| `0xDB` | writingMode | writingMode | String | Yes |
| `0xDC` | *Reserved* | - | - | - |
| `0xDD` | *Reserved* | - | - | - |
| `0xDE` | *Reserved* | - | - | - |
| `0xDF` | *Reserved* | - | - | - |

### Extended Visual Properties (0xE0-0xEF)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0xE0` | backgroundImage | backgroundImage | String | No |
| `0xE1` | backgroundRepeat | backgroundRepeat | String | No |
| `0xE2` | backgroundPosition | backgroundPosition | String | No |
| `0xE3` | backgroundSize | backgroundSize | String | No |
| `0xE4` | backgroundAttachment | backgroundAttachment | String | No |
| `0xE5` | borderStyle | borderStyle | String | No |
| `0xE6` | borderImage | borderImage | String | No |
| `0xE7` | filter | filter | String | No |
| `0xE8` | backdropFilter | backdropFilter | String | No |
| `0xE9` | clipPath | clipPath | String | No |
| `0xEA` | mask | mask | String | No |
| `0xEB` | mixBlendMode | mixBlendMode | String | No |
| `0xEC` | objectFit | objectFit | String | No |
| `0xED` | objectPosition | objectPosition | String | No |
| `0xEE` | *Reserved* | - | - | - |
| `0xEF` | *Reserved* | - | - | - |

### Special Properties (0xF0-0xFF)
| Hex | Property | camelCase Name | Data Type | Inheritable |
|-----|----------|----------------|-----------|-------------|
| `0xF0` | spans | spans | Int | No |
| `0xF1` | richTextContent | richTextContent | String | No |
| `0xF2` | accessibilityLabel | accessibilityLabel | String | No |
| `0xF3` | accessibilityRole | accessibilityRole | String | No |
| `0xF4` | accessibilityDescription | accessibilityDescription | String | No |
| `0xF5` | dataAttributes | dataAttributes | Object | No |
| `0xF6` | customProperties | customProperties | Object | No |
| `0xF7` | *Reserved* | - | - | - |
| `0xF8` | *Reserved* | - | - | - |
| `0xF9` | *Reserved* | - | - | - |
| `0xFA` | *Reserved* | - | - | - |
| `0xFB` | *Reserved* | - | - | - |
| `0xFC` | *Reserved* | - | - | - |
| `0xFD` | *Reserved* | - | - | - |
| `0xFE` | *Reserved* | - | - | - |
| `0xFF` | *Reserved* | - | - | - |

## Data Type Specifications

### Basic Data Types

#### String (Variable Length)
```
[Length:2][UTF-8 Bytes...]
```
- Length: 2-byte unsigned integer (0-65535)
- Data: UTF-8 encoded string

#### Int (4 bytes)
```
[Value:4] (Little Endian)
```
- Signed 32-bit integer
- Range: -2,147,483,648 to 2,147,483,647

#### Float (4 bytes)
```
[Value:4] (IEEE 754 Little Endian)
```
- 32-bit floating point
- IEEE 754 standard

#### Bool (1 byte)
```
[Value:1]
```
- 0x00 = false
- 0x01 = true

#### Color
```
[Type:1][Data...]
```
- Type 0x00: RGB (3 bytes) `[R][G][B]`
- Type 0x01: RGBA (4 bytes) `[R][G][B][A]`
- Type 0x02: HSL (3 floats) `[H:4][S:4][L:4]`
- Type 0x03: Named Color (String)

#### Dimension
```
[Type:1][Data...]
```
- Type 0x00: Pixels (Float) `[Value:4]`
- Type 0x01: Percentage (Float) `[Value:4]`
- Type 0x02: Auto `(No data)`
- Type 0x03: Em (Float) `[Value:4]`
- Type 0x04: Rem (Float) `[Value:4]`

#### Transform
```
[Matrix:64] (16 x 4-byte floats)
```
- 4x4 transformation matrix
- Column-major order
- IEEE 754 floats

#### Flags (4 bytes)
```
[Flags:4] (Bitfield)
```
- 32-bit bitfield for boolean flags
- Each bit represents a specific flag

#### Array
```
[Count:2][Items...]
```
- Count: Number of items (0-65535)
- Items: Serialized based on array element type

#### Object
```
[Count:2][Key-Value Pairs...]
```
- Count: Number of key-value pairs
- Each pair: `[Key:String][Value:Varies]`

### Complex Data Types

#### Property Value
```
[Type:1][Data...]
```
Property values are discriminated unions:
- Type 0x00: Null
- Type 0x01: String
- Type 0x02: Int
- Type 0x03: Float
- Type 0x04: Bool
- Type 0x05: Color
- Type 0x06: Dimension
- Type 0x07: Transform
- Type 0x08: Flags
- Type 0x09: Array
- Type 0x0A: Object

## Binary Structure

### Element Encoding
```
Element {
    element_type: u8,           // Element type hex value
    property_count: u16,        // Number of properties
    properties: [Property],     // Array of properties
    child_count: u16,           // Number of child elements
    children: [Element]         // Array of child elements
}
```

### Property Encoding
```
Property {
    property_type: u8,          // Property type hex value
    value: PropertyValue        // Property value with type discrimination
}
```

### Script/Event Encoding
```
Script {
    trigger: u8,                // Event trigger type
    code_length: u16,           // Length of script code
    code: [u8]                  // Script bytecode or source
}
```

#### Event Trigger Types
| Hex | Event | Description |
|-----|-------|-------------|
| `0x00` | onClick | Mouse click event |
| `0x01` | onHover | Mouse hover event |
| `0x02` | onChange | Value change event |
| `0x03` | onFocus | Focus gained event |
| `0x04` | onBlur | Focus lost event |
| `0x05` | onKeyPress | Key press event |
| `0x06` | onLoad | Element loaded event |
| `0x07` | *Reserved* | Future event type |

## Encoding Rules

### 1. Element Ordering
- Elements are encoded in document order (parent before children)
- Properties within an element can be in any order
- Children follow all properties

### 2. String Encoding
- All strings use UTF-8 encoding
- Length prefix includes byte count, not character count
- Empty strings have length 0

### 3. Endianness
- All multi-byte integers use little-endian encoding
- IEEE 754 floats use little-endian encoding
- This matches most modern architectures

### 4. Padding
- No padding between fields
- All data is packed tightly for minimal file size

### 5. Version Compatibility
- Files include version information in header
- Parsers should validate version compatibility
- Unknown properties/elements should be skipped gracefully

### 6. Compression
- KRB files may be compressed (indicated in header flags)
- Recommended: LZ4 for speed, DEFLATE for size
- Compression applied after binary encoding

## Implementation Guidelines

### Parser Implementation
```rust
struct KrbParser {
    data: Vec<u8>,
    position: usize,
}

impl KrbParser {
    fn read_u8(&mut self) -> u8 { /* ... */ }
    fn read_u16(&mut self) -> u16 { /* ... */ }
    fn read_u32(&mut self) -> u32 { /* ... */ }
    fn read_f32(&mut self) -> f32 { /* ... */ }
    fn read_string(&mut self) -> String { /* ... */ }
    fn read_property_value(&mut self) -> PropertyValue { /* ... */ }
    fn read_element(&mut self) -> Element { /* ... */ }
}
```

### Property Resolution
1. **Parse binary property** using hex key
2. **Resolve aliases** to canonical property names  
3. **Apply inheritance** for inheritable properties
4. **Validate values** against property constraints
5. **Convert for backend** (HTML/CSS, native rendering, etc.)

### Error Handling
- Invalid hex values should be logged and skipped
- Malformed data should fail gracefully
- Version mismatches should be reported clearly
- Large files should be streamed when possible

### Performance Considerations
- Use memory mapping for large files
- Cache parsed elements for reuse

---

## Directive Type Mappings

Modern Kryon supports advanced directives that require special binary encoding:

### Configuration Directives (0xC0-0xCF)
| Hex | Directive | Description | Binary Structure |
|-----|-----------|-------------|------------------|
| `0xC0` | `@variables` | Variable declarations | `[count:2][var_entries...]` |
| `0xC1` | `@styles` | Style definitions | `[count:2][style_entries...]` |
| `0xC2` | `@metadata` | App metadata | `[key_count:2][key_value_pairs...]` |
| `0xC3` | `@import` | File imports | `[path_len:2][path_string][type:1]` |
| `0xC4` | `@script` | Script blocks | `[lang_id:1][code_len:4][code_bytes...]` |

### State Management Directives (0xD0-0xDF)
| Hex | Directive | Description | Binary Structure |
|-----|-----------|-------------|------------------|
| `0xD0` | `@store` | Global state store | `[name_len:2][name][state_config...]` |
| `0xD1` | `@on_mount` | Mount lifecycle | `[action_len:2][action_string]` |
| `0xD2` | `@on_unmount` | Unmount lifecycle | `[action_len:2][action_string]` |
| `0xD3` | `@watch` | Variable watcher | `[var_len:2][var_name][handler...]` |

### API Integration Directives (0xE0-0xEF)
| Hex | Directive | Description | Binary Structure |
|-----|-----------|-------------|------------------|
| `0xE0` | `@api` | API configuration | `[name_len:2][name][config_block...]` |
| `0xE1` | `@fetch` | Data fetching | `[url_len:2][url][options_block...]` |
| `0xE2` | `@websocket` | WebSocket connection | `[url_len:2][url][config_block...]` |
| `0xE3` | `@database` | Database config | `[provider_len:2][provider][config...]` |

### Mobile Platform Directives (0xF0-0xFF)
| Hex | Directive | Description | Binary Structure |
|-----|-----------|-------------|------------------|
| `0xF0` | `@mobile` | Mobile platform config | `[platform_flags:1][config_block...]` |
| `0xF1` | `@device` | Device integration | `[capabilities_mask:4][config...]` |
| `0xF2` | `@hardware` | Hardware access | `[sensors_mask:4][config_block...]` |
| `0xF3` | `@navigation` | Native navigation | `[nav_type:1][config_block...]` |
| `0xF4` | `@gestures` | Gesture recognition | `[gesture_mask:2][config_block...]` |
| `0xF5` | `@haptics` | Haptic feedback | `[pattern_count:2][patterns...]` |

---

## Advanced Feature Encodings

### Global State Store Encoding (@store)
```
Structure: [0xD0][name_len:2][name_string][state_block][actions_block]

State Block:
[property_count:2]
  [prop_name_len:2][prop_name][prop_type:1][prop_value...]...

Actions Block:  
[action_count:2]
  [action_name_len:2][action_name][code_len:4][code_bytes...]...

Example Binary (hex):
D0 0009 75736572537461746520  // @store "userState"
  0002                         // 2 state properties
    0004 75736572 01 0000       // user: null (type 0x01, null value)
    0009 69734C6F6767656449 02 00 // isLoggedIn: false (type 0x02, bool false)
  0001                         // 1 action
    0005 6C6F67696E             // "login"
    0012 66756E6374696F6E206C6F67696E28757365722920... // function code
```

### Mobile Platform Configuration (@mobile)
```
Structure: [0xF0][platform_flags:1][capability_blocks...]

Platform Flags (bitmask):
0x01 = iOS support
0x02 = Android support  
0x04 = Native UI components
0x08 = Hardware access enabled

Capability Block:
[capability_type:1][config_length:2][config_data...]

Capability Types:
0x01 = Camera
0x02 = Location
0x03 = Notifications
0x04 = Storage
0x05 = Sensors

Example Binary (hex):
F0 03           // @mobile, iOS + Android
  01 0008       // Camera capability, 8 bytes config
    01 01 01 01 // photo, video, barcode, QR enabled
    01 F4       // high quality (500 = 0x01F4)
  02 0004       // Location capability, 4 bytes config  
    01 01       // GPS + network enabled
    03          // best accuracy
    01          // background enabled
```

### API Integration Encoding (@api)
```
Structure: [0xE0][name_len:2][name][config_block]

Config Block:
[base_url_len:2][base_url]
[endpoint_count:2]
  [method:1][path_len:2][path][handler_len:2][handler]...

HTTP Methods:
0x01 = GET
0x02 = POST
0x03 = PUT
0x04 = DELETE
0x05 = PATCH

Example Binary (hex):
E0 0007 757365724150490000  // @api "userAPI"
  0015 68747470733A2F2F6170692E6578616D706C652E636F6D // "https://api.example.com"
  0002                      // 2 endpoints
    01 0006 2F7573657273      // GET "/users"
    000A 676574416C6C5573657273 // "getAllUsers"
    02 0006 2F6C6F67696E      // POST "/login"  
    0009 68616E646C654C6F67696E // "handleLogin"
```

### Build System Configuration (@build)
```
Structure: [0xC5][target_count:1][target_blocks...]

Target Block:
[target_type:1][config_length:2][config_data...]

Target Types:
0x01 = Web
0x02 = Desktop (Windows)
0x03 = Desktop (macOS)
0x04 = Desktop (Linux)
0x05 = Mobile (iOS)
0x06 = Mobile (Android)
0x07 = Terminal/CLI

Web Target Config:
[output_dir_len:2][output_dir]
[index_template_len:2][index_template]
[bundle_flags:1] // 0x01=minify, 0x02=source-maps, 0x04=tree-shake

Mobile Target Config:
[app_id_len:2][app_id]
[min_version:4] // iOS: version number, Android: API level
[permissions_count:1][permission_ids...]

Example Binary (hex):
C5 03           // @build, 3 targets
  01 0010       // Web target, 16 bytes config
    0004 64697374 // "dist" output directory
    000A 696E6465782E68746D6C // "index.html" template
    07          // minify + source-maps + tree-shake
  05 0012       // iOS target, 18 bytes config  
    000F 636F6D2E6578616D706C652E617070 // "com.example.app"
    000E0000    // iOS 14.0 (version 14.0)
    03          // 3 permissions
    01 02 03    // Camera, Location, Notifications
```

### Security Configuration (@security)
```
Structure: [0xC6][csp_block][auth_block][encryption_block]

CSP Block:
[directive_count:1]
  [directive_type:1][sources_count:1][source_entries...]

Auth Block:
[provider_type:1][config_length:2][config_data...]

Encryption Block:
[key_length:1][encryption_key][algorithm:1]

Example Binary (hex):
C6                          // @security
  02                        // 2 CSP directives
    01 02                   // default-src, 2 sources
      0004 27736656C662700   // "'self'"
      0016 68747470733A2F2F6170692E6578616D706C652E636F6D // "https://api.example.com"
    02 01                   // script-src, 1 source
      0004 27736656C662700   // "'self'"
  01 0020                   // OAuth2 provider, 32 bytes config
    // OAuth2 configuration data...
  20                        // 32-byte encryption key
    // 32 bytes of key data...
  01                        // AES-256 algorithm
```

### Testing Framework Configuration (@testing)
```
Structure: [0xC7][test_count:2][test_blocks...]

Test Block:
[test_name_len:2][test_name]
[test_type:1] // 0x01=unit, 0x02=integration, 0x03=visual, 0x04=performance
[config_length:2][config_data...]

Example Binary (hex):
C7 0003                     // @testing, 3 tests
  0010 55736572204C6F67696E2054657374 // "User Login Test"
  02                        // Integration test
  0008                      // 8 bytes config
    0001                    // 1 scenario
    0004 6C6F67696E          // "login" action
  000F 427574746F6E20436C69636B2054657374 // "Button Click Test"  
  01                        // Unit test
  0004                      // 4 bytes config
    001E                    // 30 second timeout
    01                      // assertions enabled
```

### Environment Configuration (@environment)
```
Structure: [0xC8][env_count:1][env_blocks...]

Environment Block:
[env_name_len:2][env_name]
[variable_count:2]
  [var_name_len:2][var_name][var_value_len:2][var_value]...

Example Binary (hex):
C8 03                       // @environment, 3 environments
  000B 646576656C6F706D656E74 // "development"
  0003                      // 3 variables
    0007 4150495F55524C      // "API_URL"
    0016 687474703A2F2F6C6F63616C686F73743A33303030 // "http://localhost:3000"
    0005 4445425547           // "DEBUG"  
    0004 74727565             // "true"
    0008 4D4F434B5F44415441   // "MOCK_DATA"
    0004 74727565             // "true"
```

---

## Script Language Encoding

### Language Identifiers
| ID | Language | Description |
|----|----------|-------------|
| `0x01` | Lua | Primary scripting language |
| `0x02` | JavaScript | Web compatibility |
| `0x03` | Python | Data processing |
| `0x04` | Wren | Game scripting |
| `0x05` | Kryon Lisp | Functional programming |

### Script Block Structure
```
[0xC4][lang_id:1][code_len:4][code_bytes...][function_table]

Function Table:
[function_count:2]
  [func_name_len:2][func_name][offset:4][length:4]...
```

This allows runtime to quickly locate and call specific functions without parsing the entire script.

---

## Binary Size Optimizations

### String Interning
Frequently used strings are stored once with reference IDs:

```
String Table: [string_count:2][string_entries...]
String Entry: [id:2][length:2][string_bytes...]

Usage: [string_ref_marker:1][string_id:2]
```

### Property Compression
Common property combinations are pre-encoded:

```
// Instead of encoding each property separately:
backgroundColor: "#FFFFFF"
textColor: "#000000"  
padding: 16

// Use combo ID:
[combo_id:1] // 0x80 = white background + black text + 16px padding
```

### Template Variables
Compile-time constants are resolved and inlined:

```
// KRY: text: "Version " + $APP_VERSION
// KRB: [string_literal] "Version 1.2.3" (if APP_VERSION = "1.2.3")
```

This comprehensive update ensures the KRB binary format can encode all the advanced features we added to the KRY language specification!
- Lazy-load child elements when possible
- Pre-allocate collections based on counts

This binary format specification ensures efficient, unambiguous encoding of Kryon UI descriptions while maintaining compatibility across different backends and versions.