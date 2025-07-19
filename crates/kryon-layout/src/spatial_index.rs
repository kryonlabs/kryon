//! Spatial indexing for accelerated hit testing
//!
//! This module provides spatial data structures to accelerate hit testing
//! for large numbers of elements.

use kryon_core::ElementId;
use glam::Vec2;
use std::collections::HashMap;

/// A spatial index for fast hit testing using a grid-based approach
#[derive(Debug, Clone)]
pub struct SpatialIndex {
    /// Grid cell size in pixels
    cell_size: f32,
    /// Grid bounds
    bounds: Rect,
    /// Grid cells containing element IDs
    grid: HashMap<(i32, i32), Vec<ElementId>>,
    /// Element bounds for precise hit testing
    element_bounds: HashMap<ElementId, Rect>,
}

/// Axis-aligned bounding box
#[derive(Debug, Clone, Copy)]
pub struct Rect {
    pub min: Vec2,
    pub max: Vec2,
}

impl Rect {
    pub fn new(min: Vec2, max: Vec2) -> Self {
        Self { min, max }
    }

    pub fn from_position_size(position: Vec2, size: Vec2) -> Self {
        Self {
            min: position,
            max: position + size,
        }
    }

    pub fn contains_point(&self, point: Vec2) -> bool {
        point.x >= self.min.x && point.x <= self.max.x &&
        point.y >= self.min.y && point.y <= self.max.y
    }

    pub fn intersects(&self, other: &Rect) -> bool {
        self.min.x <= other.max.x && self.max.x >= other.min.x &&
        self.min.y <= other.max.y && self.max.y >= other.min.y
    }

    pub fn area(&self) -> f32 {
        (self.max.x - self.min.x) * (self.max.y - self.min.y)
    }
}

impl SpatialIndex {
    /// Create a new spatial index with the given cell size
    pub fn new(cell_size: f32, bounds: Rect) -> Self {
        Self {
            cell_size,
            bounds,
            grid: HashMap::new(),
            element_bounds: HashMap::new(),
        }
    }

    /// Insert an element into the spatial index
    pub fn insert(&mut self, element_id: ElementId, rect: Rect) {
        // Store element bounds for precise hit testing
        self.element_bounds.insert(element_id, rect);

        // Add to grid cells
        let cells = self.get_overlapping_cells(&rect);
        for cell in cells {
            self.grid.entry(cell).or_insert_with(Vec::new).push(element_id);
        }
    }

    /// Remove an element from the spatial index
    pub fn remove(&mut self, element_id: ElementId) {
        if let Some(rect) = self.element_bounds.remove(&element_id) {
            let cells = self.get_overlapping_cells(&rect);
            for cell in cells {
                if let Some(cell_elements) = self.grid.get_mut(&cell) {
                    cell_elements.retain(|&id| id != element_id);
                    if cell_elements.is_empty() {
                        self.grid.remove(&cell);
                    }
                }
            }
        }
    }

    /// Update an element's position in the spatial index
    pub fn update(&mut self, element_id: ElementId, new_rect: Rect) {
        self.remove(element_id);
        self.insert(element_id, new_rect);
    }

    /// Find all elements at the given point
    pub fn query_point(&self, point: Vec2) -> Vec<ElementId> {
        let mut results = Vec::new();
        let cell = self.point_to_cell(point);

        if let Some(cell_elements) = self.grid.get(&cell) {
            for &element_id in cell_elements {
                if let Some(rect) = self.element_bounds.get(&element_id) {
                    if rect.contains_point(point) {
                        results.push(element_id);
                    }
                }
            }
        }

        results
    }

    /// Find all elements that intersect with the given rectangle
    pub fn query_rect(&self, rect: &Rect) -> Vec<ElementId> {
        let mut results = Vec::new();
        let mut seen = std::collections::HashSet::new();
        let cells = self.get_overlapping_cells(rect);

        for cell in cells {
            if let Some(cell_elements) = self.grid.get(&cell) {
                for &element_id in cell_elements {
                    if seen.insert(element_id) {
                        if let Some(element_rect) = self.element_bounds.get(&element_id) {
                            if rect.intersects(element_rect) {
                                results.push(element_id);
                            }
                        }
                    }
                }
            }
        }

        results
    }

