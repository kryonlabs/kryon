use glam::{Vec2, Vec4};
use std::collections::HashMap;
use kryon_core::{Element, ElementId, ElementType, PropertyValue, StyleComputer};
use kryon_layout::LayoutResult;
use crate::{CommandRenderer, RenderCommand, RenderResult, types::ScrollbarOrientation};

/// Renders elements by converting them to render commands
pub struct ElementRenderer<R: CommandRenderer> {
    backend: R,
    style_computer: StyleComputer,
    viewport_size: Vec2,
}

impl<R: CommandRenderer> ElementRenderer<R> {
    pub fn new(backend: R, style_computer: StyleComputer) -> Self {
        let viewport_size = backend.viewport_size();
        Self {
            backend,
            style_computer,
            viewport_size,
        }
    }

    /// Renders a complete frame by generating and executing a single batch of commands.
    pub fn render_frame(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        layout: &LayoutResult,
        root_id: ElementId,
        clear_color: Vec4,
    ) -> RenderResult<()> {
        let mut context = self.backend.begin_frame(clear_color)?;

        if let Some(root_element) = elements.get(&root_id) {
            let mut all_commands = Vec::new();

            // Use the root element's size as defined in the KRB file for the canvas.
            let canvas_size = root_element.size;
            if canvas_size.x > 0.0 && canvas_size.y > 0.0 {
                all_commands.push(RenderCommand::SetCanvasSize(canvas_size));
            }

            // Recursively fill the command list from the element tree.
            self.collect_render_commands(&mut all_commands, elements, layout, root_id, root_element)?;

            // Sort all commands by z_index to ensure proper layering
            all_commands.sort_by_key(|cmd| {
                match cmd {
                    RenderCommand::DrawRect { z_index, .. } => *z_index,
                    RenderCommand::DrawText { z_index, .. } => *z_index,
                    RenderCommand::DrawRichText { z_index, .. } => *z_index,
                    RenderCommand::DrawScrollbar { z_index, .. } => *z_index,
                    RenderCommand::DrawLine { z_index, .. } => *z_index,
                    RenderCommand::DrawCircle { z_index, .. } => *z_index,
                    RenderCommand::DrawEllipse { z_index, .. } => *z_index,
                    RenderCommand::DrawPolygon { z_index, .. } => *z_index,
                    RenderCommand::DrawPath { z_index, .. } => *z_index,
                    RenderCommand::EmbedView { z_index, .. } => *z_index,
                    RenderCommand::NativeRendererView { z_index, .. } => *z_index,
                    _ => 0, // Commands without z_index render at base layer
                }
            });

            // Execute all commands in a single batch.
            self.backend.execute_commands(&mut context, &all_commands)?;
        }

        self.backend.end_frame(context)?;
        Ok(())
    }

