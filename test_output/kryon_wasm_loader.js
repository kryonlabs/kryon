// Kryon WASM Module Loader
// Auto-generated for loading all WASM modules

class KryonWASMLoader {
  constructor() {
    this.modules = new Map();
    this.initialized = false;
  }

  async initialize() {
    if (this.initialized) return;

    // Load kryon_logic_0 module
    if (window.kryon_logic_0WASM) {
      await window.kryon_logic_0WASM.initialize();
      this.modules.set('kryon_logic_0', window.kryon_logic_0WASM);
    }

    this.initialized = true;
    console.log('All Kryon WASM modules loaded');
  }

  getModule(module_id) {
    return this.modules.get(module_id);
  }
}

// Global loader instance
window.kryonWASMLoader = new KryonWASMLoader();
// Auto-initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
  window.kryonWASMLoader.initialize();
});
