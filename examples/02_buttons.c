#include "example_ui_font.h"
#include "ui.h"
#include "ui_scaling.h"
#include <stdio.h>
#include <raylib.h>

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "UI Buttons Example");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(screenWidth, screenHeight, GetUIScale());

    printf("UI Buttons Example\n");
    printf("======================\n\n");
    printf("This example demonstrates File UI Toolkit's unified button component:\n");
    printf("  - DrawUIGenericButton() - Single function for all button styles\n");
    printf("  - Automatic hover detection and click handling\n");
    printf("  - Theme integration with consistent styling\n");
    printf("  - ScaleUIPx() - Scale pixels by DPI factor\n");
    printf("  - GetUIScale() / SetUIScale() - DPI management\n\n");
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
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());
        SetUIColors(BLACK, RAYWHITE, LIGHTGRAY, SKYBLUE, BLUE, DARKBLUE, BLACK);

        // Draw title
        DrawUIText("UI Buttons Example", ScaleUIPx(20), ScaleUIPx(20), 24, BLACK);

        // Draw instructions
        DrawUIText("Click the buttons below to test interactions", ScaleUIPx(20), ScaleUIPx(50), 16, DARKGRAY);
        DrawUIText("(Using DrawUIGenericButton() utility)", ScaleUIPx(20), ScaleUIPx(70), 14, GRAY);

        // Button 1 - Primary style (using File UI Toolkit utility)
        int btn1_hover = 0;
        if(DrawUIGenericButton(ScaleUIPx(20), ScaleUIPx(100), ScaleUIPx(200), ScaleUIPx(40),
                                  "Primary Button", UI_BUTTON_STYLE_PRIMARY, 0, &btn1_hover)) {
            button1_clicked = 1;
        }

        // Button 2 - Secondary style
        int btn2_hover = 0;
        if(DrawUIGenericButton(ScaleUIPx(20), ScaleUIPx(150), ScaleUIPx(200), ScaleUIPx(40),
                                  "Secondary Button", UI_BUTTON_STYLE_SECONDARY, 0, &btn2_hover)) {
            button2_clicked = 1;
        }

        // Button 3 - Danger style
        int btn3_hover = 0;
        if(button3_clicked) {
            // Change to tab selected style when clicked
            DrawUIGenericButton(ScaleUIPx(20), ScaleUIPx(200), ScaleUIPx(200), ScaleUIPx(40),
                                   "Clicked!", UI_BUTTON_STYLE_TAB_SELECTED, 0, &btn3_hover);
        } else {
            if(DrawUIGenericButton(ScaleUIPx(20), ScaleUIPx(200), ScaleUIPx(200), ScaleUIPx(40),
                                       "Click Me (Danger)", UI_BUTTON_STYLE_DANGER, 0, &btn3_hover)) {
                button3_clicked = 1;
            }
        }

        // Show click status
        char status[128];
        snprintf(status, sizeof(status), "Button 1: %s", button1_clicked ? "Clicked" : "Not clicked");
        DrawUIText(status, ScaleUIPx(20), ScaleUIPx(260), 16, DARKGRAY);

        snprintf(status, sizeof(status), "Button 2: %s", button2_clicked ? "Clicked" : "Not clicked");
        DrawUIText(status, ScaleUIPx(20), ScaleUIPx(280), 16, DARKGRAY);

        snprintf(status, sizeof(status), "Button 3: %s", button3_clicked ? "Clicked" : "Not clicked");
        DrawUIText(status, ScaleUIPx(20), ScaleUIPx(300), 16, DARKGRAY);

        // Show current scale
        float current_scale = GetUIScale();
        char scale_info[64];
        snprintf(scale_info, sizeof(scale_info), "DPI Scale: %.1f (Use UP/DOWN)", current_scale);
        DrawUIText(scale_info, ScaleUIPx(20), ScaleUIPx(340), 16, DARKGRAY);

        // Show button info
        DrawUIText("Button Styles:", ScaleUIPx(300), ScaleUIPx(100), 18, BLACK);
        DrawUIText("- Primary: Blue theme color", ScaleUIPx(300), ScaleUIPx(120), 14, DARKGRAY);
        DrawUIText("- Secondary: Gray color", ScaleUIPx(300), ScaleUIPx(135), 14, DARKGRAY);
        DrawUIText("- Danger: Red for destructive actions", ScaleUIPx(300), ScaleUIPx(150), 14, DARKGRAY);
        DrawUIText("- Tab Selected: Highlighted state", ScaleUIPx(300), ScaleUIPx(165), 14, DARKGRAY);

        // Handle scale changes
        if(IsKeyPressed(KEY_UP)) {
            SetUIScale(current_scale + 0.1f);
        }
        if(IsKeyPressed(KEY_DOWN)) {
            SetUIScale(current_scale - 0.1f);
        }

        if(IsKeyPressed(KEY_ESCAPE)) {
            break;
        }

        EndDrawing();
    }
    UnloadExampleUIFont();

    CloseWindow();
    return 0;
}
