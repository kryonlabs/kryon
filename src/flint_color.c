#include "flint_color.h"

static float
flint_color_min(float a, float b)
{
    return a < b ? a : b;
}

static float
flint_color_max(float a, float b)
{
    return a > b ? a : b;
}

static float
flint_color_clampf(float value, float min, float max)
{
    if(value < min)
        return min;
    if(value > max)
        return max;
    return value;
}

static unsigned char
flint_color_byte(float value)
{
    int scaled = (int)(value * 255.0f + 0.5f);

    if(scaled < 0)
        return 0;
    if(scaled > 255)
        return 255;
    return (unsigned char)scaled;
}

static void
flint_color_to_hsl(Color c, float *h, float *s, float *l)
{
    float r = c.r / 255.0f;
    float g = c.g / 255.0f;
    float b = c.b / 255.0f;
    float max = flint_color_max(r, flint_color_max(g, b));
    float min = flint_color_min(r, flint_color_min(g, b));
    float chroma = max - min;

    *l = (max + min) * 0.5f;
    if(chroma <= 0.0f) {
        *h = 0.0f;
        *s = 0.0f;
        return;
    }

    *s = chroma / (1.0f - (2.0f * *l - 1.0f < 0.0f
                                ? -(2.0f * *l - 1.0f)
                                : (2.0f * *l - 1.0f)));
    if(max == r)
        *h = (g - b) / chroma + (g < b ? 6.0f : 0.0f);
    else if(max == g)
        *h = (b - r) / chroma + 2.0f;
    else
        *h = (r - g) / chroma + 4.0f;
    *h /= 6.0f;
}

static float
flint_color_hue_to_rgb(float p, float q, float t)
{
    if(t < 0.0f)
        t += 1.0f;
    if(t > 1.0f)
        t -= 1.0f;
    if(t < 1.0f / 6.0f)
        return p + (q - p) * 6.0f * t;
    if(t < 1.0f / 2.0f)
        return q;
    if(t < 2.0f / 3.0f)
        return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

static Color
flint_color_from_hsl(float h, float s, float l, unsigned char alpha)
{
    float r;
    float g;
    float b;
    float q;
    float p;

    h = flint_color_clampf(h, 0.0f, 1.0f);
    s = flint_color_clampf(s, 0.0f, 1.0f);
    l = flint_color_clampf(l, 0.0f, 1.0f);
    if(s <= 0.0f) {
        unsigned char gray = flint_color_byte(l);
        return (Color){gray, gray, gray, alpha};
    }

    q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    p = 2.0f * l - q;
    r = flint_color_hue_to_rgb(p, q, h + 1.0f / 3.0f);
    g = flint_color_hue_to_rgb(p, q, h);
    b = flint_color_hue_to_rgb(p, q, h - 1.0f / 3.0f);

    return (Color){flint_color_byte(r), flint_color_byte(g), flint_color_byte(b), alpha};
}

static Color
flint_adjust_lightness(Color c, int amount)
{
    float h;
    float s;
    float l;

    flint_color_to_hsl(c, &h, &s, &l);
    l = flint_color_clampf(l + amount / 255.0f, 0.0f, 1.0f);
    return flint_color_from_hsl(h, s, l, c.a);
}

Color
flint_lighten(Color c, int amount)
{
    return flint_adjust_lightness(c, amount < 0 ? 0 : amount);
}

Color
flint_darken(Color c, int amount)
{
    return flint_adjust_lightness(c, amount < 0 ? 0 : -amount);
}
