#include "kryon.h"

int
main(void)
{
    unsigned char pixels[16 * 16 * 4];
    unsigned char picture_pixels[96 * 64 * 4];
    Image icon;
    Image picture;
    Texture2D icon_texture;
    Texture2D picture_texture;
    int clicked = 0;

    for(int y = 0; y < 16; y++) {
        for(int x = 0; x < 16; x++) {
            int i = (y * 16 + x) * 4;
            int active = x == y || x == 15 - y || x < 3 || y > 12;

            pixels[i + 0] = active ? 240 : 32;
            pixels[i + 1] = active ? 72 : 120;
            pixels[i + 2] = active ? 120 : 180;
            pixels[i + 3] = active ? 255 : 220;
        }
    }
    for(int y = 0; y < 64; y++) {
        for(int x = 0; x < 96; x++) {
            int i = (y * 96 + x) * 4;

            picture_pixels[i + 0] = (unsigned char)(48 + (x * 3) % 180);
            picture_pixels[i + 1] = (unsigned char)(40 + (y * 4) % 160);
            picture_pixels[i + 2] = (unsigned char)(96 + ((x + y) * 2) % 140);
            picture_pixels[i + 3] = 255;
        }
    }
    icon = (Image){pixels, 16, 16, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    picture = (Image){picture_pixels, 96, 64, 1,
                      PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    icon_texture = LoadTextureFromImage(icon);
    picture_texture = LoadTextureFromImage(picture);

    SetSingleInstance(0);
    InitWindow(640, 360, "termi smoke");
    SetWindowSize(320, 160);
    SetTargetFPS(60);
    for(int frame = 0; frame < 120 && !WindowShouldClose(); frame++) {
        BeginDrawing();
        ClearBackground((Color){8, 12, 18, 255});
        BeginTree(10001);
        DrawRectangle(8, 8, 240, 48, (Color){24, 88, 136, 255});
        DrawRectangleLines(8, 8, 240, 48, (Color){230, 235, 240, 255});
        DrawText("Termi backend", 24, 24, 16, (Color){255, 255, 255, 255});
        if(Button((ButtonProps){
            .bounds = (Rectangle){24, 88, 180, 44},
            .label = "Button",
            .font = Text16,
            .id = 1001,
        }))
            clicked = 1;
        if(clicked)
            DrawText("Clicked", 24, 144, 16, (Color){255, 255, 255, 255});
        DrawTexturePro(icon_texture, (Rectangle){0, 0, 16, 16},
                       (Rectangle){248, 72, 32, 32}, (Vector2){0, 0}, 0.0f,
                       WHITE);
        DrawTexturePro(picture_texture, (Rectangle){0, 0, 96, 64},
                       (Rectangle){336, 88, 192, 128}, (Vector2){0, 0}, 0.0f,
                       WHITE);
        if(frame > 8)
            DrawRectangle(240, 72 + (frame % 2) * 16, 80, 24,
                          (Color){frame % 2 ? 48 : 160, 76, 128, 255});
        DrawText("Bottom", 24, 320, 16, (Color){255, 255, 255, 255});
        EndTree();
        if(frame == 0)
            BeginScissorMode(0, 0, 80, 80);
        EndDrawing();
    }
    CloseWindow();
    UnloadTexture(picture_texture);
    UnloadTexture(icon_texture);
    return 0;
}
