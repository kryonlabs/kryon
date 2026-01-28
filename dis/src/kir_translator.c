/**
 * KIR to DIS Translator
 *
 * Translates KIR (Kryon Intermediate Representation) to DIS bytecode.
 * Parses KIR JSON using cJSON and generates TaijiOS DIS instructions.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "cJSON.h"
#include "internal.h"
#include "../include/dis_codegen.h"

// ============================================================================
// KIR Type Translation
// ============================================================================

/**
 * Get DIS type index for a KIR type
 */
static int32_t get_dis_type_for_kir(DISModuleBuilder* builder, const char* kir_type) {
    // Check if already registered
    void* existing = hash_table_get(builder->type_indices, kir_type);
    if (existing) {
        return (int32_t)(intptr_t)existing;
    }

    // Register new type
    return register_type(builder, kir_type);
}

// ============================================================================
// KIR Component Translation
// ============================================================================

/**
 * Add a string to the data section
 * @param builder Module builder
 * @param str String to add
 * @return Offset in data section
 */
static uint32_t add_string_to_data(DISModuleBuilder* builder, const char* str) {
    if (!builder || !str) {
        return 0;
    }

    uint32_t offset = builder->global_offset;
    emit_data_string(builder, str, offset);
    return offset;
}

/**
 * Parse hex color and add to data section as RGB bytes
 * @param builder Module builder
 * @param hex Color string in format "#RRGGBB" or "0xRRGGBB"
 * @return Offset in data section, or 0 on error
 */
static uint32_t add_color_to_data(DISModuleBuilder* builder, const char* hex) {
    if (!builder || !hex || strlen(hex) < 7) {
        return 0;
    }

    // Skip # or 0x prefix
    const char* start = hex;
    if (hex[0] == '#') start = hex + 1;
    else if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) start = hex + 2;

    // Parse RGB values
    int r, g, b;
    if (sscanf(start, "%02x%02x%02x", &r, &g, &b) != 3) {
        return 0;
    }

    // Add to data section as 4 bytes (RGBA)
    uint32_t offset = builder->global_offset;
    emit_data_byte(builder, (uint8_t)r, offset);
    emit_data_byte(builder, (uint8_t)g, offset + 1);
    emit_data_byte(builder, (uint8_t)b, offset + 2);
    emit_data_byte(builder, 255, offset + 3);  // Alpha = 255 (opaque)

    return offset;
}

/**
 * Translate a KIR component to DIS data structure
 * This creates type descriptors and data initialization for the component tree
 */
static bool translate_component(DISModuleBuilder* builder, const char* component_json) {
    // For now, create a simple component type
    // Real implementation would parse the JSON properly

    // Register component type
    int32_t component_type = get_dis_type_for_kir(builder, "component:Box");

    // Allocate space for component in data section
    uint32_t component_offset = module_builder_allocate_global(builder, 32);  // Placeholder size

    // Initialize component data
    emit_data_word(builder, 0, component_offset);  // Component header
    emit_data_word(builder, 0, component_offset + 4);  // Props pointer
    emit_data_word(builder, 0, component_offset + 8);  // Children pointer

    return true;
}

// ============================================================================
// KIR Reactive State Translation
// ============================================================================

/**
 * Translate reactive variables to DIS global data
 */
static bool translate_reactive_state(DISModuleBuilder* builder, const char* state_json) {
    // For now, just allocate space for a counter
    // Real implementation would parse the reactive manifest

    // Allocate global variable: count (number)
    uint32_t count_offset = module_builder_allocate_global(builder, sizeof(int32_t));

    // Initialize to 0
    emit_data_word(builder, 0, count_offset);

    return true;
}

// ============================================================================
// KIR Event Handler Translation
// ============================================================================

/**
 * Translate a KIR event handler to DIS function
 * Example: "count = count + 1"
 */
