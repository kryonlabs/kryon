# Kryon Thread Safety and Async Specifications

This document defines thread safety guarantees, async operation contracts, and concurrency patterns for Kryon implementations.

## Table of Contents

- [Thread Safety Guarantees](#thread-safety-guarantees)
- [Async Operation Contracts](#async-operation-contracts)
- [Concurrency Patterns](#concurrency-patterns)
- [Memory Ordering Requirements](#memory-ordering-requirements)
- [Synchronization Primitives](#synchronization-primitives)

## Thread Safety Guarantees

### Core Thread Safety Classifications

```rust
/// Thread safety levels for Kryon components
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ThreadSafety {
    /// Not thread safe - single thread access only
    NotThreadSafe,
    /// Thread safe for concurrent reads, exclusive writes
    ReadConcurrent,
    /// Fully thread safe for all operations
    FullyConcurrent,
    /// Thread safe with external synchronization
    ExternalSync,
}

/// Thread safety requirements for each component
pub struct ThreadSafetyMatrix {
    pub element_tree: ThreadSafety,        // ReadConcurrent
    pub layout_engine: ThreadSafety,       // ReadConcurrent  
    pub render_system: ThreadSafety,       // ExternalSync
    pub script_engine: ThreadSafety,       // FullyConcurrent
    pub event_system: ThreadSafety,        // FullyConcurrent
    pub resource_manager: ThreadSafety,    // FullyConcurrent
}
```

### Element Tree Thread Safety

```rust
/// Element tree must support concurrent reads with exclusive writes
pub trait ThreadSafeElementTree: Send + Sync {
    /// Read operations that can be performed concurrently
    /// - Multiple threads can read element properties simultaneously
    /// - Layout can be computed on background threads
    /// - Hit testing can be performed from any thread
    fn get_element(&self, id: ElementId) -> Option<Arc<Element>>;
    fn get_children(&self, id: ElementId) -> Vec<ElementId>;
    fn find_element_at_point(&self, point: Point) -> Option<ElementId>;
    
    /// Write operations require exclusive access
    /// - Only one thread can modify the tree at a time
    /// - Must use proper synchronization (RwLock, Mutex, etc.)
    fn add_element(&mut self, parent: ElementId, element: Element) -> ElementId;
    fn remove_element(&mut self, id: ElementId) -> Option<Element>;
    fn update_element(&mut self, id: ElementId, updates: ElementUpdates);
    
    /// Batch operations for efficiency
    /// - Acquire write lock once for multiple operations
    /// - Notify observers after batch completion
    fn batch_update<F>(&mut self, f: F) -> BatchResult
    where
        F: FnOnce(&mut ElementTreeMut) -> BatchResult;
}

/// Implementation requirements for element tree synchronization
impl ElementTreeSync {
    /// Must use RwLock for reader-writer synchronization
    /// - Allows multiple concurrent readers
    /// - Exclusive access for writers
    /// - Should prefer parking_lot::RwLock for performance
    pub fn new() -> Self {
        Self {
            elements: Arc::new(RwLock::new(HashMap::new())),
            change_notifier: Arc::new(Notify::new()),
        }
    }
    
    /// Change notification system
    /// - Must notify layout engine when elements change
    /// - Should batch notifications to avoid spam
    /// - Must be lock-free to avoid deadlocks
    pub fn notify_changes(&self, changed_elements: &[ElementId]) {
        // Implementation must be non-blocking
        self.change_notifier.notify_waiters();
    }
}
```

### Layout Engine Thread Safety

```rust
/// Layout engine thread safety requirements
pub trait ThreadSafeLayoutEngine: Send + Sync {
    /// Layout computation can run on background threads
    /// - Must not block the main thread
    /// - Should support incremental updates
    /// - Must handle concurrent element tree reads safely
    async fn compute_layout_async(
        &self,
        elements: Arc<dyn ThreadSafeElementTree>,
        constraints: LayoutConstraints,
    ) -> Result<LayoutResult, LayoutError>;
    
    /// Layout results must be thread-safe to read
    /// - Multiple threads can read layout results
    /// - Results should be immutable once computed
    /// - Should use Arc<> for shared ownership
    fn get_layout_result(&self) -> Option<Arc<LayoutResult>>;
    
    /// Layout invalidation from any thread
    /// - Event handlers can invalidate layout
    /// - Should use atomic operations for dirty flags
    /// - Must be non-blocking
    fn invalidate_layout(&self, elements: &[ElementId]);
}

/// Layout result must be immutable and thread-safe
#[derive(Debug, Clone)]
pub struct LayoutResult {
    /// Element positions and sizes (immutable)
    pub element_rects: Arc<HashMap<ElementId, Rect>>,
    /// Layout generation number for change detection
    pub generation: AtomicU64,
    /// Timestamp when layout was computed
    pub computed_at: Instant,
}

impl LayoutResult {
    /// Thread-safe access to element rectangles
    pub fn get_element_rect(&self, id: ElementId) -> Option<Rect> {
        self.element_rects.get(&id).copied()
    }
    
    /// Check if layout is newer than given generation
    pub fn is_newer_than(&self, generation: u64) -> bool {
        self.generation.load(Ordering::Acquire) > generation
    }
}
```

### Render System Thread Safety

```rust
/// Render system requires external synchronization
/// - Commands can be generated on multiple threads
/// - Rendering must happen on designated render thread
/// - GPU resources must be managed carefully
pub trait ThreadSafeRenderSystem {
    /// Command generation is thread-safe
    /// - Multiple threads can generate render commands
    /// - Commands should be collected in thread-safe queue
    /// - Must handle command ordering correctly
    fn submit_render_commands(&self, commands: Vec<RenderCommand>);
    
    /// Rendering must happen on single thread
    /// - GPU context is typically single-threaded
    /// - Must consume commands from all threads
    /// - Should batch commands for efficiency
    fn render_frame(&mut self) -> Result<(), RenderError>;
    
    /// Resource management across threads
    /// - Textures can be loaded on background threads
    /// - Must handle GPU resource creation safely
    /// - Should use reference counting for resources
    fn load_texture_async(&self, data: Vec<u8>) -> impl Future<Output = Result<TextureId, LoadError>>;
}

/// Thread-safe render command queue
pub struct RenderCommandQueue {
    commands: Arc<Mutex<VecDeque<RenderCommand>>>,
    condvar: Arc<Condvar>,
}

impl RenderCommandQueue {
    /// Submit commands from any thread
    pub fn submit(&self, commands: Vec<RenderCommand>) {
        let mut queue = self.commands.lock().unwrap();
        queue.extend(commands);
        self.condvar.notify_one();
    }
    
    /// Consume commands on render thread
    pub fn consume_all(&self) -> Vec<RenderCommand> {
        let mut queue = self.commands.lock().unwrap();
        queue.drain(..).collect()
    }
    
    /// Wait for commands with timeout
    pub fn wait_for_commands(&self, timeout: Duration) -> bool {
        let queue = self.commands.lock().unwrap();
        if !queue.is_empty() {
            return true;
        }
        
        let (_queue, timeout_result) = self.condvar
            .wait_timeout(queue, timeout)
            .unwrap();
        !timeout_result.timed_out()
    }
}
```

## Async Operation Contracts

### Async Layout Computation

```rust
/// Async layout operations must follow these contracts
#[async_trait]
pub trait AsyncLayoutEngine: Send + Sync {
    /// Compute layout asynchronously without blocking
    /// 
    /// # Guarantees
    /// - Must not block the calling thread
    /// - Should yield periodically for cooperative multitasking
    /// - Must handle cancellation gracefully
    /// - Should support progress reporting
    async fn compute_layout_async(
        &self,
        elements: Arc<dyn ThreadSafeElementTree>,
        constraints: LayoutConstraints,
        progress: Option<ProgressReporter>,
        cancel_token: CancellationToken,
    ) -> Result<Arc<LayoutResult>, LayoutError>;
    
    /// Incremental layout updates
    /// 
    /// # Performance Requirements
    /// - Must complete faster than full recompute for < 10% changes
    /// - Should reuse previous computations where possible
    /// - Must handle dependencies correctly
    async fn update_layout_incremental(
        &self,
        previous_result: Arc<LayoutResult>,
        changed_elements: &[ElementId],
        cancel_token: CancellationToken,
    ) -> Result<Arc<LayoutResult>, LayoutError>;
}

/// Progress reporting for long-running operations
pub trait ProgressReporter: Send + Sync {
    fn report_progress(&self, completed: f32, message: &str);
    fn is_cancelled(&self) -> bool;
}

/// Cancellation token for async operations
#[derive(Clone)]
pub struct CancellationToken {
    cancelled: Arc<AtomicBool>,
}

impl CancellationToken {
    pub fn new() -> Self {
        Self {
            cancelled: Arc::new(AtomicBool::new(false)),
        }
    }
    
    pub fn cancel(&self) {
        self.cancelled.store(true, Ordering::Release);
    }
    
    pub fn is_cancelled(&self) -> bool {
        self.cancelled.load(Ordering::Acquire)
    }
    
    /// Check cancellation and yield if cancelled
    pub async fn check_cancelled(&self) -> Result<(), CancellationError> {
        if self.is_cancelled() {
            Err(CancellationError)
        } else {
            // Yield to allow other tasks to run
            tokio::task::yield_now().await;
            Ok(())
        }
    }
}
```

### Async Resource Loading

```rust
/// Async resource loading contracts
#[async_trait]
pub trait AsyncResourceManager: Send + Sync {
    /// Load texture asynchronously
    /// 
    /// # Guarantees
    /// - Must not block the main thread
    /// - Should cache results by content hash
    /// - Must handle loading failures gracefully
    /// - Should support concurrent loading
    async fn load_texture(
        &self,
        data: Vec<u8>,
        format: ImageFormat,
    ) -> Result<Arc<Texture>, LoadError>;
    
    /// Load font asynchronously
    async fn load_font(
        &self,
        data: Vec<u8>,
        size: f32,
    ) -> Result<Arc<Font>, LoadError>;
    
    /// Batch resource loading
    /// - Should load resources in parallel when possible
    /// - Must handle partial failures correctly
    /// - Should provide progress reporting
    async fn load_resources_batch(
        &self,
        requests: Vec<ResourceRequest>,
        progress: Option<ProgressReporter>,
    ) -> Vec<Result<Arc<dyn Resource>, LoadError>>;
}

/// Resource loading with priority and caching
pub struct ResourceRequest {
    pub url: String,
    pub resource_type: ResourceType,
    pub priority: LoadPriority,
    pub cache_policy: CachePolicy,
}

#[derive(Debug, Clone, Copy)]
pub enum LoadPriority {
    Critical,  // Block rendering until loaded
    High,      // Load before low priority
    Normal,    // Standard loading priority
    Low,       // Load when system is idle
}
```

### Async Script Execution

```rust
/// Async script execution contracts
#[async_trait]
pub trait AsyncScriptEngine: Send + Sync {
    /// Execute script asynchronously
    /// 
    /// # Guarantees
    /// - Must not block other scripts or UI thread
    /// - Should enforce execution time limits
    /// - Must handle script errors gracefully
    /// - Should support script cancellation
    async fn execute_script_async(
        &self,
        script: &str,
        context: ScriptContext,
        timeout: Duration,
        cancel_token: CancellationToken,
    ) -> Result<ScriptValue, ScriptError>;
    
    /// Stream script execution results
    /// - For long-running scripts that yield values
    /// - Should support backpressure handling
    /// - Must handle stream cancellation
    fn execute_script_stream(
        &self,
        script: &str,
        context: ScriptContext,
    ) -> impl Stream<Item = Result<ScriptValue, ScriptError>>;
}
```

## Concurrency Patterns

### Producer-Consumer Pattern

```rust
/// Event system using producer-consumer pattern
pub struct EventSystem {
    event_queue: Arc<crossbeam::queue::SegQueue<Event>>,
    processors: Vec<EventProcessor>,
    shutdown: Arc<AtomicBool>,
}

impl EventSystem {
    /// Start event processing threads
    pub fn start(&mut self, num_threads: usize) {
        for i in 0..num_threads {
            let queue = Arc::clone(&self.event_queue);
            let shutdown = Arc::clone(&self.shutdown);
            
            let processor = EventProcessor::new(i);
            std::thread::spawn(move || {
                processor.run(queue, shutdown);
            });
        }
    }
    
    /// Submit event from any thread
    pub fn submit_event(&self, event: Event) {
        self.event_queue.push(event);
    }
}

/// Lock-free event processor
pub struct EventProcessor {
    id: usize,
}

impl EventProcessor {
    fn run(
        &self,
        queue: Arc<crossbeam::queue::SegQueue<Event>>,
        shutdown: Arc<AtomicBool>,
    ) {
        while !shutdown.load(Ordering::Acquire) {
            if let Some(event) = queue.pop() {
                self.process_event(event);
            } else {
                // No events available, yield thread
                std::thread::yield_now();
            }
        }
    }
}
```

### Work Stealing Pattern

```rust
/// Work stealing for parallel layout computation
pub struct ParallelLayoutEngine {
    work_queues: Vec<crossbeam::deque::Worker<LayoutTask>>,
    stealers: Vec<crossbeam::deque::Stealer<LayoutTask>>,
    thread_pool: ThreadPool,
}

impl ParallelLayoutEngine {
    /// Distribute layout work across threads
    pub async fn compute_layout_parallel(
        &self,
        elements: &[ElementId],
    ) -> Result<LayoutResult, LayoutError> {
        // Create layout tasks
        let tasks = self.create_layout_tasks(elements);
        
        // Distribute tasks to worker queues
        for (i, task) in tasks.into_iter().enumerate() {
            let queue_idx = i % self.work_queues.len();
            self.work_queues[queue_idx].push(task);
        }
        
        // Workers steal tasks from each other when idle
        let results = self.thread_pool.spawn(move || {
            // Work stealing implementation
            self.execute_with_work_stealing()
        }).await?;
        
        Ok(self.combine_results(results))
    }
}
```

### Actor Pattern

```rust
/// Actor-based script execution
pub struct ScriptActor {
    mailbox: mpsc::UnboundedReceiver<ScriptMessage>,
    sender: mpsc::UnboundedSender<ScriptMessage>,
    engine: Box<dyn ScriptEngine>,
}

#[derive(Debug)]
pub enum ScriptMessage {
    Execute {
        script: String,
        context: ScriptContext,
        reply: oneshot::Sender<Result<ScriptValue, ScriptError>>,
    },
    RegisterFunction {
        name: String,
        function: Box<dyn ScriptFunction>,
    },
    Shutdown,
}

impl ScriptActor {
    /// Run actor event loop
    pub async fn run(mut self) {
        while let Some(message) = self.mailbox.recv().await {
            match message {
                ScriptMessage::Execute { script, context, reply } => {
                    let result = self.engine.execute(&script, &context);
                    let _ = reply.send(result);
                }
                ScriptMessage::RegisterFunction { name, function } => {
                    self.engine.register_function(&name, function);
                }
                ScriptMessage::Shutdown => break,
            }
        }
    }
    
    /// Get handle to communicate with actor
    pub fn handle(&self) -> ScriptActorHandle {
        ScriptActorHandle {
            sender: self.sender.clone(),
        }
    }
}
```

## Memory Ordering Requirements

### Atomic Operations

```rust
/// Memory ordering requirements for atomic operations
pub struct AtomicRequirements;

impl AtomicRequirements {
    /// Generation counters for change detection
    /// - Must use Acquire/Release ordering
    /// - Prevents reordering of dependent operations
    pub fn update_generation(counter: &AtomicU64) -> u64 {
        counter.fetch_add(1, Ordering::AcqRel)
    }
    
    /// Dirty flags for layout invalidation
    /// - Can use Relaxed ordering for simple flags
    /// - No synchronization with other memory needed
    pub fn set_dirty_flag(flag: &AtomicBool) {
        flag.store(true, Ordering::Relaxed);
    }
    
    /// Reference counting for shared resources
    /// - Must use Acquire/Release for proper cleanup
    /// - Critical for memory safety
    pub fn increment_ref_count(count: &AtomicUsize) -> usize {
        count.fetch_add(1, Ordering::Acquire)
    }
    
    pub fn decrement_ref_count(count: &AtomicUsize) -> usize {
        count.fetch_sub(1, Ordering::Release)
    }
}
```

### Lock-Free Data Structures

```rust
/// Lock-free requirements for high-performance paths
pub trait LockFreeRequirements {
    /// Event queues must be lock-free
    /// - Use crossbeam::queue::SegQueue or similar
    /// - Must handle high-frequency events without blocking
    type EventQueue: LockFreeQueue<Event>;
    
    /// Resource caches should be lock-free for reads
    /// - Use dashmap or similar concurrent hashmap
    /// - Writes can use locking but reads must be lock-free
    type ResourceCache: LockFreeCache<ResourceId, Arc<dyn Resource>>;
    
    /// Change notification must be lock-free
    /// - Use atomic flags or lock-free channels
    /// - Must not block event submission
    type ChangeNotifier: LockFreeNotifier;
}
```

## Synchronization Primitives

### Required Synchronization Tools

```rust
/// Standard synchronization primitives for Kryon
pub struct SyncPrimitives {
    /// For element tree (readers-writer lock)
    pub element_tree_lock: parking_lot::RwLock<ElementTree>,
    
    /// For render command queue (mutex + condition variable)
    pub render_queue: (Mutex<VecDeque<RenderCommand>>, Condvar),
    
    /// For script execution (semaphore for limiting concurrent scripts)
    pub script_semaphore: tokio::sync::Semaphore,
    
    /// For resource loading (rate limiting)
    pub resource_rate_limit: governor::RateLimiter,
    
    /// For cross-thread notifications
    pub change_notifier: Arc<tokio::sync::Notify>,
}

/// Deadlock prevention requirements
impl DeadlockPrevention {
    /// Lock ordering rules to prevent deadlocks
    /// 1. Always acquire element tree lock before layout lock
    /// 2. Never hold render lock while acquiring other locks  
    /// 3. Use try_lock for optional locks with timeout
    /// 4. Release locks in reverse order of acquisition
    
    /// Example of correct lock ordering
    pub fn safe_update_with_layout(&self, updates: ElementUpdates) -> Result<(), LockError> {
        // 1. Acquire element tree lock first
        let mut elements = self.element_tree_lock.write();
        
        // 2. Apply updates
        elements.apply_updates(updates);
        
        // 3. Try to acquire layout lock (don't block)
        if let Ok(mut layout) = self.layout_lock.try_write_for(Duration::from_millis(10)) {
            layout.invalidate_affected_elements(&elements);
        }
        
        // 4. Locks released in reverse order automatically
        Ok(())
    }
}
```

### Async Synchronization

```rust
/// Async synchronization primitives
pub struct AsyncSyncPrimitives {
    /// For coordinating async layout computation
    pub layout_coordinator: Arc<tokio::sync::RwLock<LayoutState>>,
    
    /// For limiting concurrent resource loading
    pub resource_semaphore: Arc<tokio::sync::Semaphore>,
    
    /// For batching async operations
    pub operation_batcher: Arc<tokio::sync::Mutex<OperationBatch>>,
    
    /// For cancelling long-running operations
    pub cancellation_tokens: DashMap<OperationId, CancellationToken>,
}
```

## Related Documentation

- [KRYON_TRAIT_SPECIFICATIONS.md](KRYON_TRAIT_SPECIFICATIONS.md) - Trait definitions
- [KRYON_PERFORMANCE_SPECIFICATIONS.md](KRYON_PERFORMANCE_SPECIFICATIONS.md) - Performance requirements
- [KRYON_RUNTIME_SYSTEM.md](../runtime/KRYON_RUNTIME_SYSTEM.md) - Runtime architecture