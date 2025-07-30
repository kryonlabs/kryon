use crate::{HtmlRenderer, HtmlResult};
use glam::{Vec2, Vec4};
use kryon_core::TextAlignment;
use kryon_render::LayoutStyle;

impl HtmlRenderer {
    /// Generate opening tag for a container element with proper nesting
    pub fn generate_begin_container_element(
        &mut self,
        element_id: &str,
        position: Vec2,
        size: Vec2,
        color: Vec4,
        border_radius: f32,
        border_width: f32,
        border_color: Vec4,
        layout_style: Option<&LayoutStyle>,
        z_index: i32,
    ) -> HtmlResult<()> {
        let element_id = self.next_element_id();
        
        // Generate opening div tag
        self.html_content.push_str(&format!(
            "  <div id=\"{}\" class=\"kryon-container\">\n",
            element_id
        ));
        
        // Generate CSS (same as rect_element)
        self.css_content.push_str(&format!(
            "#{} {{\n",
            element_id
        ));
        
        // Always use absolute positioning if position is specified (non-zero)
        // This ensures containers appear in the correct location
        if position.x != 0.0 || position.y != 0.0 {
            self.css_content.push_str("  position: absolute;\n");
            self.css_content.push_str(&format!("  left: {}px;\n", position.x));
            self.css_content.push_str(&format!("  top: {}px;\n", position.y));
        } else if let Some(layout) = layout_style {
            if let Some(display) = &layout.display {
                if display == "flex" || display == "grid" {
                    // Flex/grid containers at root use relative positioning
                    self.css_content.push_str("  position: relative;\n");
                }
            }
        }
        
        self.css_content.push_str(&format!(
            "  width: {}px;\n", size.x
        ));
        self.css_content.push_str(&format!(
            "  height: {}px;\n", size.y
        ));
        
        // Background color
        if color.w > 0.0 {
            self.css_content.push_str(&format!(
                "  background-color: rgba({}, {}, {}, {});\n",
                (color.x * 255.0) as u8,
                (color.y * 255.0) as u8,
                (color.z * 255.0) as u8,
                color.w
            ));
        }
        
        // Border
        if border_width > 0.0 && border_color.w > 0.0 {
            self.css_content.push_str(&format!(
                "  border: {}px solid rgba({}, {}, {}, {});\n",
                border_width,
                (border_color.x * 255.0) as u8,
                (border_color.y * 255.0) as u8,
                (border_color.z * 255.0) as u8,
                border_color.w
            ));
        }
        
        // Border radius
        if border_radius > 0.0 {
            self.css_content.push_str(&format!(
                "  border-radius: {}px;\n", border_radius
            ));
        }
        
        // Layout styles (flexbox, grid, etc.)
        if let Some(layout) = layout_style {
            if let Some(display) = &layout.display {
                self.css_content.push_str(&format!(
                    "  display: {};\n", display
                ));
            }
            
            if let Some(flex_direction) = &layout.flex_direction {
                self.css_content.push_str(&format!(
                    "  flex-direction: {};\n", flex_direction
                ));
            }
            
            if let Some(justify_content) = &layout.justify_content {
                self.css_content.push_str(&format!(
                    "  justify-content: {};\n", justify_content
                ));
            }
            
            if let Some(align_items) = &layout.align_items {
                self.css_content.push_str(&format!(
                    "  align-items: {};\n", align_items
                ));
            }
            
            if let Some(align_content) = &layout.align_content {
                self.css_content.push_str(&format!(
                    "  align-content: {};\n", align_content
                ));
            }
            
            if let Some(flex_wrap) = &layout.flex_wrap {
                self.css_content.push_str(&format!(
                    "  flex-wrap: {};\n", flex_wrap
                ));
            }
            
            if let Some(gap) = layout.gap {
                self.css_content.push_str(&format!(
                    "  gap: {}px;\n", gap
                ));
            }
            
            if let Some(row_gap) = layout.row_gap {
                self.css_content.push_str(&format!(
                    "  row-gap: {}px;\n", row_gap
                ));
            }
            
            if let Some(column_gap) = layout.column_gap {
                self.css_content.push_str(&format!(
                    "  column-gap: {}px;\n", column_gap
                ));
            }
        }
        
        // Z-index
        self.css_content.push_str(&format!(
            "  z-index: {};\n", z_index
        ));
        
        self.css_content.push_str("}\n");
        
        Ok(())
    }
    
