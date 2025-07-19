// crates/kryon-layout/src/lib.rs

use kryon_core::{Element, ElementId};
use glam::Vec2;
use std::collections::HashMap;

pub mod constraints;
pub mod optimized_taffy_engine;
pub mod taffy;
pub mod layout_diff;
pub mod performance;
pub mod spatial_index;

pub use constraints::*;
pub use optimized_taffy_engine::{OptimizedTaffyLayoutEngine, LayoutConfig, InvalidationRegion};
pub use taffy::{StyleConverter, TreeBuilder, LayoutComputer};
pub use layout_diff::{LayoutDiff, LayoutDiffer, PositionChange, SizeChange};
pub use performance::{LayoutProfiler, LayoutPerformanceMetrics, LayoutBenchmark, BenchmarkComplexity};
pub use spatial_index::{SpatialIndex, SpatialIndexManager, Rect, SpatialIndexStats};

// Re-export the optimized engine as the default TaffyLayoutEngine
pub use optimized_taffy_engine::OptimizedTaffyLayoutEngine as TaffyLayoutEngine;

#[derive(Debug, Clone)]
pub struct LayoutResult {
    pub computed_positions: HashMap<ElementId, Vec2>,
    pub computed_sizes: HashMap<ElementId, Vec2>,
}

pub trait LayoutEngine {
    fn compute_layout(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        root_id: ElementId,
        viewport_size: Vec2,
    ) -> LayoutResult;
}

// Legacy FlexboxLayoutEngine removed - use TaffyLayoutEngine instead
// FlexboxLayoutEngine struct and implementation (~500 lines) removed in Phase 1 Week 1-2
