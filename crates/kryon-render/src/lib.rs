use glam::{Vec2, Vec4};
use kryon_core::{Element, ElementId};
use kryon_layout::LayoutResult;

// Re-export core types
pub mod types;
pub use types::*;

pub mod commands;
pub use commands::*;

pub mod element_renderer;
pub use element_renderer::*;

pub mod events;
pub use events::InputEvent;
pub use events::KeyCode;
pub use events::MouseButton;

pub mod text_manager;
pub use text_manager::*;

pub mod embed_view;

#[cfg(feature = "wasm")]
pub mod wasm;
#[cfg(feature = "wasm")]
pub use wasm::*;

/// Error types for rendering operations
#[derive(Debug, thiserror::Error)]
pub enum RenderError {
    #[error("Renderer initialization failed: {0}")]
    InitializationFailed(String),
    #[error("Render operation failed: {0}")]
    RenderFailed(String),
    #[error("Resource not found: {0}")]
    ResourceNotFound(String),
    #[error("Unsupported operation: {0}")]
    UnsupportedOperation(String),
}

pub type RenderResult<T> = std::result::Result<T, RenderError>;

/// Core rendering trait that all backends must implement.
pub trait Renderer {
    type Surface;
    type Context;

    fn initialize(surface: Self::Surface) -> RenderResult<Self>
    where
        Self: Sized;
    fn begin_frame(&mut self, clear_color: Vec4) -> RenderResult<Self::Context>;
    fn end_frame(&mut self, context: Self::Context) -> RenderResult<()>;
    fn render_element(
        &mut self,
        context: &mut Self::Context,
        element: &Element,
        layout: &LayoutResult,
        element_id: ElementId,
    ) -> RenderResult<()>;
    fn resize(&mut self, new_size: Vec2) -> RenderResult<()>;
    fn viewport_size(&self) -> Vec2;
}

/// Trait for backends that use command-based rendering.
pub trait CommandRenderer: Renderer {
    fn execute_commands(
        &mut self,
        context: &mut Self::Context,
        commands: &[RenderCommand],
    ) -> RenderResult<()>;
    
    fn render_element(
        &mut self,
        _context: &mut Self::Context,
        _element: &Element,
        _layout: &LayoutResult,
        _element_id: ElementId,
    ) -> RenderResult<()> {
        // Default implementation - backends should override this
        Ok(())
    }
}

// Test module
#[cfg(test)]
mod test;