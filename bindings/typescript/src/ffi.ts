// Kryon FFI Bindings - Bun native bindings to libkryon C library
import { dlopen, FFIType, suffix, ptr } from 'bun:ffi';
import { resolve } from 'path';

// Determine renderer from environment
export const KRYON_RENDERER = process.env.KRYON_RENDERER || 'sdl3';

// Find the library paths based on renderer
const KRYON_LIB_PATH = process.env.KRYON_LIB_PATH ||
  (KRYON_RENDERER === 'web'
    ? resolve(import.meta.dir, '../../../../build/libkryon_web.so')
    : resolve(import.meta.dir, '../../../../build/libkryon_desktop.so'));

const KRYON_TS_WRAPPER_PATH = process.env.KRYON_TS_WRAPPER_PATH ||
  resolve(import.meta.dir, '../build/libkryon_ts_wrapper.so');

// Web output directory for web renderer
export const KRYON_WEB_OUTPUT_DIR = process.env.KRYON_WEB_OUTPUT_DIR ||
  resolve(import.meta.dir, '../../../../build/web_output');

// Load the shared libraries
const lib = dlopen(KRYON_LIB_PATH, {
  // ============================================================
  // Component Creation
  // ============================================================
  ir_create_component: {
    args: [FFIType.i32],  // IRComponentType
    returns: FFIType.ptr, // IRComponent*
  },
  ir_create_component_with_id: {
    args: [FFIType.i32, FFIType.u32],
    returns: FFIType.ptr,
  },
  ir_destroy_component: {
    args: [FFIType.ptr],
    returns: FFIType.void,
  },

  // ============================================================
  // Tree Management
  // ============================================================
  ir_add_child: {
    args: [FFIType.ptr, FFIType.ptr],  // parent, child
    returns: FFIType.void,
  },
  ir_remove_child: {
    args: [FFIType.ptr, FFIType.ptr],
    returns: FFIType.void,
  },
  ir_insert_child: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.u32],  // parent, child, index
    returns: FFIType.void,
  },
  ir_get_child: {
    args: [FFIType.ptr, FFIType.u32],
    returns: FFIType.ptr,
  },

  // ============================================================
  // Component Content
  // ============================================================
  ir_set_text_content: {
    args: [FFIType.ptr, FFIType.cstring],
    returns: FFIType.void,
  },
  ir_set_custom_data: {
    args: [FFIType.ptr, FFIType.cstring],
    returns: FFIType.void,
  },

  // ============================================================
  // Style Management
  // ============================================================
  ir_create_style: {
    args: [],
    returns: FFIType.ptr,
  },
  ir_destroy_style: {
    args: [FFIType.ptr],
    returns: FFIType.void,
  },
  ir_set_style: {
    args: [FFIType.ptr, FFIType.ptr],  // component, style
    returns: FFIType.void,
  },
  ir_get_style: {
    args: [FFIType.ptr],
    returns: FFIType.ptr,
  },

  // Style Properties
  ir_set_width: {
    args: [FFIType.ptr, FFIType.i32, FFIType.f32],  // style, type, value
    returns: FFIType.void,
  },
  ir_set_height: {
    args: [FFIType.ptr, FFIType.i32, FFIType.f32],
    returns: FFIType.void,
  },
  ir_set_visible: {
    args: [FFIType.ptr, FFIType.bool],
    returns: FFIType.void,
  },
  ir_set_background_color: {
    args: [FFIType.ptr, FFIType.u8, FFIType.u8, FFIType.u8, FFIType.u8],  // style, r, g, b, a
    returns: FFIType.void,
  },
  ir_set_border: {
    args: [FFIType.ptr, FFIType.f32, FFIType.u8, FFIType.u8, FFIType.u8, FFIType.u8, FFIType.u8],
    returns: FFIType.void,
  },
  ir_set_margin: {
    args: [FFIType.ptr, FFIType.f32, FFIType.f32, FFIType.f32, FFIType.f32],
    returns: FFIType.void,
  },
  ir_set_padding: {
    args: [FFIType.ptr, FFIType.f32, FFIType.f32, FFIType.f32, FFIType.f32],
    returns: FFIType.void,
  },
  ir_set_font: {
    args: [FFIType.ptr, FFIType.f32, FFIType.cstring, FFIType.u8, FFIType.u8, FFIType.u8, FFIType.u8, FFIType.bool, FFIType.bool],
    returns: FFIType.void,
  },
  ir_set_z_index: {
    args: [FFIType.ptr, FFIType.u32],
    returns: FFIType.void,
  },
  ir_set_opacity: {
    args: [FFIType.ptr, FFIType.f32],
    returns: FFIType.void,
  },

  // ============================================================
  // Layout Management
  // ============================================================
  ir_create_layout: {
    args: [],
    returns: FFIType.ptr,
  },
  ir_destroy_layout: {
    args: [FFIType.ptr],
    returns: FFIType.void,
  },
  ir_set_layout: {
    args: [FFIType.ptr, FFIType.ptr],
    returns: FFIType.void,
  },
  ir_get_layout: {
    args: [FFIType.ptr],
    returns: FFIType.ptr,
  },
  ir_set_flexbox: {
    args: [FFIType.ptr, FFIType.bool, FFIType.u32, FFIType.i32, FFIType.i32],  // layout, wrap, gap, main, cross
    returns: FFIType.void,
  },
  ir_set_flex_properties: {
    args: [FFIType.ptr, FFIType.u8, FFIType.u8, FFIType.u8],  // layout, grow, shrink, direction
    returns: FFIType.void,
  },
  ir_set_min_width: {
    args: [FFIType.ptr, FFIType.i32, FFIType.f32],
    returns: FFIType.void,
  },
  ir_set_min_height: {
    args: [FFIType.ptr, FFIType.i32, FFIType.f32],
    returns: FFIType.void,
  },
  ir_set_max_width: {
    args: [FFIType.ptr, FFIType.i32, FFIType.f32],
    returns: FFIType.void,
  },
  ir_set_max_height: {
    args: [FFIType.ptr, FFIType.i32, FFIType.f32],
    returns: FFIType.void,
  },
  ir_set_justify_content: {
    args: [FFIType.ptr, FFIType.i32],
    returns: FFIType.void,
  },
  ir_set_align_items: {
    args: [FFIType.ptr, FFIType.i32],
    returns: FFIType.void,
  },

  // ============================================================
  // Events
  // ============================================================
  ir_create_event: {
    args: [FFIType.i32, FFIType.cstring, FFIType.cstring],  // type, logic_id, handler_data
    returns: FFIType.ptr,
  },
  ir_destroy_event: {
    args: [FFIType.ptr],
    returns: FFIType.void,
  },
  ir_add_event: {
    args: [FFIType.ptr, FFIType.ptr],  // component, event
    returns: FFIType.void,
  },
  ir_remove_event: {
    args: [FFIType.ptr, FFIType.ptr],
    returns: FFIType.void,
  },

  // ============================================================
  // Desktop Renderer
  // ============================================================
  desktop_render_ir_component: {
    args: [FFIType.ptr, FFIType.ptr],  // root, config
    returns: FFIType.bool,
  },
  desktop_ir_renderer_create: {
    args: [FFIType.ptr],  // config
    returns: FFIType.ptr,
  },
  desktop_ir_renderer_destroy: {
    args: [FFIType.ptr],
    returns: FFIType.void,
  },
  desktop_ir_renderer_initialize: {
    args: [FFIType.ptr],
    returns: FFIType.bool,
  },
  desktop_ir_renderer_run_main_loop: {
    args: [FFIType.ptr, FFIType.ptr],  // renderer, root
    returns: FFIType.bool,
  },

  // ============================================================
  // Web Renderer (only available when using libkryon_web.so)
  // ============================================================
  // Note: These are conditionally available based on which library is loaded
  // web_ir_renderer_create: { args: [], returns: FFIType.ptr },
  // web_ir_renderer_destroy: { args: [FFIType.ptr], returns: FFIType.void },
  // web_ir_renderer_set_output_directory: { args: [FFIType.ptr, FFIType.cstring], returns: FFIType.void },
  // web_ir_renderer_render: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.bool },
  // web_render_ir_component: { args: [FFIType.ptr, FFIType.cstring], returns: FFIType.bool },

  // ============================================================
  // Convenience Component Creators
  // ============================================================
  ir_container: {
    args: [FFIType.cstring],  // tag
    returns: FFIType.ptr,
  },
  ir_text: {
    args: [FFIType.cstring],  // content
    returns: FFIType.ptr,
  },
  ir_button: {
    args: [FFIType.cstring],  // text
    returns: FFIType.ptr,
  },
  ir_input: {
    args: [FFIType.cstring],  // placeholder
    returns: FFIType.ptr,
  },
  ir_row: {
    args: [],
    returns: FFIType.ptr,
  },
  ir_column: {
    args: [],
    returns: FFIType.ptr,
  },
  ir_center: {
    args: [],
    returns: FFIType.ptr,
  },
});

