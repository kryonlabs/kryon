#include "kryon.h"

#include <string.h>

int main(void)
{
    int frames = 0;
    Image solid;
    Texture2D tex;
    RenderTexture2D target;
    Font font;
    int cps[3] = {'A', 'B', 'C'};
    Vector2 points[4] = {
        {20, 20}, {80, 22}, {70, 70}, {18, 68}
    };

    InitWindow(320, 240, "canvas smoke");
    if(!IsWindowReady())
        return 2;
    SetWindowTitle("canvas smoke title");
    SetWindowSize(320, 240);
    if(GetScreenWidth() != 320 || GetScreenHeight() != 240)
        return 3;
    if(GetRenderWidth() < 320 || GetRenderHeight() < 240)
        return 4;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    if(!IsWindowState(FLAG_WINDOW_RESIZABLE))
        return 5;
    ClearWindowState(FLAG_WINDOW_RESIZABLE);
    if(IsWindowState(FLAG_WINDOW_RESIZABLE))
        return 6;
    ToggleFullscreen();
    if(!IsWindowFullscreen())
        return 14;
    ClearWindowState(FLAG_FULLSCREEN_MODE);
    if(IsWindowFullscreen())
        return 15;
    SetMousePosition(23, 31);
    if(GetMouseX() != 23 || GetMouseY() != 31)
        return 16;
    SetMouseOffset(0, 0);
    SetMouseScale(1.0f, 1.0f);
    SetClipboardText("canvas clipboard");
    if(strcmp(GetClipboardText(), "canvas clipboard") != 0)
        return 17;
    ReplaceRoute("/canvas-smoke#/ready");
    if(strcmp(GetRoutePath(), "/canvas-smoke") != 0 ||
       strcmp(GetRouteHash(), "#/ready") != 0 ||
       GetRouteVersion() != 1)
        return 27;
    ReplaceRoute("/canvas-smoke#/ready");
    if(GetRouteVersion() != 1)
        return 28;
    PushRoute("/canvas-smoke#/done");
    if(strcmp(GetRouteHash(), "#/done") != 0 || GetRouteVersion() != 2)
        return 29;
    if(MakeDirectory("canvas_tmp/a/b") != 0)
        return 18;
    if(!DirectoryExists("canvas_tmp/a/b"))
        return 19;
    if(!SaveFileText("canvas_tmp/a/b/test.txt", "ok"))
        return 20;
    if(!FileExists("canvas_tmp/a/b/test.txt") ||
       GetFileLength("canvas_tmp/a/b/test.txt") != 2)
        return 21;
    {
        FilePathList files = LoadDirectoryFiles("canvas_tmp/a/b");

        if(files.count == 0)
            return 22;
        UnloadDirectoryFiles(files);
    }
    if(!IsPathFile("canvas_tmp/a/b/test.txt") ||
       !IsPathDirectory("canvas_tmp/a/b"))
        return 23;
    if(strcmp(GetFileName("canvas_tmp/a/b/test.txt"), "test.txt") != 0)
        return 24;
    if(strcmp(GetFileNameWithoutExt("canvas_tmp/a/b/test.txt"), "test") != 0)
        return 25;
    if(strcmp(GetPrevDirectoryPath("canvas_tmp/a/b"), "canvas_tmp/a") != 0)
        return 26;
    SetTargetFPS(60);
    solid = GenImageColor(16, 16, (Color){255, 255, 255, 255});
    if(!IsImageValid(solid))
        return 7;
    tex = LoadTextureFromImage(solid);
    if(!IsTextureValid(tex))
        return 8;
    target = LoadRenderTexture(32, 32);
    if(!IsRenderTextureValid(target))
        return 9;
    font = GetFontDefault();
    if(!IsFontValid(font) || MeasureText("hello", 16) <= 0)
        return 10;
    if(MeasureTextEx(font, "hello", 16, 1).x <= 0)
        return 11;
    if(MeasureTextCodepoints(font, cps, 3, 16, 1).x <= 0)
        return 12;
    (void)GetGlyphAtlasRec(font, 'A');
    (void)GetKeyName(KEY_A);
    (void)GetMonitorName(GetCurrentMonitor());
    (void)GetWindowScaleDPI();

    BeginTextureMode(target);
    ClearBackground((Color){0, 0, 0, 0});
    DrawTextureRec(tex, (Rectangle){0, 16, 16, -16}, (Vector2){0, 0},
                   WHITE);
    EndTextureMode();

    while(!WindowShouldClose() && frames < 3) {
        frames++;
        BeginDrawing();
        ClearBackground((Color){16, 16, 20, 255});
        DrawPixel(2, 2, WHITE);
        DrawPixelV((Vector2){3, 2}, WHITE);
        DrawRectangle(10, 10, 100, 40, (Color){45, 77, 123, 255});
        DrawRectangleV((Vector2){114, 10}, (Vector2){12, 12}, WHITE);
        DrawRectanglePro((Rectangle){148, 12, 20, 12}, (Vector2){10, 6},
                         12.0f, (Color){180, 220, 80, 255});
        DrawRectangleLines(10, 10, 100, 40, (Color){200, 200, 210, 255});
        DrawRectangleLinesEx((Rectangle){12, 58, 96, 34}, 4.0f,
                             (Color){220, 160, 80, 255});
        DrawRectangleGradientH(240, 154, 60, 18,
                               (Color){20, 80, 180, 255},
                               (Color){180, 220, 40, 255});
        DrawRectangleGradientEx((Rectangle){240, 176, 60, 18},
                                RED, GREEN, BLUE, WHITE);
        DrawCircle(200, 120, 30, (Color){0, 228, 48, 255});
        DrawCircleLinesV((Vector2){200, 120}, 34, WHITE);
        DrawCircleLinesEx((Vector2){200, 120}, 38, 3.0f,
                          (Color){40, 180, 240, 255});
        DrawLine(0, 0, 320, 240, (Color){255, 0, 0, 255});
        DrawLineV((Vector2){0, 240}, (Vector2){320, 0},
                  (Color){80, 200, 255, 255});
        DrawLineStrip(points, 4, (Color){255, 255, 255, 255});
        DrawRectangleRounded((Rectangle){130, 60, 90, 40, }, 0.5f, 8,
                             (Color){120, 80, 200, 255});
        DrawRectangleRoundedLines((Rectangle){130, 110, 90, 40}, 0.5f, 8,
                                  (Color){240, 200, 60, 255});
        DrawRing((Vector2){60, 150}, 20, 32, 10.0f, 300.0f, 24,
                 (Color){255, 109, 194, 255});
        DrawRingLines((Vector2){60, 150}, 34, 42, 10.0f, 300.0f, 24,
                      (Color){255, 255, 255, 255});
        DrawTriangleLines((Vector2){250, 14}, (Vector2){294, 16},
                          (Vector2){270, 42}, WHITE);
        DrawTriangleFan(points, 4, (Color){80, 120, 220, 80});
        DrawTriangleStrip(points, 4, (Color){220, 120, 80, 80});
        DrawRectangleGradientV(240, 60, 60, 90,
                               (Color){30, 30, 60, 255},
                               (Color){120, 30, 30, 255});
        DrawTexture(tex, 172, 12, WHITE);
        DrawTextureV(tex, (Vector2){190, 12}, (Color){255, 255, 255, 180});
        DrawTextureEx(tex, (Vector2){210, 12}, 30.0f, 1.25f, WHITE);
        DrawTexturePro(target.texture,
                       (Rectangle){0, 0, 32, 32},
                       (Rectangle){280, 204, 32, 32},
                       (Vector2){16, 16}, 10.0f, WHITE);
        /* raylib's render-target idiom: negative source height must draw
         * the target upright, not mirror it out of bounds */
        DrawTexturePro(target.texture,
                       (Rectangle){0, 0, 32, -32},
                       (Rectangle){240, 204, 32, 32},
                       (Vector2){0, 0}, 0.0f, WHITE);
        DrawText("hello canvas", 12, 200, 16, (Color){240, 240, 242, 255});
        DrawTextEx(font, "ex", (Vector2){120, 200}, 16, 1.0f, WHITE);
        DrawTextCodepoint(font, '!', (Vector2){150, 200}, 16, WHITE);
        DrawTextCodepoints(font, cps, 3, (Vector2){166, 200}, 16, 1.0f,
                           WHITE);
        DrawTextPro(font, "rot", (Vector2){220, 216}, (Vector2){0, 0},
                    -8.0f, 16, 1.0f, WHITE);
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        EndDrawing();
    }
    {
        Image shot = LoadImageFromScreen();

        if(!IsImageValid(shot))
            return 13;
        UnloadImage(shot);
    }
    UnloadRenderTexture(target);
    UnloadTexture(tex);
    UnloadImage(solid);
    CloseWindow();
    return 0;
}
