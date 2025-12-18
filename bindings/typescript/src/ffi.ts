// Kryon FFI Bindings - Bun native bindings to libkryon C library
import { dlopen, FFIType, suffix, ptr } from 'bun:ffi';
import { resolve, join, dirname } from 'path';
import { existsSync } from 'fs';
import { homedir } from 'os';

// Determine renderer from environment
export const KRYON_RENDERER = process.env.KRYON_RENDERER || 'sdl3';

// Find the library paths based on renderer
function findKryonLibrary(): string {
  if (process.env.KRYON_LIB_PATH) {
    return process.env.KRYON_LIB_PATH;
  }

  const libName = KRYON_RENDERER === 'web' ? 'libkryon_web.so' : 'libkryon_desktop.so';

  // Check standard installation paths
  const searchPaths = [
    join(homedir(), '.local', 'lib', libName),           // User install
    join('/usr', 'local', 'lib', libName),                // System install
    join('/usr', 'lib', libName),                         // System-wide
    join(dirname(dirname(dirname(dirname(import.meta.dir)))), 'build', libName)  // Relative from bindings/typescript/src/
  ];

  for (const path of searchPaths) {
    if (existsSync(path)) {
      return path;
    }
  }

  throw new Error(
    `Could not find ${libName}. Tried:\n` +
    searchPaths.map(p => `  - ${p}`).join('\n') +
    `\n\nSet KRYON_LIB_PATH environment variable to specify library location.`
  );
}

const KRYON_LIB_PATH = findKryonLibrary();

const KRYON_TS_WRAPPER_PATH = process.env.KRYON_TS_WRAPPER_PATH ||
  resolve(import.meta.dir, '../build/libkryon_ts_wrapper.so');

// Web output directory for web renderer
export const KRYON_WEB_OUTPUT_DIR = process.env.KRYON_WEB_OUTPUT_DIR ||
  resolve(import.meta.dir, '../../../../build/web_output');

// Build symbols object based on renderer type
const commonSymbols = {
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

  // ============================================================
  // Flowchart Components
  // ============================================================
  ir_flowchart: {
    args: [FFIType.i32],  // direction (IRFlowchartDirection)
    returns: FFIType.ptr,
  },
  ir_flowchart_node: {
    args: [FFIType.cstring, FFIType.i32, FFIType.cstring],  // node_id, shape, label
    returns: FFIType.ptr,
  },
  ir_flowchart_edge: {
    args: [FFIType.cstring, FFIType.cstring, FFIType.i32],  // from_id, to_id, type
    returns: FFIType.ptr,
  },
  ir_flowchart_subgraph: {
    args: [FFIType.cstring, FFIType.cstring],  // subgraph_id, title
    returns: FFIType.ptr,
  },
  ir_flowchart_edge_set_label: {
    args: [FFIType.ptr, FFIType.cstring],  // edge, label
    returns: FFIType.void,
  },
  ir_flowchart_finalize: {
    args: [FFIType.ptr],  // flowchart component
    returns: FFIType.void,
  },

  // ============================================================
  // IR Serialization
  // ============================================================
  ir_serialize_json_v3: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr],  // root, manifest, logic_block
    returns: FFIType.cstring,  // JSON string
  },
  ir_write_json_v3_file: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.cstring],  // root, manifest, logic_block, filename
    returns: FFIType.bool,
  },
  ir_reactive_manifest_create: {
    args: [],
    returns: FFIType.ptr,
  },
  ir_logic_block_create: {
    args: [],
    returns: FFIType.ptr,
  },

  // ============================================================
  // Markdown Parser (Core)
  // ============================================================
  ir_markdown_parse: {
    args: [FFIType.cstring, FFIType.u64],  // source, length
    returns: FFIType.ptr,  // IRComponent*
  },
};

const desktopSymbols = {
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
};

const webSymbols = {
  // ============================================================
  // Web Renderer
  // ============================================================
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
};

// Combine symbols based on renderer type
const symbols = KRYON_RENDERER === 'web'
  ? { ...commonSymbols, ...webSymbols }
  : { ...commonSymbols, ...desktopSymbols };

// Load the shared libraries
const lib = dlopen(KRYON_LIB_PATH, symbols);

// Load markdown plugin library
let markdownLib: ReturnType<typeof dlopen> | null = null;
try {
  const markdownPluginPath = process.env.KRYON_MARKDOWN_PLUGIN_PATH ||
    join(homedir(), '.local', 'lib', 'libkryon_markdown.so');

  console.log('[FFI] Attempting to load markdown plugin from:', markdownPluginPath);
  if (existsSync(markdownPluginPath)) {
    console.log('[FFI] ✓ File exists, loading...');
    markdownLib = dlopen(markdownPluginPath, {
      kryon_markdown_to_ir: {
        args: [FFIType.cstring, FFIType.u64],
        returns: FFIType.ptr,
      },
    });
    console.log('[FFI] ✓ Markdown plugin loaded successfully!');
  } else {
    console.log('[FFI] ✗ Not found, trying alternate path...');
    // Try relative path from kryon project
    const altPath = join(dirname(dirname(dirname(dirname(import.meta.dir)))),
                         '..', 'kryon-plugin-markdown', 'build', 'libkryon_markdown.so');
    console.log('[FFI] Alternate path:', altPath);
    if (existsSync(altPath)) {
      console.log('[FFI] ✓ Found at alternate path, loading...');
      markdownLib = dlopen(altPath, {
        kryon_markdown_to_ir: {
          args: [FFIType.cstring, FFIType.u64],
          returns: FFIType.ptr,
        },
      });
      console.log('[FFI] ✓ Markdown plugin loaded successfully!');
    } else {
      console.log('[FFI] ✗ Not found at alternate path either');
    }
  }
} catch (e) {
  console.warn('[FFI] ✗ Error loading markdown plugin:', e);
  console.warn('[FFI] Markdown files will not be processed');
}

// Merge markdown symbols into main FFI if loaded
const allSymbols = markdownLib
  ? { ...lib.symbols, ...markdownLib.symbols }
  : lib.symbols;

export const ffi = allSymbols;

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

// Export web FFI symbols (they're already loaded in the main lib when renderer === 'web')
export const webFfi = KRYON_RENDERER === 'web' ? ffi : null;

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
