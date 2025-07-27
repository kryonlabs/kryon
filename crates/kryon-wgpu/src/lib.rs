// crates/kryon-wgpu/src/lib.rs
use kryon_render::{
    Renderer, CommandRenderer, RenderCommand, RenderResult, RenderError
};
use kryon_layout::LayoutResult;
use glam::{Vec2, Vec4, Mat4};
use winit::window::Window;
use kryon_core::{TransformData, TransformPropertyType, CSSUnit, CSSUnitValue};
use wgpu::util::DeviceExt;

pub mod shaders;
pub mod vertex;
pub mod text;
pub mod resources;

#[cfg(feature = "web")]
pub mod web_renderer;

#[cfg(feature = "web")]
pub use web_renderer::WebWgpuRenderer;

use vertex::*;
use text::TextRenderer;
use resources::ResourceManager;

pub struct WgpuRenderer {
    surface: wgpu::Surface<'static>,
    device: wgpu::Device,
    queue: wgpu::Queue,
    config: wgpu::SurfaceConfiguration,
    size: Vec2,
    
    // Rendering pipeline
    rect_pipeline: wgpu::RenderPipeline,
    #[allow(dead_code)]
    text_pipeline: wgpu::RenderPipeline,
    
    // Uniform buffers
    view_proj_buffer: wgpu::Buffer,
    view_proj_bind_group: wgpu::BindGroup,
    
    // Text rendering
    text_renderer: TextRenderer,
    
    // Resource management
    _resource_manager: ResourceManager,
    
    // Vertex buffers (reusable)
    vertex_buffer: wgpu::Buffer,
    index_buffer: wgpu::Buffer,
}

pub struct WgpuRenderContext {
    encoder: wgpu::CommandEncoder,
    view: wgpu::TextureView,
    surface_texture: wgpu::SurfaceTexture,
}

impl Renderer for WgpuRenderer {
    type Surface = (std::sync::Arc<Window>, Vec2);
    type Context = WgpuRenderContext;
    
    fn initialize(surface: Self::Surface) -> RenderResult<Self> where Self: Sized {
        pollster::block_on(Self::new_async(surface.0, surface.1))
    }
    
    fn begin_frame(&mut self, _clear_color: Vec4) -> RenderResult<Self::Context> {
        let surface_texture = self.surface
            .get_current_texture()
            .map_err(|e| RenderError::RenderFailed(format!("Failed to get surface texture: {}", e)))?;
        
        let view = surface_texture.texture.create_view(&wgpu::TextureViewDescriptor::default());
        
        let encoder = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor {
            label: Some("Render Encoder"),
        });
        
        Ok(WgpuRenderContext { encoder, view, surface_texture })
    }
    
    fn end_frame(&mut self, context: Self::Context) -> RenderResult<()> {
        self.queue.submit(std::iter::once(context.encoder.finish()));
        context.surface_texture.present();
        Ok(())
    }
    
    fn render_element(
        &mut self,
        _context: &mut Self::Context,
        _element: &kryon_core::Element,
        _layout: &LayoutResult,
        _element_id: kryon_core::ElementId,
    ) -> RenderResult<()> {
        // This method is not used in command-based rendering
        Ok(())
    }
    
    fn resize(&mut self, new_size: Vec2) -> RenderResult<()> {
        if new_size.x > 0.0 && new_size.y > 0.0 {
            self.size = new_size;
            self.config.width = new_size.x as u32;
            self.config.height = new_size.y as u32;
            self.surface.configure(&self.device, &self.config);
            
            // Update projection matrix
            self.update_view_projection()?;
        }
        Ok(())
    }
    
    fn viewport_size(&self) -> Vec2 {
        self.size
    }
}

