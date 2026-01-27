#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <libgen.h>

#include "ir_core.h"
#include "../../ir/src/utils/ir_string_builder.h"
#include "ir_web_renderer.h"
#include "html_generator.h"
#include "css_generator.h"
// Global IR context for source language detection
extern IRContext* g_ir_context;

// Web IR Renderer Context
typedef struct WebIRRenderer {
    HTMLGenerator* html_generator;
    CSSGenerator* css_generator;
    char output_directory[256];
    char* source_directory;  // Directory containing source HTML/assets
    bool generate_separate_files;
    bool include_javascript_runtime;
} WebIRRenderer;

// JavaScript runtime template
static const char* javascript_runtime_template =
"// Kryon Web Runtime\n"
"// Auto-generated JavaScript for IR component interaction\n\n"
"let kryon_components = new Map();\n"
"let kryon_modals = new Map();\n"
"let kryon_lua_ready = false;\n"
"let kryon_call_queue = [];\n"
"let kryon_current_element = null;\n\n"  // Track event target for Lua handlers
"// Lua Handler Bridge - calls embedded Lua handlers by component ID\n"
"// The Lua code (embedded in <script type=\"text/lua\">) registers kryonCallHandler\n"
"function kryonCallHandler(componentId, event) {\n"
"    // Capture the event target for Lua handlers to access\n"
"    if (event && event.target) {\n"
"        kryon_current_element = event.target;\n"
"    }\n"
"    console.log('[Kryon] Call handler for component:', componentId, 'ready:', kryon_lua_ready);\n"
"    if (kryon_lua_ready && window.kryonLuaCallHandler) {\n"
"        window.kryonLuaCallHandler(componentId);\n"
"    } else {\n"
"        // Queue calls until Lua is ready\n"
"        kryon_call_queue.push(componentId);\n"
"        console.log('[Kryon] Lua not ready, queued handler call:', componentId);\n"
"    }\n"
"}\n\n"
"// Called by Lua handlers to get the current event element\n"
"function __kryon_get_event_element() {\n"
"    const el = kryon_current_element;\n"
"    if (!el) return { data: {} };\n"
"    return {\n"
"        data: {\n"
"            date: el.dataset.date || '',\n"
"            habit: el.dataset.habit || '',\n"
"            isCompleted: el.dataset.isCompleted === 'true'\n"
"        },\n"
"        element: el\n"
"    };\n"
"}\n\n"
"// Called by Lua when initialization is complete\n"
"function kryon_lua_init_complete() {\n"
"    kryon_lua_ready = true;\n"
"    console.log('[Kryon] Lua runtime ready');\n"
"    // Process queued handler calls\n"
"    while (kryon_call_queue.length > 0) {\n"
"        const componentId = kryon_call_queue.shift();\n"
"        kryonCallHandler(componentId);\n"
"    }\n"
"    // Initialize dynamic bindings now that Lua is ready\n"
"    if (typeof kryonInitDynamicBindings === 'function') {\n"
"        kryonInitDynamicBindings();\n"
"    }\n"
"}\n\n"
"// ============================================================================\n"
"// Reactive Binding System (Phase 1 Implementation)\n"
"// ============================================================================\n\n"
"// Reactive state registry - stores current state values from Lua\n"
"const kryonReactiveState = {};\n\n"
"// Binding registry - maps state keys to DOM update functions\n"
"const kryonBindingRegistry = {};\n\n"
"// Reactive state change debounce timer\n"
"let kryon_state_change_timer = null;\n"
"let kryon_pending_changes = new Map();\n\n"
"/**\n"
" * Register a DOM element to update when a state key changes\n"
" * @param {string} stateKey - The state path (e.g., 'displayedMonth.month')\n"
" * @param {string} elementId - The DOM element ID\n"
" * @param {string} updateType - 'text', 'style', 'attr', 'class', 'visibility', or 'custom'\n"
" * @param {Object} options - Additional options (styleProp, attrName, customFn)\n"
" */\n"
"function kryonRegisterBinding(stateKey, selector, updateType, options) {\n"
"    if (!kryonBindingRegistry[stateKey]) {\n"
"        kryonBindingRegistry[stateKey] = [];\n"
"    }\n"
"    // Parse options if passed as JSON string from Lua\n"
"    let opts = options || {};\n"
"    if (typeof options === 'string') {\n"
"        try { opts = JSON.parse(options); } catch (e) { opts = {}; }\n"
"    }\n"
"    // Support both elementId (legacy) and selector (new)\n"
"    const selectorValue = selector.startsWith('[') ? selector : '[data-kryon-id=\"' + selector + '\"]';\n"
"    kryonBindingRegistry[stateKey].push({ selector: selectorValue, updateType, options: opts });\n"
"    console.log('[Kryon Reactive] Registered binding:', stateKey, '->', selectorValue, '(' + updateType + ')');\n"
"}\n\n"
"/**\n"
" * Apply a single binding update\n"
" */\n"
"function kryonApplyBinding(binding, value) {\n"
"    const el = binding.selector ? document.querySelector(binding.selector) : null;\n"
"    if (!el) return;\n"
"    switch (binding.updateType) {\n"
"        case 'text': el.textContent = String(value); break;\n"
"        case 'html': el.innerHTML = String(value); break;\n"
"        case 'style': if (binding.options.styleProp) el.style[binding.options.styleProp] = value; break;\n"
"        case 'attr': if (binding.options.attrName) el.setAttribute(binding.options.attrName, value); break;\n"
"        case 'visibility': el.style.display = value ? '' : 'none'; break;\n"
"        case 'class': value ? el.classList.add(binding.options.className) : el.classList.remove(binding.options.className); break;\n"
"        case 'property':\n"
"            // Set DOM element property (e.g., disabled, checked, value)\n"
"            if (binding.options.prop) {\n"
"                el[binding.options.prop] = value;\n"
"                // For disabled, also update visual state\n"
"                if (binding.options.prop === 'disabled') {\n"
"                    el.style.opacity = value ? '0.5' : '1';\n"
"                    el.style.cursor = value ? 'default' : 'pointer';\n"
"                }\n"
"            }\n"
"            break;\n"
"        case 'custom': if (binding.options.customFn && typeof window[binding.options.customFn] === 'function') window[binding.options.customFn](el, value); break;\n"
"        default: el.textContent = String(value);\n"
"    }\n"
"}\n\n"
"/**\n"
" * Called by Lua reactive system when state changes\n"
" * @param {string} key - The state key that changed\n"
" * @param {string} jsonValue - JSON-serialized new value\n"
" */\n"
"function kryonStateChanged(key, jsonValue) {\n"
"    let value = jsonValue;\n"
"    if (typeof jsonValue === 'string') {\n"
"        try {\n"
"            if (jsonValue.startsWith('{') || jsonValue.startsWith('[')) value = JSON.parse(jsonValue);\n"
"            else if (jsonValue === 'true') value = true;\n"
"            else if (jsonValue === 'false') value = false;\n"
"            else if (!isNaN(Number(jsonValue)) && jsonValue !== '') value = Number(jsonValue);\n"
"        } catch (e) { /* Use raw string */ }\n"
"    }\n"
"    kryonReactiveState[key] = value;\n"
"    console.log('[Kryon Reactive] State changed:', key, '=', value);\n"
"    kryon_pending_changes.set(key, value);\n"
"    if (kryon_state_change_timer) clearTimeout(kryon_state_change_timer);\n"
"    kryon_state_change_timer = setTimeout(() => {\n"
"        kryon_pending_changes.forEach((val, k) => {\n"
"            (kryonBindingRegistry[k] || []).forEach(b => kryonApplyBinding(b, val));\n"
"            if (typeof val === 'object' && val !== null) {\n"
"                Object.keys(val).forEach(subKey => {\n"
"                    (kryonBindingRegistry[k + '.' + subKey] || []).forEach(b => kryonApplyBinding(b, val[subKey]));\n"
"                });\n"
"            }\n"
"        });\n"
"        kryon_pending_changes.clear();\n"
"        kryon_state_change_timer = null;\n"
"    }, 16);\n"
"}\n\n"
"// Expose reactive API to window for Lua access\n"
"window.kryonRegisterBinding = kryonRegisterBinding;\n"
"window.kryonStateChanged = kryonStateChanged;\n"
"window.kryonReactiveState = kryonReactiveState;\n\n"
"// ============================================================================\n"
"// ForEach Runtime Expansion\n"
"// ============================================================================\n\n"
"// ForEach template cache\n"
"const kryonForEachTemplates = new Map();\n"
"let kryonCloneIdCounter = 10000;\n\n"
"/**\n"
" * Expand ForEach container with data array\n"
" * @param {string} containerId - ID of the ForEach container element\n"
" * @param {Array} data - Array of data items to render\n"
" * @param {Object} parentContext - Parent context for nested ForEach\n"
" */\n"
"function kryonExpandForEach(containerId, data, parentContext) {\n"
"    const container = document.getElementById(containerId);\n"
"    if (!container) {\n"
"        console.warn('[Kryon ForEach] Container not found:', containerId);\n"
"        return;\n"
"    }\n\n"
"    const itemName = container.dataset.itemName || 'item';\n"
"    const indexName = container.dataset.indexName || 'index';\n\n"
"    // Cache template on first call\n"
"    if (!kryonForEachTemplates.has(containerId)) {\n"
"        const template = container.firstElementChild;\n"
"        if (!template) {\n"
"            console.warn('[Kryon ForEach] No template child in:', containerId);\n"
"            return;\n"
"        }\n"
"        kryonForEachTemplates.set(containerId, template.cloneNode(true));\n"
"    }\n\n"
"    const template = kryonForEachTemplates.get(containerId);\n"
"    container.innerHTML = '';\n\n"
"    if (!Array.isArray(data)) {\n"
"        console.warn('[Kryon ForEach] Data is not an array:', data);\n"
"        return;\n"
"    }\n\n"
"    data.forEach((item, index) => {\n"
"        const clone = template.cloneNode(true);\n"
"        const context = { ...parentContext, [itemName]: item, [indexName]: index };\n\n"
"        // Apply day bindings for calendar days\n"
"        kryonApplyDayBindings(clone, item, context);\n\n"
"        // Handle nested ForEach\n"
"        clone.querySelectorAll('.kryon-forEach').forEach(nested => {\n"
"            const nestedItemName = nested.dataset.itemName || 'item';\n"
"            // If the item is an array (e.g., weekRow containing days), expand it\n"
"            if (Array.isArray(item)) {\n"
"                kryonExpandForEach(nested.id, item, context);\n"
"            }\n"
"        });\n\n"
"        // Update IDs for uniqueness\n"
"        kryonUpdateCloneIds(clone);\n\n"
"        container.appendChild(clone);\n"
"    });\n\n"
"    console.log('[Kryon ForEach] Expanded', containerId, 'with', data.length, 'items');\n"
"}\n\n"
"/**\n"
" * Apply day-specific bindings to a calendar day button\n"
" * @param {Element} element - The cloned template element\n"
" * @param {Object} day - Day data object\n"
" * @param {Object} context - ForEach context with habitIndex etc.\n"
" */\n"
"function kryonApplyDayBindings(element, day, context) {\n"
"    if (!day || typeof day !== 'object') return;\n\n"
"    element.querySelectorAll('button').forEach(btn => {\n"
"        // Text: show day number only for current month\n"
"        btn.textContent = day.isCurrentMonth ? String(day.dayNumber || '') : '';\n\n"
"        // Background color based on state\n"
"        if (day.isCompleted) {\n"
"            btn.style.setProperty('--bg-color', day.themeColor || '#4a90e2');\n"
"            btn.style.backgroundColor = day.themeColor || '#4a90e2';\n"
"        } else if (!day.isCurrentMonth) {\n"
"            btn.style.setProperty('--bg-color', 'rgba(45, 45, 45, 1.00)');\n"
"            btn.style.backgroundColor = 'rgba(45, 45, 45, 1.00)';\n"
"        } else {\n"
"            btn.style.setProperty('--bg-color', 'rgba(61, 61, 61, 1.00)');\n"
"            btn.style.backgroundColor = 'rgba(61, 61, 61, 1.00)';\n"
"        }\n\n"
"        // Today border\n"
"        if (day.isToday && day.isCurrentMonth) {\n"
"            btn.style.border = '2px solid ' + (day.themeColor || '#4a90e2');\n"
"        }\n\n"
"        // Store date and habit index for click handler\n"
"        if (day.date) btn.dataset.date = day.date;\n"
"        if (context && context.habitIndex !== undefined) {\n"
"            btn.dataset.habit = String(context.habitIndex);\n"
"        }\n"
"        btn.dataset.isCompleted = day.isCompleted ? 'true' : 'false';\n\n"
"        // Disabled state\n"
"        btn.disabled = !day.isCurrentMonth || kryonIsDateInFuture(day.date);\n"
"    });\n"
"}\n\n"
"/**\n"
" * Update IDs in a cloned element tree for uniqueness\n"
" * @param {Element} element - The element to update\n"
" */\n"
"function kryonUpdateCloneIds(element) {\n"
"    if (element.id) {\n"
"        element.id = element.id + '-' + (kryonCloneIdCounter++);\n"
"    }\n"
"    Array.from(element.children).forEach(child => {\n"
"        // Skip nested ForEach containers - they'll handle their own IDs\n"
"        if (!child.classList.contains('kryon-forEach')) {\n"
"            kryonUpdateCloneIds(child);\n"
"        }\n"
"    });\n"
"}\n\n"
"/**\n"
" * Check if a date string represents a future date\n"
" * @param {string} dateStr - Date in YYYY-MM-DD format\n"
" * @returns {boolean}\n"
" */\n"
"function kryonIsDateInFuture(dateStr) {\n"
"    if (!dateStr) return false;\n"
"    const today = new Date();\n"
"    today.setHours(0, 0, 0, 0);\n"
"    const date = new Date(dateStr);\n"
"    return date > today;\n"
"}\n\n"
"// Expose ForEach expansion to window/Lua\n"
"window.kryonExpandForEach = kryonExpandForEach;\n\n"
"// ============================================================================\n"
"// Smart Refresh for Non-ForEach Containers\n"
"// ============================================================================\n\n"
"/**\n"
" * Smart refresh that works with any container structure\n"
" * - For ForEach containers: clone template approach\n"
" * - For regular containers: update existing elements in-place\n"
" */\n"
"function kryonSmartRefresh(containerId, data, context) {\n"
"    // Try getElementById first, then fallback to data-kryon-id\n"
"    let container = document.getElementById(containerId);\n"
"    if (!container) {\n"
"        // Extract numeric ID if present\n"
"        const numericId = containerId.replace(/[^0-9]/g, '');\n"
"        if (numericId) {\n"
"            container = document.querySelector('[data-kryon-id=\"' + numericId + '\"]');\n"
"        }\n"
"    }\n"
"    if (!container) {\n"
"        console.warn('[Kryon Refresh] Container not found:', containerId);\n"
"        return;\n"
"    }\n\n"
"    const isForEach = container.classList.contains('kryon-forEach');\n\n"
"    if (isForEach) {\n"
"        kryonExpandForEach(containerId, data, context);\n"
"    } else {\n"
"        kryonRefreshGrid(container, data, context);\n"
"    }\n"
"}\n\n"
"/**\n"
" * Update a grid of elements in-place (for non-ForEach containers)\n"
" * Supports 2D data (rows of columns) or 1D data\n"
" * Computes styles from day properties (isCompleted, isToday, themeColor)\n"
" */\n"
"function kryonRefreshGrid(container, data, context) {\n"
"    // Flatten 2D data to 1D\n"
"    const flatData = [];\n"
"    const is2D = Array.isArray(data) && data.length > 0 && Array.isArray(data[0]);\n\n"
"    if (is2D) {\n"
"        data.forEach((row, rowIdx) => {\n"
"            row.forEach((item, colIdx) => {\n"
"                flatData.push({ item, rowIdx, colIdx });\n"
"            });\n"
"        });\n"
"    } else {\n"
"        data.forEach((item, idx) => flatData.push({ item, rowIdx: 0, colIdx: idx }));\n"
"    }\n\n"
"    // Find all interactive leaf elements (buttons) in the container\n"
"    const buttons = container.querySelectorAll('button, [role=\"button\"]');\n\n"
"    console.log('[Kryon Refresh] Grid update:', container.id || container.dataset.kryonId, 'items:', flatData.length, 'buttons:', buttons.length);\n\n"
"    // Update each button with corresponding data\n"
"    flatData.forEach((entry, idx) => {\n"
"        const btn = buttons[idx];\n"
"        if (!btn) return;\n\n"
"        const day = entry.item;\n"
"        if (!day) return;\n\n"
"        // Update button text\n"
"        btn.textContent = day.isCurrentMonth ? String(day.dayNumber) : '';\n\n"
"        // Compute styles from day properties (matches Calendar.getDayStyle logic)\n"
"        const themeColor = day.themeColor || '#4a90e2';\n"
"        let bgColor = '#3d3d3d';\n"
"        let borderColor = 'transparent';\n\n"
"        if (day.isCompleted) {\n"
"            bgColor = themeColor;\n"
"        } else if (day.isToday && day.isCurrentMonth) {\n"
"            borderColor = themeColor;\n"
"        } else if (!day.isCurrentMonth) {\n"
"            bgColor = '#2d2d2d';\n"
"        }\n\n"
"        btn.style.backgroundColor = bgColor;\n"
"        btn.style.setProperty('--bg-color', bgColor);\n"
"        btn.style.borderColor = borderColor;\n"
"        btn.style.setProperty('--border-color', borderColor);\n\n"
"        // Update disabled state (future dates or non-current month)\n"
"        const isFuture = day.date ? kryonIsDateInFuture(day.date) : false;\n"
"        btn.disabled = isFuture || !day.isCurrentMonth;\n\n"
"        // Store date for click handler\n"
"        btn.dataset.date = day.date || '';\n"
"    });\n"
"}\n\n"
"// Expose to window\n"
"window.kryonSmartRefresh = kryonSmartRefresh;\n"
"window.kryonRefreshGrid = kryonRefreshGrid;\n\n"
"// ============================================================================\n"
"// Smart ForEach Diffing (Phase 5)\n"
"// ============================================================================\n\n"
"// Cache for tracking ForEach data by container ID\n"
"const kryonForEachDataCache = new Map();\n\n"
"/**\n"
" * Smart update ForEach container with diffing\n"
" * Only updates changed elements instead of full re-render\n"
" * @param {string} containerId - ForEach container ID\n"
" * @param {Array} newData - New data array\n"
" * @param {Object} parentContext - Parent context\n"
" */\n"
"function kryonSmartUpdateForEach(containerId, newData, parentContext) {\n"
"    const container = document.getElementById(containerId);\n"
"    if (!container) return;\n\n"
"    const oldData = kryonForEachDataCache.get(containerId) || [];\n"
"    const template = kryonForEachTemplates.get(containerId);\n"
"    const itemName = container.dataset.itemName || 'item';\n"
"    const indexName = container.dataset.indexName || 'index';\n\n"
"    // Simple length-based diffing\n"
"    const oldLen = oldData.length;\n"
"    const newLen = newData.length;\n"
"    const minLen = Math.min(oldLen, newLen);\n\n"
"    // Update existing items\n"
"    for (let i = 0; i < minLen; i++) {\n"
"        const child = container.children[i];\n"
"        if (child && !kryonDeepEqual(oldData[i], newData[i])) {\n"
"            const context = { ...parentContext, [itemName]: newData[i], [indexName]: i };\n"
"            kryonApplyDayBindings(child, newData[i], context);\n"
"        }\n"
"    }\n\n"
"    // Add new items\n"
"    if (newLen > oldLen && template) {\n"
"        for (let i = oldLen; i < newLen; i++) {\n"
"            const clone = template.cloneNode(true);\n"
"            const context = { ...parentContext, [itemName]: newData[i], [indexName]: i };\n"
"            kryonApplyDayBindings(clone, newData[i], context);\n"
"            kryonUpdateCloneIds(clone);\n"
"            container.appendChild(clone);\n"
"        }\n"
"    }\n\n"
"    // Remove extra items\n"
"    if (oldLen > newLen) {\n"
"        while (container.children.length > newLen) {\n"
"            container.removeChild(container.lastElementChild);\n"
"        }\n"
"    }\n\n"
"    // Update cache\n"
"    kryonForEachDataCache.set(containerId, [...newData]);\n"
"    console.log('[Kryon ForEach] Smart updated', containerId, ':', oldLen, '->', newLen);\n"
"}\n\n"
"/**\n"
" * Deep equality check for ForEach item diffing\n"
" */\n"
"function kryonDeepEqual(a, b) {\n"
"    if (a === b) return true;\n"
"    if (typeof a !== typeof b) return false;\n"
"    if (a === null || b === null) return a === b;\n"
"    if (typeof a !== 'object') return false;\n"
"    const keysA = Object.keys(a);\n"
"    const keysB = Object.keys(b);\n"
"    if (keysA.length !== keysB.length) return false;\n"
"    return keysA.every(k => kryonDeepEqual(a[k], b[k]));\n"
"}\n\n"
"// Expose smart update to window/Lua\n"
"window.kryonSmartUpdateForEach = kryonSmartUpdateForEach;\n\n"
"// ============================================================================\n"
"// Dynamic Binding System (Runtime Lua Expression Evaluation)\n"
"// Dependencies are discovered at runtime, not build time.\n"
"// ============================================================================\n\n"
"// Dynamic binding registry - stores bindings with Lua expressions\n"
"const kryonDynamicBindings = {};\n\n"
"/**\n"
" * Register a dynamic binding (dependencies discovered at runtime)\n"
" * @param {Object} config - Binding configuration\n"
" * @param {string} config.id - Unique binding ID\n"
" * @param {string} config.selector - CSS selector for target element\n"
" * @param {string} config.updateType - 'text', 'visibility', 'property:disabled', etc.\n"
" * @param {string} config.luaExpr - Lua expression to evaluate at runtime\n"
" */\n"
"function kryonRegisterDynamicBinding(config) {\n"
"    kryonDynamicBindings[config.id] = {\n"
"        ...config,\n"
"        deps: null,\n"
"        initialized: false\n"
"    };\n"
"    console.log('[Kryon Dynamic] Registered binding:', config.id, '->', config.selector);\n"
"}\n\n"
"/**\n"
" * Evaluate a dynamic binding and discover/cache dependencies\n"
" * @param {Object} binding - The binding object\n"
" * @returns {*} The evaluated result\n"
" */\n"
"function kryonEvalBinding(binding) {\n"
"    if (!window.kryonEvalWithTracking) {\n"
"        console.warn('[Kryon Dynamic] kryonEvalWithTracking not available');\n"
"        return null;\n"
"    }\n"
"    try {\n"
"        const response = window.kryonEvalWithTracking(binding.luaExpr);\n"
"        if (!binding.initialized || !binding.deps) {\n"
"            binding.deps = response.deps || [];\n"
"            binding.initialized = true;\n"
"            console.log('[Kryon Dynamic] Discovered deps for', binding.id, ':', binding.deps);\n"
"        }\n"
"        return response.result;\n"
"    } catch (e) {\n"
"        console.error('[Kryon Dynamic] Eval error:', binding.id, e);\n"
"        return null;\n"
"    }\n"
"}\n\n"
"/**\n"
" * Apply a dynamic binding update to the DOM\n"
" * @param {Object} binding - The binding object\n"
" */\n"
"function kryonApplyDynamicBinding(binding) {\n"
"    const result = kryonEvalBinding(binding);\n"
"    if (result === null || result === undefined) return;\n"
"    const el = document.querySelector(binding.selector);\n"
"    if (!el) {\n"
"        console.warn('[Kryon Dynamic] Element not found:', binding.selector);\n"
"        return;\n"
"    }\n"
"    const [type, prop] = (binding.updateType || 'text').split(':');\n"
"    switch (type) {\n"
"        case 'text':\n"
"            el.textContent = String(result);\n"
"            break;\n"
"        case 'html':\n"
"            el.innerHTML = String(result);\n"
"            break;\n"
"        case 'property':\n"
"            if (prop) {\n"
"                el[prop] = result;\n"
"                if (prop === 'disabled') {\n"
"                    el.style.opacity = result ? '0.5' : '1';\n"
"                    el.style.cursor = result ? 'default' : 'pointer';\n"
"                }\n"
"            }\n"
"            break;\n"
"        case 'style':\n"
"            if (prop) el.style[prop] = result;\n"
"            break;\n"
"        case 'visibility':\n"
"            el.style.display = result ? '' : 'none';\n"
"            break;\n"
"        default:\n"
"            el.textContent = String(result);\n"
"    }\n"
"    console.log('[Kryon Dynamic] Updated', binding.id, '=', result);\n"
"}\n\n"
"/**\n"
" * Initialize all dynamic bindings on page load\n"
" */\n"
"function kryonInitDynamicBindings() {\n"
"    console.log('[Kryon Dynamic] Initializing', Object.keys(kryonDynamicBindings).length, 'bindings');\n"
"    for (const [id, binding] of Object.entries(kryonDynamicBindings)) {\n"
"        kryonApplyDynamicBinding(binding);\n"
"    }\n"
"}\n\n"
"/**\n"
" * Check if a state key matches a dependency\n"
" * @param {string} stateKey - The changed state key\n"
" * @param {string} dep - The dependency path\n"
" * @returns {boolean} True if they match\n"
" */\n"
"function kryonDepMatches(stateKey, dep) {\n"
"    if (stateKey === dep) return true;\n"
"    if (dep.startsWith(stateKey + '.')) return true;\n"
"    if (stateKey.startsWith(dep + '.')) return true;\n"
"    return false;\n"
"}\n\n"
"// Hook into kryonStateChanged to trigger dynamic bindings\n"
"const originalKryonStateChanged = window.kryonStateChanged;\n"
"window.kryonStateChanged = function(key, jsonValue) {\n"
"    // Call original handler for simple bindings\n"
"    if (originalKryonStateChanged) {\n"
"        originalKryonStateChanged(key, jsonValue);\n"
"    }\n"
"    // Trigger dynamic bindings whose deps match\n"
"    for (const [id, binding] of Object.entries(kryonDynamicBindings)) {\n"
"        if (!binding.deps) continue;\n"
"        const matches = binding.deps.some(dep => kryonDepMatches(key, dep));\n"
"        if (matches) {\n"
"            console.log('[Kryon Dynamic] State', key, 'triggers binding:', id);\n"
"            kryonApplyDynamicBinding(binding);\n"
"        }\n"
"    }\n"
"};\n\n"
"// Expose dynamic binding API\n"
"window.kryonRegisterDynamicBinding = kryonRegisterDynamicBinding;\n"
"window.kryonApplyDynamicBinding = kryonApplyDynamicBinding;\n"
"window.kryonInitDynamicBindings = kryonInitDynamicBindings;\n"
"window.kryonDynamicBindings = kryonDynamicBindings;\n\n"
"// ============================================================================\n"
"// Modal Management Functions\n"
"// ============================================================================\n\n"
"function kryon_open_modal(modalId) {\n"
"    const dialog = kryon_modals.get(modalId) || document.getElementById(modalId);\n"
"    if (dialog && !dialog.open) {\n"
"        dialog.showModal();\n"
"        console.log('[Kryon] Opened modal:', modalId);\n"
"    }\n"
"}\n\n"
"function kryon_close_modal(modalId) {\n"
"    const dialog = kryon_modals.get(modalId) || document.getElementById(modalId);\n"
"    if (dialog && dialog.open) {\n"
"        dialog.close();\n"
"        console.log('[Kryon] Closed modal:', modalId);\n"
"    }\n"
"}\n\n"
"function kryon_init_modals() {\n"
"    document.querySelectorAll('dialog.kryon-modal').forEach(dialog => {\n"
"        const id = dialog.id;\n"
"        kryon_modals.set(id, dialog);\n"
"        \n"
"        // Handle backdrop clicks (clicking outside modal content closes it)\n"
"        dialog.addEventListener('click', function(e) {\n"
"            if (e.target === dialog) {\n"
"                dialog.close();\n"
"                console.log('[Kryon] Modal closed via backdrop click:', id);\n"
"            }\n"
"        });\n"
"        \n"
"        // Auto-open modals that have data-modal-open='true'\n"
"        if (dialog.dataset.modalOpen === 'true') {\n"
"            dialog.showModal();\n"
"            console.log('[Kryon] Auto-opened modal:', id);\n"
"        }\n"
"    });\n"
"    \n"
"    // Auto-wire modal triggers (elements with data-opens-modal attribute)\n"
"    document.querySelectorAll('[data-opens-modal]').forEach(trigger => {\n"
"        const modalId = trigger.dataset.opensModal;\n"
"        trigger.addEventListener('click', function() {\n"
"            kryon_open_modal(modalId);\n"
"        });\n"
"    });\n"
"    \n"
"    console.log('[Kryon] Initialized', kryon_modals.size, 'modals');\n"
"}\n\n"
"// Tab Switching Functions\n"
"function kryonSelectTab(tabGroup, index) {\n"
"    if (!tabGroup) return;\n"
"    \n"
"    // Update selected index\n"
"    tabGroup.dataset.selected = index;\n"
"    \n"
"    // Update tab buttons\n"
"    const tabs = tabGroup.querySelectorAll('[role=\"tab\"]');\n"
"    tabs.forEach((tab, i) => {\n"
"        tab.setAttribute('aria-selected', i === index ? 'true' : 'false');\n"
"    });\n"
"    \n"
"    // Update panels\n"
"    const panels = tabGroup.querySelectorAll('[role=\"tabpanel\"]');\n"
"    panels.forEach((panel, i) => {\n"
"        if (i === index) {\n"
"            panel.removeAttribute('hidden');\n"
"        } else {\n"
"            panel.setAttribute('hidden', '');\n"
"        }\n"
"    });\n"
"    \n"
"    // Call Lua handler if tab has a component ID\n"
"    const selectedTab = tabs[index];\n"
"    if (selectedTab && selectedTab.dataset.componentId) {\n"
"        kryonCallHandler(parseInt(selectedTab.dataset.componentId));\n"
"    }\n"
"    \n"
"    console.log('[Kryon] Tab selected:', index);\n"
"}\n\n"
"// Keyboard navigation for tabs\n"
"document.addEventListener('keydown', function(e) {\n"
"    if (e.target.matches('[role=\"tab\"]')) {\n"
"        const tablist = e.target.closest('[role=\"tablist\"]');\n"
"        if (!tablist) return;\n"
"        const tabs = Array.from(tablist.querySelectorAll('[role=\"tab\"]'));\n"
"        const index = tabs.indexOf(e.target);\n"
"        let newIndex = index;\n"
"        \n"
"        if (e.key === 'ArrowRight') newIndex = (index + 1) % tabs.length;\n"
"        else if (e.key === 'ArrowLeft') newIndex = (index - 1 + tabs.length) % tabs.length;\n"
"        else if (e.key === 'Home') newIndex = 0;\n"
"        else if (e.key === 'End') newIndex = tabs.length - 1;\n"
"        else return;\n"
"        \n"
"        e.preventDefault();\n"
"        tabs[newIndex].click();\n"
"        tabs[newIndex].focus();\n"
"    }\n"
"});\n\n"
"// ForEach containers registry\n"
"const kryon_forEach_containers = new Map();\n\n"
"// Initialize ForEach containers - finds and stores all ForEach elements\n"
"function kryon_init_forEach() {\n"
"    document.querySelectorAll('.kryon-forEach').forEach(container => {\n"
"        if (container.id) {\n"
"            kryon_forEach_containers.set(container.id, {\n"
"                element: container,\n"
"                eachSource: container.dataset.eachSource || '',\n"
"                itemName: container.dataset.itemName || 'item',\n"
"                indexName: container.dataset.indexName || 'index'\n"
"            });\n"
"        }\n"
"    });\n"
"    console.log('[Kryon] Initialized', kryon_forEach_containers.size, 'ForEach containers');\n"
"}\n\n"
"// Get list of ForEach container IDs for Lua to iterate\n"
"window.kryonGetForEachContainers = function() {\n"
"    return Array.from(kryon_forEach_containers.keys());\n"
"};\n\n"
"// Initialize when DOM is ready\n"
"document.addEventListener('DOMContentLoaded', function() {\n"
"    console.log('Kryon Web Runtime initialized');\n"
"    \n"
"    // Find all Kryon components (buttons, inputs, etc. with IDs)\n"
"    const elements = document.querySelectorAll('[id]');\n"
"    elements.forEach(element => {\n"
"        // Extract numeric ID from btn-1, input-2, etc.\n"
"        const match = element.id.match(/(\\d+)$/);\n"
"        if (match) {\n"
"            const id = parseInt(match[1]);\n"
"            kryon_components.set(id, element);\n"
"        }\n"
"    });\n"
"    \n"
"    console.log('Found', kryon_components.size, 'Kryon components');\n"
"    \n"
"    // Initialize modals\n"
"    kryon_init_modals();\n"
"    \n"
"    // Initialize ForEach containers\n"
"    kryon_init_forEach();\n"
"});\n\n";

