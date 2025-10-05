/**
 * @file kryon-runtime.js
 * @brief Kryon Web Runtime - Minimal placeholder (scripting disabled)
 */

// =============================================================================
// KRYON RUNTIME (SCRIPTING DISABLED)
// =============================================================================

const KryonRuntime = {
    eventHandlers: {},

    /**
     * Initialize the Kryon runtime
     */
    init() {
        console.log('🚀 Kryon Web Runtime initialized (scripting disabled)');
        this.setupGlobalEvents();
    },

    /**
     * Setup global event listeners
     */
    setupGlobalEvents() {
        window.addEventListener('resize', () => {
            this.handleWindowResize(window.innerWidth, window.innerHeight);
        });

        document.addEventListener('keydown', (event) => {
            this.handleKeyDown(event);
        });

        document.addEventListener('keyup', (event) => {
            this.handleKeyUp(event);
        });

        console.log('✅ Global event listeners registered');
    },

    /**
     * Register event handler
     */
    registerHandler(elementId, eventType, handler) {
        if (!this.eventHandlers[elementId]) {
            this.eventHandlers[elementId] = {};
        }
        this.eventHandlers[elementId][eventType] = handler;
    },

    /**
     * Window resize handler
     */
    handleWindowResize(width, height) {
        if (this.eventHandlers.window && this.eventHandlers.window.resize) {
            this.eventHandlers.window.resize(width, height);
        }
    },

    /**
     * Key down handler
     */
    handleKeyDown(event) {
        if (this.eventHandlers.window && this.eventHandlers.window.keydown) {
            this.eventHandlers.window.keydown(event.key, event);
        }
    },

    /**
     * Key up handler
     */
    handleKeyUp(event) {
        if (this.eventHandlers.window && this.eventHandlers.window.keyup) {
            this.eventHandlers.window.keyup(event.key, event);
        }
    }
};

// =============================================================================
// ELEMENT EVENT HANDLERS (PLACEHOLDERS)
// =============================================================================

function logDisabledHandler(elementId, eventName) {
    console.warn(`⚠️ Scripting disabled: ${eventName} handler for '${elementId}' not executed.`);
}

function kryonHandleClick(elementId) {
    logDisabledHandler(elementId, 'click');
}

function kryonHandleInput(elementId, value) {
    logDisabledHandler(elementId, 'input');
}

function kryonHandleCheckbox(elementId, checked) {
    logDisabledHandler(elementId, 'checkbox change');
}

function kryonHandleDropdown(elementId, value) {
    logDisabledHandler(elementId, 'dropdown change');
}

// =============================================================================
// INITIALIZATION
// =============================================================================

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        KryonRuntime.init();
    });
} else {
    KryonRuntime.init();
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = KryonRuntime;
}
