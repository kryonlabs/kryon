#ifndef FLINT_EXAMPLE_FONT_H
#define FLINT_EXAMPLE_FONT_H

#include "flint_text.h"

#include <raylib.h>
#include <stdio.h>

static Font flint_example_font = {0};

static void
flint_example_load_font(void)
{
    flint_example_font = flint_text_load_chopped_font("assets/fonts/locales.png",
                                                      "assets/fonts/locales.dat",
                                                      FLINT_TEXT_BASE_SIZE);
    if(flint_example_font.texture.id != 0) {
        SetTextureFilter(flint_example_font.texture, TEXTURE_FILTER_POINT);
        flint_text_set_font(flint_example_font);
        flint_text_set_small_font(flint_example_font);
    } else {
        fprintf(stderr, "warning: failed to load chopped example font assets\n");
    }
}

static void
flint_example_unload_font(void)
{
    flint_text_set_font((Font){0});
    flint_text_set_small_font((Font){0});
    flint_text_unload_font(&flint_example_font);
}

#endif
