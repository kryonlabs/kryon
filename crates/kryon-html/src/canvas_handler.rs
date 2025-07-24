use crate::{HtmlRenderer, HtmlResult, HtmlError, CanvasElement, CanvasType};
use glam::{Vec2, Vec4};
use kryon_render::RenderCommand;

impl HtmlRenderer {
    /// Begin a canvas element
    pub fn begin_canvas_element(
        &mut self,
        canvas_id: &str,
        position: Vec2,
        size: Vec2,
        canvas_type: CanvasType,
    ) -> HtmlResult<()> {
        let element_id = self.next_element_id();
        
        // Create canvas element
        let canvas_element = CanvasElement {
            id: element_id.clone(),
            element_type: canvas_type.clone(),
            position,
            size,
            commands: Vec::new(),
        };
        
        // Generate HTML canvas element
        match canvas_type {
            CanvasType::Canvas2D => {
                self.html_content.push_str(&format!(
                    "  <canvas id=\"{}\" class=\"kryon-canvas kryon-canvas-2d\" data-canvas-id=\"{}\"></canvas>\n",
                    element_id, canvas_id
                ));
            }
            CanvasType::Canvas3D => {
                self.html_content.push_str(&format!(
                    "  <canvas id=\"{}\" class=\"kryon-canvas kryon-canvas-3d\" data-canvas-id=\"{}\"></canvas>\n",
                    element_id, canvas_id
                ));
            }
            CanvasType::WebGPU => {
                self.html_content.push_str(&format!(
                    "  <canvas id=\"{}\" class=\"kryon-canvas kryon-canvas-webgpu\" data-canvas-id=\"{}\"></canvas>\n",
                    element_id, canvas_id
                ));
            }
        }
        
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
        self.css_content.push_str("}\n");
        
        // Generate JavaScript initialization
        self.generate_canvas_js(&element_id, &canvas_type)?;
        
        // Store canvas element for later processing
        self.canvas_elements.push(canvas_element);
        
        Ok(())
    }
    
    /// End canvas element
    pub fn end_canvas_element(&mut self) -> HtmlResult<()> {
        // Canvas commands are processed when the canvas is created
        // This is mainly for API compatibility
        Ok(())
    }
    