WebIRRenderer* web_ir_renderer_create() {
    WebIRRenderer* renderer = malloc(sizeof(WebIRRenderer));
    if (!renderer) return NULL;

    renderer->html_generator = html_generator_create();
    renderer->css_generator = css_generator_create();

    if (!renderer->html_generator || !renderer->css_generator) {
        if (renderer->html_generator) html_generator_destroy(renderer->html_generator);
        if (renderer->css_generator) css_generator_destroy(renderer->css_generator);
        free(renderer);
        return NULL;
    }

    strcpy(renderer->output_directory, ".");
    renderer->source_directory = NULL;
    renderer->generate_separate_files = true;
    renderer->include_javascript_runtime = true;

    return renderer;
}

void web_ir_renderer_destroy(WebIRRenderer* renderer) {
    if (!renderer) return;

    if (renderer->html_generator) {
        html_generator_destroy(renderer->html_generator);
    }
    if (renderer->css_generator) {
        css_generator_destroy(renderer->css_generator);
    }
    if (renderer->source_directory) {
        free(renderer->source_directory);
    }
    free(renderer);
}

void web_ir_renderer_set_output_directory(WebIRRenderer* renderer, const char* directory) {
    if (!renderer || !directory) return;

    strncpy(renderer->output_directory, directory, sizeof(renderer->output_directory) - 1);
    renderer->output_directory[sizeof(renderer->output_directory) - 1] = '\0';
}

