/* KRB v2 logic-VM test: a hand-assembled cartridge that
 *   - increments a mounted counter every frame,
 *   - draws a rect only while counter < 3 (conditional UI via JZ),
 *   - draws a circle and a ring,
 *   - reads TIME.
 * Run against kry_sw and assert pixels + state across frames. */

#include "krb.h"
#include "kry_sw.h"

#include <stdio.h>
#include <string.h>

static int
fail(const char *msg)
{
    fprintf(stderr, "krb_logic_test: %s\n", msg);
    return 1;
}

static unsigned
px(const KrySw *sw, int x, int y)
{
    const unsigned char *p = sw->pixels + (size_t)y * sw->stride + x * 4;
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) |
           ((unsigned)p[2] << 8) | (unsigned)p[3];
}

static void
wr_u16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void
wr_u32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

int
main(void)
{
    /* layout: header(32) | nodes[3] | strings | prog */
    unsigned char buf[512];
    unsigned char *nodes = buf + 32;
    unsigned char *strings;
    unsigned char *prog;
    const char *counter_path = "counter";
    unsigned counter_off;
    unsigned node_bytes = 4 * KRB_NODE_SIZE;
    unsigned string_bytes;
    unsigned prog_off_start;
    unsigned jump_over_end;
    KrbImage img;
    KrySw sw;
    int counter = 0;

    memset(buf, 0, sizeof(buf));
    strings = nodes + node_bytes;
    strings[0] = '\0';
    memcpy(strings + 1, counter_path, strlen(counter_path));
    counter_off = 1;
    string_bytes = 1 + (unsigned)strlen(counter_path) + 1;

    /* node field offsets: type@6 flags@7 bind@8 x@10 y@12 w@14 h@16
     * color@18 text@22 (packed 28-byte records) */

    /* node 0: background rect (drawn via DRAW_NODE each frame) */
    nodes[0 + 6] = KRB_NODE_BACKGROUND;
    wr_u16(nodes + 0 + 14, 400);
    wr_u16(nodes + 0 + 16, 300);
    wr_u32(nodes + 0 + 18, 0x700000ffu);

    /* node 1: conditional green rect at (10,10) 20x20 */
    nodes[28 + 6] = KRB_NODE_RECT;
    wr_u16(nodes + 28 + 10, 10);
    wr_u16(nodes + 28 + 12, 10);
    wr_u16(nodes + 28 + 14, 20);
    wr_u16(nodes + 28 + 16, 20);
    wr_u32(nodes + 28 + 18, 0x00ff00ffu);

    /* node 2: circle, center (100,100), radius 30, white */
    nodes[56 + 6] = KRB_NODE_CIRCLE;
    wr_u16(nodes + 56 + 10, 100);
    wr_u16(nodes + 56 + 12, 100);
    wr_u16(nodes + 56 + 14, 30);
    wr_u32(nodes + 56 + 18, 0x70f0f0ffu);

    /* node 3: ring, center (200,100), outer 20, inner 10, white */
    nodes[84 + 6] = KRB_NODE_RING;
    wr_u16(nodes + 84 + 10, 200);
    wr_u16(nodes + 84 + 12, 100);
    wr_u16(nodes + 84 + 14, 20);
    wr_u16(nodes + 84 + 16, 10);
    wr_u32(nodes + 84 + 18, 0x70f0f0ffu);

    prog = strings + string_bytes;

    /* program:
     *   counter = counter + 1
     *   DRAW_NODE 0                   (background)
     *   if (counter < 3)              (jump-if-false over the rect)
     *   DRAW_NODE 1
     * end:
     *   DRAW_NODE 2                    (circle, always)
     *   DRAW_NODE 3                    (ring, always) */
    {
        unsigned char *q = prog;

        *q++ = KRB_OP_PUSH_PATH;  wr_u16(q, counter_off); q += 2;
        *q++ = KRB_OP_PUSH_CONST; wr_u32(q, 1); q += 4;
        *q++ = KRB_OP_ADD;
        *q++ = KRB_OP_POP_STORE;  wr_u16(q, counter_off); q += 2;

        *q++ = KRB_OP_DRAW_NODE; wr_u16(q, 0); q += 2;

        *q++ = KRB_OP_PUSH_PATH;  wr_u16(q, counter_off); q += 2;
        *q++ = KRB_OP_PUSH_CONST; wr_u32(q, 3); q += 4;
        *q++ = KRB_OP_LT;
        *q++ = KRB_OP_JZ;
        jump_over_end = 0; /* patched below */
        {
            unsigned patch_off = (unsigned)(q - prog);
            unsigned char *patch = q;

            q += 4;
            *q++ = KRB_OP_DRAW_NODE; wr_u16(q, 1); q += 2;
            wr_u32(patch, (unsigned long)(q - prog));
            (void)patch_off;
        }
        *q++ = KRB_OP_DRAW_NODE; wr_u16(q, 2); q += 2;
        *q++ = KRB_OP_DRAW_NODE; wr_u16(q, 3); q += 2;
        prog_off_start = (unsigned)(q - prog);
    }

    wr_u32(buf + 0, KRB_MAGIC);
    wr_u16(buf + 4, 2);            /* version 2 */
    wr_u16(buf + 6, 0);
    wr_u32(buf + 8, 4);            /* node_count */
    wr_u32(buf + 12, string_bytes);
    wr_u32(buf + 16, prog_off_start);
    wr_u32(buf + 20, 0);
    wr_u32(buf + 24, 0);
    (void)jump_over_end;

    if(KrbLoad(&img, buf, sizeof(buf)) != 0)
        return fail("v2 load");
    if(KrbBindMem(&img, counter_path, &counter, KRB_I32, 4) != 0)
        return fail("bind counter");

    if(KrySwInit(&sw, NULL, 400, 300) != 0)
        return fail("sw init");
    KryBackendSelect(KrySwBackend(&sw));

    /* frame 1: counter 0 -> 1, rect visible */
    KrbExec(&img);
    if(counter != 1)
        return fail("counter did not increment");
    if(px(&sw, 15, 15) != 0x00ff00ffu)
        return fail("conditional rect missing on frame 1");
    if(px(&sw, 100, 100) != 0x70f0f0ffu)
        return fail("circle center not drawn");
    if(px(&sw, 130, 100) != 0x70f0f0ffu)
        return fail("circle edge not drawn");
    if(px(&sw, 131, 100) == 0x70f0f0ffu)
        return fail("circle exceeded radius");
    if(px(&sw, 220, 100) != 0x70f0f0ffu)
        return fail("ring outer edge not drawn");
    if(px(&sw, 200, 100) == 0x70f0f0ffu)
        return fail("ring hole is filled");

    /* frames 2..3: rect still visible at 2, gone at 3 */
    KrbExec(&img);
    if(counter != 2 || px(&sw, 15, 15) != 0x00ff00ffu)
        return fail("frame 2 state");
    KrbExec(&img);
    if(counter != 3)
        return fail("frame 3 counter");
    if(px(&sw, 15, 15) != 0x700000ffu)
        return fail("conditional rect should be hidden at counter 3");
    if(px(&sw, 100, 100) != 0x70f0f0ffu)
        return fail("circle should still draw");

    /* TIME opcode sanity: pushes a growing ms value */
    {
        unsigned char tprog[6];
        unsigned char tbuf[256];

        memset(tbuf, 0, sizeof(tbuf));
        tprog[0] = KRB_OP_TIME;
        tprog[1] = KRB_OP_POP_STORE;
        wr_u16(tprog + 2, counter_off);
        memcpy(tbuf + 32 + node_bytes + string_bytes, tprog, 4);
        wr_u32(tbuf + 0, KRB_MAGIC);
        wr_u16(tbuf + 4, 2);
        wr_u32(tbuf + 8, 4);
        wr_u32(tbuf + 12, string_bytes);
        wr_u32(tbuf + 16, 4);
        {
            KrbImage timg;
            int tcounter = 0;

            if(KrbLoad(&timg, tbuf, sizeof(tbuf)) != 0)
                return fail("time cartridge load");
            if(KrbBindMem(&timg, counter_path, &tcounter, KRB_I32, 4) != 0)
                return fail("time bind");
            KrySwAdvance(&sw, 0.5f);
            KrbExec(&timg);
            if(tcounter < 400 || tcounter > 700)
                return fail("TIME value out of range");
            KrbFree(&timg);
        }
    }

    KrySwFree(&sw);
    printf("ok\n");
    return 0;
}
