# Kryon Optimization Implementation Details

This document provides specific implementation details for all required optimizations in Kryon systems.

## Table of Contents

- [Layout Optimization Details](#layout-optimization-details)
- [Render Optimization Details](#render-optimization-details)
- [Memory Optimization Details](#memory-optimization-details)
- [Script Optimization Details](#script-optimization-details)
- [Cache Implementation Details](#cache-implementation-details)
- [Profiling Integration](#profiling-integration)

## Layout Optimization Details

### Layout Caching Implementation

```rust
/// High-performance layout cache with dependency tracking
pub struct LayoutCache {
    /// Cache entries keyed by layout signature
    cache: DashMap<LayoutCacheKey, Arc<CachedLayoutResult>>,
    /// Dependency graph for cache invalidation
    dependencies: DashMap<ElementId, HashSet<LayoutCacheKey>>,
    /// LRU eviction tracking
    access_tracker: Mutex<LinkedHashMap<LayoutCacheKey, Instant>>,
    /// Cache statistics
    stats: AtomicCacheStats,
    /// Maximum cache size
    max_entries: usize,
}

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub struct LayoutCacheKey {
    /// Element ID being laid out
    element_id: ElementId,
    /// Hash of element properties affecting layout
    properties_hash: u64,
    /// Hash of parent constraints
    constraints_hash: u64,
    /// Hash of child elements (for containers)
    children_hash: u64,
}

#[derive(Debug, Clone)]
pub struct CachedLayoutResult {
    /// Computed element rectangle
    pub rect: Rect,
    /// Child element rectangles (for containers)
    pub child_rects: HashMap<ElementId, Rect>,
    /// Baseline information for text
    pub baseline: Option<f32>,
    /// Cache generation for staleness detection
    pub generation: u64,
    /// Timestamp when cached
    pub cached_at: Instant,
}

impl LayoutCache {
    /// Create layout cache key from element and constraints
    pub fn create_cache_key(
        element: &Element,
        constraints: &LayoutConstraints,
        children: &[Element],
    ) -> LayoutCacheKey {
        use std::collections::hash_map::DefaultHasher;
        use std::hash::{Hash, Hasher};
        
        // Hash element properties that affect layout
        let mut properties_hasher = DefaultHasher::new();
        element.width.hash(&mut properties_hasher);
        element.height.hash(&mut properties_hasher);
        element.padding.hash(&mut properties_hasher);
        element.margin.hash(&mut properties_hasher);
        element.flexDirection.hash(&mut properties_hasher);
        element.justifyContent.hash(&mut properties_hasher);
        element.alignItems.hash(&mut properties_hasher);
        let properties_hash = properties_hasher.finish();
        
        // Hash constraints
        let mut constraints_hasher = DefaultHasher::new();
        constraints.available_width.to_bits().hash(&mut constraints_hasher);
        constraints.available_height.to_bits().hash(&mut constraints_hasher);
        let constraints_hash = constraints_hasher.finish();
        
        // Hash children (for containers)
        let mut children_hasher = DefaultHasher::new();
        for child in children {
            child.id.hash(&mut children_hasher);
            child.layout_generation.load(Ordering::Acquire).hash(&mut children_hasher);
        }
        let children_hash = children_hasher.finish();
        
        LayoutCacheKey {
            element_id: element.id,
            properties_hash,
            constraints_hash,
            children_hash,
        }
    }
    
    /// Get cached layout result if valid
    pub fn get(&self, key: &LayoutCacheKey) -> Option<Arc<CachedLayoutResult>> {
        let result = self.cache.get(key)?;
        
        // Update access tracking for LRU
        if let Ok(mut tracker) = self.access_tracker.try_lock() {
            tracker.remove(key);
            tracker.insert(*key, Instant::now());
        }
        
        // Update cache hit statistics
        self.stats.hits.fetch_add(1, Ordering::Relaxed);
        
        Some(result.clone())
    }
    
    /// Insert layout result into cache
    pub fn insert(&self, key: LayoutCacheKey, result: CachedLayoutResult) {
        let arc_result = Arc::new(result);
        
        // Insert into cache
        self.cache.insert(key, arc_result.clone());
        
        // Track dependencies for invalidation
        self.add_dependencies(&key, &arc_result);
        
        // Update access tracker
        if let Ok(mut tracker) = self.access_tracker.try_lock() {
            tracker.insert(key, Instant::now());
            
            // Evict old entries if cache is full
            while tracker.len() > self.max_entries {
                if let Some((old_key, _)) = tracker.pop_front() {
                    self.cache.remove(&old_key);
                    self.remove_dependencies(&old_key);
                }
            }
        }
        
        // Update statistics
        self.stats.misses.fetch_add(1, Ordering::Relaxed);
    }
    
    /// Invalidate cache entries affected by element changes
    pub fn invalidate(&self, changed_elements: &[ElementId]) {
        let mut invalidated_keys = HashSet::new();
        
        // Find all cache keys dependent on changed elements
        for element_id in changed_elements {
            if let Some(dependent_keys) = self.dependencies.get(element_id) {
                invalidated_keys.extend(dependent_keys.iter().cloned());
            }
        }
        
        // Remove invalidated entries
        for key in invalidated_keys {
            self.cache.remove(&key);
            self.remove_dependencies(&key);
            
            if let Ok(mut tracker) = self.access_tracker.try_lock() {
                tracker.remove(&key);
            }
        }
        
        // Update statistics
        self.stats.invalidations.fetch_add(1, Ordering::Relaxed);
    }
    
    /// Add dependency tracking for cache entry
    fn add_dependencies(&self, key: &LayoutCacheKey, result: &CachedLayoutResult) {
        // Track dependency on the element itself
        self.dependencies
            .entry(key.element_id)
            .or_insert_with(HashSet::new)
            .insert(*key);
        
        // Track dependencies on child elements
        for child_id in result.child_rects.keys() {
            self.dependencies
                .entry(*child_id)
                .or_insert_with(HashSet::new)
                .insert(*key);
        }
    }
    
    /// Remove dependency tracking for cache entry
    fn remove_dependencies(&self, key: &LayoutCacheKey) {
        for mut entry in self.dependencies.iter_mut() {
            entry.value_mut().remove(key);
        }
    }
    
    /// Get cache statistics
    pub fn get_stats(&self) -> CacheStats {
        CacheStats {
            hits: self.stats.hits.load(Ordering::Relaxed),
            misses: self.stats.misses.load(Ordering::Relaxed),
            invalidations: self.stats.invalidations.load(Ordering::Relaxed),
            entries: self.cache.len(),
            hit_rate: self.calculate_hit_rate(),
        }
    }
    
    /// Calculate cache hit rate
    fn calculate_hit_rate(&self) -> f32 {
        let hits = self.stats.hits.load(Ordering::Relaxed) as f32;
        let misses = self.stats.misses.load(Ordering::Relaxed) as f32;
        let total = hits + misses;
        if total > 0.0 { hits / total } else { 0.0 }
    }
}

#[derive(Debug, Default)]
struct AtomicCacheStats {
    hits: AtomicU64,
    misses: AtomicU64,
    invalidations: AtomicU64,
}
```

### Incremental Layout Updates

```rust
/// Incremental layout engine that minimizes recomputation
pub struct IncrementalLayoutEngine {
    /// Previous layout results
    previous_layout: Option<Arc<LayoutResult>>,
    /// Change tracking system
    change_tracker: ChangeTracker,
    /// Layout cache
    cache: LayoutCache,
    /// Dependency graph for efficient updates
    dependency_graph: LayoutDependencyGraph,
}

impl IncrementalLayoutEngine {
    /// Compute layout incrementally from previous state
    pub fn compute_incremental(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        changed_elements: &[ElementId],
        constraints: &LayoutConstraints,
    ) -> Result<Arc<LayoutResult>, LayoutError> {
        // If no previous layout or too many changes, do full recompute
        if self.previous_layout.is_none() || changed_elements.len() > elements.len() / 10 {
            return self.compute_full_layout(elements, constraints);
        }
        
        let previous = self.previous_layout.as_ref().unwrap();
        let mut new_layout = previous.element_rects.as_ref().clone();
        
        // Find all elements affected by changes
        let affected_elements = self.find_affected_elements(changed_elements, elements);
        
        // Recompute only affected elements
        for element_id in affected_elements {
            if let Some(element) = elements.get(&element_id) {
                let parent_constraints = self.get_parent_constraints(element_id, &new_layout, constraints);
                let element_rect = self.compute_element_layout(element, &parent_constraints)?;
                new_layout.insert(element_id, element_rect);
            }
        }
        
        let result = Arc::new(LayoutResult {
            element_rects: Arc::new(new_layout),
            generation: AtomicU64::new(previous.generation.load(Ordering::Acquire) + 1),
            computed_at: Instant::now(),
        });
        
        self.previous_layout = Some(result.clone());
        Ok(result)
    }
    
    /// Find all elements affected by the given changes
    fn find_affected_elements(
        &self,
        changed_elements: &[ElementId],
        elements: &HashMap<ElementId, Element>,
    ) -> HashSet<ElementId> {
        let mut affected = HashSet::new();
        let mut queue = VecDeque::from_iter(changed_elements.iter().cloned());
        
        while let Some(element_id) = queue.pop_front() {
            if affected.contains(&element_id) {
                continue;
            }
            
            affected.insert(element_id);
            
            // Add dependent elements
            if let Some(dependents) = self.dependency_graph.get_dependents(element_id) {
                for dependent in dependents {
                    if !affected.contains(dependent) {
                        queue.push_back(*dependent);
                    }
                }
            }
        }
        
        affected
    }
}

/// Tracks layout dependencies between elements
pub struct LayoutDependencyGraph {
    /// Maps element to elements that depend on it
    dependents: HashMap<ElementId, HashSet<ElementId>>,
    /// Maps element to elements it depends on
    dependencies: HashMap<ElementId, HashSet<ElementId>>,
}

impl LayoutDependencyGraph {
    /// Add dependency relationship
    pub fn add_dependency(&mut self, dependent: ElementId, dependency: ElementId) {
        self.dependents
            .entry(dependency)
            .or_default()
            .insert(dependent);
        
        self.dependencies
            .entry(dependent)
            .or_default()
            .insert(dependency);
    }
    
    /// Get all elements that depend on the given element
    pub fn get_dependents(&self, element_id: ElementId) -> Option<&HashSet<ElementId>> {
        self.dependents.get(&element_id)
    }
    
    /// Update dependencies after element tree changes
    pub fn update_dependencies(&mut self, elements: &HashMap<ElementId, Element>) {
        self.dependents.clear();
        self.dependencies.clear();
        
        for (element_id, element) in elements {
            // Children depend on parent
            if let Some(parent_id) = element.parent {
                self.add_dependency(*element_id, parent_id);
            }
            
            // Parent depends on children (for auto-sizing)
            for child_id in &element.children {
                self.add_dependency(element.parent.unwrap_or(*element_id), *child_id);
            }
            
            // Flex items depend on flex container
            if element.is_flex_container() {
                for child_id in &element.children {
                    self.add_dependency(*child_id, *element_id);
                }
            }
        }
    }
}
```

## Render Optimization Details

### Command Batching Implementation

```rust
/// Render command batching system for optimal GPU usage
pub struct RenderCommandBatcher {
    /// Pending commands to be batched
    pending_commands: Vec<RenderCommand>,
    /// Batched commands ready for execution
    batched_commands: Vec<BatchedRenderCommand>,
    /// Material/state tracking for batching decisions
    current_state: RenderState,
    /// Statistics tracking
    stats: BatchingStats,
}

#[derive(Debug, Clone)]
pub struct BatchedRenderCommand {
    /// Batch type for efficient execution
    pub batch_type: BatchType,
    /// Commands in this batch
    pub commands: Vec<RenderCommand>,
    /// Shared render state for the batch
    pub render_state: RenderState,
    /// Vertex/index data for the batch
    pub batch_data: BatchData,
}

#[derive(Debug, Clone, PartialEq)]
pub enum BatchType {
    /// Solid color rectangles (can use instanced rendering)
    SolidRectangles,
    /// Textured rectangles with same texture
    TexturedRectangles { texture_id: TextureId },
    /// Text rendering with same font
    TextRendering { font_id: FontId },
    /// Lines and strokes
    LineRendering,
    /// Custom rendering (cannot batch)
    Custom,
}

impl RenderCommandBatcher {
    /// Add command to batch queue
    pub fn add_command(&mut self, command: RenderCommand) {
        // Check if command can be batched with previous commands
        if self.can_batch_with_current(&command) {
            self.pending_commands.push(command);
        } else {
            // Flush current batch and start new one
            self.flush_current_batch();
            self.pending_commands.clear();
            self.pending_commands.push(command);
            self.update_current_state(&command);
        }
    }
    
    /// Check if command can be batched with current pending commands
    fn can_batch_with_current(&self, command: &RenderCommand) -> bool {
        if self.pending_commands.is_empty() {
            return true;
        }
        
        let current_batch_type = self.get_batch_type(&self.pending_commands[0]);
        let new_batch_type = self.get_batch_type(command);
        
        // Must be same batch type
        if current_batch_type != new_batch_type {
            return false;
        }
        
        // Check render state compatibility
        let command_state = self.extract_render_state(command);
        if !self.current_state.is_compatible(&command_state) {
            return false;
        }
        
        // Check batch size limits
        if self.pending_commands.len() >= MAX_BATCH_SIZE {
            return false;
        }
        
        true
    }
    
    /// Determine batch type for command
    fn get_batch_type(&self, command: &RenderCommand) -> BatchType {
        match command {
            RenderCommand::DrawRect { texture: None, .. } => BatchType::SolidRectangles,
            RenderCommand::DrawRect { texture: Some(tex), .. } => {
                BatchType::TexturedRectangles { texture_id: *tex }
            }
            RenderCommand::DrawText { font, .. } => {
                BatchType::TextRendering { font_id: *font }
            }
            RenderCommand::DrawLine { .. } => BatchType::LineRendering,
            _ => BatchType::Custom,
        }
    }
    
    /// Flush current batch to execution queue
    fn flush_current_batch(&mut self) {
        if self.pending_commands.is_empty() {
            return;
        }
        
        let batch_type = self.get_batch_type(&self.pending_commands[0]);
        let batch_data = self.create_batch_data(&self.pending_commands, &batch_type);
        
        let batched_command = BatchedRenderCommand {
            batch_type,
            commands: self.pending_commands.clone(),
            render_state: self.current_state.clone(),
            batch_data,
        };
        
        self.batched_commands.push(batched_command);
        
        // Update statistics
        self.stats.batches_created += 1;
        self.stats.commands_batched += self.pending_commands.len();
    }
    
    /// Create optimized batch data for GPU rendering
    fn create_batch_data(&self, commands: &[RenderCommand], batch_type: &BatchType) -> BatchData {
        match batch_type {
            BatchType::SolidRectangles => {
                self.create_rectangle_batch_data(commands)
            }
            BatchType::TexturedRectangles { .. } => {
                self.create_textured_batch_data(commands)
            }
            BatchType::TextRendering { .. } => {
                self.create_text_batch_data(commands)
            }
            BatchType::LineRendering => {
                self.create_line_batch_data(commands)
            }
            BatchType::Custom => BatchData::None,
        }
    }
    
    /// Create batch data for solid rectangles (instanced rendering)
    fn create_rectangle_batch_data(&self, commands: &[RenderCommand]) -> BatchData {
        let mut instance_data = Vec::new();
        
        for command in commands {
            if let RenderCommand::DrawRect { rect, color, .. } = command {
                instance_data.push(RectangleInstance {
                    position: [rect.x, rect.y],
                    size: [rect.width, rect.height],
                    color: color.to_array(),
                });
            }
        }
        
        BatchData::RectangleInstances(instance_data)
    }
    
    /// Execute all batched commands efficiently
    pub fn execute_batches<R: CommandRenderer>(&mut self, renderer: &mut R) -> Result<(), RenderError> {
        for batch in &self.batched_commands {
            self.execute_batch(batch, renderer)?;
        }
        
        self.batched_commands.clear();
        Ok(())
    }
    
    /// Execute a single batch optimally
    fn execute_batch<R: CommandRenderer>(
        &self,
        batch: &BatchedRenderCommand,
        renderer: &mut R,
    ) -> Result<(), RenderError> {
        // Set render state once for entire batch
        renderer.set_render_state(&batch.render_state)?;
        
        match &batch.batch_type {
            BatchType::SolidRectangles => {
                if let BatchData::RectangleInstances(instances) = &batch.batch_data {
                    renderer.draw_rectangle_instances(instances)?;
                }
            }
            BatchType::TexturedRectangles { texture_id } => {
                renderer.bind_texture(*texture_id)?;
                if let BatchData::TexturedInstances(instances) = &batch.batch_data {
                    renderer.draw_textured_instances(instances)?;
                }
            }
            BatchType::TextRendering { font_id } => {
                renderer.bind_font(*font_id)?;
                if let BatchData::TextInstances(instances) = &batch.batch_data {
                    renderer.draw_text_instances(instances)?;
                }
            }
            BatchType::LineRendering => {
                if let BatchData::LineInstances(instances) = &batch.batch_data {
                    renderer.draw_line_instances(instances)?;
                }
            }
            BatchType::Custom => {
                // Execute commands individually
                for command in &batch.commands {
                    renderer.execute_command(command)?;
                }
            }
        }
        
        Ok(())
    }
}

#[derive(Debug, Clone)]
pub enum BatchData {
    None,
    RectangleInstances(Vec<RectangleInstance>),
    TexturedInstances(Vec<TexturedInstance>),
    TextInstances(Vec<TextInstance>),
    LineInstances(Vec<LineInstance>),
}

#[derive(Debug, Clone, Copy)]
pub struct RectangleInstance {
    pub position: [f32; 2],
    pub size: [f32; 2],
    pub color: [f32; 4],
}

const MAX_BATCH_SIZE: usize = 1000;
```

### Texture Atlas Management

```rust
/// Dynamic texture atlas for small textures
pub struct TextureAtlas {
    /// Atlas texture ID
    atlas_texture: TextureId,
    /// Atlas dimensions
    width: u32,
    height: u32,
    /// Free space tracking
    free_spaces: Vec<Rect>,
    /// Allocated spaces
    allocated_spaces: HashMap<TextureId, AtlasAllocation>,
    /// Texture packer for optimal space usage
    packer: RectanglePacker,
    /// Atlas generation for cache invalidation
    generation: AtomicU64,
}

#[derive(Debug, Clone)]
pub struct AtlasAllocation {
    /// UV coordinates in atlas
    pub uv_rect: Rect,
    /// Original texture size
    pub original_size: Size,
    /// Atlas generation when allocated
    pub generation: u64,
}

impl TextureAtlas {
    /// Try to allocate space in atlas for texture
    pub fn allocate(&mut self, width: u32, height: u32) -> Option<AtlasAllocation> {
        // Find best fitting free space
        let size = Size { width, height };
        let allocation_rect = self.packer.pack(size)?;
        
        // Convert to UV coordinates
        let uv_rect = Rect {
            x: allocation_rect.x as f32 / self.width as f32,
            y: allocation_rect.y as f32 / self.height as f32,
            width: allocation_rect.width as f32 / self.width as f32,
            height: allocation_rect.height as f32 / self.height as f32,
        };
        
        let allocation = AtlasAllocation {
            uv_rect,
            original_size: size,
            generation: self.generation.load(Ordering::Acquire),
        };
        
        Some(allocation)
    }
    
    /// Add texture to atlas
    pub fn add_texture(&mut self, texture_data: &[u8], format: ImageFormat) -> Result<AtlasAllocation, AtlasError> {
        let (width, height) = decode_image_dimensions(texture_data, format)?;
        
        // Check if texture is too large for atlas
        if width > self.width || height > self.height {
            return Err(AtlasError::TextureTooLarge { width, height });
        }
        
        // Try to allocate space
        let allocation = self.allocate(width, height)
            .ok_or(AtlasError::InsufficientSpace)?;
        
        // Upload texture data to atlas
        self.upload_texture_region(texture_data, format, &allocation)?;
        
        Ok(allocation)
    }
    
    /// Defragment atlas by repacking textures
    pub fn defragment(&mut self) -> Result<(), AtlasError> {
        // Get all current allocations
        let current_allocations: Vec<_> = self.allocated_spaces.values().cloned().collect();
        
        // Clear atlas
        self.clear_atlas();
        
        // Re-pack all textures
        let mut new_allocations = HashMap::new();
        for (texture_id, old_allocation) in &self.allocated_spaces {
            if let Some(new_allocation) = self.allocate(
                old_allocation.original_size.width,
                old_allocation.original_size.height,
            ) {
                // Copy texture data to new location
                self.copy_texture_region(&old_allocation.uv_rect, &new_allocation.uv_rect)?;
                new_allocations.insert(*texture_id, new_allocation);
            }
        }
        
        self.allocated_spaces = new_allocations;
        self.generation.fetch_add(1, Ordering::AcqRel);
        
        Ok(())
    }
}

/// Rectangle packing algorithm for texture atlas
pub struct RectanglePacker {
    /// Available space rectangles
    free_rects: Vec<Rect>,
    /// Used space for debugging
    used_rects: Vec<Rect>,
}

impl RectanglePacker {
    /// Pack rectangle using best-fit algorithm
    pub fn pack(&mut self, size: Size) -> Option<Rect> {
        let mut best_rect = None;
        let mut best_short_side = i32::MAX;
        let mut best_long_side = i32::MAX;
        
        // Find best fitting rectangle
        for rect in &self.free_rects {
            if rect.width >= size.width as f32 && rect.height >= size.height as f32 {
                let leftover_horizontal = rect.width as i32 - size.width as i32;
                let leftover_vertical = rect.height as i32 - size.height as i32;
                let short_side = leftover_horizontal.min(leftover_vertical);
                let long_side = leftover_horizontal.max(leftover_vertical);
                
                if short_side < best_short_side || (short_side == best_short_side && long_side < best_long_side) {
                    best_rect = Some(Rect {
                        x: rect.x,
                        y: rect.y,
                        width: size.width as f32,
                        height: size.height as f32,
                    });
                    best_short_side = short_side;
                    best_long_side = long_side;
                }
            }
        }
        
        if let Some(rect) = best_rect {
            // Split the chosen rectangle
            self.split_free_node(&rect);
            self.used_rects.push(rect);
            Some(rect)
        } else {
            None
        }
    }
    
    /// Split free rectangle after allocation
    fn split_free_node(&mut self, used_rect: &Rect) {
        let mut new_rects = Vec::new();
        
        // Remove overlapping rectangles and create splits
        self.free_rects.retain(|rect| {
            if self.rectangles_overlap(rect, used_rect) {
                // Create split rectangles
                if rect.x < used_rect.x && rect.x + rect.width > used_rect.x {
                    // Left split
                    new_rects.push(Rect {
                        x: rect.x,
                        y: rect.y,
                        width: used_rect.x - rect.x,
                        height: rect.height,
                    });
                }
                if rect.x < used_rect.x + used_rect.width && rect.x + rect.width > used_rect.x + used_rect.width {
                    // Right split
                    new_rects.push(Rect {
                        x: used_rect.x + used_rect.width,
                        y: rect.y,
                        width: rect.x + rect.width - (used_rect.x + used_rect.width),
                        height: rect.height,
                    });
                }
                if rect.y < used_rect.y && rect.y + rect.height > used_rect.y {
                    // Top split
                    new_rects.push(Rect {
                        x: rect.x,
                        y: rect.y,
                        width: rect.width,
                        height: used_rect.y - rect.y,
                    });
                }
                if rect.y < used_rect.y + used_rect.height && rect.y + rect.height > used_rect.y + used_rect.height {
                    // Bottom split
                    new_rects.push(Rect {
                        x: rect.x,
                        y: used_rect.y + used_rect.height,
                        width: rect.width,
                        height: rect.y + rect.height - (used_rect.y + used_rect.height),
                    });
                }
                false // Remove this rectangle
            } else {
                true // Keep this rectangle
            }
        });
        
        // Add new split rectangles
        self.free_rects.extend(new_rects);
        
        // Remove duplicate rectangles
        self.prune_free_list();
    }
    
    /// Check if two rectangles overlap
    fn rectangles_overlap(&self, a: &Rect, b: &Rect) -> bool {
        a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y
    }
    
    /// Remove redundant free rectangles
    fn prune_free_list(&mut self) {
        let mut i = 0;
        while i < self.free_rects.len() {
            let mut j = i + 1;
            while j < self.free_rects.len() {
                if self.rectangle_contains(&self.free_rects[j], &self.free_rects[i]) {
                    self.free_rects.remove(i);
                    break;
                }
                if self.rectangle_contains(&self.free_rects[i], &self.free_rects[j]) {
                    self.free_rects.remove(j);
                } else {
                    j += 1;
                }
            }
            i += 1;
        }
    }
    
    /// Check if rectangle a contains rectangle b
    fn rectangle_contains(&self, a: &Rect, b: &Rect) -> bool {
        a.x <= b.x && a.y <= b.y && a.x + a.width >= b.x + b.width && a.y + a.height >= b.y + b.height
    }
}
```

## Memory Optimization Details

### Object Pooling Implementation

```rust
/// High-performance object pool for frequent allocations
pub struct ObjectPool<T> {
    /// Available objects ready for reuse
    available: crossbeam::queue::SegQueue<T>,
    /// Factory function for creating new objects
    factory: Box<dyn Fn() -> T + Send + Sync>,
    /// Reset function for cleaning objects before reuse
    reset: Box<dyn Fn(&mut T) + Send + Sync>,
    /// Pool statistics
    stats: PoolStats,
    /// Maximum pool size
    max_size: usize,
}

impl<T> ObjectPool<T> {
    /// Create new object pool
    pub fn new<F, R>(factory: F, reset: R, max_size: usize) -> Self
    where
        F: Fn() -> T + Send + Sync + 'static,
        R: Fn(&mut T) + Send + Sync + 'static,
    {
        Self {
            available: crossbeam::queue::SegQueue::new(),
            factory: Box::new(factory),
            reset: Box::new(reset),
            stats: PoolStats::default(),
            max_size,
        }
    }
    
    /// Get object from pool or create new one
    pub fn get(&self) -> PooledObject<T> {
        let object = if let Some(mut obj) = self.available.pop() {
            // Reset object state
            (self.reset)(&mut obj);
            self.stats.reused.fetch_add(1, Ordering::Relaxed);
            obj
        } else {
            // Create new object
            let obj = (self.factory)();
            self.stats.created.fetch_add(1, Ordering::Relaxed);
            obj
        };
        
        PooledObject {
            object: Some(object),
            pool: self,
        }
    }
    
    /// Return object to pool
    fn return_object(&self, object: T) {
        if self.available.len() < self.max_size {
            self.available.push(object);
            self.stats.returned.fetch_add(1, Ordering::Relaxed);
        } else {
            // Pool is full, drop the object
            self.stats.dropped.fetch_add(1, Ordering::Relaxed);
        }
    }
    
    /// Get pool statistics
    pub fn stats(&self) -> PoolStats {
        PoolStats {
            created: self.stats.created.load(Ordering::Relaxed),
            reused: self.stats.reused.load(Ordering::Relaxed),
            returned: self.stats.returned.load(Ordering::Relaxed),
            dropped: self.stats.dropped.load(Ordering::Relaxed),
            available: self.available.len(),
        }
    }
}

/// RAII wrapper for pooled objects
pub struct PooledObject<'a, T> {
    object: Option<T>,
    pool: &'a ObjectPool<T>,
}

impl<'a, T> PooledObject<'a, T> {
    /// Get reference to the object
    pub fn get(&self) -> &T {
        self.object.as_ref().unwrap()
    }
    
    /// Get mutable reference to the object
    pub fn get_mut(&mut self) -> &mut T {
        self.object.as_mut().unwrap()
    }
}

impl<'a, T> Drop for PooledObject<'a, T> {
    fn drop(&mut self) {
        if let Some(object) = self.object.take() {
            self.pool.return_object(object);
        }
    }
}

impl<'a, T> Deref for PooledObject<'a, T> {
    type Target = T;
    
    fn deref(&self) -> &Self::Target {
        self.get()
    }
}

impl<'a, T> DerefMut for PooledObject<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.get_mut()
    }
}

#[derive(Debug, Default, Clone)]
pub struct PoolStats {
    pub created: u64,
    pub reused: u64,
    pub returned: u64,
    pub dropped: u64,
    pub available: usize,
}

/// Specialized pools for Kryon objects
pub struct KryonObjectPools {
    pub elements: ObjectPool<Element>,
    pub render_commands: ObjectPool<RenderCommand>,
    pub layout_nodes: ObjectPool<LayoutNode>,
    pub event_handlers: ObjectPool<EventHandler>,
}

impl KryonObjectPools {
    pub fn new() -> Self {
        Self {
            elements: ObjectPool::new(
                || Element::default(),
                |element| element.reset(),
                1000,
            ),
            render_commands: ObjectPool::new(
                || RenderCommand::default(),
                |cmd| cmd.reset(),
                5000,
            ),
            layout_nodes: ObjectPool::new(
                || LayoutNode::default(),
                |node| node.reset(),
                2000,
            ),
            event_handlers: ObjectPool::new(
                || EventHandler::default(),
                |handler| handler.reset(),
                500,
            ),
        }
    }
}
```

### String Interning System

```rust
/// String interning system for memory efficiency
pub struct StringInterner {
    /// Interned strings storage
    strings: DashMap<String, Arc<str>>,
    /// Reverse lookup for debugging
    reverse_lookup: DashMap<*const str, String>,
    /// Statistics
    stats: InternerStats,
}

impl StringInterner {
    /// Intern a string, returning shared reference
    pub fn intern(&self, s: &str) -> Arc<str> {
        if let Some(interned) = self.strings.get(s) {
            self.stats.hits.fetch_add(1, Ordering::Relaxed);
            interned.clone()
        } else {
            let interned: Arc<str> = Arc::from(s);
            let ptr = Arc::as_ptr(&interned);
            
            self.strings.insert(s.to_string(), interned.clone());
            self.reverse_lookup.insert(ptr, s.to_string());
            self.stats.misses.fetch_add(1, Ordering::Relaxed);
            
            interned
        }
    }
    
    /// Get statistics
    pub fn stats(&self) -> InternerStats {
        InternerStats {
            hits: self.stats.hits.load(Ordering::Relaxed),
            misses: self.stats.misses.load(Ordering::Relaxed),
            unique_strings: self.strings.len(),
            memory_saved: self.calculate_memory_saved(),
        }
    }
    
    /// Calculate approximate memory saved by interning
    fn calculate_memory_saved(&self) -> usize {
        let mut total_saved = 0;
        for entry in self.strings.iter() {
            let string_size = entry.key().len();
            let ref_count = Arc::strong_count(entry.value());
            if ref_count > 1 {
                total_saved += string_size * (ref_count - 1);
            }
        }
        total_saved
    }
}

#[derive(Debug, Default)]
struct InternerStats {
    hits: AtomicU64,
    misses: AtomicU64,
}

/// Global string interner for Kryon
static STRING_INTERNER: Lazy<StringInterner> = Lazy::new(|| StringInterner::new());

/// Convenience function for interning strings
pub fn intern_string(s: &str) -> Arc<str> {
    STRING_INTERNER.intern(s)
}
```

## Script Optimization Details

### Script Compilation and Caching

```rust
/// Script compilation cache for performance
pub struct ScriptCompilationCache {
    /// Compiled scripts keyed by source hash
    compiled_scripts: DashMap<u64, Arc<CompiledScript>>,
    /// LRU eviction tracker
    access_tracker: Mutex<LinkedHashMap<u64, Instant>>,
    /// Cache statistics
    stats: CacheStats,
    /// Maximum cache entries
    max_entries: usize,
}

impl ScriptCompilationCache {
    /// Compile script or get from cache
    pub fn compile_or_get(
        &self,
        source: &str,
        engine: &mut dyn ScriptEngine,
    ) -> Result<Arc<CompiledScript>, ScriptError> {
        let source_hash = self.hash_source(source);
        
        // Check cache first
        if let Some(compiled) = self.compiled_scripts.get(&source_hash) {
            self.update_access_time(source_hash);
            self.stats.hits.fetch_add(1, Ordering::Relaxed);
            return Ok(compiled.clone());
        }
        
        // Compile script
        let compiled = Arc::new(engine.compile_script(source)?);
        
        // Cache the result
        self.insert_compiled(source_hash, compiled.clone());
        self.stats.misses.fetch_add(1, Ordering::Relaxed);
        
        Ok(compiled)
    }
    
    /// Hash script source for cache key
    fn hash_source(&self, source: &str) -> u64 {
        use std::collections::hash_map::DefaultHasher;
        use std::hash::{Hash, Hasher};
        
        let mut hasher = DefaultHasher::new();
        source.hash(&mut hasher);
        hasher.finish()
    }
    
    /// Insert compiled script with LRU eviction
    fn insert_compiled(&self, hash: u64, compiled: Arc<CompiledScript>) {
        self.compiled_scripts.insert(hash, compiled);
        
        // Update access tracker and handle eviction
        if let Ok(mut tracker) = self.access_tracker.try_lock() {
            tracker.insert(hash, Instant::now());
            
            // Evict oldest entries if cache is full
            while tracker.len() > self.max_entries {
                if let Some((old_hash, _)) = tracker.pop_front() {
                    self.compiled_scripts.remove(&old_hash);
                }
            }
        }
    }
    
    /// Update access time for LRU tracking
    fn update_access_time(&self, hash: u64) {
        if let Ok(mut tracker) = self.access_tracker.try_lock() {
            tracker.remove(&hash);
            tracker.insert(hash, Instant::now());
        }
    }
}

/// Pre-compiled script representation
#[derive(Debug)]
pub struct CompiledScript {
    /// Engine-specific bytecode
    pub bytecode: Vec<u8>,
    /// Script metadata
    pub metadata: ScriptMetadata,
    /// Compilation timestamp
    pub compiled_at: Instant,
    /// Source hash for cache validation
    pub source_hash: u64,
}

#[derive(Debug)]
pub struct ScriptMetadata {
    /// Function signatures defined in script
    pub functions: Vec<FunctionSignature>,
    /// Global variables accessed
    pub global_variables: HashSet<String>,
    /// External dependencies
    pub dependencies: HashSet<String>,
    /// Estimated execution complexity
    pub complexity_score: u32,
}
```

### Context Pooling System

```rust
/// Pool of script execution contexts for reuse
pub struct ScriptContextPool {
    /// Available contexts
    available_contexts: crossbeam::queue::SegQueue<ScriptContext>,
    /// Context factory
    context_factory: Box<dyn Fn() -> ScriptContext + Send + Sync>,
    /// Context reset function
    context_reset: Box<dyn Fn(&mut ScriptContext) + Send + Sync>,
    /// Pool statistics
    stats: PoolStats,
    /// Maximum pool size
    max_size: usize,
}

impl ScriptContextPool {
    /// Get context from pool or create new one
    pub fn get_context(&self) -> PooledScriptContext {
        let context = if let Some(mut ctx) = self.available_contexts.pop() {
            (self.context_reset)(&mut ctx);
            self.stats.reused.fetch_add(1, Ordering::Relaxed);
            ctx
        } else {
            let ctx = (self.context_factory)();
            self.stats.created.fetch_add(1, Ordering::Relaxed);
            ctx
        };
        
        PooledScriptContext {
            context: Some(context),
            pool: self,
        }
    }
    
    /// Return context to pool
    fn return_context(&self, context: ScriptContext) {
        if self.available_contexts.len() < self.max_size {
            self.available_contexts.push(context);
            self.stats.returned.fetch_add(1, Ordering::Relaxed);
        } else {
            self.stats.dropped.fetch_add(1, Ordering::Relaxed);
        }
    }
}

/// RAII wrapper for pooled script contexts
pub struct PooledScriptContext<'a> {
    context: Option<ScriptContext>,
    pool: &'a ScriptContextPool,
}

impl<'a> PooledScriptContext<'a> {
    pub fn execute_script(
        &mut self,
        script: &CompiledScript,
        engine: &mut dyn ScriptEngine,
    ) -> Result<ScriptValue, ScriptError> {
        engine.execute_compiled(script, self.context.as_mut().unwrap())
    }
}

impl<'a> Drop for PooledScriptContext<'a> {
    fn drop(&mut self) {
        if let Some(context) = self.context.take() {
            self.pool.return_context(context);
        }
    }
}
```

## Cache Implementation Details

### Multi-Level Cache System

```rust
/// Multi-level cache system for optimal performance
pub struct MultiLevelCache<K, V> {
    /// L1 cache (fastest, smallest)
    l1_cache: LruCache<K, V>,
    /// L2 cache (slower, larger)
    l2_cache: Arc<DashMap<K, Arc<V>>>,
    /// L3 cache (disk-based, largest)
    l3_cache: Option<DiskCache<K, V>>,
    /// Cache statistics
    stats: MultiLevelCacheStats,
}

impl<K, V> MultiLevelCache<K, V>
where
    K: Hash + Eq + Clone + Send + Sync,
    V: Clone + Send + Sync,
{
    /// Get value from cache hierarchy
    pub fn get(&mut self, key: &K) -> Option<V> {
        // Try L1 cache first
        if let Some(value) = self.l1_cache.get(key) {
            self.stats.l1_hits.fetch_add(1, Ordering::Relaxed);
            return Some(value.clone());
        }
        
        // Try L2 cache
        if let Some(value) = self.l2_cache.get(key) {
            let value = (*value).clone();
            // Promote to L1
            self.l1_cache.put(key.clone(), value.clone());
            self.stats.l2_hits.fetch_add(1, Ordering::Relaxed);
            return Some(value);
        }
        
        // Try L3 cache (disk)
        if let Some(ref l3) = self.l3_cache {
            if let Some(value) = l3.get(key) {
                // Promote to L2 and L1
                self.l2_cache.insert(key.clone(), Arc::new(value.clone()));
                self.l1_cache.put(key.clone(), value.clone());
                self.stats.l3_hits.fetch_add(1, Ordering::Relaxed);
                return Some(value);
            }
        }
        
        self.stats.misses.fetch_add(1, Ordering::Relaxed);
        None
    }
    
    /// Insert value into cache hierarchy
    pub fn insert(&mut self, key: K, value: V) {
        // Insert into all levels
        self.l1_cache.put(key.clone(), value.clone());
        self.l2_cache.insert(key.clone(), Arc::new(value.clone()));
        
        if let Some(ref l3) = self.l3_cache {
            l3.insert_async(key, value);
        }
    }
    
    /// Invalidate key from all cache levels
    pub fn invalidate(&mut self, key: &K) {
        self.l1_cache.pop(key);
        self.l2_cache.remove(key);
        
        if let Some(ref l3) = self.l3_cache {
            l3.remove_async(key);
        }
    }
}

#[derive(Debug, Default)]
pub struct MultiLevelCacheStats {
    pub l1_hits: AtomicU64,
    pub l2_hits: AtomicU64,
    pub l3_hits: AtomicU64,
    pub misses: AtomicU64,
}
```

## Profiling Integration

### Performance Profiler

```rust
/// Integrated performance profiler for optimization
pub struct KryonProfiler {
    /// Frame timing measurements
    frame_timings: CircularBuffer<FrameTiming>,
    /// Component timing measurements
    component_timings: HashMap<String, CircularBuffer<Duration>>,
    /// Memory usage tracking
    memory_tracker: MemoryTracker,
    /// Active profiling sessions
    active_sessions: DashMap<String, ProfilingSession>,
    /// Profiling configuration
    config: ProfilingConfig,
}

impl KryonProfiler {
    /// Begin profiling a component
    pub fn begin_profile(&self, component: &str) -> ProfileGuard {
        let start_time = Instant::now();
        
        ProfileGuard {
            component: component.to_string(),
            start_time,
            profiler: self,
        }
    }
    
    /// Record frame timing
    pub fn record_frame(&mut self, timing: FrameTiming) {
        self.frame_timings.push(timing);
        
        // Check for performance regressions
        if timing.total_time > self.config.performance_thresholds.frame_time {
            self.report_performance_issue(PerformanceIssue::SlowFrame {
                duration: timing.total_time,
                threshold: self.config.performance_thresholds.frame_time,
            });
        }
    }
    
    /// Generate performance report
    pub fn generate_report(&self) -> PerformanceReport {
        let frame_stats = self.calculate_frame_statistics();
        let component_stats = self.calculate_component_statistics();
        let memory_stats = self.memory_tracker.get_statistics();
        
        PerformanceReport {
            frame_stats,
            component_stats,
            memory_stats,
            timestamp: Instant::now(),
        }
    }
    
    /// Check for performance bottlenecks
    pub fn detect_bottlenecks(&self) -> Vec<PerformanceBottleneck> {
        let mut bottlenecks = Vec::new();
        
        // Analyze frame timing patterns
        let frame_stats = self.calculate_frame_statistics();
        if frame_stats.p99 > self.config.performance_thresholds.frame_time {
            bottlenecks.push(PerformanceBottleneck::SlowFrames {
                p99_time: frame_stats.p99,
                threshold: self.config.performance_thresholds.frame_time,
            });
        }
        
        // Analyze component timings
        for (component, timings) in &self.component_timings {
            let stats = calculate_timing_statistics(timings);
            if let Some(threshold) = self.config.component_thresholds.get(component) {
                if stats.mean > *threshold {
                    bottlenecks.push(PerformanceBottleneck::SlowComponent {
                        component: component.clone(),
                        mean_time: stats.mean,
                        threshold: *threshold,
                    });
                }
            }
        }
        
        // Analyze memory usage
        let memory_stats = self.memory_tracker.get_statistics();
        if memory_stats.peak_usage > self.config.memory_thresholds.peak_memory {
            bottlenecks.push(PerformanceBottleneck::HighMemoryUsage {
                peak_usage: memory_stats.peak_usage,
                threshold: self.config.memory_thresholds.peak_memory,
            });
        }
        
        bottlenecks
    }
}

/// RAII guard for automatic profiling
pub struct ProfileGuard<'a> {
    component: String,
    start_time: Instant,
    profiler: &'a KryonProfiler,
}

impl<'a> Drop for ProfileGuard<'a> {
    fn drop(&mut self) {
        let duration = self.start_time.elapsed();
        self.profiler.record_component_timing(&self.component, duration);
    }
}

/// Macro for easy profiling
macro_rules! profile {
    ($profiler:expr, $component:expr, $block:block) => {
        {
            let _guard = $profiler.begin_profile($component);
            $block
        }
    };
}

#[derive(Debug, Clone)]
pub struct FrameTiming {
    pub layout_time: Duration,
    pub render_time: Duration,
    pub script_time: Duration,
    pub event_time: Duration,
    pub total_time: Duration,
    pub frame_number: u64,
}

#[derive(Debug)]
pub enum PerformanceBottleneck {
    SlowFrames { p99_time: Duration, threshold: Duration },
    SlowComponent { component: String, mean_time: Duration, threshold: Duration },
    HighMemoryUsage { peak_usage: usize, threshold: usize },
    ExcessiveGarbageCollection { gc_time_percent: f32, threshold: f32 },
}
```

## Related Documentation

- [KRYON_TRAIT_SPECIFICATIONS.md](KRYON_TRAIT_SPECIFICATIONS.md) - Core trait definitions
- [KRYON_PERFORMANCE_SPECIFICATIONS.md](KRYON_PERFORMANCE_SPECIFICATIONS.md) - Performance requirements
- [KRYON_THREAD_SAFETY_SPECIFICATIONS.md](KRYON_THREAD_SAFETY_SPECIFICATIONS.md) - Concurrency details