void web_ir_renderer_set_source_directory(WebIRRenderer* renderer, const char* directory) {
    if (!renderer) return;

    if (renderer->source_directory) {
        free(renderer->source_directory);
        renderer->source_directory = NULL;
    }

    if (directory) {
        renderer->source_directory = strdup(directory);
    }
}

void web_ir_renderer_set_generate_separate_files(WebIRRenderer* renderer, bool separate) {
    if (!renderer) return;
    renderer->generate_separate_files = separate;
}

void web_ir_renderer_set_include_javascript_runtime(WebIRRenderer* renderer, bool include) {
    if (!renderer) return;
    renderer->include_javascript_runtime = include;
}

static bool generate_javascript_runtime(WebIRRenderer* renderer) {
    if (!renderer || !renderer->include_javascript_runtime) return true;

    char js_filename[512];
    snprintf(js_filename, sizeof(js_filename), "%s/kryon.js", renderer->output_directory);

    FILE* js_file = fopen(js_filename, "w");
    if (!js_file) return false;

    bool success = (fprintf(js_file, "%s", javascript_runtime_template) >= 0);
    fclose(js_file);

    return success;
}

// Collect image src paths from component tree
static void collect_assets(IRComponent* component, char** assets, int* count, int max) {
    if (!component) return;

    // Check for image component
    if (component->type == IR_COMPONENT_IMAGE && component->custom_data) {
        const char* src = (const char*)component->custom_data;

        // Skip external URLs (http://, https://, //, data:)
        if (strncmp(src, "http://", 7) == 0 ||
            strncmp(src, "https://", 8) == 0 ||
            strncmp(src, "//", 2) == 0 ||
            strncmp(src, "data:", 5) == 0) {
            // External URL - skip
        } else if (*count < max) {
            // Check if we already have this asset
            bool duplicate = false;
            for (int i = 0; i < *count; i++) {
                if (strcmp(assets[i], src) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                // Relative path - add to list
                assets[(*count)++] = strdup(src);
            }
        }
    }

    // Recurse into children
    for (int i = 0; i < (int)component->child_count; i++) {
        collect_assets(component->children[i], assets, count, max);
    }
}

// Create parent directories recursively
static bool create_parent_dirs(const char* path) {
    char* path_copy = strdup(path);
    char* dir = dirname(path_copy);

    // Use mkdir -p equivalent
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dir);
    int ret = system(cmd);

    free(path_copy);
    return ret == 0;
}

