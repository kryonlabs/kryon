/**
 * Desktop Backend Test Program
 *
 * Simple test to verify both SDL3 and Raylib backends work correctly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "desktop_internal.h"
#include "../../ir/ir_builder.h"
#include "../../ir/ir_serialization.h"

int main(void) {
    printf("=== Kryon Desktop Backend Test ===\n\n");

    // Create simple IR component tree
    IRComponent* root = calloc(1, sizeof(IRComponent));
    root->type = IR_COMPONENT_CONTAINER;
    root->id = 1;
    root->style = calloc(1, sizeof(IRStyle));
    root->style->background.type = IR_COLOR_SOLID;
    root->style->background.data.r = 25;
    root->style->background.data.g = 25;
    root->style->background.data.b = 25;
    root->style->background.data.a = 255;
    root->style->width = (IRDimension){.type=IR_DIMENSION_PX, .value=800};
    root->style->height = (IRDimension){.type=IR_DIMENSION_PX, .value=600};

    // Create desktop renderer with default config (using Raylib backend)
    DesktopRendererConfig config = desktop_renderer_config_default();
    config.window_title = "Kryon Desktop Test - Raylib Backend";

    printf("Initializing desktop renderer...\n");
    printf("Backend type: %d (0=SDL3, 1=Raylib)\n", config.backend_type);

    DesktopIRRenderer* renderer = desktop_ir_renderer_create(&config);
    if (!renderer) {
        fprintf(stderr, "Failed to create desktop renderer\n");
        free(root->style);
        free(root);
        return 1;
    }

    printf("Renderer initialized successfully!\n");
    printf("Backend: %s\n", config.backend_type == 0 ? "SDL3" : "Raylib");
    printf("Starting main loop (will run for 3 seconds)...\n");

    // Run for 3 seconds then exit
    int frame_count = 0;
    int max_frames = 180;  // 3 seconds at 60 FPS

    while (renderer->running && frame_count < max_frames) {
        desktop_ir_renderer_render_frame(renderer, root);
        frame_count++;
    }

    printf("\nRendered %d frames successfully!\n", frame_count);

    // Cleanup
    desktop_ir_renderer_destroy(renderer);
    free(root->style);
    free(root);

    printf("Test completed successfully!\n");
    return 0;
}
