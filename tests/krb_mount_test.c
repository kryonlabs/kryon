#include "krb.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *
wr_u16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    return p + 2;
}

static unsigned char *
wr_u32(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
    return p + 4;
}

static int
fail(const char *msg)
{
    fprintf(stderr, "krb_mount_test: %s\n", msg);
    return 1;
}

static int clicks;

static int
on_click(void *ud)
{
    (void)ud;
    clicks++;
    return 1;
}

static char g_last_text[128];

static void
cap_text(const char *s, int x, int y, int size, unsigned color)
{
    (void)x;
    (void)y;
    (void)size;
    (void)color;
    snprintf(g_last_text, sizeof(g_last_text), "%s", s);
}

/* Mouse capture for the checkbox click-toggle test. */
static int g_cap_pressed;
static int g_cap_down;
static int g_cap_mx = 4;
static int g_cap_my = 4;
static void
cap_mouse(int *x, int *y)
{
    if(x != NULL)
        *x = g_cap_mx;
    if(y != NULL)
        *y = g_cap_my;
}
static int
cap_pressed(int button)
{
    (void)button;
    return g_cap_pressed;
}
static int
cap_down(int button)
{
    (void)button;
    return g_cap_down;
}

static unsigned g_text_keys[4];
static int g_text_key_index;
static unsigned
cap_text_key(void)
{
    unsigned cp = g_text_keys[g_text_key_index];

    if(cp != 0)
        g_text_key_index++;
    return cp;
}

/* header + 1 text node + strings("\0score\0n=%d\0") + prog + no imports */
static int
build_text_image(unsigned char *buf, size_t cap, const unsigned char *prog,
                 size_t prog_len, size_t *out_len)
{
    unsigned char *p = buf;
    const char strings[] = "\0score\0n=%d\0";
    size_t string_bytes = sizeof(strings);
    size_t need = 32 + 28 + string_bytes + prog_len;

    if(need > cap)
        return -1;
    p = wr_u32(p, KRB_MAGIC);
    p = wr_u16(p, KRB_VERSION);
    p = wr_u16(p, 0);
    p = wr_u32(p, 1);
    p = wr_u32(p, (unsigned)string_bytes);
    p = wr_u32(p, (unsigned)prog_len);
    p = wr_u32(p, 0);
    p = wr_u32(p, 0);
    p = wr_u32(p, 0);            /* header pad to 32 */
    p = wr_u16(p, 0);            /* id */
    p = wr_u16(p, (unsigned)-1); /* parent */
    p = wr_u16(p, 1);            /* name_off "score" */
    *p++ = KRB_NODE_TEXT;
    *p++ = 0;
    p = wr_u16(p, 0xffff);
    p = wr_u16(p, 0);
    p = wr_u16(p, 0);
    p = wr_u16(p, 40);
    p = wr_u16(p, 16);
    p = wr_u32(p, KRB_COLOR_THEME | KRY_THEME_TEXT);
    p = wr_u16(p, 7); /* text_off "n=%d" */
    p = wr_u16(p, 16);
    *p++ = 0;
    *p++ = 0;
    memcpy(p, strings, string_bytes);
    p += string_bytes;
    memcpy(p, prog, prog_len);
    p += prog_len;
    *out_len = (size_t)(p - buf);
    return 0;
}

