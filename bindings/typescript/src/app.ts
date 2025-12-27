// Kryon App - Main entry point for running Kryon applications

import { ffi, wrapper, webFfi, cstr, createRendererConfig, KRYON_RENDERER, KRYON_WEB_OUTPUT_DIR } from './ffi.ts';
import { render } from './renderer.ts';
import type { IRNode, AppConfig, Pointer } from './types.ts';
import { ptr } from 'bun:ffi';

export interface RunOptions {
  title?: string;
  width?: number;
  height?: number;
  resizable?: boolean;
  vsync?: boolean;
  targetFps?: number;
  // Web-specific options
  outputDir?: string;
  generateSeparateFiles?: boolean;
  includeJsRuntime?: boolean;
}

const defaultOptions: Required<RunOptions> = {
  title: 'Kryon App',
  width: 800,
  height: 600,
  resizable: true,
  vsync: true,
  targetFps: 60,
  outputDir: KRYON_WEB_OUTPUT_DIR,
  generateSeparateFiles: true,
  includeJsRuntime: true,
};

// Create a renderer config pointer
function createConfig(opts: Required<RunOptions>): Pointer {
  if (wrapper) {
    // Use the C wrapper for proper struct allocation
    const configPtr = wrapper.ts_desktop_renderer_config_sdl3(
      opts.width,
      opts.height,
      cstr(opts.title)
    ) as unknown as Pointer;

    if (configPtr) {
      wrapper.ts_config_set_resizable(configPtr, opts.resizable ? 1 : 0);
      wrapper.ts_config_set_vsync(configPtr, opts.vsync ? 1 : 0);
      wrapper.ts_config_set_target_fps(configPtr, opts.targetFps);
    }

    return configPtr;
  } else {
    // Fallback: manually create the struct in memory
    const configBuffer = createRendererConfig(opts.width, opts.height, opts.title);
    return ptr(configBuffer) as unknown as Pointer;
  }
}

// Run with web renderer - generates HTML/CSS/JS files
function runWeb(rootElement: IRNode, options: Required<RunOptions>): void {
  if (!webFfi) {
    throw new Error('Web renderer not available. Make sure libkryon_web.so is built.');
  }

  // Render JSX tree to IR components
  const rootPtr = render(rootElement);
  if (!rootPtr) {
    throw new Error('Failed to render root element');
  }

  // Create web renderer
  const rendererPtr = webFfi.web_ir_renderer_create() as unknown as Pointer;
  if (!rendererPtr) {
    throw new Error('Failed to create web renderer');
  }

  // Configure renderer
  webFfi.web_ir_renderer_set_output_directory(rendererPtr, cstr(options.outputDir));
  webFfi.web_ir_renderer_set_generate_separate_files(rendererPtr, options.generateSeparateFiles);
  webFfi.web_ir_renderer_set_include_javascript_runtime(rendererPtr, options.includeJsRuntime);

  console.log(`Generating web output: ${options.title}`);
  console.log(`Output directory: ${options.outputDir}`);

  // Render to files
  const success = webFfi.web_ir_renderer_render(rendererPtr, rootPtr);

  // Cleanup
  webFfi.web_ir_renderer_destroy(rendererPtr);

  if (!success) {
    throw new Error('Failed to render web output');
  }

  console.log('Web output generated successfully!');
  console.log(`Open ${options.outputDir}/index.html in a browser to view.`);
}

// Run with desktop renderer (SDL3 or terminal)
function runDesktop(rootElement: IRNode, options: Required<RunOptions>): void {
  // Render JSX tree to IR components
  const rootPtr = render(rootElement);
  if (!rootPtr) {
    throw new Error('Failed to render root element');
  }

  // Create renderer config
  const configPtr = createConfig(options);
  if (!configPtr) {
    throw new Error('Failed to create renderer config');
  }

  // Create renderer
  const rendererPtr = ffi.desktop_ir_renderer_create(configPtr) as unknown as Pointer;
  if (!rendererPtr) {
    if (wrapper) {
      wrapper.ts_desktop_renderer_config_destroy(configPtr);
    }
    throw new Error('Failed to create renderer');
  }

  // Initialize renderer
  const initialized = ffi.desktop_ir_renderer_initialize(rendererPtr);
  if (!initialized) {
    ffi.desktop_ir_renderer_destroy(rendererPtr);
    if (wrapper) {
      wrapper.ts_desktop_renderer_config_destroy(configPtr);
    }
    throw new Error('Failed to initialize renderer');
  }

  console.log(`Starting Kryon app: ${options.title} (${options.width}x${options.height})`);
  console.log(`Renderer: ${KRYON_RENDERER}`);

  // Run main loop (blocks until window closes)
  const success = ffi.desktop_ir_renderer_run_main_loop(rendererPtr, rootPtr);

  // Cleanup
  ffi.desktop_ir_renderer_destroy(rendererPtr);
  if (wrapper) {
    wrapper.ts_desktop_renderer_config_destroy(configPtr);
  }

  if (!success) {
    throw new Error('Application exited with error');
  }
}

// Run a Kryon application with JSX root element
export function run(rootElement: IRNode, options: RunOptions = {}): void {
  const opts = { ...defaultOptions, ...options };

  // Check for serialization mode (TypeScript → .kir pipeline)
  const serializeTarget = process.env.KRYON_SERIALIZE_IR;
  if (serializeTarget) {
    // Serialize to .kir instead of rendering
    const rootPtr = render(rootElement);
    if (!rootPtr) {
      console.error('Failed to render root element');
      process.exit(1);
    }

    // TODO: Reactive manifest support
    const manifest = 0; // null pointer
    // TODO: Logic block support for event handlers
    const logicBlock = 0; // null pointer

    const success = ffi.ir_write_json_file(
      rootPtr,
      manifest,
      logicBlock,
      cstr(serializeTarget)
    );

    if (success) {
      console.log(`Serialized IR to ${serializeTarget}`);
      process.exit(0);
    } else {
      console.error('Failed to serialize IR');
      process.exit(1);
    }
  }

  // Normal rendering path
  if (KRYON_RENDERER === 'web') {
    runWeb(rootElement, opts);
  } else {
    runDesktop(rootElement, opts);
  }
}

// Simple render function for one-shot rendering
export function renderOnce(rootElement: IRNode, options: RunOptions = {}): boolean {
  const opts = { ...defaultOptions, ...options };

  const rootPtr = render(rootElement);
  if (!rootPtr) {
    return false;
  }

  if (KRYON_RENDERER === 'web') {
    if (!webFfi) {
      console.error('Web renderer not available');
      return false;
    }
    return webFfi.web_render_ir_component(rootPtr, cstr(opts.outputDir)) as boolean;
  }

  const configPtr = createConfig(opts);
  if (!configPtr) {
    return false;
  }

  const result = ffi.desktop_render_ir_component(rootPtr, configPtr) as boolean;

  if (wrapper) {
    wrapper.ts_desktop_renderer_config_destroy(configPtr);
  }

  return result;
}

// Export current renderer type
export { KRYON_RENDERER };
