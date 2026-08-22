#ifndef EXAMPLE_UI_FONT_H
#define EXAMPLE_UI_FONT_H

#include "ui_text.h"

#include "kryon.h"
static void
LoadExampleUIFont(void)
{
    Font font = LoadUIFontAsset("fonts/noto/NotoSans-Regular.ttf",
                                TextBaseSize);

    if(font.texture.id == 0)
        font = LoadUIFontAsset("../fonts/noto/NotoSans-Regular.ttf",
                               TextBaseSize);
    if(font.texture.id == 0)
        font = LoadUIFontAsset("../../fonts/noto/NotoSans-Regular.ttf",
                               TextBaseSize);
    if(font.texture.id == 0)
        font = LoadUIFontAsset("/mnt/storage/Projects/kryon/fonts/noto/NotoSans-Regular.ttf",
                               TextBaseSize);
    if(font.texture.id != 0) {
        RegisterUIFont("default", font);
        UseUIFont("default");
    }
}

static void
UnloadExampleUIFont(void)
{
    ClearUIFonts();
}

#endif
