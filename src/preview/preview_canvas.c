#include "preview_canvas.h"

Rectangle
PreviewFitRect(Rectangle bounds, int virtual_width, int virtual_height,
               PreviewScaleMode mode)
{
    float scale;
    float w;
    float h;

    if(virtual_width <= 0 || virtual_height <= 0 || bounds.width <= 0.0f ||
       bounds.height <= 0.0f)
        return (Rectangle){bounds.x, bounds.y, 0.0f, 0.0f};
    if(mode == PREVIEW_SCALE_100)
        scale = 1.0f;
    else if(mode == PREVIEW_SCALE_75)
        scale = 0.75f;
    else if(mode == PREVIEW_SCALE_50)
        scale = 0.5f;
    else {
        float sx = bounds.width / (float)virtual_width;
        float sy = bounds.height / (float)virtual_height;

        scale = sx < sy ? sx : sy;
    }
    w = (float)virtual_width * scale;
    h = (float)virtual_height * scale;
    return (Rectangle){bounds.x + (bounds.width - w) * 0.5f,
                       bounds.y + (bounds.height - h) * 0.5f, w, h};
}

Camera2D
PreviewCanvasCamera(Rectangle device, int virtual_width, int virtual_height)
{
    Camera2D camera = {0};
    float zoom = 1.0f;

    if(virtual_width > 0)
        zoom = device.width / (float)virtual_width;
    if(zoom <= 0.0f && virtual_height > 0)
        zoom = device.height / (float)virtual_height;
    if(zoom <= 0.0f)
        zoom = 1.0f;
    camera.offset = (Vector2){device.x, device.y};
    camera.target = (Vector2){0.0f, 0.0f};
    camera.rotation = 0.0f;
    camera.zoom = zoom;
    return camera;
}

Vector2
PreviewScreenToWorld(Rectangle device, int virtual_width, int virtual_height,
                     Vector2 screen)
{
    Camera2D camera = PreviewCanvasCamera(device, virtual_width,
                                          virtual_height);

    return GetScreenToWorld2D(screen, camera);
}

Vector2
PreviewWorldToScreen(Rectangle device, int virtual_width, int virtual_height,
                     Vector2 world)
{
    Camera2D camera = PreviewCanvasCamera(device, virtual_width,
                                          virtual_height);

    return GetWorldToScreen2D(world, camera);
}