    /// Find the topmost element at the given point (considering z-index)
    pub fn query_topmost(&self, point: Vec2, z_indices: &HashMap<ElementId, i32>) -> Option<ElementId> {
        let candidates = self.query_point(point);
        
        candidates.into_iter()
            .max_by_key(|&element_id| {
                z_indices.get(&element_id).copied().unwrap_or(0)
            })
    }

    /// Get all elements in the spatial index
    pub fn all_elements(&self) -> Vec<ElementId> {
        self.element_bounds.keys().copied().collect()
    }

    /// Clear all elements from the spatial index
    pub fn clear(&mut self) {
        self.grid.clear();
        self.element_bounds.clear();
    }

    /// Get statistics about the spatial index
    pub fn stats(&self) -> SpatialIndexStats {
        let total_elements = self.element_bounds.len();
        let total_cells = self.grid.len();
        let avg_elements_per_cell = if total_cells > 0 {
            self.grid.values().map(|v| v.len()).sum::<usize>() as f32 / total_cells as f32
        } else {
            0.0
        };

        SpatialIndexStats {
            total_elements,
            total_cells,
            avg_elements_per_cell,
            cell_size: self.cell_size,
            bounds: self.bounds,
        }
    }

    /// Convert a point to grid cell coordinates
    fn point_to_cell(&self, point: Vec2) -> (i32, i32) {
        let x = ((point.x - self.bounds.min.x) / self.cell_size).floor() as i32;
        let y = ((point.y - self.bounds.min.y) / self.cell_size).floor() as i32;
        (x, y)
    }

    /// Get all grid cells that overlap with the given rectangle
    fn get_overlapping_cells(&self, rect: &Rect) -> Vec<(i32, i32)> {
        let min_cell = self.point_to_cell(rect.min);
        let max_cell = self.point_to_cell(rect.max);

        let mut cells = Vec::new();
        for x in min_cell.0..=max_cell.0 {
            for y in min_cell.1..=max_cell.1 {
                cells.push((x, y));
            }
        }

        cells
    }
}

/// Statistics about a spatial index
#[derive(Debug, Clone)]
pub struct SpatialIndexStats {
    pub total_elements: usize,
    pub total_cells: usize,
    pub avg_elements_per_cell: f32,
    pub cell_size: f32,
    pub bounds: Rect,
}

/// Spatial index manager for layout systems
pub struct SpatialIndexManager {
    index: SpatialIndex,
    z_indices: HashMap<ElementId, i32>,
}

impl SpatialIndexManager {
    /// Create a new spatial index manager
    pub fn new(cell_size: f32, bounds: Rect) -> Self {
        Self {
            index: SpatialIndex::new(cell_size, bounds),
            z_indices: HashMap::new(),
        }
    }

    /// Update the spatial index with layout results
    pub fn update_from_layout(
        &mut self,
        positions: &HashMap<ElementId, Vec2>,
        sizes: &HashMap<ElementId, Vec2>,
        z_indices: &HashMap<ElementId, i32>,
    ) {
        // Clear existing data
        self.index.clear();
        self.z_indices.clear();

        // Rebuild index
        for (&element_id, &position) in positions {
            if let Some(&size) = sizes.get(&element_id) {
                let rect = Rect::from_position_size(position, size);
                self.index.insert(element_id, rect);
                
                if let Some(&z_index) = z_indices.get(&element_id) {
                    self.z_indices.insert(element_id, z_index);
                }
            }
        }
    }

    /// Find the topmost element at the given point
    pub fn hit_test(&self, point: Vec2) -> Option<ElementId> {
        self.index.query_topmost(point, &self.z_indices)
    }