int
main(void)
{
    unsigned char buf[256];
    unsigned char prog_draw[] = { KRB_OP_DRAW_TREE };
    unsigned char prog_set[8];
    size_t len = 0;
    KrbImage img;
    int value = 0;
    typedef struct {
        int score;
        float level;
        char label[16];
    } App;
    App app;
    KrbField fields[] = {
        { "score", (unsigned)offsetof(App, score), KRB_I32, 4 },
        { "level", (unsigned)offsetof(App, level), KRB_F32, 4 },
        { "label", (unsigned)offsetof(App, label), KRB_CSTR, 16 },
        { NULL, 0, 0, 0 }
    };

    KryBackendSelect(&KryBackendNull);

    if(build_text_image(buf, sizeof(buf), prog_draw, 1, &len) != 0)
        return fail("build");
    memset(&img, 0, sizeof(img));
    if(KrbLoad(&img, buf, len) != 0)
        return fail("load");

    app.score = 3;
    app.level = 1.5f;
    snprintf(app.label, sizeof(app.label), "hi");
    if(KrbMount(&img, "/app", &app, fields) != 0)
        return fail("mount");
    if(KrbReadI32(&img, "/app/score", &value) != 0 || value != 3)
        return fail("read mounted score");
    if(KrbWriteI32(&img, "/app/score", 9) != 0 || app.score != 9)
        return fail("write mounted score");
    if(KrbWriteF32(&img, "/app/level", 2.5f) != 0 || app.level != 2.5f)
        return fail("write mounted f32");
    {
        float fv = 0.0f;

        if(KrbReadF32(&img, "/app/level", &fv) != 0 || fv != 2.5f)
            return fail("read mounted f32");
    }
    if(KrbReadI32(&img, "/app/level", &value) == 0)
        return fail("f32 field must not read as i32");
    if(KrbWriteCStr(&img, "/app/label", "ok") != 0 ||
       strcmp(app.label, "ok") != 0)
        return fail("write mounted cstr");

    KrbFree(&img);
    memset(&img, 0, sizeof(img));
    if(KrbLoad(&img, buf, len) != 0)
        return fail("reload");
    value = 4;
    if(KrbBindMem(&img, "score", &value, KRB_I32, 4) != 0)
        return fail("bindmem");
    KrbDraw(&img, 0, 0, 200, 80);
    if(KrbReadI32(&img, "score", &value) != 0 || value != 4)
        return fail("bindmem read");

    /* OP_SET_I32 path_off=1 ("score"), value=42 */
    prog_set[0] = KRB_OP_SET_I32;
    prog_set[1] = 1;
    prog_set[2] = 0;
    prog_set[3] = 42;
    prog_set[4] = 0;
    prog_set[5] = 0;
    prog_set[6] = 0;
    if(build_text_image(buf, sizeof(buf), prog_set, 7, &len) != 0)
        return fail("build set");
    KrbFree(&img);
    memset(&img, 0, sizeof(img));
    if(KrbLoad(&img, buf, len) != 0)
        return fail("load set");
    value = 1;
    if(KrbBindMem(&img, "score", &value, KRB_I32, 4) != 0)
        return fail("bind set");
    if(KrbExec(&img) != 0)
        return fail("exec set");
    if(value != 42)
        return fail("set_i32 did not write");

    /* OP_CALL_HOST slot 0 */
    prog_set[0] = KRB_OP_CALL_HOST;
    prog_set[1] = 0;
    if(build_text_image(buf, sizeof(buf), prog_set, 2, &len) != 0)
        return fail("build call");
    KrbFree(&img);
    memset(&img, 0, sizeof(img));
    if(KrbLoad(&img, buf, len) != 0)
        return fail("load call");
    clicks = 0;
    if(KrbBindSlot(&img, 0, on_click, NULL) != 0)
        return fail("bind slot");
    if(KrbExec(&img) != 0)
        return fail("exec call");
    if(clicks != 1)
        return fail("call_host did not run");
    KrbFree(&img);

    /* A TEXT node bound to a float field formats through its printf format
     * (here "%.1f"); default is "%g". Built from scratch so the string
     * table carries a float format. */
    {
        static const char fstrings[] = "\0speed\0%.1f\0";
        unsigned char *p = buf;
        KryBackend cap;
        float speed = 3.14159f;

        p = wr_u32(p, KRB_MAGIC);
        p = wr_u16(p, KRB_VERSION);
        p = wr_u16(p, 0);
        p = wr_u32(p, 1);
        p = wr_u32(p, (unsigned)sizeof(fstrings));
        p = wr_u32(p, 1);
        p = wr_u32(p, 0);
        p = wr_u32(p, 0);
        p = wr_u32(p, 0);
        p = wr_u16(p, 0);
        p = wr_u16(p, (unsigned)-1);
        p = wr_u16(p, 1);            /* name_off "speed" */
        *p++ = KRB_NODE_TEXT;
        *p++ = 0;
        p = wr_u16(p, 0xffff);
        p = wr_u16(p, 0);
        p = wr_u16(p, 0);
        p = wr_u16(p, 40);
        p = wr_u16(p, 16);
        p = wr_u32(p, KRB_COLOR_THEME | KRY_THEME_TEXT);
        p = wr_u16(p, 7);            /* text_off "%.1f" */
        p = wr_u16(p, 16);
        *p++ = 0;
        *p++ = 0;
        memcpy(p, fstrings, sizeof(fstrings));
        p += sizeof(fstrings);
        *p++ = KRB_OP_DRAW_TREE;
        len = (size_t)(p - buf);

        memset(&img, 0, sizeof(img));
        if(KrbLoad(&img, buf, len) != 0)
            return fail("load float image");
        if(KrbBindMem(&img, "speed", &speed, KRB_F32, 4) != 0)
            return fail("bind float");
        cap = KryBackendNull;
        cap.text = cap_text;
        KryBackendSelect(&cap);
        g_last_text[0] = '\0';
        KrbDraw(&img, 0, 0, 200, 80);
        if(strcmp(g_last_text, "3.1") != 0)
            return fail("float text format");
    }

    /* CHECKBOX read/render/click-toggle through a mount. The cartridge owns the
     * toggle: on an in-bounds press it flips the bound value. */
    {
        static const char cstr[] = "\0cb\0Flag\0";  /* 0="" 1="cb" 4="Flag" */
        unsigned char *p = buf;
        KryBackend cap;
        int cbval;

        p = wr_u32(p, KRB_MAGIC);
        p = wr_u16(p, KRB_VERSION);
        p = wr_u16(p, 0);
        p = wr_u32(p, 1);
        p = wr_u32(p, (unsigned)sizeof(cstr));
        p = wr_u32(p, 1);
        p = wr_u32(p, 0);
        p = wr_u32(p, 0);
        p = wr_u32(p, 0);
        p = wr_u16(p, 0);                       /* id */
        p = wr_u16(p, (unsigned)-1);            /* parent */
        p = wr_u16(p, 1);                       /* name_off "cb" */
        *p++ = KRB_NODE_CHECKBOX;
        *p++ = KRB_FLAG_SCALE_W | KRB_FLAG_SCALE_H;
        p = wr_u16(p, 1);                       /* bind_slot = id */
        p = wr_u16(p, 0);                       /* x */
        p = wr_u16(p, 0);                       /* y */
        p = wr_u16(p, 16);                      /* w */
        p = wr_u16(p, 16);                      /* h */
        p = wr_u32(p, KRB_COLOR_THEME | KRY_THEME_TEXT);
        p = wr_u16(p, 4);                       /* text_off "Flag" */
        p = wr_u16(p, 16);                      /* font_size */
        *p++ = 0;
        *p++ = 0;
        memcpy(p, cstr, sizeof(cstr));
        p += sizeof(cstr);
        *p++ = KRB_OP_DRAW_TREE;
        len = (size_t)(p - buf);

        KrbFree(&img);
        memset(&img, 0, sizeof(img));
        if(KrbLoad(&img, buf, len) != 0)
            return fail("load checkbox image");
        cbval = 0;
        if(KrbBindMem(&img, "cb", &cbval, KRB_I32, 4) != 0)
            return fail("bind checkbox");
        cap = KryBackendNull;
        cap.mouse = cap_mouse;
        cap.mouse_pressed = cap_pressed;
        KryBackendSelect(&cap);
        g_cap_pressed = 1;
        KrbDraw(&img, 0, 0, 200, 80);
        if(cbval != 1)
            return fail("checkbox did not flip on press");
        g_cap_pressed = 0;
        KrbDraw(&img, 0, 0, 200, 80);
        if(cbval != 1)
            return fail("checkbox flipped without press");
    }

    /* SLIDER drag: an in-bounds held press sets the value from the mouse
     * position across [min,max]. Exercises the controls[] table + CONTROL node
     * + the new control_count header field. */
    {
        static const char sstr[] = "\0sv\0";  /* 0="" 1="sv" */
        unsigned char *p = buf;
        KryBackend cap;
        int sv;

        p = wr_u32(p, KRB_MAGIC);
        p = wr_u16(p, KRB_VERSION);
        p = wr_u16(p, 0);
        p = wr_u32(p, 1);
        p = wr_u32(p, (unsigned)sizeof(sstr));
        p = wr_u32(p, 1);
        p = wr_u32(p, 0);
        p = wr_u32(p, 1);                       /* control_count */
        p = wr_u32(p, 0);                       /* pad to 32 */
        p = wr_u16(p, 0);
        p = wr_u16(p, (unsigned)-1);
        p = wr_u16(p, 1);                       /* name_off "sv" */
        *p++ = KRB_NODE_CONTROL;
        *p++ = 0;
        p = wr_u16(p, 0);                       /* bind_slot = control 0 */
        p = wr_u16(p, 0);
        p = wr_u16(p, 0);
        p = wr_u16(p, 100);                     /* w */
        p = wr_u16(p, 16);                      /* h */
        p = wr_u32(p, KRB_COLOR_THEME | KRY_THEME_TEXT);
        p = wr_u16(p, 0);
        p = wr_u16(p, 16);
        *p++ = 0;
        *p++ = 0;
        memcpy(p, sstr, sizeof(sstr));
        p += sizeof(sstr);
        *p++ = KRB_OP_DRAW_TREE;
        *p++ = KRB_CTRL_SLIDER;
        *p++ = 0;
        p = wr_u16(p, 1);
        p = wr_u32(p, 0);                       /* min */
        p = wr_u32(p, 100);                     /* max */
        p = wr_u32(p, 1);                       /* step */
        p = wr_u16(p, 1);                       /* value_off "sv" */
        p = wr_u16(p, 0);
        p = wr_u16(p, 0);
        p = wr_u16(p, 0);
        len = (size_t)(p - buf);

        KrbFree(&img);
        memset(&img, 0, sizeof(img));
        if(KrbLoad(&img, buf, len) != 0)
            return fail("load slider image");
        sv = 0;
        if(KrbBindMem(&img, "sv", &sv, KRB_I32, 4) != 0)
            return fail("bind slider");
        cap = KryBackendNull;
        cap.mouse = cap_mouse;
        cap.mouse_down = cap_down;
        KryBackendSelect(&cap);
        g_cap_mx = 80;
        g_cap_my = 8;
        g_cap_down = 1;
        KrbDraw(&img, 0, 0, 200, 80);
        if(sv != 80)
            return fail("slider drag did not set value");
        g_cap_down = 0;
        KrbDraw(&img, 0, 0, 200, 80);
        if(sv != 80)
            return fail("slider moved without drag");
    }

    /* TEXTINPUT must encode astral codepoints as 4-byte UTF-8. */
    {
        static const char istr[] = "\0value\0";  /* 0="" 1="value" */
        unsigned char *p = buf;
        KryBackend cap;
        char value[32] = "";

        p = wr_u32(p, KRB_MAGIC);
        p = wr_u16(p, KRB_VERSION);
        p = wr_u16(p, 0);
        p = wr_u32(p, 1);
        p = wr_u32(p, (unsigned)sizeof(istr));
        p = wr_u32(p, 1);
        p = wr_u32(p, 0);
        p = wr_u32(p, 0);
        p = wr_u32(p, 0);
        p = wr_u16(p, 0);
        p = wr_u16(p, (unsigned)-1);
        p = wr_u16(p, 1);                       /* name_off "value" */
        *p++ = KRB_NODE_TEXTINPUT;
        *p++ = 0;
        p = wr_u16(p, 0xffff);
        p = wr_u16(p, 0);
        p = wr_u16(p, 0);
        p = wr_u16(p, 100);
        p = wr_u16(p, 24);
        p = wr_u32(p, KRB_COLOR_THEME | KRY_THEME_TEXT);
        p = wr_u16(p, 0);
        p = wr_u16(p, 16);
        *p++ = 0;
        *p++ = 0;
        memcpy(p, istr, sizeof(istr));
        p += sizeof(istr);
        *p++ = KRB_OP_DRAW_TREE;
        len = (size_t)(p - buf);

        KrbFree(&img);
        memset(&img, 0, sizeof(img));
        if(KrbLoad(&img, buf, len) != 0)
            return fail("load textinput image");
        if(KrbBindMem(&img, "value", value, KRB_CSTR, sizeof(value)) != 0)
            return fail("bind textinput value");
        snprintf(img.focus_path, sizeof(img.focus_path), "%s", "value");
        cap = KryBackendNull;
        cap.text_key = cap_text_key;
        KryBackendSelect(&cap);
        g_text_keys[0] = 0x1f600;
        g_text_keys[1] = 0;
        g_text_key_index = 0;
        KrbDraw(&img, 0, 0, 200, 80);
        if(strcmp(value, "\xf0\x9f\x98\x80") != 0)
            return fail("textinput emoji encoding");
    }

    KrbFree(&img);
    printf("ok mount+opcodes\n");
    return 0;
}
