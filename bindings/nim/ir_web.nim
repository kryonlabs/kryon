## Web IR Renderer Bindings for Nim
## Direct bindings to the web backend of the Universal IR system

# Import core IR bindings
import ir_core

# Types from ir_web_renderer.h
type
  WebRendererConfig* = object
    output_directory*: cstring
    generate_javascript*: bool
    generate_wasm*: bool
    minify_output*: bool
    include_source_maps*: bool

  WebIRRenderer* = ptr object
    # Internal structure defined in ir_web_renderer.h

# Renderer Creation and Management
proc web_ir_renderer_create*(config: ptr WebRendererConfig): WebIRRenderer {.importc, cdecl, header: "ir_web_renderer.h".}
proc web_ir_renderer_destroy*(renderer: WebIRRenderer) {.importc, cdecl, header: "ir_web_renderer.h".}

# Rendering Functions
proc web_ir_renderer_render*(renderer: WebIRRenderer; root: IRComponent; output_path: cstring): bool {.importc, cdecl, header: "ir_web_renderer.h".}
proc web_ir_renderer_render_to_memory*(renderer: WebIRRenderer; root: IRComponent): cstring {.importc, cdecl, header: "ir_web_renderer.h".}
proc web_ir_renderer_render_component*(root: IRComponent; output_path: cstring; config: ptr WebRendererConfig = nil): bool {.importc, cdecl, header: "ir_web_renderer.h".}

# WASM Bridge Functions
proc web_ir_renderer_compile_wasm*(nim_source: cstring; output_path: cstring): bool {.importc, cdecl, header: "ir_web_renderer.h".}
proc web_ir_renderer_generate_javascript_glue*(ir_component: IRComponent; output_path: cstring): cstring {.importc, cdecl, header: "ir_web_renderer.h".}

# HTML and CSS Generation
proc web_ir_renderer_generate_html*(root: IRComponent; output_path: cstring): bool {.importc, cdecl, header: "ir_web_renderer.h".}
proc web_ir_renderer_generate_css*(root: IRComponent; output_path: cstring): bool {.importc, cdecl, header: "ir_web_renderer.h".}

# Utility Functions
proc web_ir_renderer_validate_ir*(renderer: WebIRRenderer; root: IRComponent): bool {.importc, cdecl, header: "ir_web_renderer.h".}
proc web_ir_renderer_print_tree_info*(renderer: WebIRRenderer; root: IRComponent) {.importc, cdecl, header: "ir_web_renderer.h".}

# Configuration Helpers
proc web_renderer_config_default*(): WebRendererConfig {.importc, cdecl, header: "ir_web_renderer.h".}

# Convenience function for quick web rendering
proc web_render_ir_component*(root: IRComponent; output_path: cstring = nil; config: ptr WebRendererConfig = nil): bool {.importc, cdecl, header: "ir_web_renderer.h".}

# Helper functions to create common configurations
proc newWebConfig*(output_dir: string = "web_output"; generate_js: bool = true; generate_wasm: bool = true): WebRendererConfig =
  result = web_renderer_config_default()
  result.output_directory = cstring(output_dir)
  result.generate_javascript = generate_js
  result.generate_wasm = generate_wasm
  result.minify_output = false
  result.include_source_maps = true

proc newWebConfigMinified*(output_dir: string = "web_output"; generate_js: bool = true; generate_wasm: bool = true): WebRendererConfig =
  result = web_renderer_config_default()
  result.output_directory = cstring(output_dir)
  result.generate_javascript = generate_js
  result.generate_wasm = generate_wasm
  result.minify_output = true
  result.include_source_maps = false

# High-level convenience functions for web development
proc generateWebApp*(root: IRComponent; output_dir: string = "web_output"): bool =
  ## Generate a complete web application from an IR component tree
  ## This includes HTML, CSS, JavaScript, and optional WASM compilation
  let config = newWebConfig(output_dir)
  result = web_render_ir_component(root, cstring(output_dir & "/index.html"), addr(config))

proc generateWebAppMinimal*(root: IRComponent; output_dir: string = "web_output"): bool =
  ## Generate a minimal web application (HTML + CSS only, no JavaScript/WASM)
  let config = newWebConfig(output_dir, generate_js = false, generate_wasm = false)
  result = web_render_ir_component(root, cstring(output_dir & "/index.html"), addr(config))

proc generateWebAppWASM*(root: IRComponent; output_dir: string = "web_output"): bool =
  ## Generate a web application with WASM support for enhanced interactivity
  let config = newWebConfig(output_dir, generate_js = true, generate_wasm = true)
  result = web_render_ir_component(root, cstring(output_dir & "/index.html"), addr(config))