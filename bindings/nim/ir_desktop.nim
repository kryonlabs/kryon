## Desktop IR Renderer Bindings for Nim
## Direct bindings to the desktop backend of the Universal IR system

import os

# Import core IR bindings
import ir_core

# Add include path for desktop backend headers
{.passC: "-I" & currentSourcePath().parentDir() & "/../../backends/desktop".}

# Link against desktop backend library
{.passL: "-L" & currentSourcePath().parentDir() & "/../../build -lkryon_desktop -lkryon_ir -lharfbuzz -lfreetype -lfribidi".}

# Types from ir_desktop_renderer.h
type
  DesktopBackendType* {.size: sizeof(cint).} = enum
    DESKTOP_BACKEND_SDL3 = 0
    DESKTOP_BACKEND_GLFW
    DESKTOP_BACKEND_OPENGL

  DesktopEventType* {.size: sizeof(cint).} = enum
    DESKTOP_EVENT_QUIT = 0
    DESKTOP_EVENT_MOUSE_CLICK
    DESKTOP_EVENT_MOUSE_MOVE
    DESKTOP_EVENT_MOUSE_SCROLL
    DESKTOP_EVENT_KEY_PRESS
    DESKTOP_EVENT_KEY_RELEASE
    DESKTOP_EVENT_TEXT_INPUT
    DESKTOP_EVENT_WINDOW_RESIZE
    DESKTOP_EVENT_CUSTOM

  DesktopEvent* = object
    `type`*: DesktopEventType
    timestamp*: uint64
    data*: pointer

  DesktopEventCallback* = proc(event: ptr DesktopEvent; user_data: pointer) {.cdecl.}

  DesktopRendererConfig* {.importc: "DesktopRendererConfig", header: "ir_desktop_renderer.h".} = object
    backend_type*: DesktopBackendType
    window_title*: cstring
    window_width*: int32
    window_height*: int32
    resizable*: bool
    fullscreen*: bool
    vsync_enabled*: bool
    target_fps*: int32

  DesktopIRRenderer* = ptr object
    # Internal structure defined in ir_desktop_renderer.h

# Renderer Creation and Management
proc desktop_ir_renderer_create*(config: ptr DesktopRendererConfig): DesktopIRRenderer {.importc, cdecl, header: "ir_desktop_renderer.h".}
proc desktop_ir_renderer_destroy*(renderer: DesktopIRRenderer) {.importc, cdecl, header: "ir_desktop_renderer.h".}

# Rendering and Main Loop
proc desktop_ir_renderer_initialize*(renderer: DesktopIRRenderer): bool {.importc, cdecl, header: "ir_desktop_renderer.h".}
proc desktop_ir_renderer_render_frame*(renderer: DesktopIRRenderer; root: ptr IRComponent): bool {.importc, cdecl, header: "ir_desktop_renderer.h".}
proc desktop_ir_renderer_run_main_loop*(renderer: DesktopIRRenderer; root: ptr IRComponent): bool {.importc, cdecl, header: "ir_desktop_renderer.h".}
proc desktop_ir_renderer_stop*(renderer: DesktopIRRenderer) {.importc, cdecl, header: "ir_desktop_renderer.h".}

# Event Handling
proc desktop_ir_renderer_set_event_callback*(renderer: DesktopIRRenderer; callback: DesktopEventCallback; user_data: pointer) {.importc, cdecl, header: "ir_desktop_renderer.h".}

# Resource Management
proc desktop_ir_renderer_load_font*(renderer: DesktopIRRenderer; font_path: cstring; size: cfloat): bool {.importc, cdecl, header: "ir_desktop_renderer.h".}
proc desktop_ir_renderer_load_image*(renderer: DesktopIRRenderer; image_path: cstring): bool {.importc, cdecl, header: "ir_desktop_renderer.h".}
proc desktop_ir_renderer_clear_resources*(renderer: DesktopIRRenderer) {.importc, cdecl, header: "ir_desktop_renderer.h".}
proc desktop_ir_register_font*(name: cstring; path: cstring) {.importc, cdecl, header: "ir_desktop_renderer.h".}
proc desktop_ir_set_default_font*(name: cstring) {.importc, cdecl, header: "ir_desktop_renderer.h".}

# Utility Functions
proc desktop_ir_renderer_validate_ir*(renderer: DesktopIRRenderer; root: ptr IRComponent): bool {.importc, cdecl, header: "ir_desktop_renderer.h".}
proc desktop_ir_renderer_print_tree_info*(renderer: DesktopIRRenderer; root: ptr IRComponent) {.importc, cdecl, header: "ir_desktop_renderer.h".}
proc desktop_ir_renderer_print_performance_stats*(renderer: DesktopIRRenderer) {.importc, cdecl, header: "ir_desktop_renderer.h".}

# Configuration Helpers
proc desktop_renderer_config_default*(): DesktopRendererConfig {.importc, cdecl, header: "ir_desktop_renderer.h".}
proc desktop_renderer_config_sdl3*(width: int32; height: int32; title: cstring): DesktopRendererConfig {.importc, cdecl, header: "ir_desktop_renderer.h".}

# Convenience function for quick rendering
proc desktop_render_ir_component*(root: ptr IRComponent; config: ptr DesktopRendererConfig = nil): bool {.importc, cdecl, header: "ir_desktop_renderer.h".}

# Helper functions to create common configurations
proc newDesktopConfig*(width: int32 = 800; height: int32 = 600; title: string = "Kryon Application"): DesktopRendererConfig =
  result = desktop_renderer_config_sdl3(width, height, cstring(title))
  result.resizable = true
  result.vsync_enabled = true
  result.target_fps = 60

proc newDesktopConfigFullscreen*(): DesktopRendererConfig =
  result = desktop_renderer_config_default()
  result.fullscreen = true
  result.window_title = "Kryon Application"

proc newDesktopConfigFixedSize*(width: int32; height: int32; title: string = "Kryon Application"): DesktopRendererConfig =
  result = newDesktopConfig(width, height, title)
  result.resizable = false
