// crates/kryon-core/src/krb.rs
use crate::{Element, ElementId, ElementType, PropertyValue, Result, KryonError, TextAlignment, Style, CursorType, InteractionState, EventType, TransformData, TransformType, TransformProperty, TransformPropertyType, CSSUnitValue, CSSUnit, LayoutSize, LayoutPosition, LayoutDimension, OverflowType};
use kryon_shared::PropertyId; 
use std::collections::HashMap;
use glam::Vec4;

#[derive(Debug)]
pub struct KRBFile {
    pub header: KRBHeader,
    pub strings: Vec<String>,
    pub elements: HashMap<u32, Element>,
    pub styles: HashMap<u8, Style>, 
    pub root_element_id: Option<u32>,
    pub resources: Vec<String>,
    pub scripts: Vec<ScriptEntry>,
    pub template_variables: Vec<TemplateVariable>,
    pub template_bindings: Vec<TemplateBinding>,
    pub transforms: Vec<TransformData>,
    pub fonts: HashMap<String, String>, // font_family -> font_path
}

#[derive(Debug)]
pub struct KRBHeader {
    pub magic: [u8; 4],
    pub version: u16,
    pub flags: u16,
    pub element_count: u16,
    pub style_count: u16,
    pub component_count: u16,
    pub script_count: u16,
    pub string_count: u16,
    pub resource_count: u16,
    pub template_variable_count: u16,
    pub template_binding_count: u16,
    pub transform_count: u16,
}

#[derive(Debug, Clone)]
pub struct ScriptEntry {
    pub language: String,
    pub name: String,
    pub bytecode: Vec<u8>,
    pub source_code: Option<String>, // Store original source code for web compilation
    pub entry_points: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct TemplateVariable {
    pub name: String,
    pub value_type: u8,
    pub default_value: String,
}

#[derive(Debug, Clone)]
pub struct TemplateBinding {
    pub element_index: u16,
    pub property_id: u8,
    pub template_expression: String,
    pub variable_indices: Vec<u8>,
}

pub struct KRBParser {
    data: Vec<u8>,
    position: usize,
}

impl KRBParser {
    pub fn new(data: Vec<u8>) -> Self {
        Self { data, position: 0 }
    }
    
    pub fn parse(&mut self) -> Result<KRBFile> {
        let header = self.parse_header()?;
        
        if &header.magic != b"KRB1" {
            return Err(KryonError::InvalidKRB("Invalid magic number".to_string()));
        }
        
        if header.version > 0x0500 {
            return Err(KryonError::UnsupportedVersion(header.version));
        }
        
        let strings = self.parse_string_table(&header)?;
        let styles = self.parse_style_table(&header, &strings)?;
        let mut elements = self.parse_element_tree(&header, &strings)?;
        let resources = self.parse_resource_table(&header)?;
        let scripts = self.parse_script_table(&header, &strings)?;
        let template_variables = self.parse_template_variables(&header, &strings)?;
        let template_bindings = self.parse_template_bindings(&header, &strings)?;
        let transforms = self.parse_transforms(&header)?;
        
        // Apply transforms to elements
        self.apply_transforms_to_elements(&mut elements, &transforms)?;
        
        // Apply style-based layout flags to elements
        self.apply_style_layout_flags(&mut elements, &styles)?;
        
        // Find root element (App type) or create default App wrapper
        let root_element_id = if let Some((id, _)) = elements.iter()
            .find(|(_, element)| element.element_type == ElementType::App) {
            Some(*id)
        } else {
            // No App element found, create a default App wrapper
            Self::create_default_app_wrapper(&mut elements)
        };
        
        // Parse fonts from strings - look for adjacent font family and file patterns
        let mut fonts = HashMap::new();
        
        for (i, string) in strings.iter().enumerate() {
            // Look for font files (.ttf, .otf, etc.)
            if string.ends_with(".ttf") || string.ends_with(".otf") || string.ends_with(".woff") || string.ends_with(".woff2") {
                eprintln!("[KRB_FONT] Found font file at index {}: '{}'", i, string);
                
                // Look for a font family name in the preceding strings (within a reasonable range)
                // Font declarations are stored as adjacent strings, but other strings may be interleaved
                let mut found_font_name = false;
                for j in (0..i).rev() {
                    if i - j > 5 { break; } // Don't look too far back
                    
                    let potential_font_name = &strings[j];
                    eprintln!("[KRB_FONT] Checking potential font name at index {}: '{}'", j, potential_font_name);
                    
                    // Font family names should be simple identifiers, not style names or other data
                    // Based on the example, we expect 'calistoga' -> 'Calistoga-Regular.ttf'
                    if !potential_font_name.is_empty() && 
                       !potential_font_name.starts_with('#') && // Not a color
                       !potential_font_name.contains('/') && // Not a path
                       !potential_font_name.contains('\\') && // Not a Windows path
                       !potential_font_name.ends_with(".ttf") && // Not itself a font file
                       !potential_font_name.ends_with(".otf") &&
                       !potential_font_name.ends_with("_font") && // Not a style name like "title_font"
                       !potential_font_name.contains('.') && // Generally no dots in font names
                       !potential_font_name.contains(' ') && // No spaces in font family names
                       potential_font_name.len() < 32 && // Reasonable length
                       potential_font_name.len() > 2 && // Not too short
                       potential_font_name.chars().all(|c| c.is_alphanumeric() || c == '_' || c == '-') {
                        
                        // Check if this looks like a font family name vs other identifiers
                        // Font family names are typically lowercase and not UI element IDs
                        if potential_font_name.chars().any(|c| c.is_lowercase()) &&
                           potential_font_name != "default" { // "default" is too generic
                            eprintln!("[KRB_FONT] Parsed font mapping: '{}' -> '{}'", potential_font_name, string);
                            fonts.insert(potential_font_name.clone(), string.clone());
                            found_font_name = true;
                            break; // Take the first valid font name found
                        } else {
                            eprintln!("[KRB_FONT] Rejected font name (too generic or wrong pattern): '{}'", potential_font_name);
                        }
                    } else {
                        eprintln!("[KRB_FONT] Rejected potential font name (invalid format): '{}'", potential_font_name);
                    }
                }
                
                if !found_font_name {
                    eprintln!("[KRB_FONT] No valid font family name found for: '{}'", string);
                }
            }
        }

        Ok(KRBFile {
            header,
            strings,
            elements,
            styles,
            root_element_id,
            resources,
            scripts,
            template_variables,
            template_bindings,
            transforms,
            fonts,
        })
    }
    
