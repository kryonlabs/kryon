// Kryon TypeScript Bindings - Main Entry Point

// Re-export types
export type { IRNode, BaseProps, AppConfig, Pointer, Size, Color, Dimension } from './types.ts';
export { ComponentType, DimensionType, Alignment, EventType } from './types.ts';

// Re-export JSX runtime for manual usage
export { jsx, jsxs, jsxDEV, Fragment } from './jsx-runtime.ts';

// Re-export renderer
export { render, renderNode, getHandler } from './renderer.ts';

// Re-export app utilities
export { run, renderOnce, KRYON_RENDERER } from './app.ts';
export type { RunOptions } from './app.ts';

// Re-export FFI for advanced usage
export { ffi, webFfi, cstr, KRYON_WEB_OUTPUT_DIR } from './ffi.ts';
