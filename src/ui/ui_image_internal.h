#ifndef UI_IMAGE_INTERNAL_H
#define UI_IMAGE_INTERNAL_H

#include "ui_image.h"

Texture2D LoadImageTexture(const char *path);
Rectangle ImageFitRect(ImageProps image, Texture2D texture);
void ImageTexture(Texture2D texture, ImageProps image);

#endif
