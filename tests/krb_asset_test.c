/* KRB v2 asset-section test: a hand-assembled cartridge carrying a 2x2
 * RGBA8 asset blob, drawn by a PICTURE node through the engine's
 * texture_rgba path — no host filesystem involved. */

#include "krb.h"
#include "kry_sw.h"

#include <stdio.h>
#include <string.h>

static int
fail(const char *msg)
{
    fprintf(stderr, "krb_asset_test: %s\n", msg);
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
wr16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void
wr32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static void
write_minimal_header(unsigned char *buf, unsigned asset_bytes)
{
    wr32(buf + 0, KRB_MAGIC);
    wr16(buf + 4, 2);
    wr32(buf + 8, 0);  /* node_count */
    wr32(buf + 12, 1); /* string_bytes: root NUL only */
    wr32(buf + 16, 0); /* prog_bytes */
    wr32(buf + 20, 0); /* import_count */
    wr32(buf + 24, 0); /* control_count */
    wr32(buf + 28, asset_bytes);
    buf[32] = '\0';
}

static int
test_malformed_asset_sections(void)
{
    unsigned char buf[128];
    KrbImage img;

    memset(buf, 0, sizeof(buf));
    write_minimal_header(buf, 3);
    if(KrbLoad(&img, buf, 36) == 0)
        return fail("accepted short asset count");

    memset(buf, 0, sizeof(buf));
    write_minimal_header(buf, 4);
    wr32(buf + 33, 1);
    if(KrbLoad(&img, buf, 37) == 0)
        return fail("accepted truncated asset directory");

    memset(buf, 0, sizeof(buf));
    write_minimal_header(buf, 24);
    wr32(buf + 33, 1);       /* asset_count */
    wr32(buf + 33 + 4, 0);   /* path_off */
    wr32(buf + 33 + 8, 120); /* data_off */
    wr32(buf + 33 + 12, 16); /* size */
    if(KrbLoad(&img, buf, sizeof(buf)) == 0)
        return fail("accepted asset data past end");

    return 0;
}

int
main(void)
{
    /* layout: header(32) | node[1] | strings | prog(1) | controls(0) |
     * asset dir: count + entry(20) | blob (2x2 RGBA) */
    unsigned char buf[256];
    unsigned char *nodes = buf + 32;
    unsigned char *strings = nodes + 28;
    unsigned char *prog;
    unsigned char *adir;
    unsigned char *blob;
    unsigned path_off = 1;
    unsigned node_bytes = 28;
    unsigned string_bytes = 1 + 4 + 1; /* "\0" "tex\0" */
    unsigned prog_off = 32 + node_bytes + string_bytes;
    unsigned dir_off = prog_off + 1;
    unsigned blob_off;
    KrbImage img;
    KrySw sw;
    const unsigned char *found = NULL;
    unsigned flen = 0, fkind = 99, fw = 0, fh = 0;

    if(test_malformed_asset_sections() != 0)
        return 1;

    memset(buf, 0, sizeof(buf));
    memcpy(strings + 1, "tex", 3);
    prog = buf + prog_off;
    prog[0] = KRB_OP_DRAW_TREE;
    adir = buf + dir_off;
    blob_off = dir_off + 4 + 20;
    blob = buf + blob_off;
    /* 2x2: green, white / white, green (R < 0x80 for literal-ish look;
     * asset pixels are raw, no theme resolution) */
    blob[0] = 0x00; blob[1] = 0xf0; blob[2] = 0x00; blob[3] = 0xff;
    blob[4] = 0x70; blob[5] = 0xf0; blob[6] = 0x70; blob[7] = 0xff;
    blob[8] = 0x70; blob[9] = 0xf0; blob[10] = 0x70; blob[11] = 0xff;
    blob[12] = 0x00; blob[13] = 0xf0; blob[14] = 0x00; blob[15] = 0xff;
    wr32(adir, 1); /* asset_count */
    wr32(adir + 4, path_off);
    wr32(adir + 8, blob_off);
    wr32(adir + 12, 16);
    wr16(adir + 16, 0); /* kind: raw RGBA8 */
    wr16(adir + 18, 2); /* w */
    wr16(adir + 20, 2); /* h */
    wr16(adir + 22, 0);

    /* node: PICTURE at (4,4) 8x8, text_off = "tex" */
    nodes[6] = KRB_NODE_PICTURE;
    wr16(nodes + 10, 4);
    wr16(nodes + 12, 4);
    wr16(nodes + 14, 8);
    wr16(nodes + 16, 8);
    wr32(nodes + 18, 0x7f7f7fffu); /* half tint (R<0x80: no theme) */
    wr16(nodes + 22, path_off);

    wr32(buf + 0, KRB_MAGIC);
    wr16(buf + 4, 2);
    wr32(buf + 8, 1);   /* node_count */
    wr32(buf + 12, string_bytes);
    wr32(buf + 16, 1);  /* prog_bytes */
    wr32(buf + 24, 0);  /* control_count */
    wr32(buf + 28, 4 + 20 + 16); /* asset_bytes */

    if(KrbLoad(&img, buf, sizeof(buf)) != 0)
        return fail("load");
    if(KrbAssetFind(&img, "tex", &found, &flen, &fkind, &fw, &fh) != 0)
        return fail("asset not found");
    if(flen != 16 || fkind != 0 || fw != 2 || fh != 2 || found != blob)
        return fail("asset metadata wrong");
    if(KrbAssetFind(&img, "nope", NULL, NULL, NULL, NULL, NULL) == 0)
        return fail("bogus asset found");

    if(KrySwInit(&sw, NULL, 64, 64) != 0)
        return fail("sw init");
    KryBackendSelect(KrySwBackend(&sw));
    KrbDraw(&img, 0, 0, 64, 64);

    /* scaled 2x2 -> 8x8: quadrants 4x4 */
    if(px(&sw, 6, 6) != 0x007700ffu)
        return fail("top-left pixel not green");
    if(px(&sw, 10, 6) != 0x377737ffu)
        return fail("top-right pixel wrong");
    if(px(&sw, 6, 10) != 0x377737ffu)
        return fail("bottom-left pixel wrong");
    if(px(&sw, 10, 10) != 0x007700ffu)
        return fail("bottom-right pixel not green");
    if(px(&sw, 3, 6) != 0x00000000u)
        return fail("asset bled outside its rect");

    KrySwFree(&sw);
    printf("ok\n");
    return 0;
}
