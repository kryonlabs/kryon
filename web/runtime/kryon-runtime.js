/**
 * @file kryon-runtime.js
 * @brief Kryon Web Runtime - Event handling and Lua integration
 */

// =============================================================================
// KRYON RUNTIME
// =============================================================================

const KryonRuntime = {
    luaState: null,
    eventHandlers: {},

    /**
     * Initialize the Kryon runtime
     */
    init: function() {
        console.log('🚀 Kryon Web Runtime initialized');

        // Initialize Lua VM if fengari is available
        if (typeof fengari !== 'undefined') {
            this.initLuaVM();
        } else {
            console.warn('⚠️ Fengari not loaded - Lua scripts will not work');
        }

        // Setup global event listeners
        this.setupGlobalEvents();
    },

    /**
     * Initialize Lua VM using fengari
     */
    initLuaVM: function() {
        try {
            const lua = fengari.lua;
            const lauxlib = fengari.lauxlib;
            const lualib = fengari.lualib;

            // Create new Lua state
            this.luaState = lauxlib.luaL_newstate();
            lualib.luaL_openlibs(this.luaState);

            // Expose JavaScript functions to Lua
            this.exposeLuaAPI();

            console.log('✅ Lua VM initialized successfully');
        } catch (error) {
            console.error('❌ Failed to initialize Lua VM:', error);
        }
    },

    /**
     * Expose JavaScript API to Lua
     */
    exposeLuaAPI: function() {
        if (!this.luaState) return;

        const lua = fengari.lua;
        const lauxlib = fengari.lauxlib;
        const to_jsstring = fengari.to_jsstring;

        // Override print function to use console.log
        const printFunc = function(L) {
            const nargs = lua.lua_gettop(L);
            const args = [];
            for (let i = 1; i <= nargs; i++) {
                const arg = lauxlib.luaL_tolstring(L, i);
                args.push(to_jsstring(arg));
                lua.lua_pop(L, 1);
            }
            console.log('🐚 [Lua]:', ...args);
            return 0;
        };

        lua.lua_pushcfunction(this.luaState, printFunc);
        lua.lua_setglobal(this.luaState, fengari.to_luastring("print"));
    },

    /**
     * Execute Lua code
     */
    executeLua: function(code) {
        if (!this.luaState) {
            console.warn('⚠️ Lua VM not initialized');
            return false;
        }

        try {
            const lauxlib = fengari.lauxlib;
            const lua = fengari.lua;
            const to_luastring = fengari.to_luastring;

            const status = lauxlib.luaL_dostring(this.luaState, to_luastring(code));

            if (status !== lua.LUA_OK) {
                const error = lua.lua_tojsstring(this.luaState, -1);
                console.error('❌ Lua error:', error);
                lua.lua_pop(this.luaState, 1);
                return false;
            }

            return true;
        } catch (error) {
            console.error('❌ Lua execution error:', error);
            return false;
        }
    },

    /**
     * Call a Lua function by name
     */
    callLuaFunction: function(funcName, ...args) {
        if (!this.luaState) {
            console.warn('⚠️ Lua VM not initialized');
            return;
        }

        try {
            const lua = fengari.lua;
            const to_luastring = fengari.to_luastring;

            // Get function from global scope
            lua.lua_getglobal(this.luaState, to_luastring(funcName));

            if (!lua.lua_isfunction(this.luaState, -1)) {
                console.warn(`⚠️ Lua function '${funcName}' not found`);
                lua.lua_pop(this.luaState, 1);
                return;
            }

            // Push arguments
            for (const arg of args) {
                if (typeof arg === 'number') {
                    lua.lua_pushnumber(this.luaState, arg);
                } else if (typeof arg === 'string') {
                    lua.lua_pushstring(this.luaState, to_luastring(arg));
                } else if (typeof arg === 'boolean') {
                    lua.lua_pushboolean(this.luaState, arg);
                } else {
                    lua.lua_pushnil(this.luaState);
                }
            }

            // Call function
            const status = lua.lua_pcall(this.luaState, args.length, 0, 0);

            if (status !== lua.LUA_OK) {
                const error = lua.lua_tojsstring(this.luaState, -1);
                console.error(`❌ Error calling Lua function '${funcName}':`, error);
                lua.lua_pop(this.luaState, 1);
            }
        } catch (error) {
            console.error(`❌ Error calling Lua function '${funcName}':`, error);
        }
    },

    /**
     * Setup global event listeners
     */
    setupGlobalEvents: function() {
        // Window resize
        window.addEventListener('resize', (e) => {
            this.handleWindowResize(window.innerWidth, window.innerHeight);
        });

        // Keyboard events
        document.addEventListener('keydown', (e) => {
            this.handleKeyDown(e);
        });

        document.addEventListener('keyup', (e) => {
            this.handleKeyUp(e);
        });

        console.log('✅ Global event listeners registered');
    },

    /**
     * Register event handler
     */
    registerHandler: function(elementId, eventType, handler) {
        if (!this.eventHandlers[elementId]) {
            this.eventHandlers[elementId] = {};
        }
        this.eventHandlers[elementId][eventType] = handler;
    },

    /**
     * Window resize handler
     */
    handleWindowResize: function(width, height) {
        console.log(`📐 Window resized: ${width}x${height}`);
        // Trigger resize event handlers if registered
        if (this.eventHandlers['window'] && this.eventHandlers['window']['resize']) {
            this.eventHandlers['window']['resize'](width, height);
        }
    },

    /**
     * Key down handler
     */
    handleKeyDown: function(event) {
        // Trigger key handlers if registered
        if (this.eventHandlers['window'] && this.eventHandlers['window']['keydown']) {
            this.eventHandlers['window']['keydown'](event.key, event);
        }
    },

    /**
     * Key up handler
     */
    handleKeyUp: function(event) {
        // Trigger key handlers if registered
        if (this.eventHandlers['window'] && this.eventHandlers['window']['keyup']) {
            this.eventHandlers['window']['keyup'](event.key, event);
        }
    }
};

