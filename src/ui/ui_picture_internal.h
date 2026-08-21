#ifndef UI_PICTURE_INTERNAL_H
#define UI_PICTURE_INTERNAL_H

#include "ui_picture.h"

Texture2D LoadPictureTexture(const char *path);
Rectangle PictureFitRect(PictureProps picture, Texture2D texture);
void PictureTexture(Texture2D texture, PictureProps picture);

#endif