impl CommandRenderer for WgpuRenderer {
    fn execute_commands(
        &mut self,
        context: &mut Self::Context,
        commands: &[RenderCommand],
    ) -> RenderResult<()> {
        println!("🔧 [WGPU_DEBUG] execute_commands() called with {} commands", commands.len());
        for (i, cmd) in commands.iter().enumerate() {
            println!("🔧 [WGPU_DEBUG] Command {}: {:?}", i, cmd);
        }
        
        if commands.is_empty() {
            println!("❌ [WGPU_DEBUG] Empty command list - clearing screen only");
            // Still need to clear the screen
            let render_pass = context.encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("Clear Pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &context.view,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(wgpu::Color {
                            r: 0.1, g: 0.1, b: 0.1, a: 1.0,
                        }),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: None,
                occlusion_query_set: None,
                timestamp_writes: None,
            });
            drop(render_pass);
            return Ok(());
        }
        
        // Clear screen first
        {
            let _clear_pass = context.encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("Initial Clear Pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &context.view,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(wgpu::Color {
                            r: 0.1, g: 0.1, b: 0.1, a: 1.0,
                        }),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: None,
                occlusion_query_set: None,
                timestamp_writes: None,
            });
        } // clear_pass dropped here
        
        // Separate commands by type for batching
        let mut rect_commands = Vec::new();
        let mut text_commands = Vec::new();
        let mut image_commands = Vec::new();
        let mut line_commands = Vec::new();
        let mut circle_commands = Vec::new();
        let mut ellipse_commands = Vec::new();
        let mut polygon_commands = Vec::new();
        let mut path_commands = Vec::new();
        
        for command in commands {
            match command {
                RenderCommand::DrawRect { .. } => rect_commands.push(command),
                RenderCommand::DrawText { .. } => text_commands.push(command),
                RenderCommand::DrawImage { .. } => image_commands.push(command),
                RenderCommand::DrawLine { .. } => line_commands.push(command),
                RenderCommand::DrawCircle { .. } => circle_commands.push(command),
                RenderCommand::DrawEllipse { .. } => ellipse_commands.push(command),
                RenderCommand::DrawPolygon { .. } => polygon_commands.push(command),
                RenderCommand::DrawPath { .. } => path_commands.push(command),
                _ => {
                    println!("🔧 [WGPU_DEBUG] Unhandled command type: {:?}", command);
                }
            }
        }
        
        println!("🔧 [WGPU_DEBUG] Grouped commands: {} rects, {} text, {} lines, {} images, {} circles, {} ellipses, {} polygons, {} paths", 
                 rect_commands.len(), text_commands.len(), line_commands.len(), image_commands.len(),
                 circle_commands.len(), ellipse_commands.len(), polygon_commands.len(), path_commands.len());
        
        // Render rectangles
        if !rect_commands.is_empty() {
            println!("🔧 [WGPU_DEBUG] Calling render_rects with {} commands", rect_commands.len());
            self.render_rects(context, &rect_commands)?;
        } else {
            println!("❌ [WGPU_DEBUG] No rectangle commands to render");
        }
        
        // Render lines
        if !line_commands.is_empty() {
            self.render_lines(context, &line_commands)?;
        }
        
        // Render circles
        if !circle_commands.is_empty() {
            self.render_circles(context, &circle_commands)?;
        }
        
        // Render ellipses
        if !ellipse_commands.is_empty() {
            self.render_ellipses(context, &ellipse_commands)?;
        }
        
        // Render polygons
        if !polygon_commands.is_empty() {
            self.render_polygons(context, &polygon_commands)?;
        }
        
        // Render paths
        if !path_commands.is_empty() {
            self.render_paths(context, &path_commands)?;
        }
        
        // Render text
        if !text_commands.is_empty() {
            self.render_text(context, &text_commands)?;
        }
        
        // Render images
        if !image_commands.is_empty() {
            self.render_images(context, &image_commands)?;
        }
        
        Ok(())
    }
}

impl WgpuRenderer {

