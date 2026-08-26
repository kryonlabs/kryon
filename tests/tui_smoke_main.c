#include "kryon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void
check(const char *name, int ok)
{
    if(!ok) {
        fprintf(stderr, "tui smoke failed: %s\n", name);
        failures++;
    }
}

static int
has_key(int want)
{
    int i;

    for(i = 0; i < 16; i++) {
        int key = GetKeyPressed();

        if(key == 0)
            return 0;
        if(key == want)
            return 1;
    }
    return 0;
}

int
main(void)
{
    unsigned char red_px[4] = {220, 20, 30, 255};
    unsigned char blue_px[4] = {30, 60, 220, 255};
    Image red = {red_px, 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Image blue = {blue_px, 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D red_tex;
    Texture2D blue_tex;
    RenderTexture2D target;
    Image shot;
    Color *pixels;
    int ch;

    InitWindow(40, 20, "Kryon TUI smoke");
    check("window ready", IsWindowReady());
    check("screen width", GetScreenWidth() == 40);
    check("screen height", GetScreenHeight() == 20);

    red_tex = LoadTextureFromImage(red);
    blue_tex = LoadTextureFromImage(blue);
    target = LoadRenderTexture(4, 4);
    check("red texture", red_tex.id != 0);
    check("blue texture", blue_tex.id != 0);
    check("render texture", target.texture.id != 0);

    BeginTextureMode(target);
    ClearBackground(BLANK);
    DrawRectangle(0, 0, 4, 4, (Color){10, 180, 90, 255});
    EndTextureMode();

    BeginDrawing();
    ClearBackground((Color){4, 5, 6, 255});
    DrawRectangle(2, 2, 8, 4, (Color){100, 150, 200, 255});
    DrawRectangleLines(1, 1, 12, 7, (Color){255, 255, 255, 255});
    DrawCircle(22, 7, 4, (Color){255, 200, 0, 255});
    DrawTexture(red_tex, 14, 3, WHITE);
    DrawTexturePro(blue_tex, (Rectangle){0, 0, 1, 1},
                   (Rectangle){16, 3, 3, 3}, (Vector2){0, 0}, 0.0f,
                   WHITE);
    DrawTexture(target.texture, 24, 3, WHITE);
    DrawText("TUI", 2, 12, 8, (Color){240, 240, 240, 255});

    shot = LoadImageFromScreen();
    check("screen capture", shot.data != NULL && shot.width == 40 &&
                                shot.height == 20);
    pixels = (Color *)shot.data;
    if(pixels != NULL)
        check("captured rect pixel", pixels[2 + 2 * shot.width].r == 100);
    UnloadImage(shot);
    EndDrawing();

    WindowShouldClose();
    ch = GetCharPressed();
    check("char input", ch == 'a');
    check("key input", has_key(KEY_A) || has_key(KEY_UP));
    check("up key edge", IsKeyPressed(KEY_UP));
    check("mouse pressed", IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    check("mouse released", IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
    check("mouse position", GetMouseX() == 4 && GetMouseY() == 6);
    check("wheel", GetMouseWheelMove() > 0.0f);

    SetClipboardText("clip");
    check("clipboard", strcmp(GetClipboardText(), "clip") == 0);

    UnloadRenderTexture(target);
    UnloadTexture(red_tex);
    UnloadTexture(blue_tex);
    CloseWindow();
    return failures == 0 ? 0 : 1;
}
