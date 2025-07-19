//! Layout result diffing for minimal re-renders
//!
//! This module provides utilities to compute differences between layout results
//! to minimize unnecessary re-renders.

use crate::LayoutResult;
use kryon_core::ElementId;
use glam::Vec2;
use std::collections::{HashMap, HashSet};

/// Represents the difference between two layout results
#[derive(Debug, Clone)]
pub struct LayoutDiff {
    /// Elements that have changed position
    pub position_changes: HashMap<ElementId, PositionChange>,
    /// Elements that have changed size
    pub size_changes: HashMap<ElementId, SizeChange>,
    /// Elements that are newly added
    pub added_elements: HashSet<ElementId>,
    /// Elements that have been removed
    pub removed_elements: HashSet<ElementId>,
    /// Whether a full re-render is needed
    pub needs_full_render: bool,
}

/// Represents a position change
#[derive(Debug, Clone)]
pub struct PositionChange {
    pub old_position: Vec2,
    pub new_position: Vec2,
    pub delta: Vec2,
}

/// Represents a size change
#[derive(Debug, Clone)]
pub struct SizeChange {
    pub old_size: Vec2,
    pub new_size: Vec2,
    pub delta: Vec2,
}

/// Layout differ for computing minimal re-render regions
pub struct LayoutDiffer {
    /// Position tolerance for considering a position "unchanged"
    position_tolerance: f32,
    /// Size tolerance for considering a size "unchanged"
    size_tolerance: f32,
    /// Maximum number of changed elements before triggering full re-render
    max_changes_for_incremental: usize,
}

impl LayoutDiffer {
    /// Create a new layout differ with default tolerances
    pub fn new() -> Self {
        Self {
            position_tolerance: 0.1,
            size_tolerance: 0.1,
            max_changes_for_incremental: 100,
        }
    }

    /// Create a layout differ with custom tolerances
    pub fn with_tolerances(
        position_tolerance: f32,
        size_tolerance: f32,
        max_changes_for_incremental: usize,
    ) -> Self {
        Self {
            position_tolerance,
            size_tolerance,
            max_changes_for_incremental,
        }
    }

    /// Compute the difference between two layout results
    pub fn diff(&self, old: &LayoutResult, new: &LayoutResult) -> LayoutDiff {
        let mut diff = LayoutDiff {
            position_changes: HashMap::new(),
            size_changes: HashMap::new(),
            added_elements: HashSet::new(),
            removed_elements: HashSet::new(),
            needs_full_render: false,
        };

        // Find added and removed elements
        let old_elements: HashSet<_> = old.computed_positions.keys().collect();
        let new_elements: HashSet<_> = new.computed_positions.keys().collect();

        for &element_id in &new_elements {
            if !old_elements.contains(&element_id) {
                diff.added_elements.insert(*element_id);
            }
        }

        for &element_id in &old_elements {
            if !new_elements.contains(&element_id) {
                diff.removed_elements.insert(*element_id);
            }
        }

        // Check for position changes
        for (&element_id, &new_pos) in &new.computed_positions {
            if let Some(&old_pos) = old.computed_positions.get(&element_id) {
                if !self.positions_equal(old_pos, new_pos) {
                    diff.position_changes.insert(element_id, PositionChange {
                        old_position: old_pos,
                        new_position: new_pos,
                        delta: new_pos - old_pos,
                    });
                }
            }
        }

        // Check for size changes
        for (&element_id, &new_size) in &new.computed_sizes {
            if let Some(&old_size) = old.computed_sizes.get(&element_id) {
                if !self.sizes_equal(old_size, new_size) {
                    diff.size_changes.insert(element_id, SizeChange {
                        old_size,
                        new_size,
                        delta: new_size - old_size,
                    });
                }
            }
        }

        // Determine if full re-render is needed
        let total_changes = diff.position_changes.len() + 
                           diff.size_changes.len() + 
                           diff.added_elements.len() + 
                           diff.removed_elements.len();

        diff.needs_full_render = total_changes > self.max_changes_for_incremental;

        diff
    }

    /// Check if two positions are equal within tolerance
    fn positions_equal(&self, pos1: Vec2, pos2: Vec2) -> bool {
        (pos1 - pos2).length() <= self.position_tolerance
    }

    /// Check if two sizes are equal within tolerance
    fn sizes_equal(&self, size1: Vec2, size2: Vec2) -> bool {
        (size1 - size2).length() <= self.size_tolerance
    }