    /// Generate closing tag for a container element
    pub fn generate_end_container_element(&mut self) -> HtmlResult<()> {
        self.html_content.push_str("  </div>\n");
        Ok(())
    }

    /// Generate a div element for rectangles/containers
    pub fn generate_rect_element(
        &mut self,
        position: Vec2,
        size: Vec2,
        color: Vec4,
        border_radius: f32,
        border_width: f32,
        border_color: Vec4,
        layout_style: Option<&LayoutStyle>,
        z_index: i32,
    ) -> HtmlResult<()> {
        let element_id = self.next_element_id();
        
        // Generate HTML
        self.html_content.push_str(&format!(
            "  <div id=\"{}\" class=\"kryon-rect\"></div>\n",
            element_id
        ));
        
        // Generate CSS
        self.css_content.push_str(&format!(
            "#{} {{\n",
            element_id
        ));
        
        // Always use absolute positioning if position is specified (non-zero)
        // This ensures containers appear in the correct location
        if position.x != 0.0 || position.y != 0.0 {
            self.css_content.push_str("  position: absolute;\n");
            self.css_content.push_str(&format!("  left: {}px;\n", position.x));
            self.css_content.push_str(&format!("  top: {}px;\n", position.y));
        } else if let Some(layout) = layout_style {
            if let Some(display) = &layout.display {
                if display == "flex" || display == "grid" {
                    // Flex/grid containers at root use relative positioning
                    self.css_content.push_str("  position: relative;\n");
                }
            }
        }
        
        self.css_content.push_str(&format!(
            "  width: {}px;\n", size.x
        ));
        self.css_content.push_str(&format!(
            "  height: {}px;\n", size.y
        ));
        
        // Background color
        if color.w > 0.0 {
            self.css_content.push_str(&format!(
                "  background-color: rgba({}, {}, {}, {});\n",
                (color.x * 255.0) as u8,
                (color.y * 255.0) as u8,
                (color.z * 255.0) as u8,
                color.w
            ));
        }
        
        // Border
        if border_width > 0.0 && border_color.w > 0.0 {
            self.css_content.push_str(&format!(
                "  border: {}px solid rgba({}, {}, {}, {});\n",
                border_width,
                (border_color.x * 255.0) as u8,
                (border_color.y * 255.0) as u8,
                (border_color.z * 255.0) as u8,
                border_color.w
            ));
        }
        
        // Border radius
        if border_radius > 0.0 {
            self.css_content.push_str(&format!(
                "  border-radius: {}px;\n", border_radius
            ));
        }
        
        // Layout styles (flexbox, grid, etc.)
        if let Some(layout) = layout_style {
            if let Some(display) = &layout.display {
                self.css_content.push_str(&format!(
                    "  display: {};\n", display
                ));
            }
            
            if let Some(flex_direction) = &layout.flex_direction {
                self.css_content.push_str(&format!(
                    "  flex-direction: {};\n", flex_direction
                ));
            }
            
            if let Some(justify_content) = &layout.justify_content {
                self.css_content.push_str(&format!(
                    "  justify-content: {};\n", justify_content
                ));
            }
            
            if let Some(align_items) = &layout.align_items {
                self.css_content.push_str(&format!(
                    "  align-items: {};\n", align_items
                ));
            }
            
            if let Some(align_content) = &layout.align_content {
                self.css_content.push_str(&format!(
                    "  align-content: {};\n", align_content
                ));
            }
            
            if let Some(flex_wrap) = &layout.flex_wrap {
                self.css_content.push_str(&format!(
                    "  flex-wrap: {};\n", flex_wrap
                ));
            }
            
            if let Some(gap) = layout.gap {
                self.css_content.push_str(&format!(
                    "  gap: {}px;\n", gap
                ));
            }
            
            if let Some(row_gap) = layout.row_gap {
                self.css_content.push_str(&format!(
                    "  row-gap: {}px;\n", row_gap
                ));
            }
            
            if let Some(column_gap) = layout.column_gap {
                self.css_content.push_str(&format!(
                    "  column-gap: {}px;\n", column_gap
                ));
            }
        }
        
        // Z-index
        self.css_content.push_str(&format!(
            "  z-index: {};\n", z_index
        ));
        
        self.css_content.push_str("}\n");
        
        Ok(())
    }
    