static bool translate_handler(DISModuleBuilder* builder, const char* handler_name,
                              const char* handler_code) {
    // Get current PC for function entry
    uint32_t entry_pc = module_builder_get_pc(builder);

    // Simple handler: increment count
    // count = count + 1
    // Assuming count is at MP offset 0

    // Load count into temporary (using offset 0 from MP)
    // IMOVW 0(mp), tmp
    emit_mov_w(builder, 0, 1);  // Load from MP+0 to temp register (simplified)

    // Add immediate 1
    // IADDW $1, tmp, tmp
    emit_add_w(builder, 1, 1, 1);  // tmp = tmp + 1

    // Store back
    // IMOVW tmp, 0(mp)
    emit_mov_w(builder, 1, 0);  // Store to MP+0

    // Return
    emit_ret(builder);

    // Export the handler
    add_export(builder, handler_name, entry_pc, 0, 0);

    return true;
}

// ============================================================================
// libdraw-Based App Generation
// ============================================================================

/**
 * Generate module initialization code
 * Loads sys, draw, and wmclient modules required for libdraw apps
 */
static bool generate_module_init(DISModuleBuilder* builder) {
    // Load sys module (built-in, uses "$sys")
    emit_load(builder, "$sys");
    emit_call_import(builder, "sys", "load");

    // Load draw module (built-in, uses "$Draw")
    emit_load(builder, "$Draw");
    emit_call_import(builder, "Draw", "load");

    // Load wmclient module
    emit_load(builder, "/dis/lib/wmclient.dis");
    emit_call_import(builder, "wmclient", "load");

    // Call wmclient->init() to initialize the library
    emit_call_import(builder, "wmclient", "init");

    return true;
}

/**
 * Generate app initialization code using libdraw and wmclient
 * This creates the main entry point that:
 * 1. Loads required modules (sys, draw, wmclient)
 * 2. Creates a draw context
 * 3. Creates a window
 * 4. Renders components
 * 5. Starts event loop
 */
static bool generate_app_code(DISModuleBuilder* builder,
                               const char* title,
                               int width, int height,
                               uint32_t component_json_offset) {
    // MINIMAL TEST CASE: Just NOP + exit to test module structure
    // This should match pause.dis structure closely

    // Emit NOP instruction first so entry PC = 1 (like pause.dis)
    emit_insn(builder, 0, 0x03, 0, 0, 0);  // INOP, AXXX

    // Exit with status 0
    emit_exit(builder, 0);

    // Set entry point to PC 0 (like pause.dis)
    module_builder_set_entry(builder, 0, 1);

    // Export the init function with correct Command module signature
    // The type 0x4244b354 is the standard signature for:
    //   init: fn(ctxt: ref Draw->Context, argv: list of string)
    add_export(builder, "init", 1, 0x4244b354, 1);  // sig=1 matches pause.dis

    return true;
}

// ============================================================================
// Component Rendering Generation
// ============================================================================

/**
 * Generate color parsing code
 * Converts hex color string "#RRGGBB" to RGB values
 */
static uint32_t generate_color_parser(DISModuleBuilder* builder) {
    uint32_t pc = module_builder_get_pc(builder);

    // TODO: Implement color parsing
    // For now, just return
    emit_ret(builder);

    return pc;
}

/**
 * Generate container renderer
 * Draws a rectangle with background color and border
 * Calling convention: R0=image, R1=x, R2=y, R3=w, R4=h, R5=color_offset
 */
static uint32_t generate_container_renderer(DISModuleBuilder* builder) {
    uint32_t pc = module_builder_get_pc(builder);
    module_builder_add_function(builder, "render_container", pc);

    // Create rectangle: rect = Draw->Rect(x, y, w, h)
    // R1=x, R2=y, R3=w, R4=h already set by caller
    emit_call_import(builder, "draw", "Rect");
    // Result in R0, save temporarily
    emit_mov_w(builder, 0, 10);  // MP[10] = rect

    // Load color from data section
    // R5 has color offset, load address
    emit_mov_address(builder, 0, 5);  // R5 = address of color (simplified)

    // Draw rectangle: image->draw(rect, color, SoverD)
    // R0=image (from caller), R1=rect, R2=color, R3=draw_op (SoverD=0)
    emit_mov_w(builder, 10, 1);   // R1 = rect
    emit_mov_imm_w(builder, 0, 3); // R3 = SoverD (0)
    emit_method_call(builder, 0, 3);  // Call image->draw(rect, color, SoverD)

    emit_ret(builder);
    return pc;
}