    /// Generate JavaScript for canvas initialization
    fn generate_canvas_js(&mut self, element_id: &str, canvas_type: &CanvasType) -> HtmlResult<()> {
        self.js_content.push_str(&format!(
            "// Initialize canvas: {}\n", element_id
        ));
        
        match canvas_type {
            CanvasType::Canvas2D => {
                self.js_content.push_str(&format!(
                    "document.addEventListener('DOMContentLoaded', function() {{\n"
                ));
                self.js_content.push_str(&format!(
                    "  const canvas = document.getElementById('{}');\n", element_id
                ));
                self.js_content.push_str(&format!(
                    "  if (canvas) {{\n"
                ));
                self.js_content.push_str(&format!(
                    "    const ctx = canvas.getContext('2d');\n"
                ));
                self.js_content.push_str(&format!(
                    "    // Set canvas size\n"
                ));
                self.js_content.push_str(&format!(
                    "    canvas.width = canvas.offsetWidth;\n"
                ));
                self.js_content.push_str(&format!(
                    "    canvas.height = canvas.offsetHeight;\n"
                ));
                self.js_content.push_str(&format!(
                    "    // Initialize 2D canvas\n"
                ));
                self.js_content.push_str(&format!(
                    "    kryonCanvas2D_{}(ctx);\n", element_id.replace('-', "_")
                ));
                self.js_content.push_str(&format!(
                    "  }}\n"
                ));
                self.js_content.push_str(&format!(
                    "}});\n\n"
                ));
                
                // Generate canvas drawing function
                self.js_content.push_str(&format!(
                    "function kryonCanvas2D_{}(ctx) {{\n", element_id.replace('-', "_")
                ));
                self.js_content.push_str(&format!(
                    "  // Clear canvas\n"
                ));
                self.js_content.push_str(&format!(
                    "  ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);\n"
                ));
                self.js_content.push_str(&format!(
                    "  // Canvas drawing commands will be added here\n"
                ));
                self.js_content.push_str(&format!(
                    "}}\n\n"
                ));
            }
            CanvasType::Canvas3D => {
                self.js_content.push_str(&format!(
                    "document.addEventListener('DOMContentLoaded', function() {{\n"
                ));
                self.js_content.push_str(&format!(
                    "  const canvas = document.getElementById('{}');\n", element_id
                ));
                self.js_content.push_str(&format!(
                    "  if (canvas) {{\n"
                ));
                self.js_content.push_str(&format!(
                    "    const gl = canvas.getContext('webgl2') || canvas.getContext('webgl');\n"
                ));
                self.js_content.push_str(&format!(
                    "    if (!gl) {{\n"
                ));
                self.js_content.push_str(&format!(
                    "      console.error('WebGL not supported for canvas: {}');\n", element_id
                ));
                self.js_content.push_str(&format!(
                    "      return;\n"
                ));
                self.js_content.push_str(&format!(
                    "    }}\n"
                ));
                self.js_content.push_str(&format!(
                    "    // Set canvas size\n"
                ));
                self.js_content.push_str(&format!(
                    "    canvas.width = canvas.offsetWidth;\n"
                ));
                self.js_content.push_str(&format!(
                    "    canvas.height = canvas.offsetHeight;\n"
                ));
                self.js_content.push_str(&format!(
                    "    gl.viewport(0, 0, canvas.width, canvas.height);\n"
                ));
                self.js_content.push_str(&format!(
                    "    // Initialize 3D canvas\n"
                ));
                self.js_content.push_str(&format!(
                    "    kryonCanvas3D_{}(gl);\n", element_id.replace('-', "_")
                ));
                self.js_content.push_str(&format!(
                    "  }}\n"
                ));
                self.js_content.push_str(&format!(
                    "}});\n\n"
                ));
                
                // Generate WebGL drawing function
                self.js_content.push_str(&format!(
                    "function kryonCanvas3D_{}(gl) {{\n", element_id.replace('-', "_")
                ));
                self.js_content.push_str(&format!(
                    "  // Clear canvas\n"
                ));
                self.js_content.push_str(&format!(
                    "  gl.clearColor(0.0, 0.0, 0.0, 1.0);\n"
                ));
                self.js_content.push_str(&format!(
                    "  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);\n"
                ));
                self.js_content.push_str(&format!(
                    "  // 3D rendering commands will be added here\n"
                ));
                self.js_content.push_str(&format!(
                    "}}\n\n"
                ));
            }
            CanvasType::WebGPU => {
                self.js_content.push_str(&format!(
                    "document.addEventListener('DOMContentLoaded', async function() {{\n"
                ));
                self.js_content.push_str(&format!(
                    "  const canvas = document.getElementById('{}');\n", element_id
                ));
                self.js_content.push_str(&format!(
                    "  if (canvas) {{\n"
                ));
                self.js_content.push_str(&format!(
                    "    if (!navigator.gpu) {{\n"
                ));
                self.js_content.push_str(&format!(
                    "      console.error('WebGPU not supported for canvas: {}');\n", element_id
                ));
                self.js_content.push_str(&format!(
                    "      return;\n"
                ));
                self.js_content.push_str(&format!(
                    "    }}\n"
                ));
                self.js_content.push_str(&format!(
                    "    try {{\n"
                ));
                self.js_content.push_str(&format!(
                    "      const adapter = await navigator.gpu.requestAdapter();\n"
                ));
                self.js_content.push_str(&format!(
                    "      const device = await adapter.requestDevice();\n"
                ));
                self.js_content.push_str(&format!(
                    "      const context = canvas.getContext('webgpu');\n"
                ));
                self.js_content.push_str(&format!(
                    "      const format = navigator.gpu.getPreferredCanvasFormat();\n"
                ));
                self.js_content.push_str(&format!(
                    "      context.configure({{ device, format }});\n"
                ));
                self.js_content.push_str(&format!(
                    "      // Set canvas size\n"
                ));
                self.js_content.push_str(&format!(
                    "      canvas.width = canvas.offsetWidth;\n"
                ));
                self.js_content.push_str(&format!(
                    "      canvas.height = canvas.offsetHeight;\n"
                ));
                self.js_content.push_str(&format!(
                    "      // Initialize WebGPU canvas\n"
                ));
                self.js_content.push_str(&format!(
                    "      await kryonCanvasWebGPU_{}(device, context, format);\n", element_id.replace('-', "_")
                ));
                self.js_content.push_str(&format!(
                    "    }} catch (error) {{\n"
                ));
                self.js_content.push_str(&format!(
                    "      console.error('WebGPU initialization failed for canvas {}: ', error);\n", element_id
                ));
                self.js_content.push_str(&format!(
                    "    }}\n"
                ));
                self.js_content.push_str(&format!(
                    "  }}\n"
                ));
                self.js_content.push_str(&format!(
                    "}});\n\n"
                ));
                
                // Generate WebGPU drawing function
                self.js_content.push_str(&format!(
                    "async function kryonCanvasWebGPU_{}(device, context, format) {{\n", element_id.replace('-', "_")
                ));
                self.js_content.push_str(&format!(
                    "  // WebGPU rendering commands will be added here\n"
                ));
                self.js_content.push_str(&format!(
                    "}}\n\n"
                ));
            }
        }
        
        Ok(())
    }
    