    /// Get all elements that have changed in any way
    pub fn get_changed_elements(&self, diff: &LayoutDiff) -> HashSet<ElementId> {
        let mut changed = HashSet::new();
        
        changed.extend(diff.position_changes.keys());
        changed.extend(diff.size_changes.keys());
        changed.extend(&diff.added_elements);
        changed.extend(&diff.removed_elements);
        
        changed
    }

    /// Get elements that need visual updates (position or size changes)
    pub fn get_visual_update_elements(&self, diff: &LayoutDiff) -> HashSet<ElementId> {
        let mut changed = HashSet::new();
        
        changed.extend(diff.position_changes.keys());
        changed.extend(diff.size_changes.keys());
        changed.extend(&diff.added_elements);
        
        changed
    }

    /// Compute bounding box for all changed elements
    pub fn get_change_bounds(&self, diff: &LayoutDiff, layout: &LayoutResult) -> Option<(Vec2, Vec2)> {
        let changed_elements = self.get_visual_update_elements(diff);
        
        if changed_elements.is_empty() {
            return None;
        }

        let mut min_pos = Vec2::new(f32::INFINITY, f32::INFINITY);
        let mut max_pos = Vec2::new(f32::NEG_INFINITY, f32::NEG_INFINITY);

        for &element_id in &changed_elements {
            if let (Some(&pos), Some(&size)) = (
                layout.computed_positions.get(&element_id),
                layout.computed_sizes.get(&element_id),
            ) {
                min_pos = min_pos.min(pos);
                max_pos = max_pos.max(pos + size);
            }
        }

        if min_pos.x != f32::INFINITY && min_pos.y != f32::INFINITY {
            Some((min_pos, max_pos - min_pos))
        } else {
            None
        }
    }

    /// Get render regions for partial updates
    pub fn get_render_regions(&self, diff: &LayoutDiff, layout: &LayoutResult) -> Vec<(Vec2, Vec2)> {
        let mut regions = Vec::new();

        // Group nearby changes into regions
        let changed_elements = self.get_visual_update_elements(diff);
        let mut processed = HashSet::new();

        for &element_id in &changed_elements {
            if processed.contains(&element_id) {
                continue;
            }

            if let (Some(&pos), Some(&size)) = (
                layout.computed_positions.get(&element_id),
                layout.computed_sizes.get(&element_id),
            ) {
                let mut region_min = pos;
                let mut region_max = pos + size;

                // Find nearby elements and merge into region
                for &other_id in &changed_elements {
                    if processed.contains(&other_id) || other_id == element_id {
                        continue;
                    }

                    if let (Some(&other_pos), Some(&other_size)) = (
                        layout.computed_positions.get(&other_id),
                        layout.computed_sizes.get(&other_id),
                    ) {
                        let other_max = other_pos + other_size;
                        
                        // Check if elements are close enough to merge
                        if self.should_merge_regions(region_min, region_max, other_pos, other_max) {
                            region_min = region_min.min(other_pos);
                            region_max = region_max.max(other_max);
                            processed.insert(other_id);
                        }
                    }
                }

                regions.push((region_min, region_max - region_min));
                processed.insert(element_id);
            }
        }

        regions
    }

    /// Check if two regions should be merged
    fn should_merge_regions(&self, min1: Vec2, max1: Vec2, min2: Vec2, max2: Vec2) -> bool {
        // Simple overlap or proximity check
        let gap_threshold = 20.0; // pixels
        
        // Check if regions overlap or are close
        let x_gap = (min2.x - max1.x).max(min1.x - max2.x).max(0.0);
        let y_gap = (min2.y - max1.y).max(min1.y - max2.y).max(0.0);
        
        x_gap <= gap_threshold && y_gap <= gap_threshold
    }
}

impl Default for LayoutDiffer {
    fn default() -> Self {
        Self::new()
    }
}

impl LayoutDiff {
    /// Check if the diff represents no changes
    pub fn is_empty(&self) -> bool {
        self.position_changes.is_empty() &&
        self.size_changes.is_empty() &&
        self.added_elements.is_empty() &&
        self.removed_elements.is_empty()
    }

    /// Get total number of changes
    pub fn change_count(&self) -> usize {
        self.position_changes.len() + 
        self.size_changes.len() + 
        self.added_elements.len() + 
        self.removed_elements.len()
    }