// Copy single asset preserving directory structure
static bool copy_asset(const char* source_dir, const char* output_dir, const char* asset_path) {
    char src_full[2048];
    char dst_full[2048];

    snprintf(src_full, sizeof(src_full), "%s/%s", source_dir, asset_path);
    snprintf(dst_full, sizeof(dst_full), "%s/%s", output_dir, asset_path);

    // Check if source exists
    struct stat st;
    if (stat(src_full, &st) != 0) {
        fprintf(stderr, "[ASSET] Source not found: %s\n", src_full);
        return false;
    }

    // Create parent directories in output
    create_parent_dirs(dst_full);

    // Copy file
    // Buffer is 8192 bytes: worst case is 3 + 2048 + 2 + 2048 + 1 = 4102, well within limit
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", src_full, dst_full);
    int ret = system(cmd);

    if (ret == 0) {
        printf("✅ Copied asset: %s\n", asset_path);
    }
    return ret == 0;
}

// Copy all collected assets from source to output directory
static void copy_collected_assets(WebIRRenderer* renderer, IRComponent* root) {
    if (!renderer->source_directory) return;

    char* assets[256];
    int asset_count = 0;

    // Collect all image references
    collect_assets(root, assets, &asset_count, 256);

    if (asset_count > 0) {
        printf("📦 Copying %d assets...\n", asset_count);
    }

    // Copy each asset
    for (int i = 0; i < asset_count; i++) {
        copy_asset(renderer->source_directory,
                  renderer->output_directory,
                  assets[i]);
        free(assets[i]);
    }
}

