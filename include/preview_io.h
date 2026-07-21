#ifndef PREVIEW_IO_H
#define PREVIEW_IO_H

#include "preview_layers.h"

int LoadPreviewRectLayer(const char *path, Rectangle *rects, int capacity);
int SavePreviewRectLayer(const char *path, const Rectangle *rects, int count);
int LoadPreviewLineLayer(const char *path, PreviewLine *lines, int capacity);
int SavePreviewLineLayer(const char *path, const PreviewLine *lines, int count);
int LoadPreviewPointLayer(const char *path, Vector2 *points, int capacity);
int SavePreviewPointLayer(const char *path, const Vector2 *points, int count);
int LoadPreviewObjectLayer(const char *path, PreviewObject *objects,
                           int capacity);
int SavePreviewObjectLayer(const char *path, const PreviewObject *objects,
                           int count);

#endif
