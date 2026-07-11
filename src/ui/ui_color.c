#include "ui_color.h"

static float
UIColorMin(float a, float b)
{
    return a < b ? a : b;
}

static float
UIColorMax(float a, float b)
{
    return a > b ? a : b;
}

static float
ClampUIColorFloat(float value, float min, float max)
{
    if(value < min)
        return min;
    if(value > max)
        return max;
    return value;
}

static unsigned char
UIColorByte(float value)
{
    int scaled = (int)(value * 255.0f + 0.5f);

    if(scaled < 0)
        return 0;
    if(scaled > 255)
        return 255;
    return (unsigned char)scaled;
}

static void
UIColorToHSL(Color c, float *h, float *s, float *l)
{
    float r = c.r / 255.0f;
    float g = c.g / 255.0f;
    float b = c.b / 255.0f;
    float max = UIColorMax(r, UIColorMax(g, b));
    float min = UIColorMin(r, UIColorMin(g, b));
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
UIColorHueToRGB(float p, float q, float t)
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
UIColorFromHSL(float h, float s, float l, unsigned char alpha)
{
    float r;
    float g;
    float b;
    float q;
    float p;

    h = ClampUIColorFloat(h, 0.0f, 1.0f);
    s = ClampUIColorFloat(s, 0.0f, 1.0f);
    l = ClampUIColorFloat(l, 0.0f, 1.0f);
    if(s <= 0.0f) {
        unsigned char gray = UIColorByte(l);
        return (Color){gray, gray, gray, alpha};
    }

    q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    p = 2.0f * l - q;
    r = UIColorHueToRGB(p, q, h + 1.0f / 3.0f);
    g = UIColorHueToRGB(p, q, h);
    b = UIColorHueToRGB(p, q, h - 1.0f / 3.0f);

    return (Color){UIColorByte(r), UIColorByte(g), UIColorByte(b), alpha};
}

static Color
AdjustUIColorLightness(Color c, int amount)
{
    float h;
    float s;
    float l;

    UIColorToHSL(c, &h, &s, &l);
    l = ClampUIColorFloat(l + amount / 255.0f, 0.0f, 1.0f);
    return UIColorFromHSL(h, s, l, c.a);
}

Color
LightenUIColor(Color c, int amount)
{
    return AdjustUIColorLightness(c, amount < 0 ? 0 : amount);
}

Color
DarkenUIColor(Color c, int amount)
{
    return AdjustUIColorLightness(c, amount < 0 ? 0 : -amount);
}