export const ffi = lib.symbols;

// Try to load the TypeScript wrapper library (for struct helpers)
// This is optional - we can fall back to manual struct creation if not available
let wrapperLib: ReturnType<typeof dlopen> | null = null;
try {
  wrapperLib = dlopen(KRYON_TS_WRAPPER_PATH, {
    ts_desktop_renderer_config_sdl3: {
      args: [FFIType.i32, FFIType.i32, FFIType.cstring],
      returns: FFIType.ptr,
    },
    ts_desktop_renderer_config_destroy: {
      args: [FFIType.ptr],
      returns: FFIType.void,
    },
    ts_desktop_renderer_config_default: {
      args: [],
      returns: FFIType.ptr,
    },
    ts_config_set_resizable: {
      args: [FFIType.ptr, FFIType.i32],
      returns: FFIType.void,
    },
    ts_config_set_fullscreen: {
      args: [FFIType.ptr, FFIType.i32],
      returns: FFIType.void,
    },
    ts_config_set_vsync: {
      args: [FFIType.ptr, FFIType.i32],
      returns: FFIType.void,
    },
    ts_config_set_target_fps: {
      args: [FFIType.ptr, FFIType.i32],
      returns: FFIType.void,
    },
  });
} catch (e) {
  console.warn('TypeScript wrapper library not found, using fallback struct creation');
}

