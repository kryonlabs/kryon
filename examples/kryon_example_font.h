#ifndef EXAMPLE_UI_FONT_H
#define EXAMPLE_UI_FONT_H

#include "ui_text.h"

#include "kryon.h"
static void
LoadExampleUIFont(void)
{
    const char *paths[] = {
        "fonts/noto/NotoSans-Regular.ttf",
        "../fonts/noto/NotoSans-Regular.ttf",
        "../../fonts/noto/NotoSans-Regular.ttf"
    };
    const char *semibold_paths[] = {
        "fonts/noto/NotoSans-SemiBold.ttf",
        "../fonts/noto/NotoSans-SemiBold.ttf",
        "../../fonts/noto/NotoSans-SemiBold.ttf"
    };

    /* Retain the source so headings and labels get exact-size rasters. */
    for(unsigned int i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        if(RegisterUIFontFileSource("default", paths[i], NULL, 0)) {
            RegisterUIFontFileSource("semibold", semibold_paths[i], NULL, 0);
            UseUIFont("default");
            return;
        }
    }
}

static void
UnloadExampleUIFont(void)
{
    ClearUIFonts();
}

#endif