    async fn new_async(window: std::sync::Arc<Window>, size: Vec2) -> RenderResult<Self> {
        eprintln!("Creating WGPU instance...");
        let instance = wgpu::Instance::new(wgpu::InstanceDescriptor {
            backends: wgpu::Backends::all(),
            flags: wgpu::InstanceFlags::DEBUG,
            ..Default::default()
        });

        // The surface is now created *inside* the renderer from the window handle.
        // This resolves the type mismatch and the original ownership panic.
        let surface = instance.create_surface(window)
            .map_err(|e| RenderError::InitializationFailed(format!("Failed to create surface: {}", e)))?;

        // Debug: Enumerate all adapters first
        eprintln!("Enumerating all adapters...");
        let adapters: Vec<_> = instance.enumerate_adapters(wgpu::Backends::all()).into_iter().collect();
        eprintln!("Found {} total adapters:", adapters.len());
        for (i, adapter) in adapters.iter().enumerate() {
            let info = adapter.get_info();
            eprintln!("  Adapter {}: {} ({:?}) - {:?}", i, info.name, info.backend, info.device_type);
        }

        if adapters.is_empty() {
            return Err(RenderError::InitializationFailed("No adapters enumerated by WGPU".to_string()));
        }

        eprintln!("Requesting adapter with surface compatibility...");
        let adapter = instance
            .request_adapter(&wgpu::RequestAdapterOptions {
                power_preference: wgpu::PowerPreference::default(),
                compatible_surface: Some(&surface),
                force_fallback_adapter: false,
            })
            .await;

        let adapter = match adapter {
            Some(adapter) => {
                let info = adapter.get_info();
                eprintln!("SUCCESS: Found compatible adapter: {} ({:?})", info.name, info.backend);
                adapter
            }
            None => {
                eprintln!("ERROR: No surface-compatible adapter found!");

                // Try without surface compatibility as fallback
                eprintln!("Trying without surface compatibility...");
                let fallback_adapter = instance
                    .request_adapter(&wgpu::RequestAdapterOptions {
                        power_preference: wgpu::PowerPreference::default(),
                        compatible_surface: None,
                        force_fallback_adapter: false,
                    })
                    .await;

                match fallback_adapter {
                    Some(adapter) => {
                        let info = adapter.get_info();
                        eprintln!("FALLBACK: Using adapter without surface check: {} ({:?})", info.name, info.backend);
                        adapter
                    }
                    None => {
                        return Err(RenderError::InitializationFailed("No adapter found even without surface compatibility".to_string()));
                    }
                }
            }
        };

        let (device, queue) = adapter
            .request_device(
                &wgpu::DeviceDescriptor {
                    label: None,
                    required_features: wgpu::Features::empty(),
                    required_limits: wgpu::Limits::default(),
                },
                None,
            )
            .await
            .map_err(|e| RenderError::InitializationFailed(format!("Device request failed: {}", e)))?;

        let surface_caps = surface.get_capabilities(&adapter);
        let surface_format = surface_caps
            .formats
            .iter()
            .copied()
            .find(|f| f.is_srgb())
            .unwrap_or(surface_caps.formats[0]);

        let config = wgpu::SurfaceConfiguration {
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            format: surface_format,
            width: size.x as u32,
            height: size.y as u32,
            present_mode: surface_caps.present_modes[0],
            alpha_mode: surface_caps.alpha_modes[0],
            view_formats: vec![],
            desired_maximum_frame_latency: 2,
        };

        surface.configure(&device, &config);

        // Create uniform buffer for view-projection matrix
        let view_proj_buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("View Projection Buffer"),
            size: std::mem::size_of::<ViewProjectionUniform>() as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        let uniform_bind_group_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::VERTEX,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
            label: Some("uniform_bind_group_layout"),
        });

        let view_proj_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            layout: &uniform_bind_group_layout,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: view_proj_buffer.as_entire_binding(),
            }],
            label: Some("view_proj_bind_group"),
        });

        // Create shaders
        let rect_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("Rectangle Shader"),
            source: wgpu::ShaderSource::Wgsl(include_str!("shaders/rect.wgsl").into()),
        });

        let text_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("Text Shader"),
            source: wgpu::ShaderSource::Wgsl(include_str!("shaders/text.wgsl").into()),
        });

        // Create text rendering pipeline 
        eprintln!("🔧 [WGPU_DEBUG] Creating text renderer...");
        let text_renderer = TextRenderer::new(&device, &queue)
            .map_err(|e| RenderError::InitializationFailed(format!("Text renderer creation failed: {}", e)))?;
        eprintln!("✅ [WGPU_DEBUG] Text renderer created successfully");

        // The text pipeline needs the bind group layout from the text atlas
        eprintln!("🔧 [WGPU_DEBUG] Creating text pipeline layout with bind groups...");
        let text_pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("Text Pipeline Layout"),
            bind_group_layouts: &[&uniform_bind_group_layout, text_renderer.bind_group_layout()],
            push_constant_ranges: &[],
        });
        eprintln!("✅ [WGPU_DEBUG] Text pipeline layout created successfully");

        eprintln!("🔧 [WGPU_DEBUG] Creating text render pipeline...");
        let text_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("Text Pipeline"),
            layout: Some(&text_pipeline_layout),
            vertex: wgpu::VertexState {
                module: &text_shader,
                entry_point: "vs_main",
                buffers: &[TextVertex::desc()],
            },
            fragment: Some(wgpu::FragmentState {
                module: &text_shader,
                entry_point: "fs_main",
                targets: &[Some(wgpu::ColorTargetState {
                    format: config.format,
                    blend: Some(wgpu::BlendState::ALPHA_BLENDING),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: None, // Text can have different winding orders
                unclipped_depth: false,
                polygon_mode: wgpu::PolygonMode::Fill,
                conservative: false,
            },
            depth_stencil: None,
            multisample: wgpu::MultisampleState::default(),
            multiview: None,
        });

        // Create rectangle rendering pipeline
        let rect_pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("Rectangle Pipeline Layout"),
            bind_group_layouts: &[&uniform_bind_group_layout],
            push_constant_ranges: &[],
        });

        let rect_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("Rectangle Pipeline"),
            layout: Some(&rect_pipeline_layout),
            vertex: wgpu::VertexState {
                module: &rect_shader,
                entry_point: "vs_main",
                buffers: &[RectVertex::desc()],
            },
            fragment: Some(wgpu::FragmentState {
                module: &rect_shader,
                entry_point: "fs_main",
                targets: &[Some(wgpu::ColorTargetState {
                    format: config.format,
                    blend: Some(wgpu::BlendState::ALPHA_BLENDING),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: Some(wgpu::Face::Back),
                unclipped_depth: false,
                polygon_mode: wgpu::PolygonMode::Fill,
                conservative: false,
            },
            depth_stencil: None,
            multisample: wgpu::MultisampleState::default(),
            multiview: None,
        });
        
        // Create vertex and index buffers
        let vertex_buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("Vertex Buffer"),
            size: 1024 * 1024, // 1MB buffer
            usage: wgpu::BufferUsages::VERTEX | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        
        let index_buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("Index Buffer"),
            size: 1024 * 1024, // 1MB buffer
            usage: wgpu::BufferUsages::INDEX | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        let mut renderer = Self {
            surface,
            device,
            queue,
            config,
            size,
            rect_pipeline,
            text_pipeline,
            view_proj_buffer,
            view_proj_bind_group,
            text_renderer,
            _resource_manager: ResourceManager::new(),
            vertex_buffer,
            index_buffer,
        };

        renderer.update_view_projection()?;

        Ok(renderer)
    }
    
    fn update_view_projection(&mut self) -> RenderResult<()> {
        // Create orthographic projection matrix for 2D rendering
        // Maps screen coordinates (0,0) to (width,height) to NDC (-1,-1) to (1,1)
        let projection = Mat4::orthographic_rh(
            0.0,           // left
            self.size.x,   // right
            self.size.y,   // bottom  
            0.0,           // top
            -1.0,          // near
            1.0,           // far
        );
        
        println!("🔧 [WGPU_DEBUG] Updated projection matrix for size: {:?}", self.size);
        
        let uniform = ViewProjectionUniform {
            view_proj: projection.to_cols_array_2d(),
        };
        
        self.queue.write_buffer(
            &self.view_proj_buffer,
            0,
            bytemuck::cast_slice(&[uniform]),
        );
        
        Ok(())
    }
    
    fn render_rects(
        &mut self,
        context: &mut WgpuRenderContext,
        commands: &[&RenderCommand],
    ) -> RenderResult<()> {
        println!("🔧 [WGPU_DEBUG] render_rects called with {} commands", commands.len());
        
        if commands.is_empty() {
            return Ok(());
        }

        // Create vertices for all rectangles
        let mut vertices = Vec::new();
        let mut indices = Vec::new();
        
        for (i, command) in commands.iter().enumerate() {
            if let RenderCommand::DrawRect { position, size, color, border_width, border_color, .. } = command {
                println!("🔧 [WGPU_DEBUG] Adding rect {}: pos={:?}, size={:?}, color={:?}, border_width={}", i, position, size, color, border_width);
                
                let start_index = vertices.len() as u16;
                
                // First draw the fill rectangle
                vertices.extend_from_slice(&[
                    RectVertex { position: [position.x, position.y], color: [color.x, color.y, color.z, color.w] },
                    RectVertex { position: [position.x + size.x, position.y], color: [color.x, color.y, color.z, color.w] },
                    RectVertex { position: [position.x + size.x, position.y + size.y], color: [color.x, color.y, color.z, color.w] },
                    RectVertex { position: [position.x, position.y + size.y], color: [color.x, color.y, color.z, color.w] },
                ]);
                
                // Add 6 indices for 2 triangles
                indices.extend_from_slice(&[
                    start_index, start_index + 1, start_index + 2,
                    start_index + 2, start_index + 3, start_index,
                ]);
                
                // If there's a border, draw it as 4 rectangles (top, right, bottom, left)
                if *border_width > 0.0 && border_color.w > 0.0 {
                    println!("🔧 [WGPU_DEBUG] Adding border: width={}, color={:?}", border_width, border_color);
                    
                    // Top border
                    let border_start = vertices.len() as u16;
                    vertices.extend_from_slice(&[
                        RectVertex { position: [position.x, position.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x + size.x, position.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x + size.x, position.y + border_width], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x, position.y + border_width], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                    ]);
                    indices.extend_from_slice(&[
                        border_start, border_start + 1, border_start + 2,
                        border_start + 2, border_start + 3, border_start,
                    ]);
                    
                    // Right border
                    let border_start = vertices.len() as u16;
                    vertices.extend_from_slice(&[
                        RectVertex { position: [position.x + size.x - border_width, position.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x + size.x, position.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x + size.x, position.y + size.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x + size.x - border_width, position.y + size.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                    ]);
                    indices.extend_from_slice(&[
                        border_start, border_start + 1, border_start + 2,
                        border_start + 2, border_start + 3, border_start,
                    ]);
                    
                    // Bottom border
                    let border_start = vertices.len() as u16;
                    vertices.extend_from_slice(&[
                        RectVertex { position: [position.x, position.y + size.y - border_width], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x + size.x, position.y + size.y - border_width], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x + size.x, position.y + size.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x, position.y + size.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                    ]);
                    indices.extend_from_slice(&[
                        border_start, border_start + 1, border_start + 2,
                        border_start + 2, border_start + 3, border_start,
                    ]);
                    
                    // Left border
                    let border_start = vertices.len() as u16;
                    vertices.extend_from_slice(&[
                        RectVertex { position: [position.x, position.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x + border_width, position.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x + border_width, position.y + size.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                        RectVertex { position: [position.x, position.y + size.y], color: [border_color.x, border_color.y, border_color.z, border_color.w] },
                    ]);
                    indices.extend_from_slice(&[
                        border_start, border_start + 1, border_start + 2,
                        border_start + 2, border_start + 3, border_start,
                    ]);
                }
            }
        }
        
        if vertices.is_empty() {
            println!("❌ [WGPU_DEBUG] No vertices to render!");
            return Ok(());
        }
        
        println!("🔧 [WGPU_DEBUG] Total vertices: {}, indices: {}", vertices.len(), indices.len());
        
        // Debug print first few vertices
        if !vertices.is_empty() {
            println!("🔧 [WGPU_DEBUG] First 4 vertices:");
            for (i, vertex) in vertices.iter().take(4).enumerate() {
                println!("  Vertex {}: pos=({:.1}, {:.1}), color=({:.2}, {:.2}, {:.2}, {:.2})", 
                         i, vertex.position[0], vertex.position[1], 
                         vertex.color[0], vertex.color[1], vertex.color[2], vertex.color[3]);
            }
        }
        
        // Create temporary buffers for this frame
        let vertex_buffer = self.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("Rectangle Vertex Buffer"),
            contents: bytemuck::cast_slice(&vertices),
            usage: wgpu::BufferUsages::VERTEX,
        });
        
        let index_buffer = self.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("Rectangle Index Buffer"),
            contents: bytemuck::cast_slice(&indices),
            usage: wgpu::BufferUsages::INDEX,
        });
        
        // Render the rectangles
        {
            let mut render_pass = context.encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("Rectangle Render Pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &context.view,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Load,
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: None,
                occlusion_query_set: None,
                timestamp_writes: None,
            });
            
            println!("🔧 [WGPU_DEBUG] Setting pipeline...");
            render_pass.set_pipeline(&self.rect_pipeline);
            println!("🔧 [WGPU_DEBUG] Setting bind group...");
            render_pass.set_bind_group(0, &self.view_proj_bind_group, &[]);
            println!("🔧 [WGPU_DEBUG] Setting vertex buffer...");
            render_pass.set_vertex_buffer(0, vertex_buffer.slice(..));
            println!("🔧 [WGPU_DEBUG] Setting index buffer...");
            render_pass.set_index_buffer(index_buffer.slice(..), wgpu::IndexFormat::Uint16);
            println!("🔧 [WGPU_DEBUG] Drawing indexed with {} indices...", indices.len());
            render_pass.draw_indexed(0..indices.len() as u32, 0, 0..1);
            
            println!("✅ [WGPU_DEBUG] Rendered {} triangles", indices.len() / 3);
        }
        
        Ok(())
    }
    
    fn render_text(
        &mut self,
        _context: &mut WgpuRenderContext,
        commands: &[&RenderCommand],
    ) -> RenderResult<()> {
        for command in commands {
            if let RenderCommand::DrawText {
                position,
                text,
                font_size,
                color,
                alignment: _,
                max_width: _,
                max_height: _,
                transform,
                font_family: _, // WGPU doesn't support custom fonts yet
                z_index: _, // WGPU doesn't use z_index for text rendering ordering
            } = command {
                // Apply transform to text position if present
                let final_position = if let Some(transform_data) = transform {
                    let (scale, rotation, translation) = extract_transform_values(transform_data);
                    let transform_matrix = create_transform_matrix(scale, rotation, translation);
                    apply_transform_to_position(*position, &transform_matrix)
                } else {
                    *position
                };
                
                // Use the basic render_text method but don't submit render passes
                // Just prepare the text and generate vertices without actual rendering
                if let Err(e) = self.text_renderer.prepare_text(&self.device, &self.queue, text, *font_size) {
                    eprintln!("⚠️  [WGPU_DEBUG] Text preparation failed: {}", e);
                    continue;
                }
                
                let _vertices = self.text_renderer.generate_text_vertices(text, final_position, *font_size, *color);
                println!("⚠️  [WGPU_DEBUG] Text rendering prepared but not submitted: '{}' at {:?}", text, final_position);
                
                // Don't actually render for now to avoid texture issues
                // TODO: Implement proper render pass submission once texture issues are resolved
            }
        }
        Ok(())
    }
    
    fn render_images(
        &mut self,
        _context: &mut WgpuRenderContext,
        _commands: &[&RenderCommand],
    ) -> RenderResult<()> {
        // TODO: Implement image rendering with transform support
        // When implementing, handle transform field in RenderCommand::DrawImage
        Ok(())
    }
    
    fn render_lines(
        &mut self,
        context: &mut WgpuRenderContext,
        commands: &[&RenderCommand],
    ) -> RenderResult<()> {
        let mut vertices = Vec::new();
        let mut indices = Vec::new();
        let mut index_offset = 0u16;
        
        for command in commands {
            if let RenderCommand::DrawLine {
                start,
                end,
                color,
                width,
                transform,
                z_index: _,
            } = command {
                // Generate vertices for line (as a thin rectangle)
                let line_vertices = generate_line_vertices(*start, *end, *color, *width);
                
                // Apply transform if present
                let transformed_vertices = if let Some(transform_data) = transform {
                    apply_transform_to_vertices(line_vertices, transform_data)
                } else {
                    line_vertices
                };
                
                // Add vertices and indices
                for vertex in transformed_vertices {
                    vertices.push(vertex);
                }
                
                // Generate indices for line quad
                indices.extend_from_slice(&[
                    index_offset,
                    index_offset + 1,
                    index_offset + 2,
                    index_offset + 2,
                    index_offset + 3,
                    index_offset,
                ]);
                index_offset += 4;
            }
        }
        
        if vertices.is_empty() {
            return Ok(());
        }
        
        // Upload and render using existing rect pipeline
        self.queue.write_buffer(&self.vertex_buffer, 0, bytemuck::cast_slice(&vertices));
        self.queue.write_buffer(&self.index_buffer, 0, bytemuck::cast_slice(&indices));
        
        let mut render_pass = context.encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("Line Render Pass"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: &context.view,
                resolve_target: None,
                ops: wgpu::Operations {
                    load: wgpu::LoadOp::Load,
                    store: wgpu::StoreOp::Store,
                },
            })],
            depth_stencil_attachment: None,
            occlusion_query_set: None,
            timestamp_writes: None,
        });
        
        render_pass.set_pipeline(&self.rect_pipeline);
        render_pass.set_bind_group(0, &self.view_proj_bind_group, &[]);
        render_pass.set_vertex_buffer(0, self.vertex_buffer.slice(..));
        render_pass.set_index_buffer(self.index_buffer.slice(..), wgpu::IndexFormat::Uint16);
        render_pass.draw_indexed(0..indices.len() as u32, 0, 0..1);
        
        Ok(())
    }
    
    fn render_circles(
        &mut self,
        context: &mut WgpuRenderContext,
        commands: &[&RenderCommand],
    ) -> RenderResult<()> {
        let mut vertices = Vec::new();
        let mut indices = Vec::new();
        let mut index_offset = 0u16;
        
        for command in commands {
            if let RenderCommand::DrawCircle {
                center,
                radius,
                fill_color,
                stroke_color,
                stroke_width,
                transform,
                z_index: _,
            } = command {
                // Generate vertices for circle (as a polygon)
                let circle_vertices = generate_circle_vertices(*center, *radius, *fill_color, *stroke_color, *stroke_width);
                
                // Apply transform if present
                let transformed_vertices = if let Some(transform_data) = transform {
                    apply_transform_to_vertices(circle_vertices, transform_data)
                } else {
                    circle_vertices
                };
                
                // Add vertices and generate indices for triangulated circle
                let start_index = index_offset;
                for vertex in transformed_vertices {
                    vertices.push(vertex);
                    index_offset += 1;
                }
                
                // Generate indices for triangle fan (circle triangulation)
                let vertex_count = index_offset - start_index;
                for i in 1..(vertex_count - 1) {
                    indices.extend_from_slice(&[
                        start_index,
                        start_index + i,
                        start_index + i + 1,
                    ]);
                }
            }
        }
        
        if vertices.is_empty() {
            return Ok(());
        }
        
        // Upload and render
        self.queue.write_buffer(&self.vertex_buffer, 0, bytemuck::cast_slice(&vertices));
        self.queue.write_buffer(&self.index_buffer, 0, bytemuck::cast_slice(&indices));
        
        let mut render_pass = context.encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("Circle Render Pass"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: &context.view,
                resolve_target: None,
                ops: wgpu::Operations {
                    load: wgpu::LoadOp::Load,
                    store: wgpu::StoreOp::Store,
                },
            })],
            depth_stencil_attachment: None,
            occlusion_query_set: None,
            timestamp_writes: None,
        });
        
        render_pass.set_pipeline(&self.rect_pipeline);
        render_pass.set_bind_group(0, &self.view_proj_bind_group, &[]);
        render_pass.set_vertex_buffer(0, self.vertex_buffer.slice(..));
        render_pass.set_index_buffer(self.index_buffer.slice(..), wgpu::IndexFormat::Uint16);
        render_pass.draw_indexed(0..indices.len() as u32, 0, 0..1);
        
        Ok(())
    }
    
    fn render_ellipses(
        &mut self,
        context: &mut WgpuRenderContext,
        commands: &[&RenderCommand],
    ) -> RenderResult<()> {
        let mut vertices = Vec::new();
        let mut indices = Vec::new();
        let mut index_offset = 0u16;
        
        for command in commands {
            if let RenderCommand::DrawEllipse {
                center,
                rx,
                ry,
                fill_color,
                stroke_color,
                stroke_width,
                transform,
                z_index: _,
            } = command {
                // Generate vertices for ellipse (as a polygon)
                let ellipse_vertices = generate_ellipse_vertices(*center, *rx, *ry, *fill_color, *stroke_color, *stroke_width);
                
                // Apply transform if present
                let transformed_vertices = if let Some(transform_data) = transform {
                    apply_transform_to_vertices(ellipse_vertices, transform_data)
                } else {
                    ellipse_vertices
                };
                
                // Add vertices and generate indices for triangulated ellipse
                let start_index = index_offset;
                for vertex in transformed_vertices {
                    vertices.push(vertex);
                    index_offset += 1;
                }
                
                // Generate indices for triangle fan
                let vertex_count = index_offset - start_index;
                for i in 1..(vertex_count - 1) {
                    indices.extend_from_slice(&[
                        start_index,
                        start_index + i,
                        start_index + i + 1,
                    ]);
                }
            }
        }
        
        if vertices.is_empty() {
            return Ok(());
        }
        
        // Upload and render
        self.queue.write_buffer(&self.vertex_buffer, 0, bytemuck::cast_slice(&vertices));
        self.queue.write_buffer(&self.index_buffer, 0, bytemuck::cast_slice(&indices));
        
        let mut render_pass = context.encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("Ellipse Render Pass"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: &context.view,
                resolve_target: None,
                ops: wgpu::Operations {
                    load: wgpu::LoadOp::Load,
                    store: wgpu::StoreOp::Store,
                },
            })],
            depth_stencil_attachment: None,
            occlusion_query_set: None,
            timestamp_writes: None,
        });
        
        render_pass.set_pipeline(&self.rect_pipeline);
        render_pass.set_bind_group(0, &self.view_proj_bind_group, &[]);
        render_pass.set_vertex_buffer(0, self.vertex_buffer.slice(..));
        render_pass.set_index_buffer(self.index_buffer.slice(..), wgpu::IndexFormat::Uint16);
        render_pass.draw_indexed(0..indices.len() as u32, 0, 0..1);
        
        Ok(())
    }
    
    fn render_polygons(
        &mut self,
        context: &mut WgpuRenderContext,
        commands: &[&RenderCommand],
    ) -> RenderResult<()> {
        let mut vertices = Vec::new();
        let mut indices = Vec::new();
        let mut index_offset = 0u16;
        
        for command in commands {
            if let RenderCommand::DrawPolygon {
                points,
                fill_color,
                stroke_color,
                stroke_width,
                transform,
                z_index: _,
            } = command {
                if points.len() < 3 {
                    continue; // Skip invalid polygons
                }
                
                // Generate vertices for polygon
                let polygon_vertices = generate_polygon_vertices(points, *fill_color, *stroke_color, *stroke_width);
                
                // Apply transform if present
                let transformed_vertices = if let Some(transform_data) = transform {
                    apply_transform_to_vertices(polygon_vertices, transform_data)
                } else {
                    polygon_vertices
                };
                
                // Add vertices and generate indices for triangulated polygon
                let start_index = index_offset;
                for vertex in transformed_vertices {
                    vertices.push(vertex);
                    index_offset += 1;
                }
                
                // Generate indices for triangle fan
                let vertex_count = index_offset - start_index;
                for i in 1..(vertex_count - 1) {
                    indices.extend_from_slice(&[
                        start_index,
                        start_index + i,
                        start_index + i + 1,
                    ]);
                }
            }
        }
        
        if vertices.is_empty() {
            return Ok(());
        }
        
        // Upload and render
        self.queue.write_buffer(&self.vertex_buffer, 0, bytemuck::cast_slice(&vertices));
        self.queue.write_buffer(&self.index_buffer, 0, bytemuck::cast_slice(&indices));
        
        let mut render_pass = context.encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("Polygon Render Pass"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: &context.view,
                resolve_target: None,
                ops: wgpu::Operations {
                    load: wgpu::LoadOp::Load,
                    store: wgpu::StoreOp::Store,
                },
            })],
            depth_stencil_attachment: None,
            occlusion_query_set: None,
            timestamp_writes: None,
        });
        
        render_pass.set_pipeline(&self.rect_pipeline);
        render_pass.set_bind_group(0, &self.view_proj_bind_group, &[]);
        render_pass.set_vertex_buffer(0, self.vertex_buffer.slice(..));
        render_pass.set_index_buffer(self.index_buffer.slice(..), wgpu::IndexFormat::Uint16);
        render_pass.draw_indexed(0..indices.len() as u32, 0, 0..1);
        
        Ok(())
    }
    
    fn render_paths(
        &mut self,
        _context: &mut WgpuRenderContext,
        _commands: &[&RenderCommand],
    ) -> RenderResult<()> {
        // TODO: Implement SVG path rendering - this is complex and requires path parsing
        // For now, just log that path rendering was requested
        eprintln!("[WGPU] Path rendering not yet implemented");
        Ok(())
    }
}

