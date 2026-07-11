#ifndef EXAMPLE_UI_FONT_H
#define EXAMPLE_UI_FONT_H

#include "ui_text.h"

#include "flint.h"
#include <stdio.h>

static Font example_ui_font = {0};

static void
LoadExampleUIFont(void)
{
    example_ui_font = LoadUIChoppedFont("assets/fonts/locales.png",
                                                      "assets/fonts/locales.dat",
                                                      UI_TEXT_BASE_SIZE);
    if(example_ui_font.texture.id != 0) {
        SetTextureFilter(example_ui_font.texture, TEXTURE_FILTER_POINT);
        SetUIFont(example_ui_font);
        SetUISmallFont(example_ui_font);
    } else {
        fprintf(stderr, "warning: failed to load chopped example font assets\n");
    }
}

static void
UnloadExampleUIFont(void)
{
    SetUIFont((Font){0});
    SetUISmallFont((Font){0});
    UnloadUIFont(&example_ui_font);
}

#endif