/**
 * Generate font loader
 * Loads the default font for text rendering
 * Returns font handle in R0
 */
static uint32_t generate_font_loader(DISModuleBuilder* builder) {
    uint32_t pc = module_builder_get_pc(builder);
    module_builder_add_function(builder, "load_font", pc);

    // font = Draw->Font.open("default")
    emit_load_string_address(builder, "default", 0);  // R0 = font name
    emit_call_import(builder, "draw", "Font.open");
    // Result in R0, store to MP[108] (global font cache)
    emit_mov_w(builder, 0, 108);

    emit_ret(builder);
    return pc;
}

/**
 * Generate text renderer
 * Renders text using libdraw fonts
 * Calling convention: R0=image, R1=text, R2=x, R3=y, R4=color_offset
 */
static uint32_t generate_text_renderer(DISModuleBuilder* builder) {
    uint32_t pc = module_builder_get_pc(builder);
    module_builder_add_function(builder, "render_text", pc);

    // Load font first (placeholder)
    // emit_call(builder, load_font_pc);
    emit_mov_imm_w(builder, 0, 108);  // Placeholder: MP[108] = font handle
    emit_mov_w(builder, 108, 3);  // R3 = font

    // Create point for text position
    // p = Draw->Pt(x, y)
    // R2=x, R3 already has font, need to preserve x,y
    emit_mov_w(builder, 2, 5);  // Save y to R5
    emit_mov_w(builder, 1, 4);  // Save x to R4

    // Load color from data section
    // R4 has color offset
    emit_mov_address(builder, 0, 4);  // R4 = color address

    // Draw text: image->text(p, color, font, str)
    // R0=image, R1=p, R2=color, R3=font, R4 is used
    emit_mov_w(builder, 5, 2);  // Restore y to R2
    emit_mov_w(builder, 4, 1);  // Restore x to R1

    // Call image->text(p, color, font, str)
    emit_method_call(builder, 0, 4);

    emit_ret(builder);
    return pc;
}

/**
 * Generate component dispatcher
 * Reads component type from JSON and calls appropriate renderer
 * Simple version: just calls container renderer for now
 * Full version would parse JSON and switch on component type
 */
static uint32_t generate_component_dispatcher(DISModuleBuilder* builder) {
    uint32_t pc = module_builder_get_pc(builder);
    module_builder_add_function(builder, "render_components", pc);

    // Simple version: For now, just call container renderer directly
    // Full implementation would:
    // 1. Load component JSON from data section
    // 2. Parse "type" field
    // 3. Switch on type:
    //    - "Container" -> render_container
    //    - "Text" -> render_text
    //    - "Column" -> render_column
    //    - "Row" -> render_row

    // Look up container renderer function PC
    uint32_t container_pc = module_builder_lookup_function(builder, "render_container");
    if (container_pc != 0xFFFFFFFF) {
        emit_call(builder, container_pc);
    }
    emit_ret(builder);

    return pc;
}

/**
 * Generate event loop
 * Waits for mouse and keyboard events and dispatches handlers
 * Uses alt statement to wait on multiple channels
 */
static uint32_t generate_event_loop(DISModuleBuilder* builder) {
    uint32_t pc = module_builder_get_pc(builder);
    uint32_t loop_start = pc;

    module_builder_add_function(builder, "event_loop", pc);

    // Label: event_loop_start
    // Start alt statement to wait for events
    emit_alt(builder);

    // Case 1: Mouse event from win.ctxt.pointer
    // Load mouse channel
    emit_mov_w(builder, 104, 0);  // R0 = win
    emit_mov_p(builder, 0, 1);    // R1 = win.ctxt
    emit_mov_p(builder, 1, 2);    // R2 = win.ctxt.pointer

    // Receive from mouse channel
    emit_chan_recv(builder, 2, 3);  // R3 = mouse event

    // Handle mouse event (placeholder - should look up function PC)
    // For now, just ignore mouse events
    // emit_call(builder, "handle_mouse");

    // Jump back to start
    emit_jmp(builder, loop_start);

    // Case 2: Keyboard event from win.ctxt.kbd
    // Load keyboard channel
    emit_mov_w(builder, 104, 0);  // R0 = win
    emit_mov_p(builder, 0, 1);    // R1 = win.ctxt
    emit_mov_p(builder, 1, 4);    // R4 = win.ctxt.kbd

    // Receive from keyboard channel
    emit_chan_recv(builder, 4, 5);  // R5 = keyboard event

    // Handle keyboard event (placeholder)
    // For now, just ignore keyboard events
    // emit_call(builder, "handle_keyboard");

    // Jump back to start
    emit_jmp(builder, loop_start);

    emit_ret(builder);
    return pc;
}

