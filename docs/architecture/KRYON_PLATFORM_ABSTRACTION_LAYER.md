# Kryon Platform Abstraction Layer Specification

Complete specification of the Kryon Platform Abstraction Layer (PAL), providing unified cross-platform APIs for rendering, input, hardware access, and system integration.

> **🔗 Related Documentation**: 
> - [KRYON_RUNTIME_ARCHITECTURE.md](./KRYON_RUNTIME_ARCHITECTURE.md) - Runtime system overview
> - [KRY_LANGUAGE_SPECIFICATION.md](../language/KRY_LANGUAGE_SPECIFICATION.md) - Platform features in language
> - [KRYON_COMPILATION_PIPELINE.md](./KRYON_COMPILATION_PIPELINE.md) - Target-specific compilation

## Table of Contents
- [Architecture Overview](#architecture-overview)
- [Core Platform Abstraction](#core-platform-abstraction)
- [Rendering Abstraction](#rendering-abstraction)
- [Input System Abstraction](#input-system-abstraction)
- [File System Abstraction](#file-system-abstraction)
- [Network Abstraction](#network-abstraction)
- [Hardware Access Abstraction](#hardware-access-abstraction)
- [Platform-specific Implementations](#platform-specific-implementations)
- [Performance Considerations](#performance-considerations)
- [Error Handling](#error-handling)

---

## Architecture Overview

The Platform Abstraction Layer provides a unified API that allows Kryon applications to run identically across all supported platforms while leveraging platform-specific optimizations and capabilities.

```
┌─────────────────────────────────────────────────────────────┐
│                    Kryon Application                        │
│                    (KRY/Lisp Code)                         │
└─────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────┐
│                 Platform Abstraction Layer                 │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────┐│
│  │   Render    │ │   Input     │ │   File      │ │Hardware ││
│  │ Abstraction │ │Abstraction  │ │Abstraction  │ │Access   ││
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────┘│
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────┐│
│  │  Network    │ │   Window    │ │   Audio     │ │ System  ││
│  │ Abstraction │ │ Management  │ │Abstraction  │ │Services ││
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────┘│
└─────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────┐
│                Platform Implementations                     │
│   WebPlatform  │ DesktopPlatform │ MobilePlatform │Terminal │
│   (WebGL/      │  (wgpu/winit)   │ (Metal/Vulkan) │ (TUI)   │
│    Canvas)     │                 │                │         │
└─────────────────────────────────────────────────────────────┘
```

### Design Principles

1. **Zero-Cost Abstraction**: Platform abstractions compile away to direct platform calls
2. **Capability Discovery**: Runtime detection of platform capabilities
3. **Graceful Degradation**: Fallback behavior when capabilities aren't available
4. **Performance Transparency**: Platform-specific optimizations are preserved
5. **Type Safety**: Compile-time guarantees for platform compatibility

---

## Core Platform Abstraction

### PlatformAbstraction Trait

```rust
pub trait PlatformAbstraction: Send + Sync {
    /// Platform identification and capabilities
    fn platform_info(&self) -> &PlatformInfo;
    fn capabilities(&self) -> &PlatformCapabilities;
    
    /// Lifecycle management
    fn initialize(&mut self, config: PlatformConfig) -> Result<()>;
    fn shutdown(&mut self) -> Result<()>;
    fn suspend(&mut self) -> Result<()>;
    fn resume(&mut self) -> Result<()>;
    
    /// Main event loop integration
    fn run_event_loop<F>(&mut self, update_callback: F) -> Result<()>
    where
        F: FnMut() -> Result<()> + 'static;
    
    /// Platform-specific subsystem access
    fn get_renderer(&mut self) -> Result<Box<dyn Renderer>>;
    fn get_input_handler(&mut self) -> Result<Box<dyn InputHandler>>;
    fn get_file_system(&mut self) -> Result<Box<dyn FileSystem>>;
    fn get_network_client(&mut self) -> Result<Box<dyn NetworkClient>>;
    fn get_hardware_access(&mut self) -> Result<Box<dyn HardwareAccess>>;
    fn get_audio_system(&mut self) -> Result<Box<dyn AudioSystem>>;
    
    /// Window/display management
    fn create_window(&mut self, config: WindowConfig) -> Result<WindowId>;
    fn destroy_window(&mut self, window: WindowId) -> Result<()>;
    fn set_window_properties(&mut self, window: WindowId, properties: WindowProperties) -> Result<()>;
    fn get_window_properties(&self, window: WindowId) -> Result<WindowProperties>;
    
    /// System integration
    fn get_system_info(&self) -> Result<SystemInfo>;
    fn request_permission(&self, permission: Permission) -> Result<PermissionStatus>;
    fn show_native_dialog(&self, dialog: NativeDialog) -> Result<DialogResult>;
    fn send_notification(&self, notification: Notification) -> Result<()>;
    
    /// Performance and debugging
    fn get_performance_info(&self) -> PerformanceInfo;
    fn enable_debug_overlay(&mut self, enabled: bool) -> Result<()>;
}
```

### Platform Information and Capabilities

```rust
#[derive(Debug, Clone)]
pub struct PlatformInfo {
    pub platform_type: PlatformType,
    pub platform_version: String,
    pub device_name: String,
    pub device_model: String,
    pub cpu_architecture: CpuArchitecture,
    pub available_memory: u64,
    pub display_info: DisplayInfo,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum PlatformType {
    Web,
    DesktopWindows,
    DesktopMacOS,
    DesktopLinux,
    MobileIOS,
    MobileAndroid,
    Terminal,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum CpuArchitecture {
    X86,
    X86_64,
    ARM,
    ARM64,
    WASM,
}

#[derive(Debug, Clone)]
pub struct PlatformCapabilities {
    /// Rendering capabilities
    pub rendering: RenderingCapabilities,
    
    /// Input capabilities
    pub input: InputCapabilities,
    
    /// Hardware access
    pub hardware: HardwareCapabilities,
    
    /// Network capabilities
    pub network: NetworkCapabilities,
    
    /// File system access
    pub file_system: FileSystemCapabilities,
    
    /// Audio capabilities
    pub audio: AudioCapabilities,
    
    /// System integration
    pub system: SystemCapabilities,
}

#[derive(Debug, Clone)]
pub struct RenderingCapabilities {
    pub backend_type: RenderBackendType,
    pub max_texture_size: u32,
    pub supports_compute_shaders: bool,
    pub supports_instancing: bool,
    pub supports_multi_sampling: bool,
    pub max_render_targets: u32,
    pub supported_formats: Vec<TextureFormat>,
}

#[derive(Debug, Clone)]
pub struct InputCapabilities {
    pub has_keyboard: bool,
    pub has_mouse: bool,
    pub has_touch: bool,
    pub max_touch_points: u32,
    pub has_gamepad: bool,
    pub has_accelerometer: bool,
    pub has_gyroscope: bool,
    pub has_magnetometer: bool,
}

#[derive(Debug, Clone)]
pub struct HardwareCapabilities {
    pub has_camera: bool,
    pub has_microphone: bool,
    pub has_location: bool,
    pub has_bluetooth: bool,
    pub has_nfc: bool,
    pub has_biometrics: bool,
    pub has_haptics: bool,
    pub battery_info: bool,
}
```

---

## Rendering Abstraction

### Renderer Trait

```rust
pub trait Renderer: Send + Sync {
    /// Renderer information
    fn backend_type(&self) -> RenderBackendType;
    fn capabilities(&self) -> &RenderingCapabilities;
    
    /// Surface management
    fn create_surface(&mut self, window: WindowId, config: SurfaceConfig) -> Result<SurfaceId>;
    fn destroy_surface(&mut self, surface: SurfaceId) -> Result<()>;
    fn resize_surface(&mut self, surface: SurfaceId, size: (u32, u32)) -> Result<()>;
    
    /// Frame lifecycle
    fn begin_frame(&mut self, surface: SurfaceId) -> Result<FrameContext>;
    fn end_frame(&mut self, context: FrameContext) -> Result<()>;
    fn present(&mut self, surface: SurfaceId) -> Result<()>;
    
    /// Render commands
    fn clear(&mut self, color: Color, depth: Option<f32>, stencil: Option<u32>) -> Result<()>;
    fn set_viewport(&mut self, viewport: Viewport) -> Result<()>;
    fn set_scissor(&mut self, scissor: Option<Rect>) -> Result<()>;
    
    /// Primitive rendering
    fn draw_rect(&mut self, rect: Rect, style: &RectStyle) -> Result<()>;
    fn draw_circle(&mut self, center: Point, radius: f32, style: &CircleStyle) -> Result<()>;
    fn draw_path(&mut self, path: &Path, style: &PathStyle) -> Result<()>;
    
    /// Text rendering
    fn draw_text(&mut self, text: &str, font: &Font, position: Point, style: &TextStyle) -> Result<()>;
    fn measure_text(&self, text: &str, font: &Font) -> Result<TextMetrics>;
    
    /// Image rendering
    fn create_texture(&mut self, data: &[u8], format: TextureFormat, size: (u32, u32)) -> Result<TextureId>;
    fn destroy_texture(&mut self, texture: TextureId) -> Result<()>;
    fn draw_texture(&mut self, texture: TextureId, src: Option<Rect>, dst: Rect, opacity: f32) -> Result<()>;
    
    /// Advanced rendering
    fn create_render_target(&mut self, size: (u32, u32), format: TextureFormat) -> Result<RenderTargetId>;
    fn set_render_target(&mut self, target: Option<RenderTargetId>) -> Result<()>;
    fn create_shader(&mut self, vertex_src: &str, fragment_src: &str) -> Result<ShaderId>;
    fn use_shader(&mut self, shader: Option<ShaderId>) -> Result<()>;
    
    /// Transform stack
    fn push_transform(&mut self, transform: Transform) -> Result<()>;
    fn pop_transform(&mut self) -> Result<()>;
    fn set_transform(&mut self, transform: Transform) -> Result<()>;
    
    /// Performance monitoring
    fn get_render_stats(&self) -> RenderStats;
    fn reset_stats(&mut self);
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RenderBackendType {
    WebGL,
    WebGPU,
    Vulkan,
    Metal,
    DirectX11,
    DirectX12,
    OpenGL,
    Software,
}
```

### Platform-Specific Rendering Implementations

#### WebGL Renderer
```rust
pub struct WebGLRenderer {
    context: web_sys::WebGl2RenderingContext,
    surfaces: HashMap<SurfaceId, WebGLSurface>,
    shaders: HashMap<ShaderId, WebGLProgram>,
    textures: HashMap<TextureId, WebGLTexture>,
    current_surface: Option<SurfaceId>,
    transform_stack: Vec<Transform>,
    stats: RenderStats,
}

impl Renderer for WebGLRenderer {
    fn backend_type(&self) -> RenderBackendType {
        RenderBackendType::WebGL
    }
    
    fn begin_frame(&mut self, surface: SurfaceId) -> Result<FrameContext> {
        self.current_surface = Some(surface);
        self.context.clear(web_sys::WebGl2RenderingContext::COLOR_BUFFER_BIT);
        
        Ok(FrameContext {
            surface,
            start_time: web_sys::window().unwrap().performance().unwrap().now(),
        })
    }
    
    fn draw_rect(&mut self, rect: Rect, style: &RectStyle) -> Result<()> {
        // Use WebGL vertex buffers for efficient rectangle rendering
        let vertices = self.create_rect_vertices(rect);
        let buffer = self.context.create_buffer().ok_or(KryonError::WebGLError)?;
        
        self.context.bind_buffer(web_sys::WebGl2RenderingContext::ARRAY_BUFFER, Some(&buffer));
        
        unsafe {
            let vert_array = js_sys::Float32Array::view(&vertices);
            self.context.buffer_data_with_array_buffer_view(
                web_sys::WebGl2RenderingContext::ARRAY_BUFFER,
                &vert_array,
                web_sys::WebGl2RenderingContext::STATIC_DRAW,
            );
        }
        
        self.context.draw_arrays(web_sys::WebGl2RenderingContext::TRIANGLE_STRIP, 0, 4);
        self.stats.draw_calls += 1;
        
        Ok(())
    }
}
```

#### WGPU Renderer (Desktop)
```rust
pub struct WgpuRenderer {
    device: wgpu::Device,
    queue: wgpu::Queue,
    surfaces: HashMap<SurfaceId, WgpuSurface>,
    render_pipeline: wgpu::RenderPipeline,
    uniform_buffer: wgpu::Buffer,
    texture_bind_group_layout: wgpu::BindGroupLayout,
    current_encoder: Option<wgpu::CommandEncoder>,
}

impl Renderer for WgpuRenderer {
    fn backend_type(&self) -> RenderBackendType {
        RenderBackendType::Vulkan // or Metal/DirectX based on platform
    }
    
    fn begin_frame(&mut self, surface: SurfaceId) -> Result<FrameContext> {
        let encoder = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor {
            label: Some("Kryon Frame Encoder"),
        });
        
        self.current_encoder = Some(encoder);
        
        Ok(FrameContext {
            surface,
            start_time: std::time::Instant::now().elapsed().as_secs_f64() * 1000.0,
        })
    }
    
    fn draw_rect(&mut self, rect: Rect, style: &RectStyle) -> Result<()> {
        if let Some(encoder) = &mut self.current_encoder {
            let surface_data = &self.surfaces[&surface];
            
            let mut render_pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("Rect Render Pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &surface_data.view,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Load,
                        store: true,
                    },
                })],
                depth_stencil_attachment: None,
            });
            
            render_pass.set_pipeline(&self.render_pipeline);
            
            // Upload rectangle data to uniform buffer
            let rect_data = RectUniformData {
                position: [rect.x, rect.y],
                size: [rect.width, rect.height],
                color: style.fill_color.into(),
                border_width: style.border_width,
                border_color: style.border_color.into(),
            };
            
            self.queue.write_buffer(&self.uniform_buffer, 0, bytemuck::cast_slice(&[rect_data]));
            render_pass.draw(0..6, 0..1); // Two triangles for rectangle
            
            self.stats.draw_calls += 1;
        }
        
        Ok(())
    }
}
```

---

## Input System Abstraction

### InputHandler Trait

```rust
pub trait InputHandler: Send + Sync {
    /// Input capabilities
    fn capabilities(&self) -> &InputCapabilities;
    
    /// Event polling
    fn poll_events(&mut self) -> Vec<InputEvent>;
    fn has_pending_events(&self) -> bool;
    
    /// Keyboard input
    fn is_key_pressed(&self, key: KeyCode) -> bool;
    fn is_key_just_pressed(&self, key: KeyCode) -> bool;
    fn is_key_just_released(&self, key: KeyCode) -> bool;
    fn get_text_input(&mut self) -> Option<String>;
    fn set_text_input_active(&mut self, active: bool);
    
    /// Mouse input
    fn get_mouse_position(&self) -> Point;
    fn is_mouse_button_pressed(&self, button: MouseButton) -> bool;
    fn is_mouse_button_just_pressed(&self, button: MouseButton) -> bool;
    fn is_mouse_button_just_released(&self, button: MouseButton) -> bool;
    fn get_mouse_wheel_delta(&self) -> (f32, f32);
    fn set_cursor_type(&mut self, cursor: CursorType) -> Result<()>;
    
    /// Touch input
    fn get_touches(&self) -> &[Touch];
    fn get_touch_by_id(&self, id: TouchId) -> Option<&Touch>;
    
    /// Gamepad input
    fn get_connected_gamepads(&self) -> Vec<GamepadId>;
    fn is_gamepad_button_pressed(&self, gamepad: GamepadId, button: GamepadButton) -> bool;
    fn get_gamepad_axis(&self, gamepad: GamepadId, axis: GamepadAxis) -> f32;
    
    /// Gesture recognition
    fn register_gesture(&mut self, gesture: GestureType) -> Result<GestureId>;
    fn unregister_gesture(&mut self, gesture: GestureId) -> Result<()>;
    fn get_gesture_events(&mut self) -> Vec<GestureEvent>;
    
    /// Hardware sensors (mobile)
    fn start_accelerometer(&mut self) -> Result<()>;
    fn stop_accelerometer(&mut self) -> Result<()>;
    fn get_acceleration(&self) -> Option<(f32, f32, f32)>;
    
    fn start_gyroscope(&mut self) -> Result<()>;
    fn stop_gyroscope(&mut self) -> Result<()>;
    fn get_angular_velocity(&self) -> Option<(f32, f32, f32)>;
}

#[derive(Debug, Clone)]
pub enum InputEvent {
    /// Keyboard events
    KeyPressed { key: KeyCode, modifiers: KeyModifiers },
    KeyReleased { key: KeyCode, modifiers: KeyModifiers },
    TextInput { text: String },
    
    /// Mouse events
    MouseMoved { position: Point, delta: (f32, f32) },
    MousePressed { button: MouseButton, position: Point },
    MouseReleased { button: MouseButton, position: Point },
    MouseWheel { delta: (f32, f32), position: Point },
    
    /// Touch events
    TouchStarted { touch: Touch },
    TouchMoved { touch: Touch },
    TouchEnded { touch: Touch },
    TouchCancelled { touch_id: TouchId },
    
    /// Gamepad events
    GamepadConnected { gamepad: GamepadId },
    GamepadDisconnected { gamepad: GamepadId },
    GamepadButtonPressed { gamepad: GamepadId, button: GamepadButton },
    GamepadButtonReleased { gamepad: GamepadId, button: GamepadButton },
    GamepadAxisChanged { gamepad: GamepadId, axis: GamepadAxis, value: f32 },
    
    /// Gesture events
    GestureDetected { gesture: GestureEvent },
    
    /// Sensor events
    AccelerationChanged { x: f32, y: f32, z: f32 },
    GyroscopeChanged { x: f32, y: f32, z: f32 },
    
    /// Window events
    WindowFocused,
    WindowUnfocused,
    WindowResized { width: u32, height: u32 },
    WindowClosed,
}

#[derive(Debug, Clone)]
pub struct Touch {
    pub id: TouchId,
    pub position: Point,
    pub pressure: f32,
    pub radius: (f32, f32),
    pub phase: TouchPhase,
}

#[derive(Debug, Clone)]
pub enum GestureEvent {
    Tap { position: Point, count: u32 },
    DoubleTap { position: Point },
    LongPress { position: Point, duration: f32 },
    Pan { start: Point, current: Point, delta: (f32, f32), velocity: (f32, f32) },
    Pinch { center: Point, scale: f32, velocity: f32 },
    Rotate { center: Point, angle: f32, velocity: f32 },
    Swipe { start: Point, end: Point, direction: SwipeDirection, velocity: f32 },
}
```

---

## File System Abstraction

### FileSystem Trait

```rust
pub trait FileSystem: Send + Sync {
    /// Capabilities
    fn capabilities(&self) -> &FileSystemCapabilities;
    
    /// Path operations
    fn normalize_path(&self, path: &str) -> Result<String>;
    fn get_absolute_path(&self, path: &str) -> Result<String>;
    fn get_parent_dir(&self, path: &str) -> Result<Option<String>>;
    fn join_paths(&self, base: &str, relative: &str) -> Result<String>;
    
    /// Directory operations
    fn create_dir(&self, path: &str) -> Result<()>;
    fn create_dir_all(&self, path: &str) -> Result<()>;
    fn remove_dir(&self, path: &str) -> Result<()>;
    fn remove_dir_all(&self, path: &str) -> Result<()>;
    fn read_dir(&self, path: &str) -> Result<Vec<DirEntry>>;
    fn dir_exists(&self, path: &str) -> Result<bool>;
    
    /// File operations
    fn read_file(&self, path: &str) -> Result<Vec<u8>>;
    fn read_file_to_string(&self, path: &str) -> Result<String>;
    fn write_file(&self, path: &str, data: &[u8]) -> Result<()>;
    fn write_file_string(&self, path: &str, content: &str) -> Result<()>;
    fn append_file(&self, path: &str, data: &[u8]) -> Result<()>;
    fn remove_file(&self, path: &str) -> Result<()>;
    fn file_exists(&self, path: &str) -> Result<bool>;
    fn get_file_size(&self, path: &str) -> Result<u64>;
    fn get_file_metadata(&self, path: &str) -> Result<FileMetadata>;
    
    /// Special directories
    fn get_app_data_dir(&self) -> Result<String>;
    fn get_user_home_dir(&self) -> Result<Option<String>>;
    fn get_temp_dir(&self) -> Result<String>;
    fn get_current_dir(&self) -> Result<String>;
    
    /// File watching
    fn watch_file(&mut self, path: &str) -> Result<WatchId>;
    fn watch_directory(&mut self, path: &str, recursive: bool) -> Result<WatchId>;
    fn unwatch(&mut self, watch_id: WatchId) -> Result<()>;
    fn poll_file_events(&mut self) -> Vec<FileEvent>;
    
    /// Permissions and security
    fn set_permissions(&self, path: &str, permissions: Permissions) -> Result<()>;
    fn get_permissions(&self, path: &str) -> Result<Permissions>;
}

#[derive(Debug, Clone)]
pub struct FileSystemCapabilities {
    pub can_read: bool,
    pub can_write: bool,
    pub can_create_dirs: bool,
    pub can_watch_files: bool,
    pub can_set_permissions: bool,
    pub max_file_size: Option<u64>,
    pub supported_encodings: Vec<String>,
    pub case_sensitive: bool,
}

#[derive(Debug, Clone)]
pub struct DirEntry {
    pub name: String,
    pub path: String,
    pub is_file: bool,
    pub is_directory: bool,
    pub size: Option<u64>,
    pub modified: Option<SystemTime>,
}

#[derive(Debug, Clone)]
pub enum FileEvent {
    Created { path: String },
    Modified { path: String },
    Deleted { path: String },
    Renamed { from: String, to: String },
}
```

### Platform-Specific File System Implementations

#### Web File System (Limited)
```rust
pub struct WebFileSystem {
    capabilities: FileSystemCapabilities,
    storage: web_sys::Storage,
    indexed_db: Option<web_sys::IdbDatabase>,
}

impl FileSystem for WebFileSystem {
    fn capabilities(&self) -> &FileSystemCapabilities {
        &self.capabilities
    }
    
    fn read_file(&self, path: &str) -> Result<Vec<u8>> {
        // Use localStorage or IndexedDB based on file size
        if let Some(data) = self.storage.get_item(path).map_err(|_| KryonError::FileNotFound)? {
            Ok(data.into_bytes())
        } else {
            Err(KryonError::FileNotFound)
        }
    }
    
    fn write_file(&self, path: &str, data: &[u8]) -> Result<()> {
        let data_str = String::from_utf8_lossy(data);
        self.storage.set_item(path, &data_str).map_err(|_| KryonError::WriteError)?;
        Ok(())
    }
    
    fn get_app_data_dir(&self) -> Result<String> {
        Ok("/app_data".to_string()) // Virtual path for web
    }
}
```

#### Native File System
```rust
pub struct NativeFileSystem {
    capabilities: FileSystemCapabilities,
    watchers: HashMap<WatchId, notify::RecommendedWatcher>,
    file_events: Vec<FileEvent>,
    next_watch_id: WatchId,
}

impl FileSystem for NativeFileSystem {
    fn read_file(&self, path: &str) -> Result<Vec<u8>> {
        std::fs::read(path).map_err(|e| match e.kind() {
            std::io::ErrorKind::NotFound => KryonError::FileNotFound,
            std::io::ErrorKind::PermissionDenied => KryonError::PermissionDenied,
            _ => KryonError::IoError(e),
        })
    }
    
    fn write_file(&self, path: &str, data: &[u8]) -> Result<()> {
        std::fs::write(path, data).map_err(|e| KryonError::IoError(e))
    }
    
    fn watch_file(&mut self, path: &str) -> Result<WatchId> {
        let watch_id = self.next_watch_id;
        self.next_watch_id = WatchId(self.next_watch_id.0 + 1);
        
        let (tx, rx) = std::sync::mpsc::channel();
        let mut watcher = notify::recommended_watcher(tx).map_err(|e| KryonError::WatchError)?;
        
        watcher.watch(std::path::Path::new(path), notify::RecursiveMode::NonRecursive)
            .map_err(|e| KryonError::WatchError)?;
        
        self.watchers.insert(watch_id, watcher);
        Ok(watch_id)
    }
}
```

---

## Network Abstraction

### NetworkClient Trait

```rust
pub trait NetworkClient: Send + Sync {
    /// Capabilities
    fn capabilities(&self) -> &NetworkCapabilities;
    
    /// HTTP client
    fn http_request(&self, request: HttpRequest) -> Result<HttpResponse>;
    fn http_request_async(&self, request: HttpRequest) -> Result<Box<dyn Future<Output = Result<HttpResponse>>>>;
    
    /// WebSocket client
    fn create_websocket(&self, url: &str) -> Result<Box<dyn WebSocket>>;
    
    /// Network status
    fn is_connected(&self) -> bool;
    fn get_connection_type(&self) -> ConnectionType;
    fn get_network_info(&self) -> NetworkInfo;
    
    /// Download/Upload with progress
    fn download_file(&self, url: &str, destination: &str) -> Result<DownloadHandle>;
    fn upload_file(&self, url: &str, file_path: &str, method: HttpMethod) -> Result<UploadHandle>;
}

#[derive(Debug, Clone)]
pub struct HttpRequest {
    pub url: String,
    pub method: HttpMethod,
    pub headers: HashMap<String, String>,
    pub body: Option<Vec<u8>>,
    pub timeout: Option<Duration>,
}

#[derive(Debug, Clone)]
pub struct HttpResponse {
    pub status_code: u16,
    pub headers: HashMap<String, String>,
    pub body: Vec<u8>,
    pub response_time: Duration,
}

pub trait WebSocket: Send + Sync {
    fn send_text(&mut self, message: &str) -> Result<()>;
    fn send_binary(&mut self, data: &[u8]) -> Result<()>;
    fn receive(&mut self) -> Result<Option<WebSocketMessage>>;
    fn close(&mut self) -> Result<()>;
    fn is_connected(&self) -> bool;
}

#[derive(Debug, Clone)]
pub enum WebSocketMessage {
    Text(String),
    Binary(Vec<u8>),
    Close,
    Ping(Vec<u8>),
    Pong(Vec<u8>),
}
```

---

## Hardware Access Abstraction

### HardwareAccess Trait

```rust
pub trait HardwareAccess: Send + Sync {
    /// Hardware capabilities
    fn capabilities(&self) -> &HardwareCapabilities;
    
    /// Camera access
    fn get_camera(&self, camera_id: CameraId) -> Result<Box<dyn Camera>>;
    fn list_cameras(&self) -> Result<Vec<CameraInfo>>;
    
    /// Microphone access
    fn get_microphone(&self, mic_id: MicrophoneId) -> Result<Box<dyn Microphone>>;
    fn list_microphones(&self) -> Result<Vec<MicrophoneInfo>>;
    
    /// Location services
    fn get_location_service(&self) -> Result<Box<dyn LocationService>>;
    
    /// Bluetooth
    fn get_bluetooth(&self) -> Result<Box<dyn BluetoothAdapter>>;
    
    /// NFC
    fn get_nfc(&self) -> Result<Box<dyn NfcAdapter>>;
    
    /// Biometrics
    fn get_biometric_auth(&self) -> Result<Box<dyn BiometricAuth>>;
    
    /// Haptics/Vibration
    fn vibrate(&self, pattern: &[u64]) -> Result<()>;
    fn stop_vibration(&self) -> Result<()>;
    
    /// Battery information
    fn get_battery_info(&self) -> Result<BatteryInfo>;
    
    /// Device sensors
    fn start_sensor(&mut self, sensor: SensorType) -> Result<SensorId>;
    fn stop_sensor(&mut self, sensor_id: SensorId) -> Result<()>;
    fn read_sensor(&self, sensor_id: SensorId) -> Result<SensorReading>;
}

pub trait Camera: Send + Sync {
    fn start_preview(&mut self) -> Result<()>;
    fn stop_preview(&mut self) -> Result<()>;
    fn take_photo(&mut self) -> Result<Vec<u8>>;
    fn start_recording(&mut self, config: VideoConfig) -> Result<()>;
    fn stop_recording(&mut self) -> Result<Vec<u8>>;
    fn set_flash(&mut self, enabled: bool) -> Result<()>;
    fn set_zoom(&mut self, zoom: f32) -> Result<()>;
    fn get_capabilities(&self) -> CameraCapabilities;
}

pub trait LocationService: Send + Sync {
    fn get_current_location(&self) -> Result<Location>;
    fn start_location_updates(&mut self, config: LocationConfig) -> Result<()>;
    fn stop_location_updates(&mut self) -> Result<()>;
    fn get_location_updates(&mut self) -> Vec<Location>;
}

#[derive(Debug, Clone)]
pub struct BatteryInfo {
    pub level: f32,           // 0.0 to 1.0
    pub is_charging: bool,
    pub time_remaining: Option<Duration>,
    pub health: BatteryHealth,
}

#[derive(Debug, Clone)]
pub enum SensorType {
    Accelerometer,
    Gyroscope,
    Magnetometer,
    AmbientLight,
    Proximity,
    Temperature,
    Humidity,
    Pressure,
}
```

---

## Platform-specific Implementations

### Web Platform Implementation

```rust
pub struct WebPlatform {
    window: web_sys::Window,
    document: web_sys::Document,
    canvas: Option<web_sys::HtmlCanvasElement>,
    
    // Subsystems
    renderer: Option<WebGLRenderer>,
    input_handler: WebInputHandler,
    file_system: WebFileSystem,
    network_client: WebNetworkClient,
    
    // Platform info
    info: PlatformInfo,
    capabilities: PlatformCapabilities,
}

impl WebPlatform {
    pub fn new() -> Result<Self> {
        let window = web_sys::window().ok_or(KryonError::PlatformInitError)?;
        let document = window.document().ok_or(KryonError::PlatformInitError)?;
        
        let info = PlatformInfo {
            platform_type: PlatformType::Web,
            platform_version: Self::get_browser_version()?,
            device_name: "Web Browser".to_string(),
            device_model: Self::get_device_model()?,
            cpu_architecture: CpuArchitecture::WASM,
            available_memory: Self::get_available_memory()?,
            display_info: Self::get_display_info(&window)?,
        };
        
        let capabilities = PlatformCapabilities {
            rendering: RenderingCapabilities {
                backend_type: RenderBackendType::WebGL,
                max_texture_size: Self::get_max_texture_size()?,
                supports_compute_shaders: false,
                supports_instancing: true,
                supports_multi_sampling: true,
                max_render_targets: 4,
                supported_formats: vec![
                    TextureFormat::RGBA8,
                    TextureFormat::RGB8,
                    TextureFormat::Alpha8,
                ],
            },
            input: InputCapabilities {
                has_keyboard: true,
                has_mouse: true,
                has_touch: Self::has_touch_support()?,
                max_touch_points: Self::get_max_touch_points()?,
                has_gamepad: Self::has_gamepad_support()?,
                has_accelerometer: false,
                has_gyroscope: false,
                has_magnetometer: false,
            },
            hardware: HardwareCapabilities {
                has_camera: Self::has_camera_access()?,
                has_microphone: Self::has_microphone_access()?,
                has_location: Self::has_location_access()?,
                has_bluetooth: Self::has_bluetooth_access()?,
                has_nfc: false,
                has_biometrics: false,
                has_haptics: Self::has_haptic_support()?,
                battery_info: Self::has_battery_api()?,
            },
            network: NetworkCapabilities {
                has_http: true,
                has_websocket: true,
                has_download: true,
                has_upload: true,
                max_request_size: Some(100 * 1024 * 1024), // 100MB
                supports_cors: true,
            },
            file_system: FileSystemCapabilities {
                can_read: false, // Limited to localStorage/IndexedDB
                can_write: true,
                can_create_dirs: false,
                can_watch_files: false,
                can_set_permissions: false,
                max_file_size: Some(10 * 1024 * 1024), // 10MB
                supported_encodings: vec!["utf-8".to_string()],
                case_sensitive: true,
            },
            audio: AudioCapabilities {
                can_play: true,
                can_record: Self::has_microphone_access()?,
                supported_formats: vec!["mp3", "wav", "ogg"].into_iter().map(String::from).collect(),
                max_channels: 2,
                sample_rates: vec![44100, 48000],
            },
            system: SystemCapabilities {
                can_show_dialogs: true,
                can_send_notifications: Self::has_notification_support()?,
                can_access_clipboard: true,
                can_fullscreen: true,
                can_minimize: false,
                can_set_title: true,
            },
        };
        
        Ok(Self {
            window,
            document,
            canvas: None,
            renderer: None,
            input_handler: WebInputHandler::new()?,
            file_system: WebFileSystem::new()?,
            network_client: WebNetworkClient::new()?,
            info,
            capabilities,
        })
    }
}

impl PlatformAbstraction for WebPlatform {
    fn platform_info(&self) -> &PlatformInfo {
        &self.info
    }
    
    fn capabilities(&self) -> &PlatformCapabilities {
        &self.capabilities
    }
    
    fn run_event_loop<F>(&mut self, mut update_callback: F) -> Result<()>
    where
        F: FnMut() -> Result<()> + 'static,
    {
        let closure = Closure::wrap(Box::new(move || {
            update_callback().unwrap_or_else(|e| {
                web_sys::console::error_1(&format!("Update error: {:?}", e).into());
            });
        }) as Box<dyn FnMut()>);
        
        self.window.request_animation_frame(closure.as_ref().unchecked_ref())?;
        closure.forget();
        
        Ok(())
    }
}
```

### Desktop Platform Implementation

```rust
pub struct DesktopPlatform {
    event_loop: Option<winit::event_loop::EventLoop<()>>,
    windows: HashMap<WindowId, winit::window::Window>,
    
    // Graphics
    instance: wgpu::Instance,
    surfaces: HashMap<WindowId, wgpu::Surface>,
    device: Option<wgpu::Device>,
    queue: Option<wgpu::Queue>,
    
    // Subsystems
    renderer: Option<WgpuRenderer>,
    input_handler: DesktopInputHandler,
    file_system: NativeFileSystem,
    network_client: NativeNetworkClient,
    hardware_access: DesktopHardwareAccess,
    
    // Platform info
    info: PlatformInfo,
    capabilities: PlatformCapabilities,
}

impl DesktopPlatform {
    pub fn new() -> Result<Self> {
        let event_loop = winit::event_loop::EventLoop::new();
        let instance = wgpu::Instance::new(wgpu::Backends::all());
        
        let info = PlatformInfo {
            platform_type: Self::detect_platform_type(),
            platform_version: Self::get_os_version()?,
            device_name: Self::get_device_name()?,
            device_model: Self::get_device_model()?,
            cpu_architecture: Self::detect_cpu_architecture(),
            available_memory: Self::get_available_memory()?,
            display_info: Self::get_display_info()?,
        };
        
        let capabilities = Self::detect_capabilities(&instance)?;
        
        Ok(Self {
            event_loop: Some(event_loop),
            windows: HashMap::new(),
            instance,
            surfaces: HashMap::new(),
            device: None,
            queue: None,
            renderer: None,
            input_handler: DesktopInputHandler::new()?,
            file_system: NativeFileSystem::new()?,
            network_client: NativeNetworkClient::new()?,
            hardware_access: DesktopHardwareAccess::new()?,
            info,
            capabilities,
        })
    }
    
    fn detect_platform_type() -> PlatformType {
        #[cfg(target_os = "windows")]
        return PlatformType::DesktopWindows;
        #[cfg(target_os = "macos")]
        return PlatformType::DesktopMacOS;
        #[cfg(target_os = "linux")]
        return PlatformType::DesktopLinux;
        #[cfg(not(any(target_os = "windows", target_os = "macos", target_os = "linux")))]
        return PlatformType::DesktopLinux; // Default fallback
    }
}
```

---

## Performance Considerations

### Platform-Specific Optimizations

1. **Web Platform**:
   - Use WebGL vertex buffer objects for batch rendering
   - Implement texture atlasing to reduce draw calls
   - Use requestAnimationFrame for smooth animations
   - Leverage Web Workers for background processing

2. **Desktop Platform**:
   - Use wgpu compute shaders for parallel processing
   - Implement multi-threaded rendering with command buffers
   - Use memory-mapped files for large asset loading
   - Leverage platform-specific GPU optimizations

3. **Mobile Platform**:
   - Use tile-based rendering for mobile GPUs
   - Implement aggressive texture compression
   - Use hardware-accelerated video decoding
   - Optimize for battery usage and thermal throttling

### Memory Management

```rust
pub struct PlatformMemoryManager {
    /// Platform-specific memory allocator
    allocator: Box<dyn Allocator>,
    
    /// Memory pools by size class
    pools: HashMap<usize, MemoryPool>,
    
    /// GPU memory management
    gpu_memory: GpuMemoryManager,
    
    /// Memory pressure monitoring
    pressure_monitor: MemoryPressureMonitor,
}

impl PlatformMemoryManager {
    pub fn allocate_aligned(&mut self, size: usize, alignment: usize) -> Result<*mut u8> {
        // Use platform-specific aligned allocation
        #[cfg(target_os = "windows")]
        return self.windows_aligned_alloc(size, alignment);
        #[cfg(target_os = "macos")]
        return self.macos_aligned_alloc(size, alignment);
        #[cfg(target_os = "linux")]
        return self.linux_aligned_alloc(size, alignment);
        #[cfg(target_arch = "wasm32")]
        return self.wasm_aligned_alloc(size, alignment);
    }
    
    pub fn handle_memory_pressure(&mut self, level: MemoryPressureLevel) -> Result<()> {
        match level {
            MemoryPressureLevel::Low => {
                // Free cached resources
                self.gpu_memory.free_unused_textures()?;
            }
            MemoryPressureLevel::Medium => {
                // More aggressive cleanup
                self.pools.values_mut().for_each(|pool| pool.compact());
                self.gpu_memory.compact()?;
            }
            MemoryPressureLevel::Critical => {
                // Emergency cleanup
                self.gpu_memory.emergency_cleanup()?;
                self.force_garbage_collection()?;
            }
        }
        Ok(())
    }
}
```

---

## Error Handling

### Platform-Specific Error Types

```rust
#[derive(Debug, thiserror::Error)]
pub enum PlatformError {
    #[error("Platform initialization failed: {0}")]
    InitializationFailed(String),
    
    #[error("Window creation failed: {0}")]
    WindowCreationFailed(String),
    
    #[error("Renderer initialization failed: {0}")]
    RendererInitializationFailed(String),
    
    #[error("Hardware access denied: {0}")]
    HardwareAccessDenied(String),
    
    #[error("Permission required: {0}")]
    PermissionRequired(Permission),
    
    #[error("Platform feature not supported: {0}")]
    FeatureNotSupported(String),
    
    #[error("Network error: {0}")]
    NetworkError(String),
    
    #[error("File system error: {0}")]
    FileSystemError(String),
    
    // Platform-specific errors
    #[cfg(target_arch = "wasm32")]
    #[error("WebGL error: {0}")]
    WebGLError(String),
    
    #[cfg(not(target_arch = "wasm32"))]
    #[error("wgpu error: {0}")]
    WgpuError(#[from] wgpu::Error),
    
    #[cfg(target_os = "windows")]
    #[error("Windows API error: {0}")]
    WindowsError(#[from] windows::core::Error),
    
    #[cfg(target_os = "macos")]
    #[error("macOS error: {0}")]
    MacOSError(String),
    
    #[cfg(target_os = "linux")]
    #[error("Linux error: {0}")]
    LinuxError(String),
}

pub trait PlatformErrorHandler {
    fn handle_error(&mut self, error: PlatformError) -> ErrorAction;
    fn should_retry(&self, error: &PlatformError) -> bool;
    fn get_fallback_action(&self, error: &PlatformError) -> Option<FallbackAction>;
}

#[derive(Debug, Clone)]
pub enum ErrorAction {
    Continue,
    Retry,
    Fallback(FallbackAction),
    Abort,
}

#[derive(Debug, Clone)]
pub enum FallbackAction {
    UseSoftwareRenderer,
    DisableFeature(String),
    ShowErrorDialog(String),
    RequestPermission(Permission),
}
```

This comprehensive Platform Abstraction Layer specification provides the foundation for building truly cross-platform Kryon applications while maintaining optimal performance and native platform integration capabilities.