    /// Process canvas drawing commands
    pub fn process_canvas_commands(&mut self, commands: &[RenderCommand]) -> HtmlResult<()> {
        for command in commands {
            match command {
                RenderCommand::DrawCanvasRect { position, size, fill_color, stroke_color, stroke_width } => {
                    self.add_canvas_rect_command(*position, *size, *fill_color, *stroke_color, *stroke_width)?;
                }
                RenderCommand::DrawCanvasCircle { center, radius, fill_color, stroke_color, stroke_width } => {
                    self.add_canvas_circle_command(*center, *radius, *fill_color, *stroke_color, *stroke_width)?;
                }
                RenderCommand::DrawCanvasLine { start, end, color, width } => {
                    self.add_canvas_line_command(*start, *end, *color, *width)?;
                }
                RenderCommand::DrawCanvasText { position, text, font_size, color, font_family, alignment } => {
                    self.add_canvas_text_command(*position, text, *font_size, *color, font_family, *alignment)?;
                }
                _ => {
                    // Skip non-canvas commands
                }
            }
        }
        Ok(())
    }
    
    fn add_canvas_rect_command(
        &mut self,
        position: Vec2,
        size: Vec2,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
    ) -> HtmlResult<()> {
        // Get the current canvas (last one in the list)
        let canvas_count = self.canvas_elements.len();
        if canvas_count == 0 {
            return Err(HtmlError::CanvasFailed("No active canvas".to_string()));
        }
        
        let canvas_id = format!("kryon-element-{}", canvas_count);
        let function_name = format!("kryonCanvas2D_{}", canvas_id.replace('-', "_"));
        
        // Add drawing commands to the canvas function
        let js_commands = if let Some(fill) = fill_color {
            format!(
                "  ctx.fillStyle = 'rgba({}, {}, {}, {})';\n  ctx.fillRect({}, {}, {}, {});\n",
                (fill.x * 255.0) as u8,
                (fill.y * 255.0) as u8,
                (fill.z * 255.0) as u8,
                fill.w,
                position.x, position.y, size.x, size.y
            )
        } else {
            String::new()
        };
        
        let js_commands = if let Some(stroke) = stroke_color {
            format!(
                "{}  ctx.strokeStyle = 'rgba({}, {}, {}, {})';\n  ctx.lineWidth = {};\n  ctx.strokeRect({}, {}, {}, {});\n",
                js_commands,
                (stroke.x * 255.0) as u8,
                (stroke.y * 255.0) as u8,
                (stroke.z * 255.0) as u8,
                stroke.w,
                stroke_width,
                position.x, position.y, size.x, size.y
            )
        } else {
            js_commands
        };
        
        // Insert the commands into the canvas function
        self.js_content = self.js_content.replace(
            &format!("  // Canvas drawing commands will be added here\n"),
            &format!("  // Canvas drawing commands will be added here\n{}", js_commands)
        );
        
        Ok(())
    }
    
