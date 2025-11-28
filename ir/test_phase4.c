// Test Phase 4 features: JSON serialization, animations, gradients
#include "ir_core.h"
#include "ir_builder.h"
#include "ir_serialization.h"
#include "ir_animation.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== Phase 4 Feature Test ===\n\n");

    // Create context and root
    IRContext* ctx = ir_create_context();
    ir_set_context(ctx);

    IRComponent* root = ir_container("root");
    ir_set_root(root);

    // Add some components
    IRComponent* button = ir_button("Test Button");
    ir_add_child(root, button);

    IRComponent* text = ir_text("Hello, Phase 4!");
    ir_add_child(root, text);

    // Test 1: JSON Serialization
    printf("Test 1: JSON Serialization\n");
    char* json = ir_serialize_json(root);
    if (json) {
        printf("✓ JSON serialization successful\n");
        printf("  JSON length: %lu bytes\n", strlen(json));
        printf("  First 100 chars: %.100s...\n", json);
        free(json);
    } else {
        printf("✗ JSON serialization failed\n");
        return 1;
    }

    // Test 2: Animation System
    printf("\nTest 2: Animation System\n");
    IRAnimationContext* anim_ctx = ir_animation_context_create();
    if (anim_ctx) {
        printf("✓ Animation context created\n");

        // Create a fade-in animation
        IRAnimationState* fade = ir_animation_fade_in(button, 1.0f);
        if (fade) {
            printf("✓ Fade-in animation created\n");
            ir_animation_set_easing(fade, ir_ease_out_quad);
            ir_animation_start(anim_ctx, fade);
            printf("  Animation started\n");

            // Simulate a frame update
            ir_animation_update(anim_ctx, 0.016f);  // 16ms frame
            uint32_t active_count = ir_animation_get_active_count(anim_ctx);
            printf("  Active animations: %u\n", active_count);
        } else {
            printf("✗ Failed to create animation\n");
        }

        ir_animation_context_destroy(anim_ctx);
    } else {
        printf("✗ Failed to create animation context\n");
        return 1;
    }

    // Test 3: Gradient Support
    printf("\nTest 3: Gradient Support\n");
    IRGradient* linear = ir_gradient_create_linear(45.0f);
    if (linear) {
        printf("✓ Linear gradient created (angle: 45°)\n");
        ir_gradient_add_stop(linear, 0.0f, 255, 0, 0, 255);    // Red
        ir_gradient_add_stop(linear, 0.5f, 0, 255, 0, 255);    // Green
        ir_gradient_add_stop(linear, 1.0f, 0, 0, 255, 255);    // Blue
        printf("  Added %u color stops\n", linear->stop_count);
        ir_gradient_destroy(linear);
    } else {
        printf("✗ Failed to create linear gradient\n");
        return 1;
    }

    IRGradient* radial = ir_gradient_create_radial(0.5f, 0.5f);
    if (radial) {
        printf("✓ Radial gradient created (center: 0.5, 0.5)\n");
        ir_gradient_add_stop(radial, 0.0f, 255, 255, 255, 255);  // White
        ir_gradient_add_stop(radial, 1.0f, 0, 0, 0, 255);        // Black
        printf("  Added %u color stops\n", radial->stop_count);
        ir_gradient_destroy(radial);
    } else {
        printf("✗ Failed to create radial gradient\n");
        return 1;
    }

    IRGradient* conic = ir_gradient_create_conic(0.5f, 0.5f);
    if (conic) {
        printf("✓ Conic gradient created (center: 0.5, 0.5)\n");
        ir_gradient_add_stop(conic, 0.0f, 255, 0, 0, 255);    // Red
        ir_gradient_add_stop(conic, 0.33f, 0, 255, 0, 255);   // Green
        ir_gradient_add_stop(conic, 0.66f, 0, 0, 255, 255);   // Blue
        ir_gradient_add_stop(conic, 1.0f, 255, 0, 0, 255);    // Red
        printf("  Added %u color stops\n", conic->stop_count);
        ir_gradient_destroy(conic);
    } else {
        printf("✗ Failed to create conic gradient\n");
        return 1;
    }

    // Cleanup
    ir_destroy_context(ctx);

    printf("\n=== All Phase 4 tests passed! ===\n");
    return 0;
}
