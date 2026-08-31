#include "kryon.h"
#include <stdio.h>

static int failures;

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "FAIL: %s got %d want %d\n", name, got, want);
    failures++;
}

static void
check_rect(const char *name, Rectangle got, Rectangle want)
{
    if((int)got.x == (int)want.x && (int)got.y == (int)want.y &&
       (int)got.width == (int)want.width && (int)got.height == (int)want.height)
        return;
    fprintf(stderr, "FAIL: %s got {%d,%d,%d,%d} want {%d,%d,%d,%d}\n",
            name, (int)got.x, (int)got.y, (int)got.width, (int)got.height,
            (int)want.x, (int)want.y, (int)want.width, (int)want.height);
    failures++;
}

int
main(void)
{
    SpriteSheet sheet = SpriteSheetGrid("player.png", 25, 45, 3, 2);

    check_int("frame count", SpriteSheetFrameCount(sheet), 6);
    check_int("wrap positive", SpriteSheetNormalizeFrame(sheet, 7), 1);
    check_int("wrap negative", SpriteSheetNormalizeFrame(sheet, -1), 5);
    check_rect("frame 0 source", SpriteSheetFrameSource(sheet, 0),
               (Rectangle){0, 0, 25, 45});
    check_rect("frame 4 source", SpriteSheetFrameSource(sheet, 4),
               (Rectangle){25, 45, 25, 45});
    check_rect("invalid source",
               SpriteSheetFrameSource(SpriteSheetGrid("bad.png", 0, 45, 3, 2), 0),
               (Rectangle){0, 0, 0, 0});

    return failures == 0 ? 0 : 1;
}