    /// Recursively collects render commands for an element and its children.
    pub fn collect_render_commands(
        &self,
        all_commands: &mut Vec<RenderCommand>,
        elements: &HashMap<ElementId, Element>,
        layout: &LayoutResult,
        element_id: ElementId,
        element: &Element,
    ) -> RenderResult<()> {
        // Check visibility
        if !self.is_element_visible(elements, element_id, element) {
            eprintln!("❌ [RENDER_ELEMENT] Element {} ('{}') not visible - skipping render", element_id, element.id);
            return Ok(()); // Skip invisible elements and their children.
        }

        eprintln!("✅ [RENDER_ELEMENT] Rendering element {} ('{}') - visible", element_id, element.id);

        // Set clipping if overflow is hidden.
        let position = layout.computed_positions.get(&element_id).copied();
        let size = layout.computed_sizes.get(&element_id).copied();
        
        if element.overflow_x == kryon_core::OverflowType::Hidden || 
           element.overflow_y == kryon_core::OverflowType::Hidden {
            if let (Some(pos), Some(sz)) = (position, size) {
                all_commands.push(RenderCommand::SetClip {
                    position: pos,
                    size: sz,
                });
            }
        }

        // Generate commands for the current element and append them.
        let mut element_commands = self.element_to_commands(element, layout, element_id)?;
        all_commands.append(&mut element_commands);

        // Check if we need to add scrollbar for overflow
        if (element.overflow_x == kryon_core::OverflowType::Scroll || 
            element.overflow_y == kryon_core::OverflowType::Scroll) && 
            position.is_some() && size.is_some() {
            
            let pos = position.unwrap();
            let sz = size.unwrap();
            
            // Get z-index for scrollbar (should be above content)
            let z_index = element.z_index + 1000; // Scrollbar should be above content
            
            // Add vertical scrollbar if needed
            if element.overflow_y == kryon_core::OverflowType::Scroll {
                // Calculate content height (sum of children heights)
                let mut content_height: f32 = 0.0;
                for &child_id in &element.children {
                    if let Some(child_size) = layout.computed_sizes.get(&child_id) {
                        if let Some(child_pos) = layout.computed_positions.get(&child_id) {
                            let child_bottom = child_pos.y + child_size.y - pos.y;
                            content_height = content_height.max(child_bottom);
                        }
                    }
                }

                // Only show scrollbar if content exceeds viewport
                if content_height > sz.y {
                    all_commands.push(RenderCommand::DrawScrollbar {
                        position: Vec2::new(pos.x + sz.x - 12.0, pos.y),
                        size: Vec2::new(12.0, sz.y),
                        orientation: ScrollbarOrientation::Vertical,
                        scroll_position: 0.0, // TODO: implement scroll tracking
                        content_size: content_height,
                        viewport_size: sz.y,
                        track_color: Vec4::new(0.8, 0.8, 0.8, 0.3),
                        thumb_color: Vec4::new(0.5, 0.5, 0.5, 0.8),
                        border_color: Vec4::new(0.3, 0.3, 0.3, 0.5),
                        border_width: 1.0,
                        z_index,
                    });
                }
            }
            
            // Add horizontal scrollbar if needed
            if element.overflow_x == kryon_core::OverflowType::Scroll {
                // Calculate content width (rightmost child position + width)
                let mut content_width: f32 = 0.0;
                for &child_id in &element.children {
                    if let Some(child_size) = layout.computed_sizes.get(&child_id) {
                        if let Some(child_pos) = layout.computed_positions.get(&child_id) {
                            let child_right = child_pos.x + child_size.x - pos.x;
                            content_width = content_width.max(child_right);
                        }
                    }
                }

                // Only show scrollbar if content exceeds viewport
                if content_width > sz.x {
                    all_commands.push(RenderCommand::DrawScrollbar {
                        position: Vec2::new(pos.x, pos.y + sz.y - 12.0),
                        size: Vec2::new(sz.x, 12.0),
                        orientation: ScrollbarOrientation::Horizontal,
                        scroll_position: 0.0, // TODO: implement scroll tracking
                        content_size: content_width,
                        viewport_size: sz.x,
                        track_color: Vec4::new(0.8, 0.8, 0.8, 0.3),
                        thumb_color: Vec4::new(0.5, 0.5, 0.5, 0.8),
                        border_color: Vec4::new(0.3, 0.3, 0.3, 0.5),
                        border_width: 1.0,
                        z_index,
                    });
                }
            }
        }

        // Recursively process children.
        for &child_id in &element.children {
            if let Some(child_element) = elements.get(&child_id) {
                self.collect_render_commands(all_commands, elements, layout, child_id, child_element)?;
            }
        }

        // Clear clipping after rendering children if it was set.
        if element.overflow_x == kryon_core::OverflowType::Hidden || 
           element.overflow_y == kryon_core::OverflowType::Hidden {
            all_commands.push(RenderCommand::ClearClip);
        }

        Ok(())
    }

    /// Check if an element should be rendered based on visibility
    fn is_element_visible(&self, elements: &HashMap<ElementId, Element>, _element_id: ElementId, element: &Element) -> bool {
        // First check the element's own visibility
        if !element.visible {
            return false;
        }

        // Then check parent visibility recursively
        if let Some(parent_id) = element.parent {
            if let Some(parent_element) = elements.get(&parent_id) {
                return self.is_element_visible(elements, parent_id, parent_element);
            }
        }

        true // Root element or element not found - consider visible
    }