bool web_ir_renderer_render(WebIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    printf("🌐 Rendering IR component tree to web format...\n");

    // Create output directory if it doesn't exist
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", renderer->output_directory);
    if (system(mkdir_cmd) != 0) {
        printf("Warning: Could not create output directory '%s'\n", renderer->output_directory);
    }

    // Pass source metadata to HTML generator so it knows to include Fengari
    if (g_ir_context && g_ir_context->source_metadata) {
        html_generator_set_metadata(renderer->html_generator, g_ir_context->source_metadata);
    }

    // Generate HTML
    if (renderer->generate_separate_files) {
        char html_filename[512];
        snprintf(html_filename, sizeof(html_filename), "%s/index.html", renderer->output_directory);

        if (!html_generator_write_to_file(renderer->html_generator, root, html_filename)) {
            printf("❌ Failed to write HTML file\n");
            return false;
        }
        printf("✅ Generated HTML: %s\n", html_filename);
    }

    // Generate CSS (skip if embedded in HTML)
    if (renderer->generate_separate_files && !renderer->html_generator->options.embedded_css) {
        char css_filename[512];
        snprintf(css_filename, sizeof(css_filename), "%s/kryon.css", renderer->output_directory);

        if (!css_generator_write_to_file(renderer->css_generator, root, css_filename)) {
            printf("❌ Failed to write CSS file\n");
            return false;
        }
        printf("✅ Generated CSS: %s\n", css_filename);
    } else if (renderer->html_generator->options.embedded_css) {
        printf("✅ CSS embedded in HTML\n");
    }

    // Generate JavaScript runtime
    if (!generate_javascript_runtime(renderer)) {
        printf("❌ Failed to write JavaScript runtime\n");
        return false;
    }
    printf("✅ Generated JavaScript: %s/kryon.js\n", renderer->output_directory);

    // Copy referenced assets (images, etc.)
    copy_collected_assets(renderer, root);

    // Generate statistics
    const char* html_buffer = html_generator_get_buffer(renderer->html_generator);
    const char* css_buffer = css_generator_get_buffer(renderer->css_generator);

    if (html_buffer && css_buffer) {
        printf("📊 Generation Statistics:\n");
        printf("   HTML size: %zu bytes\n", html_generator_get_size(renderer->html_generator));
        printf("   CSS size: %zu bytes\n", css_generator_get_size(renderer->css_generator));
        printf("   Output directory: %s\n", renderer->output_directory);
    }

    printf("🎉 Web rendering complete!\n");
    return true;
}