// ============================================================================
// Main Translation Entry Point
// ============================================================================

/**
 * Translate KIR JSON to DIS bytecode
 */
bool translate_kir_to_dis(DISModuleBuilder* builder, const char* kir_json) {
    if (!builder || !kir_json) {
        dis_codegen_set_error("NULL builder or JSON");
        return false;
    }

    // Parse JSON using cJSON
    cJSON* root = cJSON_Parse(kir_json);
    if (!root) {
        const char* error = cJSON_GetErrorPtr();
        if (error) {
            char buf[256];
            snprintf(buf, sizeof(buf), "JSON parse error at: %s", error);
            dis_codegen_set_error(buf);
        } else {
            dis_codegen_set_error("Unknown JSON parse error");
        }
        return false;
    }

    // Extract app config
    cJSON* app = cJSON_GetObjectItem(root, "app");
    if (!app) {
        dis_codegen_set_error("Missing 'app' section in KIR");
        cJSON_Delete(root);
        return false;
    }

    cJSON* title_item = cJSON_GetObjectItem(app, "windowTitle");
    cJSON* width_item = cJSON_GetObjectItem(app, "windowWidth");
    cJSON* height_item = cJSON_GetObjectItem(app, "windowHeight");

    if (!title_item || !width_item || !height_item) {
        dis_codegen_set_error("Missing required app fields (windowTitle, windowWidth, windowHeight)");
        cJSON_Delete(root);
        return false;
    }

    const char* title = title_item->valuestring;
    int width = width_item->valueint;
    int height = height_item->valueint;

    // Extract component tree
    cJSON* component_tree = cJSON_GetObjectItem(root, "root");
    if (!component_tree) {
        dis_codegen_set_error("Missing 'root' component tree in KIR");
        cJSON_Delete(root);
        return false;
    }

    // Serialize component tree to JSON string for data section
    char* component_json = cJSON_Print(component_tree);
    if (!component_json) {
        dis_codegen_set_error("Failed to serialize component tree");
        cJSON_Delete(root);
        return false;
    }

    // TODO: Add component tree to data section
    // For MVP, we skip this to avoid data section format issues
    // uint32_t comp_offset = builder->global_offset;
    // emit_data_string(builder, component_json, comp_offset);
    uint32_t comp_offset = 0;  // Not used in MVP

    // Check for reactive state
    cJSON* reactive = cJSON_GetObjectItem(root, "reactive_manifest");
    bool has_reactive = (reactive != NULL);

    // Check for event handlers
    cJSON* logic = cJSON_GetObjectItem(root, "logic_block");
    bool has_handlers = (logic != NULL);

    // Translate reactive state if present
    if (has_reactive) {
        if (!translate_reactive_state(builder, cJSON_Print(reactive))) {
            free(component_json);
            cJSON_Delete(root);
            return false;
        }
    }

    // Generate app initialization code with libdraw
    if (!generate_app_code(builder, title, width, height, comp_offset)) {
        dis_codegen_set_error("Failed to generate app code");
        free(component_json);
        cJSON_Delete(root);
        return false;
    }

    // Translate event handlers if present
    if (has_handlers) {
        cJSON* functions = cJSON_GetObjectItem(logic, "functions");
        if (functions) {
            cJSON* func = NULL;
            cJSON_ArrayForEach(func, functions) {
                cJSON* name = cJSON_GetObjectItem(func, "name");
                if (name) {
                    translate_handler(builder, name->valuestring, "");
                }
            }
        }
    }

    free(component_json);
    cJSON_Delete(root);

    return true;
}