    /// Translates a single element into one or more `RenderCommand`s.
    /// This function is the heart of the renderer logic.
    fn element_to_commands(
        &self,
        element: &Element,
        layout: &LayoutResult,
        element_id: ElementId,
    ) -> RenderResult<Vec<RenderCommand>> {
        let mut commands = Vec::new();

        // Get the final computed style for the element using its current interaction state.
        let style = self.style_computer.compute_with_state(element_id, element.current_state);

        // Get the position and size FROM THE LAYOUT ENGINE. This is the single source of truth.
        let Some(position) = layout.computed_positions.get(&element_id).copied() else {
            eprintln!("🚫 [RENDER_CMD] Element {} ('{}') has no position in layout", element_id, element.id);
            return Ok(commands); // Element not positioned by layout, so it can't be drawn.
        };
        let Some(size) = layout.computed_sizes.get(&element_id).copied() else {
            eprintln!("🚫 [RENDER_CMD] Element {} ('{}') has no size in layout", element_id, element.id);
            return Ok(commands); // Element has no size, so it can't be drawn.
        };

        eprintln!("🎨 [RENDER_CMD] Element {} ('{}') pos={:?}, size={:?}, type={:?}", 
            element_id, element.id, position, size, element.element_type);

        // Generate commands based on element type
        match element.element_type {
            ElementType::Container | ElementType::App | ElementType::Button => {
                self.render_container(&mut commands, element, &style, position, size);
            },
            ElementType::Text => {
                self.render_text(&mut commands, element, &style, position, size);
            },
            ElementType::Input => {
                self.render_input(&mut commands, element, &style, position, size);
            },
            ElementType::Image => {
                self.render_image(&mut commands, element, &style, position, size);
            },
            ElementType::Canvas => {
                self.render_canvas(&mut commands, element, &style, position, size, element_id);
            },
            // Canvas3D would be handled as a Canvas with 3D content
            ElementType::EmbedView => {
                self.render_embed_view(&mut commands, element, position, size, element_id);
            },
            // NativeRendererView is now handled by EmbedView
            // Slider would be a custom element type
            // TabBar, Dropdown etc. would be custom element types
            ElementType::Link => {
                self.render_link(&mut commands, element, &style, position, size);
            },
            ElementType::Video => {
                self.render_video(&mut commands, element, &style, position, size);
            },
            ElementType::Custom(_custom_type) => {
                // Handle custom element types
                self.render_custom(&mut commands, element, &style, position, size, _custom_type);
            },
        }

        Ok(commands)
    }

    // Helper methods for rendering specific element types
    fn render_container(&self, commands: &mut Vec<RenderCommand>, element: &Element, style: &kryon_core::ComputedStyle, position: Vec2, size: Vec2) {
        // Render background
        if style.background_color.w > 0.0 {
            commands.push(RenderCommand::DrawRect {
                position,
                size,
                color: style.background_color,
                border_radius: style.border_radius,
                border_width: style.border_width,
                border_color: style.border_color,
                transform: None, // TODO: implement transforms
                shadow: if let Some(PropertyValue::String(s)) = element.custom_properties.get("shadow") {
                    Some(s.clone())
                } else {
                    None
                },
                z_index: element.z_index,
            });
        }

        // Special handling for buttons with text
        if element.element_type == ElementType::Button && !element.text.is_empty() {
            eprintln!("[RENDER_TEXT] Element {}: text='{}', alignment={:?}, size={:?}", 
                element.id, element.text, element.text_alignment, size);
            
            commands.push(RenderCommand::DrawText {
                position,
                text: element.text.clone(),
                font_size: style.font_size,
                color: style.text_color,
                alignment: element.text_alignment,
                max_width: Some(size.x),
                max_height: Some(size.y),
                transform: None, // TODO: implement transforms
                font_family: Some(element.font_family.clone()),
                z_index: element.z_index + 1, // Text above background
            });
        }
    }

    fn render_text(&self, commands: &mut Vec<RenderCommand>, element: &Element, style: &kryon_core::ComputedStyle, position: Vec2, size: Vec2) {
        // TODO: implement rich text support
        if false {
            commands.push(RenderCommand::DrawRichText {
                position,
                rich_text: kryon_core::RichText::default(), // placeholder
                max_width: Some(size.x),
                max_height: Some(size.y),
                default_color: style.text_color,
                alignment: Some(element.text_alignment),
                transform: None, // TODO: implement transforms
                z_index: element.z_index,
            });
        } else if !element.text.is_empty() {
            eprintln!("[RENDER_TEXT] Element {}: text='{}', alignment={:?}, size={:?}", 
                element.id, element.text, element.text_alignment, size);
            
            commands.push(RenderCommand::DrawText {
                position,
                text: element.text.clone(),
                font_size: style.font_size,
                color: style.text_color,
                alignment: element.text_alignment,
                max_width: Some(size.x),
                max_height: Some(size.y),
                transform: None, // TODO: implement transforms
                font_family: Some(element.font_family.clone()),
                z_index: element.z_index,
            });
        }
    }