export const wrapper = wrapperLib?.symbols ?? null;

// Load web renderer library if in web mode
let webLib: ReturnType<typeof dlopen> | null = null;
if (KRYON_RENDERER === 'web') {
  try {
    const webLibPath = resolve(import.meta.dir, '../../../../build/libkryon_web.so');
    webLib = dlopen(webLibPath, {
      web_ir_renderer_create: {
        args: [],
        returns: FFIType.ptr,
      },
      web_ir_renderer_destroy: {
        args: [FFIType.ptr],
        returns: FFIType.void,
      },
      web_ir_renderer_set_output_directory: {
        args: [FFIType.ptr, FFIType.cstring],
        returns: FFIType.void,
      },
      web_ir_renderer_set_generate_separate_files: {
        args: [FFIType.ptr, FFIType.bool],
        returns: FFIType.void,
      },
      web_ir_renderer_set_include_javascript_runtime: {
        args: [FFIType.ptr, FFIType.bool],
        returns: FFIType.void,
      },
      web_ir_renderer_render: {
        args: [FFIType.ptr, FFIType.ptr],  // renderer, root
        returns: FFIType.bool,
      },
      web_render_ir_component: {
        args: [FFIType.ptr, FFIType.cstring],  // root, output_dir
        returns: FFIType.bool,
      },
    });
    console.log('Web renderer library loaded');
  } catch (e) {
    console.warn('Web renderer library not found:', e);
  }
}

export const webFfi = webLib?.symbols ?? null;

// Helper to convert string to C string pointer
export function toCString(str: string): ReturnType<typeof ptr> {
  const encoder = new TextEncoder();
  const bytes = encoder.encode(str + '\0');
  return ptr(bytes);
}

// Create a C string buffer that won't be garbage collected during the call
export function cstr(str: string): number {
  const buf = Buffer.from(str + '\0', 'utf8');
  return ptr(buf) as unknown as number;
}

// DesktopRendererConfig struct layout (for manual creation if wrapper not available)
// typedef struct DesktopRendererConfig {
//     DesktopBackendType backend_type;  // i32 at offset 0
//     int window_width;                  // i32 at offset 4
//     int window_height;                 // i32 at offset 8
//     const char* window_title;          // ptr at offset 16 (8-byte aligned)
//     bool resizable;                    // bool at offset 24
//     bool fullscreen;                   // bool at offset 25
//     bool vsync_enabled;                // bool at offset 26
//     int target_fps;                    // i32 at offset 28 (4-byte aligned after 3 bools + padding)
// } DesktopRendererConfig;
// Total size: 32 bytes (with padding)

const DESKTOP_BACKEND_SDL3 = 0;

export function createRendererConfig(width: number, height: number, title: string): Uint8Array {
  // Struct size is 32 bytes with proper alignment
  const buffer = new ArrayBuffer(32);
  const view = new DataView(buffer);
  const uint8 = new Uint8Array(buffer);

  // backend_type (i32 at offset 0)
  view.setInt32(0, DESKTOP_BACKEND_SDL3, true); // little-endian

  // window_width (i32 at offset 4)
  view.setInt32(4, width, true);

  // window_height (i32 at offset 8)
  view.setInt32(8, height, true);

  // window_title (ptr at offset 16 - need to store the pointer)
  // We'll store this separately and update
  const titlePtr = cstr(title);
  // Store as 64-bit pointer (BigInt)
  view.setBigUint64(16, BigInt(titlePtr), true);

  // resizable (bool at offset 24)
  uint8[24] = 1;  // true

  // fullscreen (bool at offset 25)
  uint8[25] = 0;  // false

  // vsync_enabled (bool at offset 26)
  uint8[26] = 1;  // true

  // target_fps (i32 at offset 28)
  view.setInt32(28, 60, true);

  return uint8;
}