bool web_ir_renderer_render_to_memory(WebIRRenderer* renderer, IRComponent* root,
                                       char** html_output, char** css_output, char** js_output) {
    if (!renderer || !root) return false;

    // Generate HTML to memory
    if (html_output) {
        const char* html = html_generator_generate(renderer->html_generator, root);
        *html_output = html ? strdup(html) : NULL;
    }

    // Generate CSS to memory
    if (css_output) {
        const char* css = css_generator_generate(renderer->css_generator, root);
        *css_output = css ? strdup(css) : NULL;
    }

    // Generate JavaScript runtime to memory
    if (js_output && renderer->include_javascript_runtime) {
        *js_output = strdup(javascript_runtime_template);
    }

    return true;
}

bool web_ir_renderer_validate_ir(WebIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    printf("🔍 Validating IR component tree...\n");

    // Basic validation
    if (!ir_validate_component(root)) {
        printf("❌ IR validation failed\n");
        return false;
    }

    printf("✅ IR validation passed\n");
    return true;
}

void web_ir_renderer_print_tree_info(WebIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    printf("🌳 IR Component Tree Information:\n");
    ir_print_component_info(root, 0);
}

// Utility functions for debugging
void web_ir_renderer_print_stats(WebIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    printf("📈 Web Renderer Statistics:\n");
    printf("   Output directory: %s\n", renderer->output_directory);
    printf("   Separate files: %s\n", renderer->generate_separate_files ? "Yes" : "No");
    printf("   JS Runtime: %s\n", renderer->include_javascript_runtime ? "Yes" : "No");
    printf("   HTML Generator: %s\n", renderer->html_generator ? "Created" : "Failed");
    printf("   CSS Generator: %s\n", renderer->css_generator ? "Created" : "Failed");
}