    /// Generate a text element
    pub fn generate_text_element(
        &mut self,
        position: Vec2,
        text: &str,
        font_size: f32,
        color: Vec4,
        alignment: TextAlignment,
        max_width: Option<f32>,
        max_height: Option<f32>,
        font_family: &Option<String>,
        z_index: i32,
    ) -> HtmlResult<()> {
        let element_id = self.next_element_id();
        
        // Escape HTML characters
        let escaped_text = html_escape::encode_text(text);
        
        // Generate HTML
        self.html_content.push_str(&format!(
            "  <div id=\"{}\" class=\"kryon-text\">{}</div>\n",
            element_id, escaped_text
        ));
        
        // Generate CSS
        self.css_content.push_str(&format!(
            "#{} {{\n", element_id
        ));
        
        // Text elements inside flex/grid containers should not be absolutely positioned
        // They should flow with their parent container's layout
        // Only use absolute positioning for standalone text elements at specific positions
        
        // For now, don't apply absolute positioning to text elements
        // This allows them to flow naturally within flex/grid containers
        // In the future, we could track parent container context to make smarter decisions
        if position.x != 0.0 || position.y != 0.0 {
            // TODO: Only apply absolute positioning if the text is not inside a flex/grid container
            // For now, comment this out to fix text not appearing in containers
            // self.css_content.push_str("  position: absolute;\n");
            // self.css_content.push_str(&format!("  left: {}px;\n", position.x));
            // self.css_content.push_str(&format!("  top: {}px;\n", position.y));
        }
        
        // Font size
        self.css_content.push_str(&format!(
            "  font-size: {}px;\n", font_size
        ));
        
        // Color
        self.css_content.push_str(&format!(
            "  color: rgba({}, {}, {}, {});\n",
            (color.x * 255.0) as u8,
            (color.y * 255.0) as u8,
            (color.z * 255.0) as u8,
            color.w
        ));
        
        // Font family
        if let Some(family) = font_family {
            self.css_content.push_str(&format!(
                "  font-family: \"{}\", Arial, sans-serif;\n", family
            ));
        }
        
        // Text alignment
        let text_align = match alignment {
            TextAlignment::Start => "left",
            TextAlignment::Center => "center",
            TextAlignment::End => "right",
            TextAlignment::Justify => "justify",
        };
        self.css_content.push_str(&format!(
            "  text-align: {};\n", text_align
        ));
        
        // Max width/height
        if let Some(width) = max_width {
            self.css_content.push_str(&format!(
                "  max-width: {}px;\n", width
            ));
            self.css_content.push_str("  overflow-wrap: break-word;\n");
        }
        
        if let Some(height) = max_height {
            self.css_content.push_str(&format!(
                "  max-height: {}px;\n", height
            ));
            self.css_content.push_str("  overflow: hidden;\n");
        }
        
        // Z-index
        self.css_content.push_str(&format!(
            "  z-index: {};\n", z_index
        ));
        
        // Line height for better text rendering
        self.css_content.push_str(&format!(
            "  line-height: {}px;\n", font_size * 1.2
        ));
        
        self.css_content.push_str("}\n");
        
        Ok(())
    }
    
