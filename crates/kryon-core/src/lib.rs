// crates/kryon-core/src/lib.rs

// Re-export unified types from kryon-shared as the single source of truth
pub use kryon_shared::types::*;

pub mod krb;
pub mod elements;
pub mod properties;
pub mod property_registry;
pub mod optimized_property_cache;
pub mod resources;
pub mod events;
pub mod style;
pub mod layout_units;
pub mod text;
pub mod css_transforms;


pub use elements::*;
pub use properties::*;
pub use property_registry::*;
pub use optimized_property_cache::*;
pub use krb::*;
pub use resources::*;
pub use events::*;
pub use style::*;
pub use layout_units::*;
pub use text::*;
pub use css_transforms::*;

// Re-export the optimized property cache as the default
pub use optimized_property_cache::OptimizedPropertyCache as PropertyCache;

// Additional exports for optimized layout system
pub use optimized_property_cache::CacheStats;

// Re-export common types for convenience
pub use glam::{Vec2, Vec4};

// Add missing types for optimized app
#[derive(Debug, Clone)]
pub struct InvalidationRegion {
    pub elements: Vec<ElementId>,
    pub full_tree: bool,
    pub bounds: Option<taffy::Rect<f32>>,
} 


#[derive(Debug, thiserror::Error)]
pub enum KryonError {
    #[error("Invalid KRB file: {0}")]
    InvalidKRB(String),
    
    #[error("Unsupported version: {0}")]
    UnsupportedVersion(u16),
    
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    
    #[error("Missing section: {0}")]
    MissingSection(String),
    
    #[error("Invalid element type: {0}")]
    InvalidElementType(u8),
    
    #[error("Component not found: {0}")]
    ComponentNotFound(String),
}

pub type Result<T> = std::result::Result<T, KryonError>;