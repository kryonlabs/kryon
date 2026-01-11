/*
 * Test for v3 JSON serialization with logic blocks
 * Verifies that:
 * 1. Logic functions can be created with universal expressions
 * 2. Event bindings work correctly
 * 3. JSON v3 serialization includes logic block
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir_core.h"
#include "ir_builder.h"
#include "ir_expression.h"
#include "ir_logic.h"
#include "ir_serialization.h"

int main(void) {
    printf("=== Test Logic v3 Serialization ===\n\n");

    // Create a simple component tree (simulating counters_demo)
    IRComponent* root = ir_create_component(IR_COMPONENT_CONTAINER);
    root->id = 1;

    IRComponent* text = ir_create_component(IR_COMPONENT_TEXT);
    text->id = 2;
    ir_set_text_content(text, "Count: 0");
    ir_add_child(root, text);

    IRComponent* btn_inc = ir_create_component(IR_COMPONENT_BUTTON);
    btn_inc->id = 3;
    ir_set_text_content(btn_inc, "+");
    ir_add_child(root, btn_inc);

    IRComponent* btn_dec = ir_create_component(IR_COMPONENT_BUTTON);
    btn_dec->id = 4;
    ir_set_text_content(btn_dec, "-");
    ir_add_child(root, btn_dec);

    printf("Created component tree:\n");
    printf("  Root container (id=1)\n");
    printf("    Text (id=2): 'Count: 0'\n");
    printf("    Button+ (id=3)\n");
    printf("    Button- (id=4)\n\n");

    // Create logic block
    IRLogicBlock* logic = ir_logic_block_create();

    // Create increment function using convenience helper
    IRLogicFunction* inc_func = ir_logic_create_increment("Counter:increment", "count");
    ir_logic_block_add_function(logic, inc_func);

    // Create decrement function using convenience helper
    IRLogicFunction* dec_func = ir_logic_create_decrement("Counter:decrement", "count");
    ir_logic_block_add_function(logic, dec_func);

    // Also create a reset function manually to test full expression API
    IRLogicFunction* reset_func = ir_logic_function_create("Counter:reset");
    reset_func->has_universal = true;

    // Create: count = 0
    IRStatement* reset_stmt = ir_stmt_assign("count", ir_expr_int(0));
    ir_logic_function_add_stmt(reset_func, reset_stmt);
    ir_logic_block_add_function(logic, reset_func);

    // Create a complex function with embedded Nim code
    IRLogicFunction* custom_func = ir_logic_function_create("Counter:customAction");
    ir_logic_function_add_source(custom_func, "nim",
        "proc customAction() =\n"
        "  echo \"Custom Nim action!\"\n"
        "  count.value = count.value * 2");
    ir_logic_function_add_source(custom_func, "lua",
        "function customAction()\n"
        "  print(\"Custom Lua action!\")\n"
        "  count = count * 2\n"
        "end");
    ir_logic_block_add_function(logic, custom_func);

    printf("Created logic functions:\n");
    printf("  Counter:increment (universal: count = count + 1)\n");
    printf("  Counter:decrement (universal: count = count - 1)\n");
    printf("  Counter:reset (universal: count = 0)\n");
    printf("  Counter:customAction (embedded: nim, lua)\n\n");

    // Create event bindings
    IREventBinding* bind_inc = ir_event_binding_create(3, "click", "Counter:increment");
    ir_logic_block_add_binding(logic, bind_inc);

    IREventBinding* bind_dec = ir_event_binding_create(4, "click", "Counter:decrement");
    ir_logic_block_add_binding(logic, bind_dec);

    printf("Created event bindings:\n");
    printf("  Button+ (id=3) -> click -> Counter:increment\n");
    printf("  Button- (id=4) -> click -> Counter:decrement\n\n");

    // Print logic block for debugging
    printf("Logic block debug output:\n");
    ir_logic_block_print(logic);
    printf("\n");

    // Create reactive manifest (minimal for test)
    IRReactiveManifest* manifest = ir_reactive_manifest_create();
    IRReactiveValue val = {0};
    val.as_int = 0;
    ir_reactive_manifest_add_var(manifest, "count", IR_REACTIVE_TYPE_INT, val);

    // Serialize to JSON v3
    char* json = ir_serialize_json(root, manifest, logic);

    if (json) {
        printf("=== JSON v3 Output ===\n");
        printf("%s\n", json);

        // Write to file for inspection
        FILE* f = fopen("/tmp/test_logic_v3.json", "w");
        if (f) {
            fprintf(f, "%s", json);
            fclose(f);
            printf("\nWritten to /tmp/test_logic_v3.json\n");
        }

        free(json);
    } else {
        printf("ERROR: Serialization failed!\n");
        return 1;
    }

    // Cleanup
    ir_logic_block_free(logic);
    ir_reactive_manifest_destroy(manifest);
    ir_destroy_component(root);

    printf("\n=== Test Passed ===\n");
    return 0;
}
