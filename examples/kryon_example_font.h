#ifndef EXAMPLE_TEXT_FONT_H
#define EXAMPLE_TEXT_FONT_H

#include "ui_text.h"

#include "kryon.h"
static void
LoadExampleTextFont(void)
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
        if(RegisterTextFontFileSource("default", paths[i], NULL, 0)) {
            RegisterTextFontFileSource("semibold", semibold_paths[i], NULL, 0);
            UseTextFont("default");
            return;
        }
    }
}

static void
UnloadExampleTextFont(void)
{
    ClearTextFonts();
}

#endif