    /// Find all elements in the given rectangle
    pub fn query_region(&self, rect: &Rect) -> Vec<ElementId> {
        self.index.query_rect(rect)
    }

    /// Get performance statistics
    pub fn stats(&self) -> SpatialIndexStats {
        self.index.stats()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_spatial_index_basic() {
        let bounds = Rect::new(Vec2::new(0.0, 0.0), Vec2::new(1000.0, 1000.0));
        let mut index = SpatialIndex::new(100.0, bounds);

        // Insert some elements
        let rect1 = Rect::from_position_size(Vec2::new(50.0, 50.0), Vec2::new(100.0, 100.0));
        let rect2 = Rect::from_position_size(Vec2::new(200.0, 200.0), Vec2::new(100.0, 100.0));
        
        index.insert(1, rect1);
        index.insert(2, rect2);

        // Test point queries
        let results = index.query_point(Vec2::new(100.0, 100.0));
        assert!(results.contains(&1));
        assert!(!results.contains(&2));

        let results = index.query_point(Vec2::new(250.0, 250.0));
        assert!(!results.contains(&1));
        assert!(results.contains(&2));
    }

    #[test]
    fn test_spatial_index_rect_query() {
        let bounds = Rect::new(Vec2::new(0.0, 0.0), Vec2::new(1000.0, 1000.0));
        let mut index = SpatialIndex::new(100.0, bounds);

        // Insert elements
        let rect1 = Rect::from_position_size(Vec2::new(50.0, 50.0), Vec2::new(100.0, 100.0));
        let rect2 = Rect::from_position_size(Vec2::new(200.0, 200.0), Vec2::new(100.0, 100.0));
        
        index.insert(1, rect1);
        index.insert(2, rect2);

        // Query overlapping rectangle
        let query_rect = Rect::from_position_size(Vec2::new(100.0, 100.0), Vec2::new(150.0, 150.0));
        let results = index.query_rect(&query_rect);
        
        assert!(results.contains(&1));
        assert!(results.contains(&2));
    }

    #[test]
    fn test_spatial_index_update() {
        let bounds = Rect::new(Vec2::new(0.0, 0.0), Vec2::new(1000.0, 1000.0));
        let mut index = SpatialIndex::new(100.0, bounds);

        // Insert element
        let rect1 = Rect::from_position_size(Vec2::new(50.0, 50.0), Vec2::new(100.0, 100.0));
        index.insert(1, rect1);

        // Verify initial position
        let results = index.query_point(Vec2::new(100.0, 100.0));
        assert!(results.contains(&1));

        // Update position
        let rect2 = Rect::from_position_size(Vec2::new(200.0, 200.0), Vec2::new(100.0, 100.0));
        index.update(1, rect2);

        // Verify new position
        let results = index.query_point(Vec2::new(100.0, 100.0));
        assert!(!results.contains(&1));
        
        let results = index.query_point(Vec2::new(250.0, 250.0));
        assert!(results.contains(&1));
    }

    #[test]
    fn test_spatial_index_topmost() {
        let bounds = Rect::new(Vec2::new(0.0, 0.0), Vec2::new(1000.0, 1000.0));
        let mut index = SpatialIndex::new(100.0, bounds);

        // Insert overlapping elements
        let rect = Rect::from_position_size(Vec2::new(50.0, 50.0), Vec2::new(100.0, 100.0));
        index.insert(1, rect);
        index.insert(2, rect);

        // Test topmost with z-indices
        let mut z_indices = HashMap::new();
        z_indices.insert(1, 1);
        z_indices.insert(2, 2);

        let topmost = index.query_topmost(Vec2::new(100.0, 100.0), &z_indices);
        assert_eq!(topmost, Some(2));

        // Change z-indices
        z_indices.insert(1, 3);
        let topmost = index.query_topmost(Vec2::new(100.0, 100.0), &z_indices);
        assert_eq!(topmost, Some(1));
    }
}