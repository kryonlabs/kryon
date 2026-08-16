#include "kry_backend_rec.h"
#include "kry_sw.h"

#include <stdio.h>
#include <string.h>

static int
fail(const char *msg)
{
    fprintf(stderr, "kry_sw_test: %s\n", msg);
    return 1;
}

static unsigned
px(const KrySw *sw, int x, int y)
{
    const unsigned char *p = sw->pixels + (size_t)y * sw->stride + x * 4;
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) |
           ((unsigned)p[2] << 8) | (unsigned)p[3];
}

int
main(void)
{
    KrySw sw;
    const KryBackend *b;

    if(KrySwInit(&sw, NULL, 32, 32) != 0)
        return fail("init");
    b = KrySwBackend(&sw);

    /* clear paints the whole target */
    b->clear(0xff0000ffu);
    if(px(&sw, 0, 0) != 0xff0000ffu || px(&sw, 31, 31) != 0xff0000ffu)
        return fail("clear did not cover the target");

    /* rect fill, including clamping outside the target */
    b->rect(4, 4, 4, 4, 0x00ff00ffu);
    if(px(&sw, 5, 5) != 0x00ff00ffu || px(&sw, 3, 5) != 0xff0000ffu ||
       px(&sw, 8, 5) != 0xff0000ffu)
        return fail("rect fill wrong");
    b->rect(30, 30, 8, 8, 0x0000ffffu);
    if(px(&sw, 31, 31) != 0x0000ffffu)
        return fail("rect did not clamp to target");

    /* clipping confines drawing */
    b->clear(0xff0000ffu);
    b->clip_push(0, 0, 16, 32);
    b->rect(0, 0, 32, 32, 0x00ff00ffu);
    b->clip_pop();
    if(px(&sw, 15, 15) != 0x00ff00ffu || px(&sw, 16, 15) != 0xff0000ffu)
        return fail("clip not respected");

    /* text writes pixels inside its cell and measure matches advance */
    b->clear(0x00000000u);
    b->text("A", 2, 2, 16, 0xffffffffu);
    {
        int x;
        int y;
        int lit = 0;
        for(y = 2; y < 18; y++)
            for(x = 2; x < 10; x++)
                if(px(&sw, x, y) == 0xffffffffu)
                    lit++;
        if(lit == 0)
            return fail("text drew nothing");
        if(lit == 8 * 16)
            return fail("text drew a solid block");
    }
    if(b->measure_text("AB", 16) != 16)
        return fail("measure_text mismatch");

    /* rgb565 conversion: pure red -> 0xf800 */
    {
        unsigned short dst[32 * 32];
        KrySwToRGB565(&sw, dst);
        b->clear(0xff0000ffu);
        KrySwToRGB565(&sw, dst);
        if(dst[0] != 0xf800u)
            return fail("rgb565 red conversion");
    }

    /* recording wraps sw and counts draw calls */
    {
        KryBackendRec rec;
        const KryBackend *rb = KryBackendRecBackend(&rec, NULL, b);
        KryBackendSelect(rb);
        KryBackendCurrent()->clear(0xff0000ffu);
        KryBackendCurrent()->rect(1, 1, 2, 2, 0x00ff00ffu);
        if(px(&sw, 1, 1) != 0x00ff00ffu)
            return fail("recorder did not delegate to inner engine");
        KryBackendCurrent()->text("hi", 0, 0, 8, 0xffffffffu);
        if(KryBackendRecCalls(&rec) != 3)
            return fail("recorder call count");
    }

    KrySwFree(&sw);
    printf("ok\n");
    return 0;
}
