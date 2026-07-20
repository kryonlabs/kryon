#include "example_ui_font.h"
#include "theme.h"
#include "theme_meta.h"
#include "ui.h"
#include <stdio.h>
#include "kryon.h"

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "UI Buttons Example");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(screenWidth, screenHeight, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    printf("UI Buttons Example\n");
    printf("======================\n\n");
    printf("This example demonstrates Kryon's unified button component:\n");
    printf("  - DrawUIGenericButton() - Single function for all button styles\n");
    printf("  - Automatic hover detection and click handling\n");
    printf("  - Theme integration with consistent styling\n");
    printf("Click buttons to see momentary actions\n");
    printf("Press ESC to exit\n\n");

    int click_count = 0;
    char last_action[128] = "No button clicked yet.";

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        // Draw title
        DrawUIText("UI Buttons Example", ScaleUIPx(20), ScaleUIPx(20), 24, GetThemeText());

        // Draw instructions
        DrawUIText("Click the buttons below to test interactions", ScaleUIPx(20), ScaleUIPx(50), 16, GetThemeText());
        DrawUIText("Buttons return a one-frame click event.", ScaleUIPx(20), ScaleUIPx(70), 14, GetThemeIcon());

        // Button 1 - Primary style (using Kryon utility)
        int btn1_hover = 0;
        if(DrawUIGenericButton(ScaleUIPx(20), ScaleUIPx(100), ScaleUIPx(200), ScaleUIPx(40),
                                  "Primary Button", UI_BUTTON_STYLE_PRIMARY, 0, &btn1_hover)) {
            click_count++;
            snprintf(last_action, sizeof(last_action), "Primary Button clicked.");
        }

        // Button 2 - Secondary style
        int btn2_hover = 0;
        if(DrawUIGenericButton(ScaleUIPx(20), ScaleUIPx(150), ScaleUIPx(200), ScaleUIPx(40),
                                  "Secondary Button", UI_BUTTON_STYLE_SECONDARY, 0, &btn2_hover)) {
            click_count++;
            snprintf(last_action, sizeof(last_action), "Secondary Button clicked.");
        }

        // Button 3 - Danger style
        int btn3_hover = 0;
        if(DrawUIGenericButton(ScaleUIPx(20), ScaleUIPx(200), ScaleUIPx(200), ScaleUIPx(40),
                                  "Danger Button", UI_BUTTON_STYLE_DANGER, 0, &btn3_hover)) {
            click_count++;
            snprintf(last_action, sizeof(last_action), "Danger Button clicked.");
        }

        // Show click status
        char status[128];
        snprintf(status, sizeof(status), "Clicks: %d", click_count);
        DrawUIText(status, ScaleUIPx(20), ScaleUIPx(260), 16, GetThemeText());
        DrawUIText(last_action, ScaleUIPx(20), ScaleUIPx(285), 16, GetThemeText());

        // Show button info
        DrawUIText("Button Styles:", ScaleUIPx(300), ScaleUIPx(100), 18, GetThemeText());
        DrawUIText("- Primary: theme button color", ScaleUIPx(300), ScaleUIPx(120), 14, GetThemeText());
        DrawUIText("- Secondary: theme surface color", ScaleUIPx(300), ScaleUIPx(135), 14, GetThemeText());
        DrawUIText("- Danger: destructive action color", ScaleUIPx(300), ScaleUIPx(150), 14, GetThemeText());

        if(IsKeyPressed(KEY_ESCAPE)) {
            break;
        }

        EndDrawing();
    }
    UnloadExampleUIFont();

    CloseWindow();
    return 0;
}