    /// Generate an input element
    pub fn generate_input_element(
        &mut self,
        position: Vec2,
        size: Vec2,
        text: &str,
        placeholder: &str,
        font_size: f32,
        text_color: Vec4,
        background_color: Vec4,
        border_color: Vec4,
        border_width: f32,
        border_radius: f32,
        is_focused: bool,
        is_readonly: bool,
    ) -> HtmlResult<()> {
        let element_id = self.next_element_id();
        
        // Escape HTML attributes
        let escaped_text = html_escape::encode_text(text);
        let escaped_placeholder = html_escape::encode_text(placeholder);
        
        // Generate HTML
        let readonly_attr = if is_readonly { " readonly" } else { "" };
        let focused_attr = if is_focused { " autofocus" } else { "" };
        
        self.html_content.push_str(&format!(
            "  <input id=\"{}\" type=\"text\" class=\"kryon-input\" value=\"{}\" placeholder=\"{}\"{}{}/>\n",
            element_id, escaped_text, escaped_placeholder, readonly_attr, focused_attr
        ));
        
        // Generate CSS
        self.css_content.push_str(&format!(
            "#{} {{\n", element_id
        ));
        self.css_content.push_str(&format!(
            "  position: absolute;\n"
        ));
        self.css_content.push_str(&format!(
            "  left: {}px;\n", position.x
        ));
        self.css_content.push_str(&format!(
            "  top: {}px;\n", position.y
        ));
        self.css_content.push_str(&format!(
            "  width: {}px;\n", size.x
        ));
        self.css_content.push_str(&format!(
            "  height: {}px;\n", size.y
        ));
        self.css_content.push_str(&format!(
            "  font-size: {}px;\n", font_size
        ));
        
        // Colors
        self.css_content.push_str(&format!(
            "  color: rgba({}, {}, {}, {});\n",
            (text_color.x * 255.0) as u8,
            (text_color.y * 255.0) as u8,
            (text_color.z * 255.0) as u8,
            text_color.w
        ));
        
        if background_color.w > 0.0 {
            self.css_content.push_str(&format!(
                "  background-color: rgba({}, {}, {}, {});\n",
                (background_color.x * 255.0) as u8,
                (background_color.y * 255.0) as u8,
                (background_color.z * 255.0) as u8,
                background_color.w
            ));
        }
        
        // Border
        if border_width > 0.0 && border_color.w > 0.0 {
            self.css_content.push_str(&format!(
                "  border: {}px solid rgba({}, {}, {}, {});\n",
                border_width,
                (border_color.x * 255.0) as u8,
                (border_color.y * 255.0) as u8,
                (border_color.z * 255.0) as u8,
                border_color.w
            ));
        }
        
        // Border radius
        if border_radius > 0.0 {
            self.css_content.push_str(&format!(
                "  border-radius: {}px;\n", border_radius
            ));
        }
        
        // Remove default input styling
        self.css_content.push_str("  box-sizing: border-box;\n");
        self.css_content.push_str("  padding: 4px 8px;\n");
        self.css_content.push_str("  outline: none;\n");
        
        self.css_content.push_str("}\n");
        
        Ok(())
    }
    