// =============================================================================
// ELEMENT EVENT HANDLERS (Called from inline event handlers)
// =============================================================================

function kryonHandleClick(elementId) {
    console.log(`🖱️ Button clicked: ${elementId}`);

    // Call Lua function if handler is registered
    // Convention: element IDs like "myButton" -> Lua function "handleMyButtonClick"
    const funcName = `handle${elementId}Click`;
    KryonRuntime.callLuaFunction(funcName);
}

function kryonHandleInput(elementId, value) {
    console.log(`⌨️ Input changed: ${elementId} = ${value}`);

    // Call Lua function if handler is registered
    const funcName = `handle${elementId}Input`;
    KryonRuntime.callLuaFunction(funcName, value);
}

function kryonHandleCheckbox(elementId, checked) {
    console.log(`☑️ Checkbox changed: ${elementId} = ${checked}`);

    // Call Lua function if handler is registered
    const funcName = `handle${elementId}Change`;
    KryonRuntime.callLuaFunction(funcName, checked);
}

function kryonHandleDropdown(elementId, value) {
    console.log(`📋 Dropdown changed: ${elementId} = ${value}`);

    // Call Lua function if handler is registered
    const funcName = `handle${elementId}Change`;
    KryonRuntime.callLuaFunction(funcName, value);
}

// =============================================================================
// LOAD EMBEDDED LUA SCRIPTS
// =============================================================================

function kryonLoadLuaScripts() {
    const luaScripts = document.querySelectorAll('script[type="text/lua"]');

    luaScripts.forEach((script) => {
        const code = script.textContent;
        console.log(`📜 Loading Lua script (${code.length} bytes)`);
        KryonRuntime.executeLua(code);
    });
}

// =============================================================================
// INITIALIZATION
// =============================================================================

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        KryonRuntime.init();
        kryonLoadLuaScripts();
    });
} else {
    KryonRuntime.init();
    kryonLoadLuaScripts();
}

// Export for module usage
if (typeof module !== 'undefined' && module.exports) {
    module.exports = KryonRuntime;
}