// Convenience function for quick rendering
bool web_render_ir_component(IRComponent* root, const char* output_dir) {
    return web_render_ir_component_with_options(root, output_dir, false);
}

// Convenience function with options
bool web_render_ir_component_with_options(IRComponent* root, const char* output_dir, bool embedded_css) {
    return web_render_ir_component_with_manifest(root, output_dir, embedded_css, NULL);
}

// Convenience function with manifest (for CSS variable support)
bool web_render_ir_component_with_manifest(IRComponent* root, const char* output_dir,
                                           bool embedded_css, IRReactiveManifest* manifest) {
    WebIRRenderer* renderer = web_ir_renderer_create();
    if (!renderer) return false;

    if (output_dir) {
        web_ir_renderer_set_output_directory(renderer, output_dir);
    }

    // Set embedded CSS option on the HTML generator
    if (embedded_css && renderer->html_generator) {
        renderer->html_generator->options.embedded_css = true;
    }

    // Set manifest on both generators for CSS variable output
    if (manifest) {
        if (renderer->css_generator) {
            css_generator_set_manifest(renderer->css_generator, manifest);
        }
        // Also set on HTML generator for embedded CSS mode
        if (renderer->html_generator) {
            html_generator_set_manifest(renderer->html_generator, manifest);
        }
    }

    bool success = web_ir_renderer_render(renderer, root);
    web_ir_renderer_destroy(renderer);

    return success;
}

// Set manifest on renderer (for CSS variables and reactive bindings)
void web_ir_renderer_set_manifest(WebIRRenderer* renderer, IRReactiveManifest* manifest) {
    if (!renderer || !manifest) return;

    // Pass to CSS generator for CSS variable support
    if (renderer->css_generator) {
        css_generator_set_manifest(renderer->css_generator, manifest);
    }

    // Pass to HTML generator for reactive binding serialization
    if (renderer->html_generator) {
        html_generator_set_manifest(renderer->html_generator, manifest);
    }
}

// ============================================================================
// JavaScript Reactive System Generation (Phase 2)
// ============================================================================

// Helper: escape string for JavaScript
static void js_escape_string(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return;

    size_t di = 0;
    for (size_t si = 0; src[si] && di < dest_size - 1; si++) {
        char c = src[si];
        if (c == '\\' || c == '"' || c == '\'' || c == '\n' || c == '\r' || c == '\t') {
            if (di + 2 >= dest_size) break;
            dest[di++] = '\\';
            switch (c) {
                case '\n': dest[di++] = 'n'; break;
                case '\r': dest[di++] = 'r'; break;
                case '\t': dest[di++] = 't'; break;
                default: dest[di++] = c; break;
            }
        } else {
            dest[di++] = c;
        }
    }
    dest[di] = '\0';
}

// Helper: get JS type name for reactive var type
static const char* js_type_for_reactive_var(IRReactiveVarType type) {
    switch (type) {
        case IR_REACTIVE_TYPE_INT: return "number";
        case IR_REACTIVE_TYPE_FLOAT: return "number";
        case IR_REACTIVE_TYPE_STRING: return "string";
        case IR_REACTIVE_TYPE_BOOL: return "boolean";
        case IR_REACTIVE_TYPE_CUSTOM: return "object";
        default: return "any";
    }
}

// Helper: get JS default value for reactive var type
static const char* js_default_for_reactive_var(IRReactiveVarType type) {
    switch (type) {
        case IR_REACTIVE_TYPE_INT: return "0";
        case IR_REACTIVE_TYPE_FLOAT: return "0.0";
        case IR_REACTIVE_TYPE_STRING: return "\"\"";
        case IR_REACTIVE_TYPE_BOOL: return "false";
        case IR_REACTIVE_TYPE_CUSTOM: return "{}";
        default: return "null";
    }
}

// Helper: get JS binding type string
static const char* js_binding_type_string(IRBindingType type) {
    switch (type) {
        case IR_BINDING_TEXT: return "textContent";
        case IR_BINDING_CONDITIONAL: return "visibility";
        case IR_BINDING_ATTRIBUTE: return "attribute";
        case IR_BINDING_FOR_LOOP: return "forEach";
        case IR_BINDING_CUSTOM: return "custom";
        default: return "unknown";
    }
}

