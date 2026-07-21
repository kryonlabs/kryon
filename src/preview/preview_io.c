#include "preview_io.h"

#include <stdio.h>

int
LoadPreviewRectLayer(const char *path, Rectangle *rects, int capacity)
{
    FILE *file;
    int count = 0;
    char line[256];

    if(path == 0 || rects == 0 || capacity <= 0)
        return -1;
    file = fopen(path, "r");
    if(file == 0)
        return -1;
    while(fgets(line, sizeof(line), file) != 0 && count < capacity) {
        Rectangle r = {0};

        if(line[0] == '#' || line[0] == '\n')
            continue;
        if(sscanf(line, "%f %f %f %f", &r.x, &r.y, &r.width, &r.height) != 4)
            continue;
        if(r.width < 0.0f) {
            r.x += r.width;
            r.width = -r.width;
        }
        if(r.height < 0.0f) {
            r.y += r.height;
            r.height = -r.height;
        }
        if(r.width > 0.0f && r.height > 0.0f)
            rects[count++] = r;
    }
    fclose(file);
    return count;
}

int
SavePreviewRectLayer(const char *path, const Rectangle *rects, int count)
{
    FILE *file;

    if(path == 0 || rects == 0 || count < 0)
        return 0;
    file = fopen(path, "w");
    if(file == 0)
        return 0;
    fprintf(file, "# x y width height\n");
    for(int i = 0; i < count; i++)
        fprintf(file, "%.3f %.3f %.3f %.3f\n", rects[i].x, rects[i].y,
                rects[i].width, rects[i].height);
    fclose(file);
    return 1;
}
int
LoadPreviewLineLayer(const char *path, PreviewLine *lines, int capacity)
{
    FILE *file;
    int count = 0;
    char line[256];

    if(path == 0 || lines == 0 || capacity <= 0)
        return -1;
    file = fopen(path, "r");
    if(file == 0)
        return -1;
    while(fgets(line, sizeof(line), file) != 0 && count < capacity) {
        PreviewLine item;

        if(line[0] == '#' || line[0] == '\n')
            continue;
        if(sscanf(line, "%f %f %f %f", &item.a.x, &item.a.y, &item.b.x,
                  &item.b.y) == 4)
            lines[count++] = item;
    }
    fclose(file);
    return count;
}

int
SavePreviewLineLayer(const char *path, const PreviewLine *lines, int count)
{
    FILE *file;

    if(path == 0 || lines == 0 || count < 0)
        return 0;
    file = fopen(path, "w");
    if(file == 0)
        return 0;
    fprintf(file, "# x1 y1 x2 y2\n");
    for(int i = 0; i < count; i++)
        fprintf(file, "%.3f %.3f %.3f %.3f\n", lines[i].a.x, lines[i].a.y,
                lines[i].b.x, lines[i].b.y);
    fclose(file);
    return 1;
}

int
LoadPreviewPointLayer(const char *path, Vector2 *points, int capacity)
{
    FILE *file;
    int count = 0;
    char line[256];

    if(path == 0 || points == 0 || capacity <= 0)
        return -1;
    file = fopen(path, "r");
    if(file == 0)
        return -1;
    while(fgets(line, sizeof(line), file) != 0 && count < capacity) {
        if(line[0] == '#' || line[0] == '\n')
            continue;
        if(sscanf(line, "%f %f", &points[count].x, &points[count].y) == 2)
            count++;
    }
    fclose(file);
    return count;
}

int
SavePreviewPointLayer(const char *path, const Vector2 *points, int count)
{
    FILE *file;

    if(path == 0 || points == 0 || count < 0)
        return 0;
    file = fopen(path, "w");
    if(file == 0)
        return 0;
    fprintf(file, "# x y\n");
    for(int i = 0; i < count; i++)
        fprintf(file, "%.3f %.3f\n", points[i].x, points[i].y);
    fclose(file);
    return 1;
}

int
LoadPreviewObjectLayer(const char *path, PreviewObject *objects, int capacity)
{
    FILE *file;
    int count = 0;
    char line[256];

    if(path == 0 || objects == 0 || capacity <= 0)
        return -1;
    file = fopen(path, "r");
    if(file == 0)
        return -1;
    while(fgets(line, sizeof(line), file) != 0 && count < capacity) {
        PreviewObject object = {0};
        int n;

        if(line[0] == '#' || line[0] == '\n')
            continue;
        n = sscanf(line, "%d %f %f %f %f", &object.type, &object.bounds.x,
                   &object.bounds.y, &object.bounds.width,
                   &object.bounds.height);
        if(n == 3) {
            object.bounds.width = 1.0f;
            object.bounds.height = 1.0f;
            objects[count++] = object;
        } else if(n == 5) {
            objects[count++] = object;
        }
    }
    fclose(file);
    return count;
}

int
SavePreviewObjectLayer(const char *path, const PreviewObject *objects,
                       int count)
{
    FILE *file;

    if(path == 0 || objects == 0 || count < 0)
        return 0;
    file = fopen(path, "w");
    if(file == 0)
        return 0;
    fprintf(file, "# type x y width height\n");
    for(int i = 0; i < count; i++)
        fprintf(file, "%d %.3f %.3f %.3f %.3f\n", objects[i].type,
                objects[i].bounds.x, objects[i].bounds.y,
                objects[i].bounds.width, objects[i].bounds.height);
    fclose(file);
    return 1;
}