/// Extract transform values from TransformData
fn extract_transform_values(transform: &TransformData) -> (Vec2, f32, Vec2) {
    let mut scale = Vec2::new(1.0, 1.0);
    let mut rotation = 0.0f32;
    let mut translation = Vec2::new(0.0, 0.0);
    
    for property in &transform.properties {
        match property.property_type {
            TransformPropertyType::Scale => {
                let value = css_unit_to_pixels(&property.value);
                scale = Vec2::new(value, value);
            }
            TransformPropertyType::ScaleX => {
                scale.x = css_unit_to_pixels(&property.value);
            }
            TransformPropertyType::ScaleY => {
                scale.y = css_unit_to_pixels(&property.value);
            }
            TransformPropertyType::TranslateX => {
                translation.x = css_unit_to_pixels(&property.value);
            }
            TransformPropertyType::TranslateY => {
                translation.y = css_unit_to_pixels(&property.value);
            }
            TransformPropertyType::Rotate => {
                rotation = css_unit_to_radians(&property.value);
            }
            _ => {
                eprintln!("[WGPU_TRANSFORM] Unsupported transform property: {:?}", property.property_type);
            }
        }
    }
    
    (scale, rotation, translation)
}

/// Convert CSS unit value to pixels (simplified)
fn css_unit_to_pixels(unit_value: &CSSUnitValue) -> f32 {
    match unit_value.unit {
        CSSUnit::Pixels => unit_value.value as f32,
        CSSUnit::Number => unit_value.value as f32,
        CSSUnit::Em => unit_value.value as f32 * 16.0, // Assume 16px base
        CSSUnit::Rem => unit_value.value as f32 * 16.0, // Assume 16px base
        CSSUnit::Percentage => unit_value.value as f32 / 100.0,
        _ => {
            eprintln!("[WGPU_TRANSFORM] Unsupported CSS unit for size: {:?}", unit_value.unit);
            unit_value.value as f32
        }
    }
}