// Generate JavaScript reactive system from manifest
char* generate_js_reactive_system(IRReactiveManifest* manifest) {
    // Use IRStringBuilder for dynamic allocation to handle large JSON values
    IRStringBuilder* sb = ir_sb_create(8192);
    if (!sb) return NULL;

    // Header
    ir_sb_append(sb,
        "// Kryon Reactive System (Generated from IRReactiveManifest)\n"
        "// This file provides reactive state management for the web app\n"
        "// State changes automatically update the DOM via bindings\n\n"
    );

    // If no manifest or empty, generate stub
    if (!manifest || manifest->variable_count == 0) {
        ir_sb_append(sb,
            "// No reactive variables found in manifest\n"
            "const kryonState = {};\n"
            "const kryonBindings = {};\n\n"
            "// Stub update function\n"
            "function kryonUpdateBindings(prop) {\n"
            "  console.log('[Kryon Reactive] No bindings configured for:', prop);\n"
            "}\n\n"
            "// Export for Lua bridge\n"
            "window.kryonState = kryonState;\n"
            "window.kryonUpdateBindings = kryonUpdateBindings;\n"
        );
        char* result = ir_sb_build(sb);
        char* output = strdup(result);
        ir_sb_free(sb);
        return output;
    }

    // Generate state object from manifest.variables
    ir_sb_append(sb,
        "// Reactive state (from IRReactiveManifest.variables)\n"
        "const kryonStateData = {\n"
    );

    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        IRReactiveVarDescriptor* var = &manifest->variables[i];
        if (!var->name) continue;

        // Get initial value - use ir_sb_appendf for dynamic allocation
        if (var->initial_value_json && strlen(var->initial_value_json) > 0) {
            // Use provided initial value JSON (can be arbitrarily large)
            ir_sb_appendf(sb, "  %s: %s,\n", var->name, var->initial_value_json);
        } else {
            // Use default based on type
            switch (var->type) {
                case IR_REACTIVE_TYPE_INT:
                    ir_sb_appendf(sb, "  %s: %ld,\n", var->name, (long)var->value.as_int);
                    break;
                case IR_REACTIVE_TYPE_FLOAT:
                    ir_sb_appendf(sb, "  %s: %f,\n", var->name, var->value.as_float);
                    break;
                case IR_REACTIVE_TYPE_STRING:
                    if (var->value.as_string) {
                        char escaped[256];
                        js_escape_string(escaped, var->value.as_string, sizeof(escaped));
                        ir_sb_appendf(sb, "  %s: \"%s\",\n", var->name, escaped);
                    } else {
                        ir_sb_appendf(sb, "  %s: \"\",\n", var->name);
                    }
                    break;
                case IR_REACTIVE_TYPE_BOOL:
                    ir_sb_appendf(sb, "  %s: %s,\n", var->name, var->value.as_bool ? "true" : "false");
                    break;
                default:
                    ir_sb_appendf(sb, "  %s: %s,\n", var->name, js_default_for_reactive_var(var->type));
                    break;
            }
        }
    }

    ir_sb_append(sb, "};\n\n");

    // Generate bindings map from manifest.bindings
    ir_sb_append(sb,
        "// DOM bindings (from IRReactiveManifest.bindings)\n"
        "const kryonBindings = {\n"
    );

    // Group bindings by variable ID
    // First, create a map from var_id to var_name
    char var_names[64][64];  // Up to 64 variables
    for (uint32_t i = 0; i < manifest->variable_count && i < 64; i++) {
        if (manifest->variables[i].name) {
            strncpy(var_names[manifest->variables[i].id], manifest->variables[i].name, 63);
            var_names[manifest->variables[i].id][63] = '\0';
        }
    }

    // Track which variables have bindings
    bool has_bindings[64] = {false};
    for (uint32_t i = 0; i < manifest->binding_count; i++) {
        uint32_t var_id = manifest->bindings[i].reactive_var_id;
        if (var_id < 64) has_bindings[var_id] = true;
    }

    // Generate bindings grouped by variable
    for (uint32_t v = 0; v < manifest->variable_count && v < 64; v++) {
        uint32_t var_id = manifest->variables[v].id;
        if (!has_bindings[var_id]) continue;

        const char* var_name = manifest->variables[v].name;
        if (!var_name) continue;

        ir_sb_appendf(sb, "  '%s': [\n", var_name);

        for (uint32_t i = 0; i < manifest->binding_count; i++) {
            IRReactiveBinding* binding = &manifest->bindings[i];
            if (binding->reactive_var_id != var_id) continue;

            // Use data-kryon-id attribute selector for reliable element lookups
            ir_sb_appendf(sb, "    { selector: '[data-kryon-id=\"%u\"]', type: '%s'%s },\n",
                     binding->component_id,
                     js_binding_type_string(binding->binding_type),
                     binding->expression ? ", expr: true" : "");
        }

        ir_sb_append(sb, "  ],\n");
    }

    ir_sb_append(sb, "};\n\n");

    // Generate reactive proxy
    ir_sb_append(sb,
        "// Reactive proxy for automatic DOM updates\n"
        "const kryonState = new Proxy(kryonStateData, {\n"
        "  set(target, prop, value) {\n"
        "    const oldValue = target[prop];\n"
        "    target[prop] = value;\n"
        "    if (oldValue !== value) {\n"
        "      kryonUpdateBindings(prop, value, oldValue);\n"
        "    }\n"
        "    return true;\n"
        "  },\n"
        "  get(target, prop) {\n"
        "    return target[prop];\n"
        "  }\n"
        "});\n\n"
    );

    // Generate update function
    ir_sb_append(sb,
        "// Update DOM elements bound to a reactive property\n"
        "function kryonUpdateBindings(prop, newValue, oldValue) {\n"
        "  console.log('[Kryon Reactive] State changed:', prop, '=', newValue);\n"
        "  const bindings = kryonBindings[prop] || [];\n"
        "  bindings.forEach(binding => {\n"
        "    // Use querySelector with data-kryon-id selector for reliable lookups\n"
        "    const el = binding.selector ? document.querySelector(binding.selector) : null;\n"
        "    if (!el) {\n"
        "      console.warn('[Kryon Reactive] Element not found:', binding.selector);\n"
        "      return;\n"
        "    }\n"
        "    console.log('[Kryon Reactive] Updating element', binding.selector, 'with', newValue);\n"
        "    switch (binding.type) {\n"
        "      case 'text':\n"
        "      case 'textContent':\n"
        "        el.textContent = String(newValue);\n"
        "        break;\n"
        "      case 'visibility':\n"
        "        el.style.display = newValue ? '' : 'none';\n"
        "        break;\n"
        "      case 'attribute':\n"
        "        // binding.attr specifies which attribute to update\n"
        "        if (binding.attr) el.setAttribute(binding.attr, newValue);\n"
        "        break;\n"
        "      case 'forEach':\n"
        "        // ForEach requires re-rendering the list\n"
        "        console.log('[Kryon Reactive] ForEach update for', prop);\n"
        "        break;\n"
        "      default:\n"
        "        // Default to text update\n"
        "        el.textContent = String(newValue);\n"
        "    }\n"
        "  });\n"
        "}\n\n"
    );

    // Generate Lua bridge functions
    ir_sb_append(sb,
        "// Lua bridge: Allow Lua handlers to access/modify state\n"
        "window.kryonState = kryonState;\n"
        "window.kryonUpdateBindings = kryonUpdateBindings;\n\n"
        "// Helper for Lua to get state\n"
        "window.kryonGetState = function(key) {\n"
        "  return kryonState[key];\n"
        "};\n\n"
        "// Helper for Lua to set state\n"
        "window.kryonSetState = function(key, value) {\n"
        "  kryonState[key] = value;\n"
        "};\n\n"
        "console.log('[Kryon Reactive] Reactive system initialized with',\n"
        "            Object.keys(kryonStateData).length, 'variables');\n"
    );

    // Build and return output
    char* result = ir_sb_build(sb);
    char* output = strdup(result);
    ir_sb_free(sb);
    return output;
}

// Write JavaScript reactive system to output directory
bool write_js_reactive_system(const char* output_dir, IRReactiveManifest* manifest) {
    if (!output_dir) return false;

    char* js_code = generate_js_reactive_system(manifest);
    if (!js_code) return false;

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/kryon-reactive.js", output_dir);

    FILE* f = fopen(filepath, "w");
    if (!f) {
        free(js_code);
        return false;
    }

    fprintf(f, "%s", js_code);
    fclose(f);

    printf("✅ Generated JavaScript reactive system: %s\n", filepath);
    free(js_code);
    return true;
}