    fn parse_style_table(&mut self, header: &KRBHeader, strings: &[String]) -> Result<HashMap<u8, Style>> {
        let style_offset = self.read_u32_at(32) as usize;
        let mut styles = HashMap::new();
        
        eprintln!("[STYLE] Parsing {} styles from offset 0x{:X}", header.style_count, style_offset);
        
        self.position = style_offset;
        
        for i in 0..header.style_count {
            // Add bounds checking before reading
            if self.position + 3 > self.data.len() {
                eprintln!("[STYLE] ERROR: Not enough data to read style header at position {}, file size {}", self.position, self.data.len());
                break;
            }
            
            let style_id = self.read_u8(); // Read the actual style ID from file
            let name_index = self.read_u8() as usize;
            let property_count = self.read_u8();

            // Sanity check the values
            if name_index > 200 || property_count > 50 {
                eprintln!("[STYLE] ERROR: Corrupted style data at position {}: style_id={}, name_index={}, property_count={}", 
                         self.position - 3, style_id, name_index, property_count);
                eprintln!("[STYLE] ERROR: This suggests we're reading beyond the style table or the file is corrupted");
                break;
            }

            let name = if name_index < strings.len() {
                strings[name_index].clone()
            } else {
                format!("style_{}", style_id)
            };

            eprintln!("[STYLE] Style {} (iteration {}): name='{}', name_index={}, props={}", style_id, i, name, name_index, property_count);

            let mut properties = HashMap::new();
            for j in 0..property_count {
                // Check if we have enough data for property header
                if self.position + 3 > self.data.len() {
                    eprintln!("[STYLE] ERROR: Not enough data to read property {} header at position {}", j, self.position);
                    break;
                }
                
                let prop_id = self.read_u8();
                let _value_type = self.read_u8(); // We can use this for more robust parsing later
                let size = self.read_u8();
                
                // Sanity check property size
                if size > 200 {
                    eprintln!("[STYLE] ERROR: Property {} has unrealistic size {}, skipping", j, size);
                    break;
                }
                
                // Check if we have enough data for the property value
                if self.position + size as usize > self.data.len() {
                    eprintln!("[STYLE] ERROR: Not enough data to read property {} value (size {}) at position {}", j, size, self.position);
                    break;
                }
                
                eprintln!("[STYLE]   Property {}: id=0x{:02X}, size={}", j, prop_id, size);
                
                let value = match prop_id {
                    0x01 | 0x02 | 0x03 => PropertyValue::Color(self.read_color()),
                    0x04 | 0x05 => { // BorderWidth, BorderRadius
                        if size == 2 {
                            PropertyValue::Float(self.read_u16() as f32)
                        } else if size == 1 {
                            PropertyValue::Float(self.read_u8() as f32)
                        } else {
                            // Skip if wrong size
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x06 => {
                        if size == 1 {
                            PropertyValue::Int(self.read_u8() as i32) // Layout flags
                        } else {
                            // Skip if wrong size
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x19 => { // Width
                        if size == 2 {
                            PropertyValue::Float(self.read_u16() as f32)
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x1B => { // LayoutFlags
                        if size == 1 {
                            PropertyValue::Int(self.read_u8() as i32)
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x09 => { // FontSize
                        if size == 2 {
                            PropertyValue::Float(self.read_u16() as f32)
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x0A => { // FontWeight
                        if size == 2 {
                            PropertyValue::Int(self.read_u16() as i32)
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x0B => {
                        if size == 1 {
                            PropertyValue::Int(self.read_u8() as i32) // TextAlignment
                        } else {
                            // Skip if wrong size
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x0C => { // FontFamily
                        if size == 1 {
                            let string_index = self.read_u8() as usize;
                            if string_index < strings.len() {
                                PropertyValue::String(strings[string_index].clone())
                            } else {
                                eprintln!("[STYLE]     FontFamily: invalid string index {}", string_index);
                                continue;
                            }
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x07 => { // Margin
                        if size == 1 {
                            PropertyValue::Float(self.read_u8() as f32)
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x1A => { // Height
                        if size == 2 {
                            PropertyValue::Float(self.read_u16() as f32)
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    // Modern Taffy layout properties (0x40-0x4F range) - REMOVED duplicates, now handled in custom property section
                    0x42 => { // FlexWrap
                        if size == 1 {
                            let string_index = self.read_u8() as usize;
                            if string_index < strings.len() {
                                PropertyValue::String(strings[string_index].clone())
                            } else {
                                eprintln!("[STYLE]     FlexWrap: invalid string index {}", string_index);
                                continue;
                            }
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x43 => { // FlexGrow
                        if size == 4 {
                            let flex_grow_bytes = [self.read_u8(), self.read_u8(), self.read_u8(), self.read_u8()];
                            PropertyValue::Float(f32::from_le_bytes(flex_grow_bytes))
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x44 => { // FlexShrink
                        if size == 4 {
                            let flex_shrink_bytes = [self.read_u8(), self.read_u8(), self.read_u8(), self.read_u8()];
                            PropertyValue::Float(f32::from_le_bytes(flex_shrink_bytes))
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x45 => { // FlexBasis
                        if size == 1 {
                            let string_index = self.read_u8() as usize;
                            if string_index < strings.len() {
                                PropertyValue::String(strings[string_index].clone())
                            } else {
                                eprintln!("[STYLE]     FlexBasis: invalid string index {}", string_index);
                                continue;
                            }
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x40 => { // Display (byte value)
                        if size == 1 {
                            let value = self.read_u8() as i32;
                            eprintln!("[STYLE]     Display: parsed value={}", value);
                            PropertyValue::Int(value)
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x41 => { // FlexDirection (byte value)
                        if size == 1 {
                            let value = self.read_u8() as i32;
                            eprintln!("[STYLE]     FlexDirection: parsed value={}", value);
                            PropertyValue::Int(value)
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x46 => { // AlignItems (byte value)
                        if size == 1 {
                            let value = self.read_u8() as i32;
                            eprintln!("[STYLE]     AlignItems: parsed value={}", value);
                            PropertyValue::Int(value)
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    // Modern Taffy alignment properties - REMOVED duplicates with wrong hex mappings
                    // 0x46 = AlignItems, 0x47 = AlignContent, 0x48 = AlignSelf 
                    // Now handled correctly in custom property section with centralized PropertyId
                    0x49 => { // JustifyContent (byte value)
                        if size == 1 {
                            PropertyValue::Int(self.read_u8() as i32)
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x4A => { // JustifyContent (bytes)
                        if size == 1 {
                            let value = self.read_u8();
                            let justify_str = match value {
                                0 => "flex-start",
                                1 => "flex-end",
                                2 => "center",
                                3 => "space-between",
                                4 => "space-around", 
                                5 => "space-evenly",
                                _ => "flex-start", // Default
                            };
                            PropertyValue::String(justify_str.to_string())
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x4B => { // JustifySelf
                        if size == 1 {
                            let string_index = self.read_u8() as usize;
                            if string_index < strings.len() {
                                PropertyValue::String(strings[string_index].clone())
                            } else {
                                eprintln!("[STYLE]     JustifySelf: invalid string index {}", string_index);
                                continue;
                            }
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x50 => { // Position
                        if size == 1 {
                            let value = self.read_u8();
                            let position_str = match value {
                                0 => "static",
                                1 => "relative",
                                2 => "absolute",
                                3 => "fixed",
                                4 => "sticky",
                                _ => "static", // Default
                            };
                            PropertyValue::String(position_str.to_string())
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x51 => { // Left
                        if size == 2 {
                            let left = self.read_u16() as f32;
                            PropertyValue::Float(left)
                        } else {
                            for _ in 0..size { self.read_u8(); }
                            continue;
                        }
                    }
                    0x52 => { // Top
                        if size == 2 {
                            let top = self.read_u16() as f32;
                            PropertyValue::Float(top)
                        } else {
                            if !self.skip_bytes(size as usize) {
                                eprintln!("[STYLE] WARNING: Failed to skip {} bytes for property 0x{:02X}, stopping style parsing", size, prop_id);
                                break;
                            }
                            continue;
                        }
                    }
                    // Add other property types here
                    _ => {
                        // For unknown properties, read the raw bytes and display them
                        eprintln!("[STYLE]     Unknown property 0x{:02X}, size={}, reading raw bytes...", prop_id, size);
                        
                        // Check if we have enough bytes remaining
                        let bytes_remaining = self.data.len() - self.position;
                        if size as usize > bytes_remaining {
                            eprintln!("[STYLE]     ERROR: Property size {} exceeds remaining bytes {}, stopping style parsing to prevent crash", size, bytes_remaining);
                            break; // Stop parsing this style to prevent crash
                        }
                        
                        // Safely skip the unknown property bytes
                        if !self.skip_bytes(size as usize) {
                            eprintln!("[STYLE]     ERROR: Failed to skip {} bytes for unknown property 0x{:02X}, stopping style parsing", size, prop_id);
                            break;
                        }
                        continue;
                    }
                };
                properties.insert(prop_id, value);
            }
            
            eprintln!("[STYLE] Loaded style {}: '{}' with {} properties", style_id, name, properties.len());
            // Ensure we don't overwrite existing styles with the same ID
            if !styles.contains_key(&style_id) {
                styles.insert(style_id, Style { name, properties });
            } else {
                eprintln!("[STYLE] WARNING: Duplicate style ID {} - skipping", style_id);
            }
        }

        eprintln!("[STYLE] Parsed {} styles total", styles.len());
        Ok(styles)
    }

    fn parse_header(&mut self) -> Result<KRBHeader> {
        if self.data.len() < 68 {
            return Err(KryonError::InvalidKRB("File too small".to_string()));
        }
        
        let mut magic = [0u8; 4];
        magic.copy_from_slice(&self.data[0..4]);
        
        Ok(KRBHeader {
            magic,
            version: self.read_u16_at(4),
            flags: self.read_u16_at(6),
            element_count: self.read_u16_at(8),
            style_count: self.read_u16_at(10),
            component_count: self.read_u16_at(12),
            script_count: self.read_u16_at(16),
            string_count: self.read_u16_at(18),
            resource_count: self.read_u16_at(20),
            template_variable_count: self.read_u16_at(22),
            template_binding_count: self.read_u16_at(24),
            transform_count: self.read_u16_at(26),
        })
    }
    
    fn parse_string_table(&mut self, header: &KRBHeader) -> Result<Vec<String>> {
        let string_offset = self.read_u32_at(48) as usize;
        let mut strings = Vec::new();
        
        self.position = string_offset;
        
        for _ in 0..header.string_count {
            let length = self.read_u8() as usize;
            if self.position + length > self.data.len() {
                panic!("KRB parsing error: trying to read string of length {} at position {} but data length is {}", length, self.position, self.data.len());
            }
            let string_data = &self.data[self.position..self.position + length];
            let string = String::from_utf8_lossy(string_data).to_string();
            strings.push(string);
            self.position += length;
        }
        
        Ok(strings)
    }
    
    fn parse_element_tree(&mut self, header: &KRBHeader, strings: &[String]) -> Result<HashMap<u32, Element>> {
        let element_offset = self.read_u32_at(28) as usize;
        let mut elements = HashMap::new();
        
        self.position = element_offset;
        
        for element_id in 0..header.element_count {
            let element = self.parse_element(element_id as u32, strings)?;
            elements.insert(element_id as u32, element);
        }
        
        // Build parent-child relationships from tree structure
        self.build_element_hierarchy(&mut elements, header.element_count as u32);
        
        Ok(elements)
    }
    fn parse_element(&mut self, element_id: u32, strings: &[String]) -> Result<Element> {
        let mut element = Element::default();
        
        // Parse element header (19 bytes)
        let element_type = ElementType::from(self.read_u8());
        let id_index = self.read_u8();
        let pos_x = self.read_u16() as f32;
        let pos_y = self.read_u16() as f32;
        let width = self.read_u16() as f32;
        let height = self.read_u16() as f32;
        let layout_flags = self.read_u8();
        let style_id = self.read_u8();
        let checked = self.read_u8() != 0;
        let property_count = self.read_u8();
        let child_count = self.read_u8();
        let _event_count = self.read_u8();
        let _animation_count = self.read_u8();
        let custom_prop_count = self.read_u8();
        let _state_prop_count = self.read_u8();
        
        element.element_type = element_type;
        
        // Set default cursor type for interactive elements
        if element_type == ElementType::Button {
            element.cursor = CursorType::Pointer;
            eprintln!("[PARSE] Auto-set cursor to Pointer for Button element");
        }
        
        element.id = if id_index > 0 && (id_index as usize) < strings.len() {
            strings[id_index as usize].clone()
        } else {
            format!("element_{}", element_id)
        };
        
        element.style_id = style_id; 
        // Initialize layout fields with pixel values
        element.layout_position = LayoutPosition::pixels(pos_x, pos_y);
        element.layout_size = LayoutSize::pixels(width, height);
        
        // TODO: Parse percentage values from custom properties when compiler supports it
        // For now we'll use a simple enhancement for testing
        element.layout_flags = layout_flags;
        
        // Set initial interaction state based on checked field
        element.current_state = if checked {
            InteractionState::Checked
        } else {
            InteractionState::Normal
        };
        
        // Store original layout_flags for later style merging
        let _original_layout_flags = layout_flags;
        
        eprintln!("[PARSE] Element {}: type={:?}, style_id={}, layout_flags={:08b}, props={}, children={}, custom_props={}", 
            element_id, element_type, style_id, layout_flags, property_count, child_count, custom_prop_count);
        
        // Parse standard properties
        for i in 0..property_count {
            eprintln!("[PARSE] Parsing standard property {} for element {}", i, element_id);
            self.parse_standard_property(&mut element, strings)?;
        }
        
        // Parse custom properties  
        for i in 0..custom_prop_count {
            eprintln!("[PARSE] Parsing custom property {} for element {}", i, element_id);
            self.parse_custom_property(&mut element, strings)?;
        }
        
        // Skip state property sets (TODO: implement properly)
        for _ in 0.._state_prop_count {
            let _state_flags = self.read_u8();
            let state_property_count = self.read_u8();
            for _ in 0..state_property_count {
                // Skip state properties (same format as standard properties)
                let _property_id = self.read_u8();
                let _value_type = self.read_u8();
                let size = self.read_u8();
                for _ in 0..size {
                    self.read_u8();
                }
            }
        }
        
        // Parse events
        for _ in 0.._event_count {
            let event_type_id = self.read_u8();
            let callback_string_index = self.read_u8() as usize;
            
            if let Some(event_type) = self.event_type_from_id(event_type_id) {
                if callback_string_index < strings.len() {
                    let callback_name = strings[callback_string_index].clone();
                    element.event_handlers.insert(event_type, callback_name);
                    eprintln!("[EVENT] Added {} event handler: {}", self.event_type_name(event_type), strings[callback_string_index]);
                }
            }
        }
        
        // Skip child element offsets (TODO: implement properly)
        for _ in 0..child_count {
            self.read_u16(); // child offset
        }
        
        // Initialize children vector based on child_count
        element.children = Vec::with_capacity(child_count as usize);
        
        // Convert modern CSS properties to layout_flags if present
        self.convert_css_properties_to_layout_flags(&mut element);
        
        // Check for percentage values in custom properties
        self.parse_percentage_properties(&mut element);
        
        // Initialize InputState for Input elements
        if element.element_type == ElementType::Input {
            // Determine input type from custom properties
            let input_type = element.custom_properties.get("input_type")
                .and_then(|v| v.as_string())
                .unwrap_or("text");
            
            let kryon_input_type = match input_type {
                "password" => crate::elements::InputType::Password,
                "email" => crate::elements::InputType::Email,
                "number" => crate::elements::InputType::Number,
                "search" => crate::elements::InputType::Search,
                "tel" => crate::elements::InputType::Tel,
                "url" => crate::elements::InputType::Url,
                "textarea" => crate::elements::InputType::Textarea,
                "range" => crate::elements::InputType::Range,
                "checkbox" => crate::elements::InputType::Checkbox,
                "radio" => crate::elements::InputType::Radio,
                _ => crate::elements::InputType::Text, // Default
            };
            
            let mut input_state = crate::elements::InputState::new(kryon_input_type);
            
            // Set placeholder from custom properties
            if let Some(PropertyValue::String(placeholder)) = element.custom_properties.get("placeholder") {
                input_state.placeholder = placeholder.clone();
            }
            
            // Set initial value from custom properties
            if let Some(PropertyValue::String(value)) = element.custom_properties.get("value") {
                input_state.text = value.clone();
                input_state.cursor_position = value.len();
            }
            
            // Set max length from custom properties  
            if let Some(PropertyValue::Int(max_len)) = element.custom_properties.get("maxlength") {
                input_state.max_length = *max_len as usize;
            }
            
            element.input_state = Some(input_state);
            eprintln!("[INPUT_INIT] Initialized InputState for element '{}' with type {:?}", element.id, kryon_input_type);
        }
        
        Ok(element)
    }
    
    fn build_element_hierarchy(&self, elements: &mut HashMap<u32, Element>, element_count: u32) {
        // Build parent-child relationships based on tree structure
        // Elements are written in depth-first traversal order
        
        let mut parent_stack = vec![0u32]; // Start with root element
        let mut remaining_children: Vec<u32> = Vec::new();
        
        // Initialize remaining children counts
        for i in 0..element_count {
            if let Some(element) = elements.get(&i) {
                remaining_children.push(element.children.capacity() as u32);
            } else {
                remaining_children.push(0);
            }
        }
        
        // Process elements in order, assigning parents
        for element_id in 1..element_count { // Skip root element (0)
            // Find the current parent (top of stack with remaining children)
            while let Some(&current_parent) = parent_stack.last() {
                if remaining_children[current_parent as usize] > 0 {
                    // This element is a child of current_parent
                    if let Some(parent) = elements.get_mut(&current_parent) {
                        parent.children.push(element_id);
                        eprintln!("[KRB] Element {}: added child {}", current_parent, element_id);
                    }
                    
                    if let Some(child) = elements.get_mut(&element_id) {
                        child.parent = Some(current_parent);
                        eprintln!("[KRB] Element {}: set parent [{}]", element_id, current_parent);
                    }
                    
                    remaining_children[current_parent as usize] -= 1;
                    
                    // If this element has children, add it to the stack
                    if remaining_children[element_id as usize] > 0 {
                        parent_stack.push(element_id);
                    }
                    break;
                } else {
                    // Current parent has no more children, pop it
                    parent_stack.pop();
                }
            }
        }
    }
    
    fn parse_standard_property(&mut self, element: &mut Element, strings: &[String]) -> Result<()> {
        let property_id = self.read_u8();
        let value_type = self.read_u8();
        let size = self.read_u8();
        
        eprintln!("[PROP] Property ID: 0x{:02X}, value_type: 0x{:02X}, size: {}", property_id, value_type, size);
        
        // Use centralized PropertyId system for ALL properties
        let property_enum = PropertyId::from_u8(property_id);
        let property_name = match property_enum {
            PropertyId::BackgroundColor => "background_color",
            PropertyId::TextColor => "text_color",
            PropertyId::BorderColor => "border_color",
            PropertyId::BorderWidth => "border_width",
            PropertyId::BorderRadius => "border_radius",
            PropertyId::TextContent => "text_content",
            PropertyId::FontSize => "font_size",
            PropertyId::FontWeight => "font_weight",
            PropertyId::TextAlignment => "text_alignment",
            PropertyId::FontFamily => "font_family",
            PropertyId::Display => "display",
            PropertyId::FlexDirection => "flex_direction",
            PropertyId::AlignItems => "align_items",
            PropertyId::AlignContent => "align_content",
            PropertyId::AlignSelf => "align_self",
            PropertyId::JustifyContent => "justify_content",
            PropertyId::Gap => "gap",
            PropertyId::Padding => "padding",
            PropertyId::Margin => "margin",
            PropertyId::Width => "width",
            PropertyId::Height => "height",
            _ => "unknown_property",
        };
        
        eprintln!("[PROP] {}: processing...", property_name);
        
        // For now, just store all properties as custom properties using centralized names
        match value_type {
            0x01 => { // Vec4 (color)
                if size == 4 {
                    let color = self.read_color();
                    element.custom_properties.insert(property_name.to_string(), PropertyValue::Color(color));
                    eprintln!("[PROP] {}: {:?}", property_name, color);
                } else {
                    eprintln!("[PROP] {}: size mismatch for Vec4, expected 4, got {}, skipping", property_name, size);
                    for _ in 0..size { self.read_u8(); }
                }
            }
            0x02 => { // Bytes (enum value or integer)
                if size == 1 {
                    let byte_value = self.read_u8();
                    let string_value = match property_enum {
                        PropertyId::AlignItems => match byte_value {
                            0 => "flex-start",
                            1 => "flex-end", 
                            2 => "center",
                            3 => "stretch",
                            4 => "baseline",
                            _ => "flex-start",
                        },
                        PropertyId::AlignContent => match byte_value {
                            0 => "flex-start",
                            1 => "flex-end",
                            2 => "center", 
                            3 => "stretch",
                            4 => "space-between",
                            5 => "space-around",
                            _ => "flex-start",
                        },
                        PropertyId::Display => match byte_value {
                            0 => "none",
                            1 => "block",
                            2 => "flex",
                            3 => "grid",
                            _ => "flex",
                        },
                        PropertyId::FlexDirection => match byte_value {
                            0 => "row",
                            1 => "column",
                            2 => "row-reverse",
                            3 => "column-reverse",
                            _ => "row",
                        },
                        _ => {
                            eprintln!("[PROP] {}: unknown byte value {}, storing as number", property_name, byte_value);
                            &byte_value.to_string()
                        }
                    };
                    element.custom_properties.insert(property_name.to_string(), PropertyValue::String(string_value.to_string()));
                    eprintln!("[PROP] {}: '{}' (from byte {})", property_name, string_value, byte_value);
                } else if size == 2 {
                    // Handle 2-byte integers (e.g., width, height)
                    let int_value = self.read_u16();
                    element.custom_properties.insert(property_name.to_string(), PropertyValue::Int(int_value as i32));
                    eprintln!("[PROP] {}: {} (from u16)", property_name, int_value);
                } else {
                    eprintln!("[PROP] {}: size mismatch for bytes, expected 1 or 2, got {}, skipping", property_name, size);
                    for _ in 0..size { self.read_u8(); }
                }
            }
            0x04 => { // String reference
                if size == 1 {
                    let string_index = self.read_u8() as usize;
                    if string_index < strings.len() {
                        let string_value = strings[string_index].clone();
                        element.custom_properties.insert(property_name.to_string(), PropertyValue::String(string_value.clone()));
                        eprintln!("[PROP] {}: '{}' (from string index {})", property_name, string_value, string_index);
                    } else {
                        eprintln!("[PROP] {}: invalid string index {}, skipping", property_name, string_index);
                    }
                } else {
                    eprintln!("[PROP] {}: size mismatch for string, expected 1, got {}, skipping", property_name, size);
                    for _ in 0..size { self.read_u8(); }
                }
            }
            0x07 => { // Single byte enum (same as 0x02 but different encoding)
                if size == 1 {
                    let enum_value = self.read_u8();
                    let string_value = match property_enum {
                        PropertyId::AlignItems => match enum_value {
                            0 => "flex-start",
                            1 => "flex-end",
                            2 => "center", 
                            3 => "stretch",
                            4 => "baseline",
                            _ => "flex-start",
                        },
                        PropertyId::AlignContent => match enum_value {
                            0 => "flex-start",
                            1 => "flex-end",
                            2 => "center",
                            3 => "stretch", 
                            4 => "space-between",
                            5 => "space-around",
                            _ => "flex-start",
                        },
                        PropertyId::Display => match enum_value {
                            0 => "none",
                            1 => "block",
                            2 => "flex", 
                            3 => "grid",
                            _ => "flex",
                        },
                        PropertyId::FlexDirection => match enum_value {
                            0 => "row",
                            1 => "column",
                            2 => "row-reverse",
                            3 => "column-reverse", 
                            _ => "row",
                        },
                        PropertyId::TextAlignment => match enum_value {
                            0 => "start",
                            1 => "center",
                            2 => "end",
                            3 => "justify",
                            _ => "start",
                        },
                        _ => {
                            eprintln!("[PROP] {}: unknown enum value {}, storing as number", property_name, enum_value);
                            &enum_value.to_string()
                        }
                    };
                    element.custom_properties.insert(property_name.to_string(), PropertyValue::String(string_value.to_string()));
                    eprintln!("[PROP] {}: '{}' (from enum {})", property_name, string_value, enum_value);
                } else {
                    eprintln!("[PROP] {}: size mismatch for enum, expected 1, got {}, skipping", property_name, size);
                    for _ in 0..size { self.read_u8(); }
                }
            }
            0x08 => { // Integer
                if size == 1 {
                    let int_value = self.read_u8() as i32;
                    element.custom_properties.insert(property_name.to_string(), PropertyValue::Int(int_value));
                    eprintln!("[PROP] {}: {}", property_name, int_value);
                } else if size == 2 {
                    let int_value = self.read_u16() as i32;
                    element.custom_properties.insert(property_name.to_string(), PropertyValue::Int(int_value));
                    eprintln!("[PROP] {}: {}", property_name, int_value);
                } else {
                    eprintln!("[PROP] {}: unsupported integer size {}, skipping", property_name, size);
                    for _ in 0..size { self.read_u8(); }
                }
            }
            _ => {
                eprintln!("[PROP] {}: unsupported value_type 0x{:02X}, skipping {} bytes", property_name, value_type, size);
                for _ in 0..size { self.read_u8(); }
            }
        }
        
        Ok(())
    }
    // ALL LEGACY PROPERTY PARSING REMOVED - Now using centralized PropertyId system above
    
    fn parse_custom_property(&mut self, element: &mut Element, strings: &[String]) -> Result<()> {
        let key_index = self.read_u8() as usize;
        let value_type = self.read_u8();
        let size = self.read_u8();
        
        let key = if key_index < strings.len() {
            strings[key_index].clone()
        } else {
            // Invalid key - must still consume the value bytes to stay in sync
            for _ in 0..size {
                self.read_u8();
            }
            return Ok(());
        };
        
        let value = match value_type {
            0x01 if size == 1 => PropertyValue::Int(self.read_u8() as i32),
            0x02 if size == 2 => PropertyValue::Int(self.read_u16() as i32),
            0x03 if size == 4 => PropertyValue::Color(self.read_color()),
            0x04 if size == 1 => {
                let string_index = self.read_u8() as usize;
                if string_index < strings.len() {
                    PropertyValue::String(strings[string_index].clone())
                } else {
                    PropertyValue::String(String::new())
                }
            }
            _ => {
                // Unknown value type or size mismatch - consume bytes and skip
                for _ in 0..size {
                    self.read_u8();
                }
                return Ok(());
            }
        };
        
        element.custom_properties.insert(key, value);
        Ok(())
    }
    
    /// Convert modern CSS properties to legacy layout_flags
    fn convert_css_properties_to_layout_flags(&self, element: &mut Element) {
        let mut layout_flags = element.layout_flags; // Start with existing flags
        
        // Extract CSS properties
        let display = element.custom_properties.get("display")
            .and_then(|v| if let PropertyValue::String(s) = v { Some(s) } else { None });
        let flex_direction = element.custom_properties.get("flex_direction")
            .and_then(|v| if let PropertyValue::String(s) = v { Some(s) } else { None });
        let align_items = element.custom_properties.get("align_items")
            .and_then(|v| if let PropertyValue::String(s) = v { Some(s) } else { None });
        let justify_content = element.custom_properties.get("justify_content")
            .and_then(|v| if let PropertyValue::String(s) = v { Some(s) } else { None });
        let flex_grow = element.custom_properties.get("flex_grow")
            .and_then(|v| if let PropertyValue::Float(f) = v { Some(*f) } else { None });
        let position = element.custom_properties.get("position")
            .and_then(|v| if let PropertyValue::String(s) = v { Some(s) } else { None });
        
        // Process position property first, regardless of flexbox properties
        // Position affects layout behavior and needs to be handled separately
        if let Some(pos) = position {
            match pos.as_str() {
                "absolute" => {
                    layout_flags |= 0x02; // LAYOUT_ABSOLUTE_BIT
                    eprintln!("[CSS_CONVERT_POSITION] Element '{}': position=absolute -> layout_flags=0x{:02X}", 
                        element.id, layout_flags);
                    element.layout_flags = layout_flags;
                }
                "relative" => {
                    // Relative positioning might need a different flag if supported
                    eprintln!("[CSS_CONVERT_POSITION] Element '{}': position=relative (no layout flag change needed)", 
                        element.id);
                }
                "static" => {
                    // Static is default, no special flag needed
                    eprintln!("[CSS_CONVERT_POSITION] Element '{}': position=static (default)", 
                        element.id);
                }
                _ => {
                    eprintln!("[CSS_CONVERT_POSITION] Element '{}': unknown position value '{}', ignoring", 
                        element.id, pos);
                }
            }
        }
        
        // Skip legacy layout flag conversion if element has modern flexbox properties
        // Let the Taffy engine handle these properly instead of corrupting with legacy flags
        let has_modern_flexbox = align_items.is_some() || justify_content.is_some() || 
                                flex_direction.is_some() || 
                                (display.is_some() && display.unwrap() == "flex");
        
        if has_modern_flexbox {
            eprintln!("[CSS_CONVERT_SKIP] Element '{}': Skipping legacy layout flag conversion for modern flexbox properties", element.id);
            return; // Don't set layout flags, let Taffy handle it
        }
        
        // Process flex container properties if we have display: flex (legacy mode only)
        if let Some(display_val) = display {
            if display_val == "flex" {
                // Set direction
                if let Some(direction) = flex_direction {
                    match direction.as_str() {
                        "row" => {
                            layout_flags = (layout_flags & !0x03) | 0x00; // LAYOUT_DIRECTION_ROW
                        }
                        "column" => {
                            layout_flags = (layout_flags & !0x03) | 0x01; // LAYOUT_DIRECTION_COLUMN
                        }
                        _ => {}
                    }
                }
                
                // Set alignment based on justify_content and align_items
                // For flexbox, justify_content controls main axis, align_items controls cross axis
                // Our layout engine uses alignment for both cross axis and main axis centering
                let mut should_center = false;
                
                if let Some(align) = align_items {
                    match align.as_str() {
                        "center" | "centre" => {
                            should_center = true;
                        }
                        "flex-start" | "start" => {
                            layout_flags = (layout_flags & !0x0C) | 0x00; // LAYOUT_ALIGNMENT_START  
                        }
                        "flex-end" | "end" => {
                            layout_flags = (layout_flags & !0x0C) | 0x08; // LAYOUT_ALIGNMENT_END
                        }
                        _ => {}
                    }
                }
                
                // Handle justify_content for main axis alignment
                if let Some(justify) = justify_content {
                    match justify.as_str() {
                        "center" | "centre" => {
                            should_center = true;
                        }
                        "start" | "flex-start" => {
                            layout_flags = (layout_flags & !0x0C) | 0x00; // LAYOUT_ALIGNMENT_START
                        }
                        "end" | "flex-end" => {
                            layout_flags = (layout_flags & !0x0C) | 0x08; // LAYOUT_ALIGNMENT_END
                        }
                        _ => {}
                    }
                }
                
                if should_center {
                    layout_flags = (layout_flags & !0x0C) | 0x04; // LAYOUT_ALIGNMENT_CENTER
                }
                
                eprintln!("[CSS_CONVERT] Element '{}': display={}, flex_direction={:?}, align_items={:?}, justify_content={:?} -> layout_flags=0x{:02X}", 
                    element.id, display_val, flex_direction, align_items, justify_content, layout_flags);
                
                element.layout_flags = layout_flags;
            }
        }
        
        // Process flex item properties (like flex_grow) regardless of display property
        // These apply to children of flex containers
        if let Some(grow) = flex_grow {
            if grow > 0.0 {
                layout_flags |= 0x20; // LAYOUT_GROW_BIT
                eprintln!("[CSS_CONVERT_FLEX_ITEM] Element '{}': flex_grow={} -> layout_flags=0x{:02X}", 
                    element.id, grow, layout_flags);
                element.layout_flags = layout_flags;
            }
        }
    }
    
    fn parse_resource_table(&mut self, header: &KRBHeader) -> Result<Vec<String>> {
        let resource_offset = self.read_u32_at(52) as usize;
        let mut resources = Vec::new();
        
        self.position = resource_offset;
        
        for _ in 0..header.resource_count {
            let length = self.read_u8() as usize;
            if self.position + length > self.data.len() {
                panic!("KRB parsing error: trying to read resource of length {} at position {} but data length is {}", length, self.position, self.data.len());
            }
            let resource_data = &self.data[self.position..self.position + length];
            let resource = String::from_utf8_lossy(resource_data).to_string();
            resources.push(resource);
            self.position += length;
        }
        
        Ok(resources)
    }
    
    fn parse_script_table(&mut self, header: &KRBHeader, strings: &[String]) -> Result<Vec<ScriptEntry>> {
        let script_offset = self.read_u32_at(44) as usize;
        let mut scripts = Vec::new();
        
        self.position = script_offset;
        
        for _ in 0..header.script_count {
            let language_id = self.read_u8();
            let name_index = self.read_u8();
            let storage_format = self.read_u8();
            let entry_point_count = self.read_u8();
            let data_size = self.read_u16() as usize;
            
            let language = match language_id {
                0x01 => "lua".to_string(),
                0x02 => "javascript".to_string(),
                0x03 => "python".to_string(),
                0x04 => "wren".to_string(),
                _ => "unknown".to_string(),
            };
            
            let name = if name_index > 0 && (name_index as usize) < strings.len() {
                strings[name_index as usize].clone()
            } else {
                format!("script_{}", scripts.len())
            };
            
            // Parse entry points
            let mut entry_points = Vec::new();
            for _ in 0..entry_point_count {
                let func_name_index = self.read_u8() as usize;
                if func_name_index < strings.len() {
                    entry_points.push(strings[func_name_index].clone());
                }
            }
            
            // Parse script bytecode
            let bytecode = if storage_format == 0 { // Inline
                if self.position + data_size > self.data.len() {
                    panic!("KRB parsing error: trying to read bytecode of size {} at position {} but data length is {}", data_size, self.position, self.data.len());
                }
                let bytecode_data = &self.data[self.position..self.position + data_size];
                bytecode_data.to_vec()
            } else { // External
                // For external storage, we would need to load from resource
                // For now, return empty vec - this should be handled by the compiler
                Vec::new()
            };
            
            if storage_format == 0 {
                self.position += data_size;
            }
            
            scripts.push(ScriptEntry {
                language,
                name,
                bytecode,
                source_code: None, // Not available in KRB files
                entry_points,
            });
        }
        
        Ok(scripts)
    }
    
    fn parse_template_variables(&mut self, header: &KRBHeader, strings: &[String]) -> Result<Vec<TemplateVariable>> {
        let template_var_offset = self.read_u32_at(56) as usize;
        let mut template_variables = Vec::new();
        
        println!("PARSE: template_variable_count = {}, offset = 0x{:X}", header.template_variable_count, template_var_offset);
        
        self.position = template_var_offset;
        
        for i in 0..header.template_variable_count {
            let name_index = self.read_u8() as usize;
            let value_type = self.read_u8();
            let default_value_index = self.read_u8() as usize;
            
            let name = if name_index < strings.len() {
                strings[name_index].clone()
            } else {
                format!("template_var_{}", template_variables.len())
            };
            
            let default_value = if default_value_index < strings.len() {
                strings[default_value_index].clone()
            } else {
                String::new()
            };
            
            println!("PARSE: template_variable[{}]: name='{}' (idx={}), type={}, default='{}' (idx={})", 
                i, name, name_index, value_type, default_value, default_value_index);
            
            template_variables.push(TemplateVariable {
                name,
                value_type,
                default_value,
            });
        }
        
        Ok(template_variables)
    }
    
    fn parse_template_bindings(&mut self, header: &KRBHeader, strings: &[String]) -> Result<Vec<TemplateBinding>> {
        let template_binding_offset = self.read_u32_at(60) as usize;
        let mut template_bindings = Vec::new();
        
        println!("PARSE: template_binding_count = {}, offset = 0x{:X}", header.template_binding_count, template_binding_offset);
        
        self.position = template_binding_offset;
        
        for i in 0..header.template_binding_count {
            let element_index = self.read_u16();
            let property_id = self.read_u8();
            let template_expression_index = self.read_u8() as usize;
            let variable_count = self.read_u8();
            
            let template_expression = if template_expression_index < strings.len() {
                strings[template_expression_index].clone()
            } else {
                String::new()
            };
            
            let mut variable_indices = Vec::new();
            for _ in 0..variable_count {
                variable_indices.push(self.read_u8());
            }
            
            println!("PARSE: template_binding[{}]: element={}, property=0x{:02X}, expr='{}' (idx={}), vars={:?}", 
                i, element_index, property_id, template_expression, template_expression_index, variable_indices);
            
            template_bindings.push(TemplateBinding {
                element_index,
                property_id,
                template_expression,
                variable_indices,
            });
        }
        
        Ok(template_bindings)
    }
    
    fn parse_transforms(&mut self, header: &KRBHeader) -> Result<Vec<TransformData>> {
        let transform_offset = self.read_u32_at(64) as usize;
        let mut transforms = Vec::new();
        
        println!("PARSE: transform_count = {}, offset = 0x{:X}", header.transform_count, transform_offset);
        
        self.position = transform_offset;
        
        for i in 0..header.transform_count {
            let transform_type = self.read_u8();
            let property_count = self.read_u8();
            
            let transform_type_enum = match transform_type {
                0x01 => TransformType::Transform2D,
                0x02 => TransformType::Transform3D,
                0x03 => TransformType::Matrix2D,
                0x04 => TransformType::Matrix3D,
                _ => TransformType::Transform2D, // Default fallback
            };
            
            let mut properties = Vec::new();
            for j in 0..property_count {
                let property_type = self.read_u8();
                let value_type = self.read_u8();
                let size = self.read_u8();
                
                let property_type_enum = match property_type {
                    0x01 => TransformPropertyType::Scale,
                    0x02 => TransformPropertyType::ScaleX,
                    0x03 => TransformPropertyType::ScaleY,
                    0x04 => TransformPropertyType::TranslateX,
                    0x05 => TransformPropertyType::TranslateY,
                    0x06 => TransformPropertyType::Rotate,
                    0x07 => TransformPropertyType::SkewX,
                    0x08 => TransformPropertyType::SkewY,
                    0x09 => TransformPropertyType::ScaleZ,
                    0x0A => TransformPropertyType::TranslateZ,
                    0x0B => TransformPropertyType::RotateX,
                    0x0C => TransformPropertyType::RotateY,
                    0x0D => TransformPropertyType::RotateZ,
                    0x0E => TransformPropertyType::Perspective,
                    0x0F => TransformPropertyType::Matrix,
                    _ => TransformPropertyType::Scale, // Default fallback
                };
                
                // Parse the value based on value_type
                let css_unit_value = match value_type {
                    0x19 => { // CSSUnit
                        if size >= 9 { // 8 bytes for f64 + 1 byte for unit
                            let value_bytes = [self.read_u8(), self.read_u8(), self.read_u8(), self.read_u8(),
                                             self.read_u8(), self.read_u8(), self.read_u8(), self.read_u8()];
                            let value = f64::from_le_bytes(value_bytes);
                            let unit_byte = self.read_u8();
                            
                            let unit = match unit_byte {
                                0x01 => CSSUnit::Pixels,
                                0x02 => CSSUnit::Em,
                                0x03 => CSSUnit::Rem,
                                0x04 => CSSUnit::ViewportWidth,
                                0x05 => CSSUnit::ViewportHeight,
                                0x06 => CSSUnit::Percentage,
                                0x07 => CSSUnit::Degrees,
                                0x08 => CSSUnit::Radians,
                                0x09 => CSSUnit::Turns,
                                0x0A => CSSUnit::Number,
                                _ => CSSUnit::Number, // Default fallback
                            };
                            
                            CSSUnitValue { value, unit }
                        } else {
                            // Skip malformed data
                            for _ in 0..size {
                                self.read_u8();
                            }
                            continue;
                        }
                    }
                    _ => {
                        // Skip unknown value types
                        for _ in 0..size {
                            self.read_u8();
                        }
                        continue;
                    }
                };
                
                properties.push(TransformProperty {
                    property_type: property_type_enum,
                    value: css_unit_value,
                });
                
                println!("PARSE: transform[{}].property[{}]: type={:?}, value={:?}", 
                    i, j, property_type_enum, properties.last().unwrap().value);
            }
            
            transforms.push(TransformData {
                transform_type: transform_type_enum,
                properties,
            });
            
            println!("PARSE: transform[{}]: type={:?}, properties={}", 
                i, transform_type_enum, transforms.last().unwrap().properties.len());
        }
        
        Ok(transforms)
    }
    
    fn apply_transforms_to_elements(
        &mut self,
        elements: &mut HashMap<ElementId, Element>,
        transforms: &[TransformData],
    ) -> Result<()> {
        for (element_id, element) in elements.iter_mut() {
            if let Some(PropertyValue::Int(transform_index)) = element.custom_properties.get("transform_index") {
                let idx = *transform_index as usize;
                if idx < transforms.len() {
                    let transform_data = &transforms[idx];
                    eprintln!("[TRANSFORM] Applying transform {} to element {}", idx, element_id);
                    
                    // Apply the transform data to the element's transform
                    element.transform = self.transform_data_to_transform(transform_data)?;
                    eprintln!("[TRANSFORM] Element {} transform applied: {:?}", element_id, element.transform);
                } else {
                    eprintln!("[TRANSFORM] Warning: Transform index {} out of bounds for element {}", idx, element_id);
                }
            }
        }
        Ok(())
    }
    
    fn transform_data_to_transform(&self, transform_data: &TransformData) -> Result<crate::Transform> {
        use crate::Transform;
        use glam::Vec3;
        
        let mut result_transform = Transform::identity();
        
        // Apply each transform property by combining transforms
        for property in &transform_data.properties {
            let individual_transform = match property.property_type {
                TransformPropertyType::TranslateX => {
                    Transform::translate(property.value.value as f32, 0.0, 0.0)
                }
                TransformPropertyType::TranslateY => {
                    Transform::translate(0.0, property.value.value as f32, 0.0)
                }
                TransformPropertyType::TranslateZ => {
                    Transform::translate(0.0, 0.0, property.value.value as f32)
                }
                TransformPropertyType::ScaleX => {
                    Transform::scale(property.value.value as f32, 1.0, 1.0)
                }
                TransformPropertyType::ScaleY => {
                    Transform::scale(1.0, property.value.value as f32, 1.0)
                }
                TransformPropertyType::ScaleZ => {
                    Transform::scale(1.0, 1.0, property.value.value as f32)
                }
                TransformPropertyType::Scale => {
                    let scale_value = property.value.value as f32;
                    Transform::scale_uniform(scale_value)
                }
                TransformPropertyType::RotateX => {
                    let angle = match property.value.unit {
                        CSSUnit::Degrees => property.value.value as f32,
                        CSSUnit::Radians => (property.value.value as f32).to_degrees(),
                        _ => property.value.value as f32, // Default to degrees
                    };
                    Transform::rotate_axis(Vec3::X, angle)
                }
                TransformPropertyType::RotateY => {
                    let angle = match property.value.unit {
                        CSSUnit::Degrees => property.value.value as f32,
                        CSSUnit::Radians => (property.value.value as f32).to_degrees(),
                        _ => property.value.value as f32, // Default to degrees
                    };
                    Transform::rotate_axis(Vec3::Y, angle)
                }
                TransformPropertyType::RotateZ | TransformPropertyType::Rotate => {
                    let angle = match property.value.unit {
                        CSSUnit::Degrees => property.value.value as f32,
                        CSSUnit::Radians => (property.value.value as f32).to_degrees(),
                        _ => property.value.value as f32, // Default to degrees
                    };
                    Transform::rotate_z(angle)
                }
                TransformPropertyType::SkewX => {
                    let angle = match property.value.unit {
                        CSSUnit::Degrees => property.value.value as f32,
                        CSSUnit::Radians => (property.value.value as f32).to_degrees(),
                        _ => property.value.value as f32, // Default to degrees
                    };
                    Transform::skew(angle, 0.0)
                }
                TransformPropertyType::SkewY => {
                    let angle = match property.value.unit {
                        CSSUnit::Degrees => property.value.value as f32,
                        CSSUnit::Radians => (property.value.value as f32).to_degrees(),
                        _ => property.value.value as f32, // Default to degrees
                    };
                    Transform::skew(0.0, angle)
                }
                TransformPropertyType::Perspective => {
                    Transform::perspective(property.value.value as f32)
                }
                TransformPropertyType::Matrix => {
                    // Matrix transforms would need special handling
                    eprintln!("[TRANSFORM] Matrix transforms not supported yet");
                    Transform::identity()
                }
            };
            
            // Combine transforms (multiply matrices)
            result_transform = result_transform.combine(&individual_transform);
        }
        
        Ok(result_transform)
    }
    
    fn create_default_app_wrapper(elements: &mut HashMap<ElementId, Element>) -> Option<ElementId> {
        if elements.is_empty() {
            return None;
        }
        
        // Find the next available element ID
        let app_id = elements.keys().max().unwrap_or(&0) + 1;
        
        // Create a default App element with sensible defaults
        let mut app_element = Element {
            id: "auto_generated_app".to_string(),
            element_type: ElementType::App,
            parent: None,
            children: Vec::new(),
            style_id: 0,
            layout_position: LayoutPosition::pixels(0.0, 0.0),
            layout_size: LayoutSize::pixels(800.0, 600.0),
            layout_flags: 0,
            gap: 0.0,
            overflow_x: OverflowType::Visible,
            overflow_y: OverflowType::Visible,
            max_width: None,
            max_height: None,
            transform: crate::Transform::default(),
            background_color: Vec4::new(0.1, 0.1, 0.1, 1.0), // Dark gray background
            text_color: Vec4::new(1.0, 1.0, 1.0, 1.0), // White text
            border_color: Vec4::new(0.0, 0.0, 0.0, 0.0), // Transparent border
            border_width: 0.0,
            border_radius: 0.0,
            opacity: 1.0,
            visible: true,
            z_index: 0,
            text: "Auto-generated App".to_string(),
            font_size: 14.0,
            font_weight: crate::elements::FontWeight::Normal,
            font_family: "default".to_string(),
            text_alignment: crate::elements::TextAlignment::Start,
            cursor: crate::elements::CursorType::Default,
            disabled: false,
            current_state: crate::elements::InteractionState::Normal,
            input_state: None,
            select_state: None,
            custom_properties: HashMap::new(),
            state_properties: HashMap::new(),
            event_handlers: HashMap::new(),
            component_name: None,
            is_component_instance: false,
            native_backend: None,
            native_render_script: None,
            native_config: HashMap::new(),
            embed_view_type: None,
            embed_view_source: None,
            embed_view_config: HashMap::new(),
        };
        
        // Collect all current root elements (elements with no parent)
        let mut root_elements = Vec::new();
        for (id, element) in elements.iter() {
            if element.parent.is_none() {
                root_elements.push(*id);
            }
        }
        
        // Make all current root elements children of the new App
        app_element.children = root_elements.clone();
        
        // Update parent references for all root elements
        for root_id in &root_elements {
            if let Some(element) = elements.get_mut(root_id) {
                element.parent = Some(app_id);
            }
        }
        
        // Insert the App element
        elements.insert(app_id, app_element);
        
        eprintln!("[AUTO_APP] Created default App wrapper with ID {} containing {} child elements", 
                 app_id, root_elements.len());
        
        Some(app_id)
    }
    
    fn apply_style_layout_flags(&self, elements: &mut HashMap<ElementId, Element>, styles: &HashMap<u8, Style>) -> Result<()> {
        for (_element_id, element) in elements.iter_mut() {
            if element.style_id > 0 {
                if let Some(style_block) = styles.get(&element.style_id) {
                    // Apply layout flags - Check property ID 0x06 and 0x1A for layout flags
                    let layout_prop = style_block.properties.get(&0x06)
                        .or_else(|| style_block.properties.get(&0x1A));
                    
                    if let Some(layout_prop) = layout_prop {
                        if let Some(layout_flags) = layout_prop.as_int() {
                            let new_flags = layout_flags as u8;
                            eprintln!("[STYLE_LAYOUT] Applying layout flags 0x{:02X} from style '{}' to element", 
                                new_flags, style_block.name);
                            element.layout_flags = new_flags;
                        }
                    }
                    
                    // Apply width property (0x19)
                    if let Some(width_prop) = style_block.properties.get(&0x19) {
                        if let Some(width) = width_prop.as_float() {
                            eprintln!("[STYLE_LAYOUT] Applying width {} from style '{}' to element", 
                                width, style_block.name);
                            element.layout_size.width = LayoutDimension::Pixels(width);
                        }
                    }
                    
                    // Apply height property (0x1A)
                    if let Some(height_prop) = style_block.properties.get(&0x1A) {
                        if let Some(height) = height_prop.as_float() {
                            eprintln!("[STYLE_LAYOUT] Applying height {} from style '{}' to element", 
                                height, style_block.name);
                            element.layout_size.height = LayoutDimension::Pixels(height);
                        }
                    }
                    
                    // Apply left position property (0x51)
                    if let Some(left_prop) = style_block.properties.get(&0x51) {
                        if let Some(left) = left_prop.as_float() {
                            eprintln!("[STYLE_LAYOUT] Applying left position {} from style '{}' to element", 
                                left, style_block.name);
                            element.layout_position.x = LayoutDimension::Pixels(left);
                            element.custom_properties.insert("left".to_string(), PropertyValue::Float(left));
                        }
                    }
                    
                    // Apply top position property (0x52)
                    if let Some(top_prop) = style_block.properties.get(&0x52) {
                        if let Some(top) = top_prop.as_float() {
                            eprintln!("[STYLE_LAYOUT] Applying top position {} from style '{}' to element", 
                                top, style_block.name);
                            element.layout_position.y = LayoutDimension::Pixels(top);
                            element.custom_properties.insert("top".to_string(), PropertyValue::Float(top));
                        }
                    }
                    
                    // Apply text alignment property (0x0B) to Button, Text, and Link elements
                    if element.element_type == ElementType::Button || element.element_type == ElementType::Text || element.element_type == ElementType::Link {
                        let element_name = match element.element_type {
                            ElementType::Button => "Button",
                            ElementType::Text => "Text", 
                            ElementType::Link => "Link",
                            _ => "Unknown"
                        };
                        eprintln!("[STYLE_LAYOUT] Checking text alignment for {} element with style_id={}, style_name='{}'", 
                            element_name, element.style_id, style_block.name);
                        eprintln!("[STYLE_LAYOUT] Style '{}' has {} properties: {:?}", 
                            style_block.name, style_block.properties.len(), style_block.properties.keys().collect::<Vec<_>>());
                        
                        if let Some(alignment_prop) = style_block.properties.get(&0x0B) {
                            if let Some(alignment) = alignment_prop.as_int() {
                                eprintln!("[STYLE_LAYOUT] Applying text_alignment {} from style '{}' to element", 
                                    alignment, style_block.name);
                                element.text_alignment = match alignment {
                                    0 => TextAlignment::Start,
                                    1 => TextAlignment::Center,
                                    2 => TextAlignment::End,
                                    3 => TextAlignment::Justify,
                                    _ => TextAlignment::Start,
                                };
                            } else {
                                eprintln!("[STYLE_LAYOUT] Found text_alignment property but failed to get as_int()");
                            }
                        } else {
                            eprintln!("[STYLE_LAYOUT] No text_alignment property (0x0B) found in style '{}'", style_block.name);
                        }
                        
                        // Apply font properties
                        if let Some(font_size_prop) = style_block.properties.get(&0x09) {
                            if let Some(font_size) = font_size_prop.as_float() {
                                eprintln!("[STYLE_LAYOUT] Applying font_size {} from style '{}' to element", 
                                    font_size, style_block.name);
                                element.font_size = font_size;
                            }
                        }
                        
                        if let Some(font_weight_prop) = style_block.properties.get(&0x0A) {
                            if let Some(weight) = font_weight_prop.as_int() {
                                eprintln!("[STYLE_LAYOUT] Applying font_weight {} from style '{}' to element", 
                                    weight, style_block.name);
                                element.font_weight = match weight {
                                    300 => crate::elements::FontWeight::Light,
                                    700 => crate::elements::FontWeight::Bold,
                                    900 => crate::elements::FontWeight::Heavy,
                                    _ => crate::elements::FontWeight::Normal,
                                };
                            }
                        }
                        
                        if let Some(font_family_prop) = style_block.properties.get(&0x0C) {
                            if let Some(font_family) = font_family_prop.as_string() {
                                eprintln!("[STYLE_LAYOUT] Applying font_family '{}' from style '{}' to element", 
                                    font_family, style_block.name);
                                element.font_family = font_family.to_string();
                            }
                        }
                        
                        // Apply layout properties to custom_properties for LayoutStyle::from_element_properties
                        if element.element_type == ElementType::Container {
                            // Display property (0x40)
                            if let Some(display_prop) = style_block.properties.get(&0x40) {
                                if let Some(display_byte) = display_prop.as_int() {
                                    let display_str = match display_byte {
                                        0 => "none",
                                        1 => "block",
                                        2 => "flex",
                                        3 => "grid",
                                        _ => "flex",
                                    };
                                    element.custom_properties.insert("display".to_string(), PropertyValue::String(display_str.to_string()));
                                    eprintln!("[STYLE_LAYOUT] Applied display '{}' from style '{}' to element custom_properties", display_str, style_block.name);
                                }
                            }
                            
                            // FlexDirection property (0x41)  
                            if let Some(flex_dir_prop) = style_block.properties.get(&0x41) {
                                if let Some(flex_dir_byte) = flex_dir_prop.as_int() {
                                    let flex_dir_str = match flex_dir_byte {
                                        0 => "row",
                                        1 => "column",
                                        2 => "row-reverse",
                                        3 => "column-reverse",
                                        _ => "row",
                                    };
                                    element.custom_properties.insert("flex_direction".to_string(), PropertyValue::String(flex_dir_str.to_string()));
                                    eprintln!("[STYLE_LAYOUT] Applied flex_direction '{}' from style '{}' to element custom_properties", flex_dir_str, style_block.name);
                                }
                            }
                            
                            // AlignItems property (0x46)
                            if let Some(align_items_prop) = style_block.properties.get(&0x46) {
                                if let Some(align_items_byte) = align_items_prop.as_int() {
                                    let align_items_str = match align_items_byte {
                                        0 => "flex-start",
                                        1 => "flex-end", 
                                        2 => "center",
                                        3 => "stretch",
                                        4 => "baseline",
                                        _ => "flex-start",
                                    };
                                    element.custom_properties.insert("align_items".to_string(), PropertyValue::String(align_items_str.to_string()));
                                    eprintln!("[STYLE_LAYOUT] Applied align_items '{}' from style '{}' to element custom_properties", align_items_str, style_block.name);
                                }
                            }
                            
                            // JustifyContent property (0x49)
                            if let Some(justify_content_prop) = style_block.properties.get(&0x49) {
                                if let Some(justify_content_byte) = justify_content_prop.as_int() {
                                    let justify_content_str = match justify_content_byte {
                                        0 => "flex-start",
                                        1 => "flex-end",
                                        2 => "center", 
                                        3 => "stretch",
                                        4 => "space-between",
                                        5 => "space-around",
                                        _ => "flex-start",
                                    };
                                    element.custom_properties.insert("justify_content".to_string(), PropertyValue::String(justify_content_str.to_string()));
                                    eprintln!("[STYLE_LAYOUT] Applied justify_content '{}' from style '{}' to element custom_properties", justify_content_str, style_block.name);
                                }
                            }
                        }
                    }
                    
                    // Apply Taffy layout properties to custom_properties using centralized PropertyId types
                    let taffy_properties = [
                        PropertyId::Display,
                        PropertyId::FlexDirection,
                        PropertyId::FlexWrap,
                        PropertyId::FlexGrow,
                        PropertyId::FlexShrink,
                        PropertyId::FlexBasis,
                        PropertyId::AlignItems,
                        PropertyId::AlignContent,
                        PropertyId::AlignSelf,
                        PropertyId::JustifyContent,
                        PropertyId::JustifyItems,
                        PropertyId::JustifySelf,
                        PropertyId::Position,
                        PropertyId::Left,
                        PropertyId::Top,
                        PropertyId::Right,
                        PropertyId::Bottom,
                        PropertyId::Gap,
                        PropertyId::Padding,
                        PropertyId::Margin,
                    ];
                    
                    for property_id in taffy_properties {
                        let prop_id_u8 = property_id.to_u8();
                        if let Some(taffy_prop) = style_block.properties.get(&prop_id_u8) {
                            // Use centralized snake_case property name conversion
                            let prop_name = match property_id {
                                PropertyId::Display => "display",
                                PropertyId::FlexDirection => "flex_direction",
                                PropertyId::FlexWrap => "flex_wrap",
                                PropertyId::FlexGrow => "flex_grow",
                                PropertyId::FlexShrink => "flex_shrink",
                                PropertyId::FlexBasis => "flex_basis",
                                PropertyId::AlignItems => "align_items",
                                PropertyId::AlignContent => "align_content",
                                PropertyId::AlignSelf => "align_self",
                                PropertyId::JustifyContent => "justify_content",
                                PropertyId::JustifyItems => "justify_items",
                                PropertyId::JustifySelf => "justify_self",
                                PropertyId::Position => "position",
                                PropertyId::Left => "left",
                                PropertyId::Top => "top",
                                PropertyId::Right => "right",
                                PropertyId::Bottom => "bottom",
                                PropertyId::Gap => "gap",
                                PropertyId::Padding => "padding",
                                PropertyId::Margin => "margin",
                                _ => continue, // Skip unknown properties
                            };
                            
                            element.custom_properties.insert(prop_name.to_string(), taffy_prop.clone());
                            
                            // CRITICAL FIX: Handle position property to set layout flags
                            if property_id == PropertyId::Position {
                                if let Some(position_value) = taffy_prop.as_string() {
                                    if position_value == "absolute" {
                                        element.layout_flags |= 0x02; // Set absolute positioning flag
                                    }
                                }
                            }
                        }
                    }
                    
                    // Legacy fallback for hardcoded styles
                    if style_block.name == "containerstyle" && element.layout_flags == 0 {
                        element.layout_flags = 0x05;
                        eprintln!("[STYLE_LAYOUT] Applied layout: center (0x05) to containerstyle element");
                    }
                    
                }
            }
        }
        Ok(())
    }
    
    /// Public method to re-apply style properties to specific elements
    /// This is used when template variables change style_id values
    pub fn reapply_styles_for_elements(&self, elements: &mut HashMap<ElementId, Element>, styles: &HashMap<u8, Style>, element_ids: &[ElementId]) -> Result<()> {
        for &element_id in element_ids {
            if let Some(element) = elements.get_mut(&element_id) {
                if element.style_id > 0 {
                    if let Some(style_block) = styles.get(&element.style_id) {
                        eprintln!("[STYLE_REAPPLY] Re-applying style '{}' (ID: {}) to element {}", 
                            style_block.name, element.style_id, element_id);
                        
                        // Apply width property (0x19)
                        if let Some(width_prop) = style_block.properties.get(&0x19) {
                            if let Some(width) = width_prop.as_float() {
                                eprintln!("[STYLE_REAPPLY] Applying width {} from style '{}' to element {}", 
                                    width, style_block.name, element_id);
                                element.layout_size.width = LayoutDimension::Pixels(width);
                            }
                        }
                        
                        // Apply height property (0x1A)
                        if let Some(height_prop) = style_block.properties.get(&0x1A) {
                            if let Some(height) = height_prop.as_float() {
                                eprintln!("[STYLE_REAPPLY] Applying height {} from style '{}' to element {}", 
                                    height, style_block.name, element_id);
                                element.layout_size.height = LayoutDimension::Pixels(height);
                            }
                        }
                        
                        // Apply display property (0x40) - CRITICAL FOR VISIBILITY
                        if let Some(display_prop) = style_block.properties.get(&0x40) {
                            if let Some(display_byte) = display_prop.as_int() {
                                let display_value = match display_byte {
                                    0 => "none",
                                    1 => "block",
                                    2 => "flex",
                                    3 => "grid",
                                    _ => "flex",
                                };
                                eprintln!("[STYLE_REAPPLY] Applying display '{}' from style '{}' to element {}", 
                                    display_value, style_block.name, element_id);
                                
                                // Set the display property in the element
                                element.custom_properties.insert("display".to_string(), PropertyValue::String(display_value.to_string()));
                                
                                // Handle display: none as equivalent to visibility: hidden
                                element.visible = display_value != "none";
                                eprintln!("[STYLE_REAPPLY] Set element {} visibility to {}", element_id, element.visible);
                            }
                        }
                        
                        // Apply flex_direction property (0x41)
                        if let Some(flex_dir_prop) = style_block.properties.get(&0x41) {
                            if let Some(flex_dir_byte) = flex_dir_prop.as_int() {
                                let flex_dir_value = match flex_dir_byte {
                                    0 => "row",
                                    1 => "column",
                                    2 => "row-reverse",
                                    3 => "column-reverse",
                                    _ => "row",
                                };
                                eprintln!("[STYLE_REAPPLY] Applying flex_direction '{}' from style '{}' to element {}", 
                                    flex_dir_value, style_block.name, element_id);
                                element.custom_properties.insert("flex_direction".to_string(), PropertyValue::String(flex_dir_value.to_string()));
                            }
                        }
                        
                        // Apply align_items property (0x46)  
                        if let Some(align_items_prop) = style_block.properties.get(&0x46) {
                            if let Some(align_items_byte) = align_items_prop.as_int() {
                                let align_items_value = match align_items_byte {
                                    0 => "flex-start",
                                    1 => "flex-end", 
                                    2 => "center",
                                    3 => "stretch",
                                    4 => "baseline",
                                    _ => "flex-start",
                                };
                                eprintln!("[STYLE_REAPPLY] Applying align_items '{}' from style '{}' to element {}", 
                                    align_items_value, style_block.name, element_id);
                                element.custom_properties.insert("align_items".to_string(), PropertyValue::String(align_items_value.to_string()));
                            }
                        }
                        
                        // Apply justify_content property (0x49)
                        if let Some(justify_content_prop) = style_block.properties.get(&0x49) {
                            if let Some(justify_content_byte) = justify_content_prop.as_int() {
                                let justify_content_value = match justify_content_byte {
                                    0 => "flex-start",
                                    1 => "flex-end",
                                    2 => "center", 
                                    3 => "stretch",
                                    4 => "space-between",
                                    5 => "space-around",
                                    _ => "flex-start",
                                };
                                eprintln!("[STYLE_REAPPLY] Applying justify_content '{}' from style '{}' to element {}", 
                                    justify_content_value, style_block.name, element_id);
                                element.custom_properties.insert("justify_content".to_string(), PropertyValue::String(justify_content_value.to_string()));
                            }
                        }
                        
                        // Apply background_color property (0x01)
                        if let Some(bg_color_prop) = style_block.properties.get(&0x01) {
                            if let Some(bg_color_value) = bg_color_prop.as_color() {
                                eprintln!("[STYLE_REAPPLY] Applying background_color {:?} from style '{}' to element {}", 
                                    bg_color_value, style_block.name, element_id);
                                
                                element.custom_properties.insert("background_color".to_string(), PropertyValue::Color(bg_color_value));
                            }
                        }
                        
                        // Apply border_color property (0x03)
                        if let Some(border_color_prop) = style_block.properties.get(&0x03) {
                            if let Some(border_color_value) = border_color_prop.as_color() {
                                eprintln!("[STYLE_REAPPLY] Applying border_color {:?} from style '{}' to element {}", 
                                    border_color_value, style_block.name, element_id);
                                
                                element.custom_properties.insert("border_color".to_string(), PropertyValue::Color(border_color_value));
                            }
                        }
                        
                        // Apply border_width property (0x04)
                        if let Some(border_width_prop) = style_block.properties.get(&0x04) {
                            if let Some(border_width) = border_width_prop.as_float() {
                                eprintln!("[STYLE_REAPPLY] Applying border_width {} from style '{}' to element {}", 
                                    border_width, style_block.name, element_id);
                                
                                element.custom_properties.insert("border_width".to_string(), PropertyValue::Float(border_width));
                            }
                        }
                        
                        // Apply display property (important for visibility)
                        if let Some(display_prop) = style_block.properties.get(&0x03) {
                            if let Some(display_value) = display_prop.as_string() {
                                eprintln!("[STYLE_REAPPLY] Applying display '{}' from style '{}' to element {}", 
                                    display_value, style_block.name, element_id);
                                element.custom_properties.insert("display".to_string(), PropertyValue::String(display_value.to_string()));
                            }
                        }
                        
                        // CRITICAL FIX: Apply position property (0x50) and set layout flags
                        if let Some(position_prop) = style_block.properties.get(&0x50) {
                            if let Some(position_value) = position_prop.as_string() {
                                eprintln!("[STYLE_REAPPLY] Applying position '{}' from style '{}' to element {}", 
                                    position_value, style_block.name, element_id);
                                element.custom_properties.insert("position".to_string(), PropertyValue::String(position_value.to_string()));
                                
                                // Set layout flags for absolute positioning
                                if position_value == "absolute" {
                                    element.layout_flags |= 0x02; // Set absolute positioning flag
                                    eprintln!("[STYLE_REAPPLY] Position: absolute -> set layout_flags to 0x{:02X} for element {}", 
                                        element.layout_flags, element_id);
                                }
                            }
                        }
                        
                        // Apply other important layout properties
                        if let Some(layout_prop) = style_block.properties.get(&0x06)
                            .or_else(|| style_block.properties.get(&0x1A)) {
                            if let Some(layout_flags) = layout_prop.as_int() {
                                let new_flags = layout_flags as u8;
                                eprintln!("[STYLE_REAPPLY] Applying layout flags 0x{:02X} from style '{}' to element {}", 
                                    new_flags, style_block.name, element_id);
                                element.layout_flags = new_flags;
                            }
                        }
                    }
                }
            }
        }
        Ok(())
    }
}

impl KRBFile {
    /// Re-apply styles for specific elements (public interface)
    pub fn reapply_styles_for_elements(&self, elements: &mut HashMap<ElementId, Element>, element_ids: &[ElementId]) -> Result<()> {
        // Apply styles directly without needing a parser instance
        for &element_id in element_ids {
            if let Some(element) = elements.get_mut(&element_id) {
                if element.style_id > 0 {
                    if let Some(style_block) = self.styles.get(&element.style_id) {
                        eprintln!("[STYLE_REAPPLY] Re-applying style '{}' (ID: {}) to element {}", 
                            style_block.name, element.style_id, element_id);
                        
                        // Apply width property (0x19)
                        if let Some(width_prop) = style_block.properties.get(&0x19) {
                            if let Some(width) = width_prop.as_float() {
                                eprintln!("[STYLE_REAPPLY] Applying width {} from style '{}' to element {}", 
                                    width, style_block.name, element_id);
                                element.layout_size.width = LayoutDimension::Pixels(width);
                            }
                        }
                        
                        // Apply height property (0x1A)
                        if let Some(height_prop) = style_block.properties.get(&0x1A) {
                            if let Some(height) = height_prop.as_float() {
                                eprintln!("[STYLE_REAPPLY] Applying height {} from style '{}' to element {}", 
                                    height, style_block.name, element_id);
                                element.layout_size.height = LayoutDimension::Pixels(height);
                            }
                        }
                        
                        // Apply display property (0x40) - CRITICAL FOR VISIBILITY
                        if let Some(display_prop) = style_block.properties.get(&0x40) {
                            if let Some(display_byte) = display_prop.as_int() {
                                let display_value = match display_byte {
                                    0 => "none",
                                    1 => "block",
                                    2 => "flex",
                                    3 => "grid",
                                    _ => "flex",
                                };
                                eprintln!("[STYLE_REAPPLY] Applying display '{}' from style '{}' to element {}", 
                                    display_value, style_block.name, element_id);
                                
                                // Set the display property in the element
                                element.custom_properties.insert("display".to_string(), PropertyValue::String(display_value.to_string()));
                                
                                // Handle display: none as equivalent to visibility: hidden
                                element.visible = display_value != "none";
                                eprintln!("[STYLE_REAPPLY] Set element {} visibility to {}", element_id, element.visible);
                            }
                        }
                        
                        // Apply flex_direction property (0x41)
                        if let Some(flex_dir_prop) = style_block.properties.get(&0x41) {
                            if let Some(flex_dir_byte) = flex_dir_prop.as_int() {
                                let flex_dir_value = match flex_dir_byte {
                                    0 => "row",
                                    1 => "column",
                                    2 => "row-reverse",
                                    3 => "column-reverse",
                                    _ => "row",
                                };
                                eprintln!("[STYLE_REAPPLY] Applying flex_direction '{}' from style '{}' to element {}", 
                                    flex_dir_value, style_block.name, element_id);
                                element.custom_properties.insert("flex_direction".to_string(), PropertyValue::String(flex_dir_value.to_string()));
                            }
                        }
                        
                        // Apply align_items property (0x46)  
                        if let Some(align_items_prop) = style_block.properties.get(&0x46) {
                            if let Some(align_items_byte) = align_items_prop.as_int() {
                                let align_items_value = match align_items_byte {
                                    0 => "flex-start",
                                    1 => "flex-end", 
                                    2 => "center",
                                    3 => "stretch",
                                    4 => "baseline",
                                    _ => "flex-start",
                                };
                                eprintln!("[STYLE_REAPPLY] Applying align_items '{}' from style '{}' to element {}", 
                                    align_items_value, style_block.name, element_id);
                                element.custom_properties.insert("align_items".to_string(), PropertyValue::String(align_items_value.to_string()));
                            }
                        }
                        
                        // Apply justify_content property (0x49)
                        if let Some(justify_content_prop) = style_block.properties.get(&0x49) {
                            if let Some(justify_content_byte) = justify_content_prop.as_int() {
                                let justify_content_value = match justify_content_byte {
                                    0 => "flex-start",
                                    1 => "flex-end",
                                    2 => "center", 
                                    3 => "stretch",
                                    4 => "space-between",
                                    5 => "space-around",
                                    _ => "flex-start",
                                };
                                eprintln!("[STYLE_REAPPLY] Applying justify_content '{}' from style '{}' to element {}", 
                                    justify_content_value, style_block.name, element_id);
                                element.custom_properties.insert("justify_content".to_string(), PropertyValue::String(justify_content_value.to_string()));
                            }
                        }
                        
                        // Apply background_color property (0x01)
                        if let Some(bg_color_prop) = style_block.properties.get(&0x01) {
                            if let Some(bg_color_value) = bg_color_prop.as_color() {
                                eprintln!("[STYLE_REAPPLY] Applying background_color {:?} from style '{}' to element {}", 
                                    bg_color_value, style_block.name, element_id);
                                
                                element.custom_properties.insert("background_color".to_string(), PropertyValue::Color(bg_color_value));
                            }
                        }
                        
                        // Apply border_color property (0x03)
                        if let Some(border_color_prop) = style_block.properties.get(&0x03) {
                            if let Some(border_color_value) = border_color_prop.as_color() {
                                eprintln!("[STYLE_REAPPLY] Applying border_color {:?} from style '{}' to element {}", 
                                    border_color_value, style_block.name, element_id);
                                
                                element.custom_properties.insert("border_color".to_string(), PropertyValue::Color(border_color_value));
                            }
                        }
                        
                        // Apply border_width property (0x04)
                        if let Some(border_width_prop) = style_block.properties.get(&0x04) {
                            if let Some(border_width) = border_width_prop.as_float() {
                                eprintln!("[STYLE_REAPPLY] Applying border_width {} from style '{}' to element {}", 
                                    border_width, style_block.name, element_id);
                                
                                element.custom_properties.insert("border_width".to_string(), PropertyValue::Float(border_width));
                            }
                        }
                        
                        // Apply display property (important for visibility)
                        if let Some(display_prop) = style_block.properties.get(&0x03) {
                            if let Some(display_value) = display_prop.as_string() {
                                eprintln!("[STYLE_REAPPLY] Applying display '{}' from style '{}' to element {}", 
                                    display_value, style_block.name, element_id);
                                element.custom_properties.insert("display".to_string(), PropertyValue::String(display_value.to_string()));
                            }
                        }
                        
                        // CRITICAL FIX: Apply position property (0x50) and set layout flags
                        if let Some(position_prop) = style_block.properties.get(&0x50) {
                            if let Some(position_value) = position_prop.as_string() {
                                eprintln!("[STYLE_REAPPLY] Applying position '{}' from style '{}' to element {}", 
                                    position_value, style_block.name, element_id);
                                element.custom_properties.insert("position".to_string(), PropertyValue::String(position_value.to_string()));
                                
                                // Set layout flags for absolute positioning
                                if position_value == "absolute" {
                                    element.layout_flags |= 0x02; // Set absolute positioning flag
                                    eprintln!("[STYLE_REAPPLY] Position: absolute -> set layout_flags to 0x{:02X} for element {}", 
                                        element.layout_flags, element_id);
                                }
                            }
                        }
                        
                        // Apply other important layout properties
                        if let Some(layout_prop) = style_block.properties.get(&0x06)
                            .or_else(|| style_block.properties.get(&0x1A)) {
                            if let Some(layout_flags) = layout_prop.as_int() {
                                let new_flags = layout_flags as u8;
                                eprintln!("[STYLE_REAPPLY] Applying layout flags 0x{:02X} from style '{}' to element {}", 
                                    new_flags, style_block.name, element_id);
                                element.layout_flags = new_flags;
                            }
                        }
                    }
                }
            }
        }
        Ok(())
    }
}

impl KRBParser {
    // Helper methods for reading binary data
    
    /// Safely skip a number of bytes, checking bounds first
    fn skip_bytes(&mut self, size: usize) -> bool {
        let bytes_remaining = self.data.len() - self.position;
        if size > bytes_remaining {
            eprintln!("WARNING: Attempted to skip {} bytes but only {} remaining, skipping to end", size, bytes_remaining);
            self.position = self.data.len();
            return false;
        }
        self.position += size;
        true
    }
    
    fn read_u8(&mut self) -> u8 {
        if self.position >= self.data.len() {
            eprintln!("DEBUG: Attempted to read u8 at position {} but file is only {} bytes", self.position, self.data.len());
            eprintln!("DEBUG: This suggests the KRB file is truncated or the parser is reading more than expected");
            panic!("KRB parsing error: trying to read u8 at position {} but data length is {}", self.position, self.data.len());
        }
        let value = self.data[self.position];
        self.position += 1;
        value
    }
    
    fn read_u16(&mut self) -> u16 {
        if self.position + 2 > self.data.len() {
            panic!("KRB parsing error: trying to read u16 at position {} but data length is {}", self.position, self.data.len());
        }
        let value = u16::from_le_bytes([self.data[self.position], self.data[self.position + 1]]);
        self.position += 2;
        value
    }
    
    fn read_u16_at(&self, offset: usize) -> u16 {
        if offset + 2 > self.data.len() {
            panic!("KRB parsing error: trying to read u16 at offset {} but data length is {}", offset, self.data.len());
        }
        u16::from_le_bytes([self.data[offset], self.data[offset + 1]])
    }
    
    fn _read_u32(&mut self) -> u32 {
        if self.position + 4 > self.data.len() {
            panic!("KRB parsing error: trying to read u32 at position {} but data length is {}", self.position, self.data.len());
        }
        let value = u32::from_le_bytes([
            self.data[self.position],
            self.data[self.position + 1],
            self.data[self.position + 2],
            self.data[self.position + 3],
        ]);
        self.position += 4;
        value
    }
    
    fn read_u32_at(&self, offset: usize) -> u32 {
        if offset + 4 > self.data.len() {
            panic!("KRB parsing error: trying to read u32 at offset {} but data length is {}", offset, self.data.len());
        }
        u32::from_le_bytes([
            self.data[offset],
            self.data[offset + 1],
            self.data[offset + 2],
            self.data[offset + 3],
        ])
    }
    
    /// Parse percentage values from custom properties and update layout fields
    fn parse_percentage_properties(&self, element: &mut Element) {
        // Check for width percentage or numeric value
        match element.custom_properties.get("width") {
            Some(PropertyValue::String(width_str)) => {
                if let Ok(dimension) = self.parse_dimension_string(width_str) {
                    element.layout_size.width = dimension;
                }
            }
            Some(PropertyValue::Float(width)) => {
                element.layout_size.width = LayoutDimension::Pixels(*width);
            }
            Some(PropertyValue::Int(width)) => {
                element.layout_size.width = LayoutDimension::Pixels(*width as f32);
            }
            _ => {}
        }
        
        // Check for height percentage or numeric value
        match element.custom_properties.get("height") {
            Some(PropertyValue::String(height_str)) => {
                if let Ok(dimension) = self.parse_dimension_string(height_str) {
                    element.layout_size.height = dimension;
                }
            }
            Some(PropertyValue::Float(height)) => {
                element.layout_size.height = LayoutDimension::Pixels(*height);
            }
            Some(PropertyValue::Int(height)) => {
                element.layout_size.height = LayoutDimension::Pixels(*height as f32);
            }
            _ => {}
        }
        
        // Check for position percentages or numeric values
        match element.custom_properties.get("pos_x") {
            Some(PropertyValue::String(x_str)) => {
                if let Ok(dimension) = self.parse_dimension_string(x_str) {
                    element.layout_position.x = dimension;
                }
            }
            Some(PropertyValue::Float(x)) => {
                element.layout_position.x = LayoutDimension::Pixels(*x);
            }
            Some(PropertyValue::Int(x)) => {
                element.layout_position.x = LayoutDimension::Pixels(*x as f32);
            }
            _ => {}
        }
        
        match element.custom_properties.get("pos_y") {
            Some(PropertyValue::String(y_str)) => {
                if let Ok(dimension) = self.parse_dimension_string(y_str) {
                    element.layout_position.y = dimension;
                }
            }
            Some(PropertyValue::Float(y)) => {
                element.layout_position.y = LayoutDimension::Pixels(*y);
            }
            Some(PropertyValue::Int(y)) => {
                element.layout_position.y = LayoutDimension::Pixels(*y as f32);
            }
            _ => {}
        }
        
        // Check for text content
        if let Some(PropertyValue::String(text)) = element.custom_properties.get("text_content") {
            element.text = text.clone();
        }
        
        // Debug: Show all custom properties for text elements
        if element.element_type == crate::elements::ElementType::Text {
            eprintln!("[DEBUG] Text element {} (text='{}') custom properties:", element.id, element.text);
            for (key, value) in &element.custom_properties {
                eprintln!("[DEBUG]   {} = {:?}", key, value);
            }
            eprintln!("[DEBUG] Text element current text_alignment: {:?}", element.text_alignment);
        }
        
        // Check for text alignment from element properties (0x0B)
        if let Some(PropertyValue::Int(alignment_int)) = element.custom_properties.get("text_alignment") {
            element.text_alignment = match *alignment_int {
                0 => crate::elements::TextAlignment::Start,
                1 => crate::elements::TextAlignment::Center,
                2 => crate::elements::TextAlignment::End,
                3 => crate::elements::TextAlignment::Justify,
                _ => crate::elements::TextAlignment::Start,
            };
            eprintln!("[PARSE] Set text_alignment to {:?} from element property value {}", element.text_alignment, alignment_int);
        } else if let Some(PropertyValue::String(alignment_str)) = element.custom_properties.get("text_alignment") {
            element.text_alignment = match alignment_str.as_str() {
                "left" | "start" => crate::elements::TextAlignment::Start,
                "center" => crate::elements::TextAlignment::Center,
                "right" | "end" => crate::elements::TextAlignment::End,
                "justify" => crate::elements::TextAlignment::Justify,
                _ => {
                    eprintln!("[WARNING] Unknown text_alignment value: {}", alignment_str);
                    crate::elements::TextAlignment::Start
                }
            };
            eprintln!("[PARSE] Set text_alignment to {:?} from custom property '{}'", element.text_alignment, alignment_str);
        } else {
            eprintln!("[DEBUG] No text_alignment property found in custom_properties");
        }
    }
    
    /// Parse a dimension string (e.g., "50%", "100px", "auto") into a LayoutDimension
    fn parse_dimension_string(&self, value: &str) -> Result<LayoutDimension> {
        Ok(LayoutDimension::from_string(value))
    }
    
    fn event_type_from_id(&self, id: u8) -> Option<EventType> {
        match id {
            // --- THIS IS THE CORRECTED MAPPING ---
            0x01 => Some(EventType::Click),
            0x02 => Some(EventType::Press),   // Assuming you have this event type
            0x03 => Some(EventType::Release), // Assuming you have this event type
            0x04 => Some(EventType::Hover),
            0x05 => Some(EventType::Focus),
            0x06 => Some(EventType::Blur),
            0x07 => Some(EventType::Change),
            0x08 => Some(EventType::Submit),
            _ => None, // Safely ignore unknown event types
        }
    }
    fn event_type_name(&self, event_type: EventType) -> &'static str {
        match event_type {
            EventType::Click => "Click",
            EventType::Press => "Press",
            EventType::Release => "Release",
            EventType::Hover => "Hover",
            EventType::Focus => "Focus",
            EventType::Blur => "Blur",
            EventType::Change => "Change",
            EventType::Submit => "Submit",
        }
    }
    
    fn read_color(&mut self) -> Vec4 {
        let r = self.read_u8() as f32 / 255.0;
        let g = self.read_u8() as f32 / 255.0;
        let b = self.read_u8() as f32 / 255.0;
        let a = self.read_u8() as f32 / 255.0;
        Vec4::new(r, g, b, a)
    }
}

pub fn load_krb_file(path: &str) -> Result<KRBFile> {
    let data = std::fs::read(path)?;
    load_krb_from_bytes(&data)
}

pub fn load_krb_from_bytes(data: &[u8]) -> Result<KRBFile> {
    let mut parser = KRBParser::new(data.to_vec());
    let krb_file = parser.parse()?;
    
    // DEBUG: Print everything we parsed
    eprintln!("=== KRB FILE DEBUG ===");
    eprintln!("Header: element_count={}, style_count={}, string_count={}, transform_count={}", 
        krb_file.header.element_count, krb_file.header.style_count, krb_file.header.string_count, krb_file.header.transform_count);
    
    // Add explicit style debugging
    if krb_file.header.style_count == 0 {
        eprintln!("WARNING: No styles found in KRB file! This means:");
        eprintln!("  - Styles were not compiled into the KRB");
        eprintln!("  - Elements will use default colors (black text, transparent backgrounds)");
        eprintln!("  - The original .kry file styles were lost during compilation");
    }
    
    eprintln!("Strings ({}):", krb_file.strings.len());
    for (i, s) in krb_file.strings.iter().enumerate() {
        eprintln!("  [{}]: '{}'", i, s);
    }
    
    eprintln!("Elements ({}):", krb_file.elements.len());
    for (id, element) in &krb_file.elements {
        eprintln!("  [{}]: type={:?}, id='{}', pos=({:.1},{:.1}), size=({:.1},{:.1}), children={}, text='{}'",
            id, element.element_type, element.id, 
            element.layout_position.x.to_pixels(1.0), element.layout_position.y.to_pixels(1.0),
            element.layout_size.width.to_pixels(1.0), element.layout_size.height.to_pixels(1.0),
            element.children.len(), element.text);
    }
    
    eprintln!("Transforms ({}):", krb_file.transforms.len());
    for (i, transform) in krb_file.transforms.iter().enumerate() {
        eprintln!("  [{}]: type={:?}, properties={}", i, transform.transform_type, transform.properties.len());
        for (j, prop) in transform.properties.iter().enumerate() {
            eprintln!("    [{}]: {:?} = {:?}", j, prop.property_type, prop.value);
        }
    }
    
    eprintln!("Root element ID: {:?}", krb_file.root_element_id);
    if let Some(root_id) = krb_file.root_element_id {
        if let Some(root_element) = krb_file.elements.get(&root_id) {
            if root_element.id == "auto_generated_app" {
                eprintln!("NOTE: Auto-generated App wrapper created for standalone rendering");
            }
        }
    }
    eprintln!("=== END DEBUG ===");
    
    Ok(krb_file)
}