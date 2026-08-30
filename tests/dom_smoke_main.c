#include "kryon.h"
#include "kry_inject.h"

#include <string.h>

int main(void)
{
    int frames = 0;
    int clicked = 0;
    Image solid;
    Texture2D tex;
    Font font;

    InitWindow(320, 240, "dom smoke");
    if(!IsWindowReady())
        return 2;
    solid = GenImageColor(18, 18, (Color){90, 150, 210, 255});
    tex = LoadTextureFromImage(solid);
    font = GetFontDefault();

    while(!WindowShouldClose() && frames < 3) {
        frames++;
        if(frames == 1) {
            InjectMousePosition(76.0f, 168.0f);
            InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
            InjectPump();
        } else if(frames == 2) {
            InjectMousePosition(76.0f, 168.0f);
            InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
            InjectPump();
        }
        BeginDrawing();
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), 1.0f);
        SetPageTitle("DOM smoke title");
        SetPageDescription("DOM smoke description");
        SetPageCanonicalURL("/dom-smoke");
        SetPageThemeColor((Color){18, 20, 24, 255});
        ReplaceRoute("/dom-smoke#ready");
        ClearBackground((Color){18, 20, 24, 255});
        DrawRectangleGradientV(250, 18, 52, 24,
                               (Color){20, 70, 180, 255},
                               (Color){70, 180, 90, 255});
        BeginUI(Key("dom-page"));
        Page((PageProps){
            .bounds = {0, 0, 320, 240},
            .title = "DOM smoke title",
            .description = "DOM smoke description",
            .canonical_url = "/dom-smoke",
            .theme_color = {18, 20, 24, 255},
            .gap = 8,
            .padding = 0,
            .key = Key("page")
        });
        Heading((HeadingProps){
            .bounds = {12, 6, 150, 28},
            .text = "DOM Page",
            .level = 1,
            .font = 20,
            .color = {245, 245, 245, 255},
            .key = Key("heading")
        });
        Link((LinkProps){
            .bounds = {210, 192, 84, 24},
            .text = "Docs",
            .href = "/docs",
            .font = 16,
            .focus_id = 302,
            .color = {140, 190, 255, 255}
        });
        End();
        EndUI();
        DrawRectangle(10, 10, 120, 34, (Color){45, 77, 123, 255});
        DrawRectangleRounded((Rectangle){10, 54, 120, 34}, 0.35f, 8,
                             (Color){130, 82, 190, 255});
        DrawRectangleRoundedLinesEx((Rectangle){148, 54, 80, 34}, 0.35f, 8,
                                    2.0f, (Color){220, 180, 60, 255});
        DrawLine(10, 104, 180, 124, (Color){240, 80, 70, 255});
        DrawCircle(232, 72, 18, (Color){80, 190, 120, 255});
        BeginScissorMode(10, 142, 130, 30);
        DrawText("hello dom", 12, 146, 16, (Color){245, 245, 245, 255});
        EndScissorMode();
        DrawTexture(tex, 180, 142, WHITE);
        if(StyledButton(24, 150, 104, 36, "Click", ButtonStyleSecondary,
                        0, NULL))
            clicked = 1;
        if(clicked)
            DrawText("clicked", 24, 194, 16, (Color){255, 255, 255, 255});
        if(GetRouteVersion() == 1)
            DrawText("route version", 146, 104, 16,
                     (Color){255, 255, 255, 255});
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        EndUIFrame();
        EndDrawing();
    }

    UnloadTexture(tex);
    UnloadImage(solid);
    CloseWindow();
    return 0;
}