/// Convert CSS unit value to radians for rotation
fn css_unit_to_radians(unit_value: &CSSUnitValue) -> f32 {
    match unit_value.unit {
        CSSUnit::Degrees => unit_value.value as f32 * std::f32::consts::PI / 180.0,
        CSSUnit::Radians => unit_value.value as f32,
        CSSUnit::Turns => unit_value.value as f32 * 2.0 * std::f32::consts::PI,
        _ => {
            eprintln!("[WGPU_TRANSFORM] Unsupported CSS unit for rotation: {:?}", unit_value.unit);
            unit_value.value as f32
        }
    }
}

/// Create a transformation matrix for WGPU
fn create_transform_matrix(scale: Vec2, rotation: f32, translation: Vec2) -> Mat4 {
    let scale_matrix = Mat4::from_scale(scale.extend(1.0));
    let rotation_matrix = Mat4::from_rotation_z(rotation);
    let translation_matrix = Mat4::from_translation(translation.extend(0.0));
    
    translation_matrix * rotation_matrix * scale_matrix
}

/// Apply transform to position using transformation matrix
fn apply_transform_to_position(position: Vec2, transform_matrix: &Mat4) -> Vec2 {
    let transformed = transform_matrix.transform_point3(position.extend(0.0));
    Vec2::new(transformed.x, transformed.y)
}

/// Apply transform to vertices using transformation matrix
fn apply_transform_to_vertices(vertices: Vec<RectVertex>, transform_data: &TransformData) -> Vec<RectVertex> {
    let (scale, rotation, translation) = extract_transform_values(transform_data);
    let transform_matrix = create_transform_matrix(scale, rotation, translation);
    
    vertices.into_iter().map(|mut vertex| {
        let transformed = transform_matrix.transform_point3(Vec2::new(vertex.position[0], vertex.position[1]).extend(0.0));
        vertex.position[0] = transformed.x;
        vertex.position[1] = transformed.y;
        vertex
    }).collect()
}