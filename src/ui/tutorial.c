#include "ui_internal.h"
#include "ui_picture_internal.h"

void
DrawUITutorialImagePlaceholder(const char *label, int x, int y, int w, int h)
{
    DrawRectangle(x, y, w, h, DarkenUIColor(c_bg, 12));
    DrawUIBevel(x, y, w, h, DarkenUIColor(c_bg, 45), LightenUIColor(c_bg, 35));
    int font = GetUIFontSize();
    int tw = MeasureUIText(label, font);
    DrawUIText(label, x + w / 2 - tw / 2, GetUIControlTextY(label, y, h, font), font, c_text);
}

void
DrawUITutorialImage(Texture2D texture, const char *fallback, int x, int y, int w, int h)
{
    PictureProps picture = {0};

    if(texture.id == 0) {
        DrawUITutorialImagePlaceholder(fallback, x, y, w, h);
        return;
    }

    picture.bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    picture.fit = PICTURE_FIT_COVER;
    picture.tint = WHITE;
    picture.style.enabled = 1;
    PictureTexture(texture, picture);
}

/* ================================================================
 * MODAL DIALOGS
 * ================================================================ */
