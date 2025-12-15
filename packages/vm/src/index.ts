/**
 * @kryon/vm - Lightweight TypeScript VM for Kryon bytecode
 *
 * This package provides a minimal VM (~4KB gzipped) that can:
 * - Load and execute .krb bytecode files
 * - Handle events and dispatch to handlers
 * - Manage reactive state
 * - Render UI to the DOM
 *
 * Usage:
 *
 * // Mode 1: Standalone
 * import { createApp } from '@kryon/vm';
 * await createApp('/app.krb', document.getElementById('root'));
 *
 * // Mode 2: Manual control
 * import { KryonVM, render } from '@kryon/vm';
 * const vm = new KryonVM();
 * await vm.loadFromUrl('/app.krb');
 * render(vm, document.getElementById('root'));
 *
 * // Mode 3: Embedded bytecode
 * import { KryonVM } from '@kryon/vm';
 * const vm = new KryonVM();
 * vm.load(base64ToArrayBuffer(EMBEDDED_BYTECODE));
 */

// Core VM
export { KryonVM, VMOptions, RendererInterface, ComponentRef, CallFrame } from './vm';

// Bytecode loader
export {
  parseKRB,
  loadKRB,
  KRBModule,
  KRBHeader,
  KRBSectionHeader,
  KRBFunction,
  KRBEventBinding,
  KRBValue,
  KRBValueType,
  KRB_MAGIC,
  KRB_VERSION_MAJOR,
  KRB_VERSION_MINOR,
} from './loader';

// DOM renderer
export { DOMRenderer, ComponentNode, RenderOptions, render, createApp } from './renderer';

// Opcodes (for debugging/disassembly)
export { Opcode, PropertyID, SectionType, OPCODE_NAMES } from './opcodes';

// ============================================================================
// Convenience functions for browser usage
// ============================================================================

/**
 * Run a Kryon app from a URL
 * This is the simplest way to embed a Kryon app in a page
 */
export async function run(
  url: string,
  container?: HTMLElement | string
): Promise<KryonVM> {
  const { KryonVM } = await import('./vm');
  const { render } = await import('./renderer');

  // Resolve container
  let el: HTMLElement;
  if (typeof container === 'string') {
    const found = document.getElementById(container);
    if (!found) throw new Error(`Container not found: ${container}`);
    el = found;
  } else if (container) {
    el = container;
  } else {
    // Create a full-page container
    el = document.createElement('div');
    el.style.width = '100vw';
    el.style.height = '100vh';
    el.style.margin = '0';
    el.style.padding = '0';
    document.body.appendChild(el);
  }

  const vm = new KryonVM();
  await vm.loadFromUrl(url);
  render(vm, el);

  return vm;
}

/**
 * Create a runtime from embedded bytecode (base64)
 */
export function createRuntime(
  base64Bytecode: string,
  container?: HTMLElement | string
): Promise<KryonVM> {
  return new Promise((resolve, reject) => {
    try {
      // Decode base64
      const binary = atob(base64Bytecode);
      const bytes = new Uint8Array(binary.length);
      for (let i = 0; i < binary.length; i++) {
        bytes[i] = binary.charCodeAt(i);
      }

      import('./vm').then(({ KryonVM }) => {
        import('./renderer').then(({ render }) => {
          const vm = new KryonVM();
          vm.load(bytes.buffer);

          if (container) {
            let el: HTMLElement;
            if (typeof container === 'string') {
              const found = document.getElementById(container);
              if (!found) {
                reject(new Error(`Container not found: ${container}`));
                return;
              }
              el = found;
            } else {
              el = container;
            }
            render(vm, el);
          }

          resolve(vm);
        });
      });
    } catch (e) {
      reject(e);
    }
  });
}

// ============================================================================
// Global Kryon object for <script> tag usage
// ============================================================================

// Expose global API when loaded via script tag
if (typeof window !== 'undefined') {
  (window as any).Kryon = {
    run,
    createRuntime,
    KryonVM: async () => (await import('./vm')).KryonVM,
    render: async () => (await import('./renderer')).render,
  };
}
