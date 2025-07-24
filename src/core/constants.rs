// FILE: src/core/constants.rs

// KRB Format Constants
pub const KRB_MAGIC: &[u8; 4] = b"KRB1";
pub const KRB_VERSION_MAJOR: u8 = 0;
pub const KRB_VERSION_MINOR: u8 = 5;
pub const KRB_HEADER_SIZE: usize = 72;
pub const KRB_ELEMENT_HEADER_SIZE: usize = 19;

// Header Flags
pub const FLAG_HAS_STYLES: u16 = 1 << 0;
pub const FLAG_HAS_COMPONENT_DEFS: u16 = 1 << 1;
pub const FLAG_HAS_ANIMATIONS: u16 = 1 << 2;
pub const FLAG_HAS_RESOURCES: u16 = 1 << 3;
pub const FLAG_COMPRESSED: u16 = 1 << 4;
pub const FLAG_FIXED_POINT: u16 = 1 << 5;
pub const FLAG_EXTENDED_COLOR: u16 = 1 << 6;
pub const FLAG_HAS_APP: u16 = 1 << 7;
pub const FLAG_HAS_SCRIPTS: u16 = 1 << 8;
pub const FLAG_HAS_STATE_PROPERTIES: u16 = 1 << 9;
pub const FLAG_HAS_TEMPLATE_VARIABLES: u16 = 1 << 10;
pub const FLAG_HAS_TRANSFORMS: u16 = 1 << 11;

// Layout flags (must match renderer's LayoutDirection enum)
pub const LAYOUT_DIRECTION_MASK: u8 = 0x03;
pub const LAYOUT_DIRECTION_ROW: u8 = 0;        // Row layout
pub const LAYOUT_DIRECTION_COLUMN: u8 = 1;     // Column layout  
pub const LAYOUT_DIRECTION_ABSOLUTE: u8 = 2;   // Absolute positioning
// Note: ROW_REV and COL_REV are not supported by the renderer

pub const LAYOUT_ALIGNMENT_MASK: u8 = 0x0C;
pub const LAYOUT_ALIGNMENT_START: u8 = 0 << 2;
pub const LAYOUT_ALIGNMENT_CENTER: u8 = 1 << 2;
pub const LAYOUT_ALIGNMENT_END: u8 = 2 << 2;
pub const LAYOUT_ALIGNMENT_SPACE_BETWEEN: u8 = 3 << 2;

pub const LAYOUT_WRAP_BIT: u8 = 1 << 4;
pub const LAYOUT_GROW_BIT: u8 = 1 << 5;
pub const LAYOUT_ABSOLUTE_BIT: u8 = 1 << 6;

// Event Types
pub const EVENT_TYPE_CLICK: u8 = 0x01;
pub const EVENT_TYPE_PRESS: u8 = 0x02;
pub const EVENT_TYPE_RELEASE: u8 = 0x03;
pub const EVENT_TYPE_HOVER: u8 = 0x04;
pub const EVENT_TYPE_FOCUS: u8 = 0x05;
pub const EVENT_TYPE_BLUR: u8 = 0x06;
pub const EVENT_TYPE_CHANGE: u8 = 0x07;
pub const EVENT_TYPE_SUBMIT: u8 = 0x08;

// State flags for pseudo-selectors
pub const STATE_HOVER: u8 = 1 << 0;
pub const STATE_ACTIVE: u8 = 1 << 1;
pub const STATE_FOCUS: u8 = 1 << 2;
pub const STATE_DISABLED: u8 = 1 << 3;
pub const STATE_CHECKED: u8 = 1 << 4;

// Compiler limits
pub const MAX_ELEMENTS: usize = 1024;
pub const MAX_STRINGS: usize = 1024;
pub const MAX_PROPERTIES: usize = 64;
pub const MAX_CUSTOM_PROPERTIES: usize = 32;
pub const MAX_STYLES: usize = 256;
pub const MAX_CHILDREN: usize = 256;
pub const MAX_EVENTS: usize = 16;
pub const MAX_RESOURCES: usize = 256;
pub const MAX_INCLUDE_DEPTH: usize = 16;
pub const MAX_COMPONENT_DEFS: usize = 128;
pub const MAX_BLOCK_DEPTH: usize = 64;
pub const MAX_LINE_LENGTH: usize = 2048;
