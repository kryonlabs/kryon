#include <stdio.h>
#include "../../ir/ir_core.h"
#include "../../ir/ir_serialization.h"

extern IRContext* g_ir_context;

int main() {
    IRComponent* root = ir_read_json_file("/tmp/website.kir");
    
    printf("Root component: %p\n", (void*)root);
    printf("Global context: %p\n", (void*)g_ir_context);
    
    if (g_ir_context && g_ir_context->metadata) {
        printf("Metadata exists!\n");
        printf("  Title: %s\n", g_ir_context->metadata->window_title ? g_ir_context->metadata->window_title : "NULL");
        printf("  Width: %d\n", g_ir_context->metadata->window_width);
        printf("  Height: %d\n", g_ir_context->metadata->window_height);
    } else {
        printf("No metadata!\n");
    }
    
    if (root) ir_destroy_component(root);
    return 0;
}
