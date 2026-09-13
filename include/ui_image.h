#ifndef KRYON_IMAGE_H
#define KRYON_IMAGE_H

/*
 * UI image widget + shared image texture cache.
 *
 * The public widget is Image(ImageProps). Host drawing support is internal
 * because raylib already owns Image as a decoded-image-in-memory type.
 */

#include "ui_image_props.generated.h"

#define KRY_IMAGE_CACHE_MAX 128

#endif
