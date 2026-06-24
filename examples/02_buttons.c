#include "flint_example_font.h"
#include "flint_ui.h"
#include "flint_scaling.h"
#include <stdio.h>
#include <raylib.h>

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Flint UI Buttons Example");
    SetTargetFPS(60);
    flint_example_load_font();

    printf("Flint UI Buttons Example\n");
    printf("======================\n\n");
    printf("This example demonstrates Flint's unified button component:\n");
    printf("  - ui_draw_generic_button() - Single function for all button styles\n");
    printf("  - Automatic hover detection and click handling\n");
    printf("  - Theme integration with consistent styling\n");
    printf("  - flint_px() - Scale pixels by DPI factor\n");
    printf("  - flint_get_dpi_scale() / flint_set_dpi_scale() - DPI management\n\n");
    printf("Click buttons to see interactions\n");
    printf("Press UP/DOWN to change DPI scale\n");
    printf("Press ESC to exit\n\n");

    // Test button states
    int button1_clicked = 0;
    int button2_clicked = 0;
    int button3_clicked = 0;

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Draw title
        flint_text_draw("Flint UI Buttons Example", flint_px(20), flint_px(20), 24, BLACK);

        // Draw instructions
        flint_text_draw("Click the buttons below to test interactions", flint_px(20), flint_px(50), 16, DARKGRAY);
        flint_text_draw("(Using ui_draw_generic_button() utility)", flint_px(20), flint_px(70), 14, GRAY);

        // Button 1 - Primary style (using Flint utility)
        int btn1_hover = 0;
        if(ui_draw_generic_button(flint_px(20), flint_px(100), flint_px(200), flint_px(40),
                                  "Primary Button", UI_BUTTON_STYLE_PRIMARY, 0, &btn1_hover)) {
            button1_clicked = 1;
        }

        // Button 2 - Secondary style
        int btn2_hover = 0;
        if(ui_draw_generic_button(flint_px(20), flint_px(150), flint_px(200), flint_px(40),
                                  "Secondary Button", UI_BUTTON_STYLE_SECONDARY, 0, &btn2_hover)) {
            button2_clicked = 1;
        }

        // Button 3 - Danger style
        int btn3_hover = 0;
        if(button3_clicked) {
            // Change to tab selected style when clicked
            ui_draw_generic_button(flint_px(20), flint_px(200), flint_px(200), flint_px(40),
                                   "Clicked!", UI_BUTTON_STYLE_TAB_SELECTED, 0, &btn3_hover);
        } else {
            if(ui_draw_generic_button(flint_px(20), flint_px(200), flint_px(200), flint_px(40),
                                       "Click Me (Danger)", UI_BUTTON_STYLE_DANGER, 0, &btn3_hover)) {
                button3_clicked = 1;
            }
        }

        // Show click status
        char status[128];
        snprintf(status, sizeof(status), "Button 1: %s", button1_clicked ? "Clicked" : "Not clicked");
        flint_text_draw(status, flint_px(20), flint_px(260), 16, DARKGRAY);

        snprintf(status, sizeof(status), "Button 2: %s", button2_clicked ? "Clicked" : "Not clicked");
        flint_text_draw(status, flint_px(20), flint_px(280), 16, DARKGRAY);

        snprintf(status, sizeof(status), "Button 3: %s", button3_clicked ? "Clicked" : "Not clicked");
        flint_text_draw(status, flint_px(20), flint_px(300), 16, DARKGRAY);

        // Show current scale
        float current_scale = flint_get_dpi_scale();
        char scale_info[64];
        snprintf(scale_info, sizeof(scale_info), "DPI Scale: %.1f (Use UP/DOWN)", current_scale);
        flint_text_draw(scale_info, flint_px(20), flint_px(340), 16, DARKGRAY);

        // Show button info
        flint_text_draw("Button Styles:", flint_px(300), flint_px(100), 18, BLACK);
        flint_text_draw("- Primary: Blue theme color", flint_px(300), flint_px(120), 14, DARKGRAY);
        flint_text_draw("- Secondary: Gray color", flint_px(300), flint_px(135), 14, DARKGRAY);
        flint_text_draw("- Danger: Red for destructive actions", flint_px(300), flint_px(150), 14, DARKGRAY);
        flint_text_draw("- Tab Selected: Highlighted state", flint_px(300), flint_px(165), 14, DARKGRAY);

        // Handle scale changes
        if(IsKeyPressed(KEY_UP)) {
            flint_set_dpi_scale(current_scale + 0.1f);
        }
        if(IsKeyPressed(KEY_DOWN)) {
            flint_set_dpi_scale(current_scale - 0.1f);
        }

        if(IsKeyPressed(KEY_ESCAPE)) {
            break;
        }

        EndDrawing();
    }
    flint_example_unload_font();

    CloseWindow();
    return 0;
}
