// crates/kryon-wgpu/src/vertex.rs
use bytemuck::{Pod, Zeroable};
use glam::{Vec2, Vec4};

#[repr(C)]
#[derive(Copy, Clone, Debug, Pod, Zeroable)]
pub struct ViewProjectionUniform {
    pub view_proj: [[f32; 4]; 4],
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Pod, Zeroable)]
pub struct RectVertex {
    pub position: [f32; 2],
    pub color: [f32; 4],
}

impl RectVertex {
    pub fn desc() -> wgpu::VertexBufferLayout<'static> {
        wgpu::VertexBufferLayout {
            array_stride: std::mem::size_of::<RectVertex>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &[
                wgpu::VertexAttribute {
                    offset: 0,
                    shader_location: 0,
                    format: wgpu::VertexFormat::Float32x2,
                },
                wgpu::VertexAttribute {
                    offset: std::mem::size_of::<[f32; 2]>() as wgpu::BufferAddress,
                    shader_location: 1,
                    format: wgpu::VertexFormat::Float32x4,
                },
            ],
        }
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Pod, Zeroable)]
pub struct TextVertex {
    pub position: [f32; 2],
    pub tex_coords: [f32; 2],
    pub color: [f32; 4],
}

impl TextVertex {
    pub fn desc() -> wgpu::VertexBufferLayout<'static> {
        wgpu::VertexBufferLayout {
            array_stride: std::mem::size_of::<TextVertex>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &[
                wgpu::VertexAttribute {
                    offset: 0,
                    shader_location: 0,
                    format: wgpu::VertexFormat::Float32x2,
                },
                wgpu::VertexAttribute {
                    offset: std::mem::size_of::<[f32; 2]>() as wgpu::BufferAddress,
                    shader_location: 1,
                    format: wgpu::VertexFormat::Float32x2,
                },
                wgpu::VertexAttribute {
                    offset: std::mem::size_of::<[f32; 4]>() as wgpu::BufferAddress,
                    shader_location: 2,
                    format: wgpu::VertexFormat::Float32x4,
                },
            ],
        }
    }
}

pub fn generate_rounded_rect_vertices(
    position: Vec2,
    size: Vec2,
    color: Vec4,
    _border_radius: f32,
    _border_width: f32,
    _border_color: Vec4,
) -> Vec<RectVertex> {
    // For now, generate a simple quad (TODO: implement rounded corners)
    let x = position.x;
    let y = position.y;
    let w = size.x;
    let h = size.y;
    
    vec![
        RectVertex {
            position: [x, y],
            color: color.into(),
        },
        RectVertex {
            position: [x + w, y],
            color: color.into(),
        },
        RectVertex {
            position: [x + w, y + h],
            color: color.into(),
        },
        RectVertex {
            position: [x, y + h],
            color: color.into(),
        },
    ]
}

pub fn generate_line_vertices(
    start: Vec2,
    end: Vec2,
    color: Vec4,
    width: f32,
) -> Vec<RectVertex> {
    // Generate a thin rectangle for the line
    let dx = end.x - start.x;
    let dy = end.y - start.y;
    let length = (dx * dx + dy * dy).sqrt();
    
    if length == 0.0 {
        return vec![];
    }
    
    // Calculate perpendicular vector for width
    let half_width = width / 2.0;
    let perp_x = -dy / length * half_width;
    let perp_y = dx / length * half_width;
    
    vec![
        RectVertex {
            position: [start.x + perp_x, start.y + perp_y],
            color: color.into(),
        },
        RectVertex {
            position: [end.x + perp_x, end.y + perp_y],
            color: color.into(),
        },
        RectVertex {
            position: [end.x - perp_x, end.y - perp_y],
            color: color.into(),
        },
        RectVertex {
            position: [start.x - perp_x, start.y - perp_y],
            color: color.into(),
        },
    ]
}

pub fn generate_circle_vertices(
    center: Vec2,
    radius: f32,
    fill_color: Option<Vec4>,
    _stroke_color: Option<Vec4>,
    _stroke_width: f32,
) -> Vec<RectVertex> {
    let mut vertices = Vec::new();
    
    if let Some(fill) = fill_color {
        let segments = 32;
        
        // Center vertex
        vertices.push(RectVertex {
            position: [center.x, center.y],
            color: fill.into(),
        });
        
        // Generate circle vertices
        for i in 0..=segments {
            let angle = (i as f32) * 2.0 * std::f32::consts::PI / (segments as f32);
            let x = center.x + angle.cos() * radius;
            let y = center.y + angle.sin() * radius;
            
            vertices.push(RectVertex {
                position: [x, y],
                color: fill.into(),
            });
        }
    }
    
    // TODO: Handle stroke rendering
    vertices
}

pub fn generate_ellipse_vertices(
    center: Vec2,
    rx: f32,
    ry: f32,
    fill_color: Option<Vec4>,
    _stroke_color: Option<Vec4>,
    _stroke_width: f32,
) -> Vec<RectVertex> {
    let mut vertices = Vec::new();
    
    if let Some(fill) = fill_color {
        let segments = 32;
        
        // Center vertex
        vertices.push(RectVertex {
            position: [center.x, center.y],
            color: fill.into(),
        });
        
        // Generate ellipse vertices
        for i in 0..=segments {
            let angle = (i as f32) * 2.0 * std::f32::consts::PI / (segments as f32);
            let x = center.x + angle.cos() * rx;
            let y = center.y + angle.sin() * ry;
            
            vertices.push(RectVertex {
                position: [x, y],
                color: fill.into(),
            });
        }
    }
    
    // TODO: Handle stroke rendering
    vertices
}

pub fn generate_polygon_vertices(
    points: &[Vec2],
    fill_color: Option<Vec4>,
    _stroke_color: Option<Vec4>,
    _stroke_width: f32,
) -> Vec<RectVertex> {
    let mut vertices = Vec::new();
    
    if let Some(fill) = fill_color {
        if points.len() >= 3 {
            // Calculate centroid for triangle fan
            let centroid = points.iter().fold(Vec2::ZERO, |acc, p| acc + *p) / points.len() as f32;
            
            // Center vertex
            vertices.push(RectVertex {
                position: [centroid.x, centroid.y],
                color: fill.into(),
            });
            
            // Add all polygon vertices
            for point in points {
                vertices.push(RectVertex {
                    position: [point.x, point.y],
                    color: fill.into(),
                });
            }
            
            // Close the polygon by adding the first vertex again
            if !points.is_empty() {
                vertices.push(RectVertex {
                    position: [points[0].x, points[0].y],
                    color: fill.into(),
                });
            }
        }
    }
    
    // TODO: Handle stroke rendering
    vertices
}