    fn render_input(&self, commands: &mut Vec<RenderCommand>, element: &Element, style: &kryon_core::ComputedStyle, position: Vec2, size: Vec2) {
        let text = element.custom_properties.get("value")
            .and_then(|v| v.as_string())
            .unwrap_or(&element.text)
            .to_string();
        
        let placeholder = element.custom_properties.get("placeholder")
            .and_then(|v| v.as_string())
            .unwrap_or("")
            .to_string();
        
        let is_focused = element.custom_properties.get("focused")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);
        
        let is_readonly = element.custom_properties.get("readonly")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);

        commands.push(RenderCommand::DrawTextInput {
            position,
            size,
            text,
            placeholder,
            font_size: style.font_size,
            text_color: style.text_color,
            background_color: style.background_color,
            border_color: if is_focused { 
                Vec4::new(0.2, 0.5, 0.8, 1.0) 
            } else { 
                style.border_color 
            },
            border_width: style.border_width.max(1.0),
            border_radius: style.border_radius,
            is_focused,
            is_readonly,
            transform: None, // TODO: implement transforms
        });
    }

    fn render_image(&self, commands: &mut Vec<RenderCommand>, element: &Element, style: &kryon_core::ComputedStyle, position: Vec2, size: Vec2) {
        if let Some(PropertyValue::String(source)) = element.custom_properties.get("src") {
            commands.push(RenderCommand::DrawImage {
                position,
                size,
                source: source.clone(),
                opacity: style.opacity,
                transform: None, // TODO: implement transforms
            });
        }
    }

    fn render_canvas(&self, commands: &mut Vec<RenderCommand>, _element: &Element, _style: &kryon_core::ComputedStyle, position: Vec2, size: Vec2, element_id: ElementId) {
        commands.push(RenderCommand::BeginCanvas {
            canvas_id: format!("canvas_{}", element_id),
            position,
            size,
        });

        // Canvas drawing commands would be added here based on script execution
        
        commands.push(RenderCommand::EndCanvas);
    }


    fn render_embed_view(&self, commands: &mut Vec<RenderCommand>, element: &Element, position: Vec2, size: Vec2, element_id: ElementId) {
        let view_type = element.custom_properties.get("view_type")
            .and_then(|v| v.as_string())
            .unwrap_or("webview")
            .to_string();
        
        let source = element.custom_properties.get("source")
            .and_then(|v| v.as_string())
            .map(|s| s.to_string());
        
        let config = element.custom_properties.clone();

        commands.push(RenderCommand::EmbedView {
            view_id: format!("embed_{}", element_id),
            view_type,
            source,
            position,
            size,
            config,
            z_index: element.z_index,
        });
    }




    fn render_link(&self, commands: &mut Vec<RenderCommand>, element: &Element, style: &kryon_core::ComputedStyle, position: Vec2, size: Vec2) {
        // Link is rendered as text with special styling
        self.render_text(commands, element, style, position, size);
    }

    fn render_video(&self, commands: &mut Vec<RenderCommand>, element: &Element, _style: &kryon_core::ComputedStyle, position: Vec2, size: Vec2) {
        // Video would use EmbedView with video player
        let element_id = element.custom_properties.get("element_id")
            .and_then(|v| match v {
                PropertyValue::Float(n) => Some(*n as u32),
                PropertyValue::Int(n) => Some(*n as u32),
                _ => None
            })
            .unwrap_or(0);
        self.render_embed_view(commands, element, position, size, element_id);
    }

    fn render_custom(&self, commands: &mut Vec<RenderCommand>, element: &Element, style: &kryon_core::ComputedStyle, position: Vec2, size: Vec2, _custom_type: u8) {
        // For now, render custom elements as containers
        self.render_container(commands, element, style, position, size);
    }

    // Getters
    pub fn backend(&self) -> &R {
        &self.backend
    }

    pub fn backend_mut(&mut self) -> &mut R {
        &mut self.backend
    }

    pub fn resize(&mut self, new_size: Vec2) -> RenderResult<()> {
        self.viewport_size = new_size;
        self.backend.resize(new_size)
    }
}