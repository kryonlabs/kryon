/* Pure-math surface functions shared by every backend.
 *
 * These symbols (color fades, rectangle collision, the 2D camera transforms,
 * UTF-8 decoding) are backend-independent arithmetic over surface structs:
 * drawing nothing and touching no GPU state. Defining them once here means
 * the raylib wrappers, the null backend, and any future backend all answer
 * them identically - tools/generate-kryon-compat.sh skips these names in
 * both generated files. Formulas track raylib's (rshapes.c, rtextures.c,
 * rcore.c GetCameraMatrix2D, rtext.c) so behavior is unchanged. */

#include "kryon.h"

#include <math.h>

Color Fade(Color color, float alpha)
{
    Color result = color;

    if(alpha < 0.0f) alpha = 0.0f;
    else if(alpha > 1.0f) alpha = 1.0f;

    result.a = (unsigned char)(255.0f*alpha);

    return result;
}

Color ColorLerp(Color color1, Color color2, float factor)
{
    Color color = {0};

    if(factor < 0.0f) factor = 0.0f;
    else if(factor > 1.0f) factor = 1.0f;

    color.r = (unsigned char)((1.0f - factor)*color1.r + factor*color2.r);
    color.g = (unsigned char)((1.0f - factor)*color1.g + factor*color2.g);
    color.b = (unsigned char)((1.0f - factor)*color1.b + factor*color2.b);
    color.a = (unsigned char)((1.0f - factor)*color1.a + factor*color2.a);

    return color;
}

bool CheckCollisionPointRec(Vector2 point, Rectangle rec)
{
    return (point.x >= rec.x) && (point.x < (rec.x + rec.width)) &&
           (point.y >= rec.y) && (point.y < (rec.y + rec.height));
}

bool CheckCollisionRecs(Rectangle rec1, Rectangle rec2)
{
    return (rec1.x < (rec2.x + rec2.width) && (rec1.x + rec1.width) > rec2.x) &&
           (rec1.y < (rec2.y + rec2.height) && (rec1.y + rec1.height) > rec2.y);
}

/* Camera2D transform composed analytically: translate by -target, rotate by
 * camera.rotation, uniform scale by zoom, translate by offset (raylib's
 * GetCameraMatrix2D chain with row-vector application order). */
static Vector2 k_camera_forward(Vector2 p, Camera2D camera)
{
    float cosr = cosf(camera.rotation*DEG2RAD);
    float sinr = sinf(camera.rotation*DEG2RAD);
    float dx = p.x - camera.target.x;
    float dy = p.y - camera.target.y;
    Vector2 result;

    result.x = (dx*cosr - dy*sinr)*camera.zoom + camera.offset.x;
    result.y = (dx*sinr + dy*cosr)*camera.zoom + camera.offset.y;
    return result;
}

static Vector2 k_camera_inverse(Vector2 s, Camera2D camera)
{
    float cosr = cosf(camera.rotation*DEG2RAD);
    float sinr = sinf(camera.rotation*DEG2RAD);
    float dx = (s.x - camera.offset.x)/camera.zoom;
    float dy = (s.y - camera.offset.y)/camera.zoom;
    Vector2 result;

    result.x = (dx*cosr + dy*sinr) + camera.target.x;
    result.y = (-dx*sinr + dy*cosr) + camera.target.y;
    return result;
}

Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera)
{
    return k_camera_forward(position, camera);
}

Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera)
{
    return k_camera_inverse(position, camera);
}

int GetCodepointNext(const char *text, int *codepointSize)
{
    const char *ptr = text;
    int codepoint = 0x3f;       /* Codepoint (defaults to '?') */
    *codepointSize = 1;
    if(text == NULL) return codepoint;

    if(0xf0 == (0xf8 & ptr[0])) {
        /* 4 byte UTF-8 codepoint */
        if(((ptr[1] & 0xC0) ^ 0x80) || ((ptr[2] & 0xC0) ^ 0x80) || ((ptr[3] & 0xC0) ^ 0x80)) return codepoint;
        codepoint = ((0x07 & ptr[0]) << 18) | ((0x3f & ptr[1]) << 12) | ((0x3f & ptr[2]) << 6) | (0x3f & ptr[3]);
        *codepointSize = 4;
    } else if(0xe0 == (0xf0 & ptr[0])) {
        /* 3 byte UTF-8 codepoint */
        if(((ptr[1] & 0xC0) ^ 0x80) || ((ptr[2] & 0xC0) ^ 0x80)) return codepoint;
        codepoint = ((0x0f & ptr[0]) << 12) | ((0x3f & ptr[1]) << 6) | (0x3f & ptr[2]);
        *codepointSize = 3;
    } else if(0xc0 == (0xe0 & ptr[0])) {
        /* 2 byte UTF-8 codepoint */
        if((ptr[1] & 0xC0) ^ 0x80) return codepoint;
        codepoint = ((0x1f & ptr[0]) << 6) | (0x3f & ptr[1]);
        *codepointSize = 2;
    } else if(0x00 == (0x80 & ptr[0])) {
        /* 1 byte UTF-8 codepoint */
        codepoint = ptr[0];
        *codepointSize = 1;
    }

    return codepoint;
}