    /// Generate a checkbox element
    pub fn generate_checkbox_element(
        &mut self,
        position: Vec2,
        size: Vec2,
        is_checked: bool,
        text: &str,
        font_size: f32,
        text_color: Vec4,
        background_color: Vec4,
        border_color: Vec4,
        border_width: f32,
        check_color: Vec4,
    ) -> HtmlResult<()> {
        let element_id = self.next_element_id();
        let checkbox_id = format!("{}-checkbox", element_id);
        let label_id = format!("{}-label", element_id);
        
        // Escape HTML
        let escaped_text = html_escape::encode_text(text);
        
        // Generate HTML
        let checked_attr = if is_checked { " checked" } else { "" };
        
        self.html_content.push_str(&format!(
            "  <div id=\"{}\" class=\"kryon-checkbox-container\">\n", element_id
        ));
        self.html_content.push_str(&format!(
            "    <input id=\"{}\" type=\"checkbox\" class=\"kryon-checkbox\"{}/>\n",
            checkbox_id, checked_attr
        ));
        self.html_content.push_str(&format!(
            "    <label id=\"{}\" for=\"{}\" class=\"kryon-checkbox-label\">{}</label>\n",
            label_id, checkbox_id, escaped_text
        ));
        self.html_content.push_str("  </div>\n");
        
        // Generate CSS
        self.css_content.push_str(&format!(
            "#{} {{\n", element_id
        ));
        self.css_content.push_str(&format!(
            "  position: absolute;\n"
        ));
        self.css_content.push_str(&format!(
            "  left: {}px;\n", position.x
        ));
        self.css_content.push_str(&format!(
            "  top: {}px;\n", position.y
        ));
        self.css_content.push_str(&format!(
            "  width: {}px;\n", size.x
        ));
        self.css_content.push_str(&format!(
            "  height: {}px;\n", size.y
        ));
        self.css_content.push_str("  display: flex;\n");
        self.css_content.push_str("  align-items: center;\n");
        self.css_content.push_str("}\n");
        
        // Checkbox styles
        self.css_content.push_str(&format!(
            "#{} {{\n", checkbox_id
        ));
        self.css_content.push_str("  appearance: none;\n");
        self.css_content.push_str("  width: 18px;\n");
        self.css_content.push_str("  height: 18px;\n");
        self.css_content.push_str("  margin-right: 8px;\n");
        
        if background_color.w > 0.0 {
            self.css_content.push_str(&format!(
                "  background-color: rgba({}, {}, {}, {});\n",
                (background_color.x * 255.0) as u8,
                (background_color.y * 255.0) as u8,
                (background_color.z * 255.0) as u8,
                background_color.w
            ));
        }
        
        if border_width > 0.0 && border_color.w > 0.0 {
            self.css_content.push_str(&format!(
                "  border: {}px solid rgba({}, {}, {}, {});\n",
                border_width,
                (border_color.x * 255.0) as u8,
                (border_color.y * 255.0) as u8,
                (border_color.z * 255.0) as u8,
                border_color.w
            ));
        }
        
        self.css_content.push_str("}\n");
        
        // Checked state
        self.css_content.push_str(&format!(
            "#{}:checked {{\n", checkbox_id
        ));
        self.css_content.push_str(&format!(
            "  background-color: rgba({}, {}, {}, {});\n",
            (check_color.x * 255.0) as u8,
            (check_color.y * 255.0) as u8,
            (check_color.z * 255.0) as u8,
            check_color.w
        ));
        self.css_content.push_str("}\n");
        
        // Label styles
        self.css_content.push_str(&format!(
            "#{} {{\n", label_id
        ));
        self.css_content.push_str(&format!(
            "  font-size: {}px;\n", font_size
        ));
        self.css_content.push_str(&format!(
            "  color: rgba({}, {}, {}, {});\n",
            (text_color.x * 255.0) as u8,
            (text_color.y * 255.0) as u8,
            (text_color.z * 255.0) as u8,
            text_color.w
        ));
        self.css_content.push_str("  cursor: pointer;\n");
        self.css_content.push_str("}\n");
        
        Ok(())
    }
    
    /// Generate an image element
    pub fn generate_image_element(
        &mut self,
        position: Vec2,
        size: Vec2,
        source: &str,
        opacity: f32,
    ) -> HtmlResult<()> {
        let element_id = self.next_element_id();
        
        // Escape HTML attributes
        let escaped_source = html_escape::encode_text(source);
        
        // Generate HTML
        self.html_content.push_str(&format!(
            "  <img id=\"{}\" class=\"kryon-image\" src=\"{}\" alt=\"\"/>\n",
            element_id, escaped_source
        ));
        
        // Generate CSS
        self.css_content.push_str(&format!(
            "#{} {{\n", element_id
        ));
        self.css_content.push_str(&format!(
            "  position: absolute;\n"
        ));
        self.css_content.push_str(&format!(
            "  left: {}px;\n", position.x
        ));
        self.css_content.push_str(&format!(
            "  top: {}px;\n", position.y
        ));
        self.css_content.push_str(&format!(
            "  width: {}px;\n", size.x
        ));
        self.css_content.push_str(&format!(
            "  height: {}px;\n", size.y
        ));
        self.css_content.push_str(&format!(
            "  opacity: {};\n", opacity
        ));
        self.css_content.push_str("  object-fit: cover;\n");
        self.css_content.push_str("}\n");
        
        Ok(())
    }
}

// Add html_escape as a dependency
mod html_escape {
    pub fn encode_text(text: &str) -> String {
        text.replace('&', "&amp;")
            .replace('<', "&lt;")
            .replace('>', "&gt;")
            .replace('"', "&quot;")
            .replace('\'', "&#39;")
    }
}