use glam::{Vec3, Vec4};

/// Camera configuration for 3D rendering
#[derive(Debug, Clone)]
pub struct CameraData {
    pub position: Vec3,
    pub target: Vec3,
    pub up: Vec3,
    pub fov: f32,
    pub near_plane: f32,
    pub far_plane: f32,
}

/// Shadow configuration
#[derive(Debug, Clone)]
pub struct Shadow {
    pub blur: f32,
    pub color: Vec4,
    pub offset_x: f32,
    pub offset_y: f32,
}

/// Image data for texture rendering
#[derive(Debug, Clone)]
pub struct ImageData {
    pub width: u32,
    pub height: u32,
    pub data: Vec<u8>,
}

/// Directional light for 3D rendering
#[derive(Debug, Clone)]
pub struct DirectionalLight {
    pub direction: Vec3,
    pub color: Vec4,
    pub intensity: f32,
}

/// Point light for 3D rendering
#[derive(Debug, Clone)]
pub struct PointLight {
    pub position: Vec3,
    pub color: Vec4,
    pub intensity: f32,
    pub range: f32,
}

/// Scrollbar orientation
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ScrollbarOrientation {
    Vertical,
    Horizontal,
}