    /// Check if only positions changed (no size changes)
    pub fn only_position_changes(&self) -> bool {
        self.size_changes.is_empty() &&
        self.added_elements.is_empty() &&
        self.removed_elements.is_empty() &&
        !self.position_changes.is_empty()
    }

    /// Check if only sizes changed (no position changes)
    pub fn only_size_changes(&self) -> bool {
        self.position_changes.is_empty() &&
        self.added_elements.is_empty() &&
        self.removed_elements.is_empty() &&
        !self.size_changes.is_empty()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_layout_diff_empty() {
        let differ = LayoutDiffer::new();
        let layout = LayoutResult {
            computed_positions: HashMap::new(),
            computed_sizes: HashMap::new(),
        };

        let diff = differ.diff(&layout, &layout);
        assert!(diff.is_empty());
        assert_eq!(diff.change_count(), 0);
    }

    #[test]
    fn test_layout_diff_position_change() {
        let differ = LayoutDiffer::new();
        
        let mut old_layout = LayoutResult {
            computed_positions: HashMap::new(),
            computed_sizes: HashMap::new(),
        };
        old_layout.computed_positions.insert(0, Vec2::new(10.0, 20.0));
        old_layout.computed_sizes.insert(0, Vec2::new(100.0, 50.0));
        
        let mut new_layout = LayoutResult {
            computed_positions: HashMap::new(),
            computed_sizes: HashMap::new(),
        };
        new_layout.computed_positions.insert(0, Vec2::new(15.0, 25.0));
        new_layout.computed_sizes.insert(0, Vec2::new(100.0, 50.0));

        let diff = differ.diff(&old_layout, &new_layout);
        assert_eq!(diff.position_changes.len(), 1);
        assert_eq!(diff.size_changes.len(), 0);
        assert!(diff.only_position_changes());
    }

    #[test]
    fn test_layout_diff_added_element() {
        let differ = LayoutDiffer::new();
        
        let old_layout = LayoutResult {
            computed_positions: HashMap::new(),
            computed_sizes: HashMap::new(),
        };
        
        let mut new_layout = LayoutResult {
            computed_positions: HashMap::new(),
            computed_sizes: HashMap::new(),
        };
        new_layout.computed_positions.insert(0, Vec2::new(10.0, 20.0));
        new_layout.computed_sizes.insert(0, Vec2::new(100.0, 50.0));

        let diff = differ.diff(&old_layout, &new_layout);
        assert_eq!(diff.added_elements.len(), 1);
        assert!(diff.added_elements.contains(&0));
    }

    #[test]
    fn test_layout_diff_tolerance() {
        let differ = LayoutDiffer::with_tolerances(1.0, 1.0, 100);
        
        let mut old_layout = LayoutResult {
            computed_positions: HashMap::new(),
            computed_sizes: HashMap::new(),
        };
        old_layout.computed_positions.insert(0, Vec2::new(10.0, 20.0));
        
        let mut new_layout = LayoutResult {
            computed_positions: HashMap::new(),
            computed_sizes: HashMap::new(),
        };
        new_layout.computed_positions.insert(0, Vec2::new(10.5, 20.5));

        let diff = differ.diff(&old_layout, &new_layout);
        assert!(diff.is_empty()); // Within tolerance
    }

    #[test]
    fn test_get_change_bounds() {
        let differ = LayoutDiffer::new();
        
        let mut layout = LayoutResult {
            computed_positions: HashMap::new(),
            computed_sizes: HashMap::new(),
        };
        layout.computed_positions.insert(0, Vec2::new(10.0, 20.0));
        layout.computed_sizes.insert(0, Vec2::new(100.0, 50.0));
        layout.computed_positions.insert(1, Vec2::new(50.0, 30.0));
        layout.computed_sizes.insert(1, Vec2::new(200.0, 100.0));

        let mut diff = LayoutDiff {
            position_changes: HashMap::new(),
            size_changes: HashMap::new(),
            added_elements: HashSet::new(),
            removed_elements: HashSet::new(),
            needs_full_render: false,
        };
        diff.added_elements.insert(0);
        diff.added_elements.insert(1);

        let bounds = differ.get_change_bounds(&diff, &layout);
        assert!(bounds.is_some());
        let (min_pos, size) = bounds.unwrap();
        assert_eq!(min_pos, Vec2::new(10.0, 20.0));
        assert_eq!(size, Vec2::new(240.0, 110.0));
    }
}