    fn add_canvas_circle_command(
        &mut self,
        center: Vec2,
        radius: f32,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
    ) -> HtmlResult<()> {
        let canvas_count = self.canvas_elements.len();
        if canvas_count == 0 {
            return Err(HtmlError::CanvasFailed("No active canvas".to_string()));
        }
        
        let mut js_commands = format!(
            "  ctx.beginPath();\n  ctx.arc({}, {}, {}, 0, 2 * Math.PI);\n",
            center.x, center.y, radius
        );
        
        if let Some(fill) = fill_color {
            js_commands.push_str(&format!(
                "  ctx.fillStyle = 'rgba({}, {}, {}, {})';\n  ctx.fill();\n",
                (fill.x * 255.0) as u8,
                (fill.y * 255.0) as u8,
                (fill.z * 255.0) as u8,
                fill.w
            ));
        }
        
        if let Some(stroke) = stroke_color {
            js_commands.push_str(&format!(
                "  ctx.strokeStyle = 'rgba({}, {}, {}, {})';\n  ctx.lineWidth = {};\n  ctx.stroke();\n",
                (stroke.x * 255.0) as u8,
                (stroke.y * 255.0) as u8,
                (stroke.z * 255.0) as u8,
                stroke.w,
                stroke_width
            ));
        }
        
        // Insert the commands into the canvas function
        self.js_content = self.js_content.replace(
            &format!("  // Canvas drawing commands will be added here\n"),
            &format!("  // Canvas drawing commands will be added here\n{}", js_commands)
        );
        
        Ok(())
    }
    
    fn add_canvas_line_command(
        &mut self,
        start: Vec2,
        end: Vec2,
        color: Vec4,
        width: f32,
    ) -> HtmlResult<()> {
        let canvas_count = self.canvas_elements.len();
        if canvas_count == 0 {
            return Err(HtmlError::CanvasFailed("No active canvas".to_string()));
        }
        
        let js_commands = format!(
            "  ctx.beginPath();\n  ctx.moveTo({}, {});\n  ctx.lineTo({}, {});\n  ctx.strokeStyle = 'rgba({}, {}, {}, {})';\n  ctx.lineWidth = {};\n  ctx.stroke();\n",
            start.x, start.y, end.x, end.y,
            (color.x * 255.0) as u8,
            (color.y * 255.0) as u8,
            (color.z * 255.0) as u8,
            color.w,
            width
        );
        
        // Insert the commands into the canvas function
        self.js_content = self.js_content.replace(
            &format!("  // Canvas drawing commands will be added here\n"),
            &format!("  // Canvas drawing commands will be added here\n{}", js_commands)
        );
        
        Ok(())
    }
    
    fn add_canvas_text_command(
        &mut self,
        position: Vec2,
        text: &str,
        font_size: f32,
        color: Vec4,
        font_family: &Option<String>,
        _alignment: kryon_core::TextAlignment,
    ) -> HtmlResult<()> {
        let canvas_count = self.canvas_elements.len();
        if canvas_count == 0 {
            return Err(HtmlError::CanvasFailed("No active canvas".to_string()));
        }
        
        let font_spec = if let Some(family) = font_family {
            format!("{}px {}", font_size, family)
        } else {
            format!("{}px Arial", font_size)
        };
        
        let js_commands = format!(
            "  ctx.font = '{}';\n  ctx.fillStyle = 'rgba({}, {}, {}, {})';\n  ctx.fillText('{}', {}, {});\n",
            font_spec,
            (color.x * 255.0) as u8,
            (color.y * 255.0) as u8,
            (color.z * 255.0) as u8,
            color.w,
            text.replace('\'', "\\'"),
            position.x,
            position.y
        );
        
        // Insert the commands into the canvas function
        self.js_content = self.js_content.replace(
            &format!("  // Canvas drawing commands will be added here\n"),
            &format!("  // Canvas drawing commands will be added here\n{}", js_commands)
        );
        
        Ok(())
    }
}