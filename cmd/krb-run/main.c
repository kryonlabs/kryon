/*
 * krb-run — headless KRB cartridge runner (plan 11, phase 1).
 *
 * Loads a .krb, renders it with the kry_sw software rasterizer
 * (optionally wrapped in the recording backend), and writes the frame as
 * a PNG and/or the vtable call stream as text. Golden files for engine
 * conformance are produced with --png/--record and compared by
 * tests/krb_engine_test.sh.
 *
 * --backend fb (plan 11, phase 2) presents frames on the Linux framebuffer
 * (/dev/fb0 by default, or $FB_DEVICE / --fb-device). The device may also
 * be a regular file: kry_sw renders into it as a raw framebuffer with the
 * --fb-format pixel layout and the --w/--h geometry, which makes the
 * conversion testable without console hardware.
 *
 * Usage: krb-run [--png out.png] [--record out.txt] [--w W] [--h H]
 *                [--frames N | --backend fb [--fb-device PATH]
 *                 [--fb-format xrgb8888|rgb565]] file.krb
 */

#include "krb.h"
#include "kry_backend_rec.h"
#include "kry_sw.h"
#include "png_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#define KRB_RUN_FB 1
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/fb.h>
#endif

#ifdef KRB_RUN_FB
static volatile sig_atomic_t g_fb_done;

static void
fb_sigint(int signo)
{
    (void)signo;
    g_fb_done = 1;
}

typedef struct FbTarget {
    unsigned char *mem; /* mmap'd device or malloc'd file image */
    size_t len;
    int fd;
    int w;
    int h;
    int bpp;      /* bits per pixel from the device or the format flag */
    int roff;     /* bit offsets for the packed formats */
    int goff;
    int boff;
    int is_file;  /* regular-file target: written back on close */
    char path[256];
} FbTarget;

/* Open the framebuffer (or a raw file stand-in) and read its geometry. */
static int
fb_open(FbTarget *fb, const char *device, const char *format, int w, int h)
{
    struct fb_var_screeninfo vi;
    struct fb_fix_screeninfo fi;

    memset(fb, 0, sizeof(*fb));
    snprintf(fb->path, sizeof(fb->path), "%s", device);
    fb->fd = open(device, O_RDWR | O_CREAT, 0644);
    if(fb->fd < 0) {
        fprintf(stderr, "krb-run: cannot open %s\n", device);
        return -1;
    }
    fb->is_file = ioctl(fb->fd, FBIOGET_VSCREENINFO, &vi) != 0 ||
                  ioctl(fb->fd, FBIOGET_FSCREENINFO, &fi) != 0;
    if(!fb->is_file) {
        if(vi.bits_per_pixel != 32 && vi.bits_per_pixel != 16) {
            fprintf(stderr, "krb-run: %s: unsupported %u bpp\n",
                    device, vi.bits_per_pixel);
            close(fb->fd);
            return -1;
        }
        if(fi.type != FB_TYPE_PACKED_PIXELS) {
            fprintf(stderr, "krb-run: %s: not packed-pixels\n", device);
            close(fb->fd);
            return -1;
        }
        fb->w = (int)vi.xres;
        fb->h = (int)vi.yres;
        fb->bpp = (int)vi.bits_per_pixel;
        fb->roff = (int)vi.red.offset;
        fb->goff = (int)vi.green.offset;
        fb->boff = (int)vi.blue.offset;
        fb->len = (size_t)fi.smem_len;
        if(w > 0 && h > 0 && (w != fb->w || h != fb->h))
            fprintf(stderr,
                    "krb-run: note: --w/--h ignored, %s is %dx%d\n",
                    device, fb->w, fb->h);
    } else {
        /* raw file target: geometry comes from --w/--h and --fb-format */
        fb->w = w > 0 ? w : 800;
        fb->h = h > 0 ? h : 600;
        fb->bpp = format != NULL && strcmp(format, "rgb565") == 0 ? 16 : 32;
        fb->roff = 16;
        fb->goff = 8;
        fb->boff = 0;
        fb->len = (size_t)fb->w * fb->h * (fb->bpp / 8);
        if(ftruncate(fb->fd, (off_t)fb->len) != 0) {
            fprintf(stderr, "krb-run: cannot size %s\n", device);
            close(fb->fd);
            return -1;
        }
    }
    fb->mem = mmap(NULL, fb->len, PROT_READ | PROT_WRITE, MAP_SHARED,
                   fb->fd, 0);
    if(fb->mem == MAP_FAILED) {
        fprintf(stderr, "krb-run: cannot map %s\n", device);
        close(fb->fd);
        return -1;
    }
    memset(fb->mem, 0, fb->len);
    return 0;
}

static void
fb_close(FbTarget *fb)
{
    if(fb->mem != NULL && fb->mem != MAP_FAILED) {
        if(fb->is_file && msync(fb->mem, fb->len, MS_SYNC) != 0) {
            /* fall back to an explicit write-back for filesystems without
             * shared-file mmap semantics */
            if(lseek(fb->fd, 0, SEEK_SET) == 0 &&
               write(fb->fd, fb->mem, fb->len) != (ssize_t)fb->len)
                fprintf(stderr, "krb-run: warning: %s may be truncated\n",
                        fb->path);
        }
        munmap(fb->mem, fb->len);
    }
    if(fb->fd >= 0)
        close(fb->fd);
}

/* Blit the kry_sw dirty rect into the framebuffer, converting RGBA8 to
 * the target layout (32-bit truecolor by bit offsets, or RGB565). */
static void
fb_present(const FbTarget *fb, const KrySw *sw)
{
    int dx, dy, dw, dh;
    int x, y;

    KrySwDirty((KrySw *)sw, &dx, &dy, &dw, &dh);
    if(dw <= 0 || dh <= 0)
        return;
    if(dx + dw > sw->w)
        dw = sw->w - dx;
    if(dy + dh > sw->h)
        dh = sw->h - dy;
    if(dx + dw > fb->w) {
        if(dx >= fb->w)
            return;
        dw = fb->w - dx;
    }
    if(dy + dh > fb->h) {
        if(dy >= fb->h)
            return;
        dh = fb->h - dy;
    }
    for(y = 0; y < dh; y++) {
        const unsigned char *src = sw->pixels +
            (size_t)(dy + y) * sw->stride + (size_t)dx * 4;
        unsigned char *dst = fb->mem +
            (size_t)(dy + y) * fb->w * (fb->bpp / 8) +
            (size_t)dx * (fb->bpp / 8);

        for(x = 0; x < dw; x++) {
            unsigned r = src[x * 4];
            unsigned g = src[x * 4 + 1];
            unsigned b = src[x * 4 + 2];

            if(fb->bpp == 32) {
                unsigned px = 0;

                px |= (r << fb->roff) | (g << fb->goff) | (b << fb->boff);
                if(fb->roff == 16 && fb->goff == 8 && fb->boff == 0) {
                    /* xrgb8888: force opaque */
                    px |= 0xffu << 24;
                }
                dst[x * 4 + 0] = (unsigned char)(px & 0xff);
                dst[x * 4 + 1] = (unsigned char)((px >> 8) & 0xff);
                dst[x * 4 + 2] = (unsigned char)((px >> 16) & 0xff);
                dst[x * 4 + 3] = (unsigned char)((px >> 24) & 0xff);
            } else {
                unsigned short px = (unsigned short)
                    (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));

                dst[x * 2 + 0] = (unsigned char)(px & 0xff);
                dst[x * 2 + 1] = (unsigned char)(px >> 8);
            }
        }
    }
}
#endif /* KRB_RUN_FB */

int
main(int argc, char **argv)
{
    const char *png_path = NULL;
    const char *rec_path = NULL;
    const char *krb_path = NULL;
    const char *fb_device = NULL;
    const char *fb_format = NULL;
    int backend_fb = 0;
    int w = 800;
    int h = 600;
    int frames = 1;
    int i;
    KrbImage img;
    KrySw sw;
    KryBackendRec rec;
    long calls = -1;
    FILE *recf = NULL;

    for(i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--png") == 0 && i + 1 < argc)
            png_path = argv[++i];
        else if(strcmp(argv[i], "--record") == 0 && i + 1 < argc)
            rec_path = argv[++i];
        else if(strcmp(argv[i], "--w") == 0 && i + 1 < argc)
            w = atoi(argv[++i]);
        else if(strcmp(argv[i], "--h") == 0 && i + 1 < argc)
            h = atoi(argv[++i]);
        else if(strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            frames = atoi(argv[++i]);
        else if(strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            if(strcmp(argv[i + 1], "fb") != 0) {
                fprintf(stderr, "krb-run: unknown backend %s\n", argv[i + 1]);
                return 2;
            }
            backend_fb = 1;
            i++;
        }
        else if(strcmp(argv[i], "--fb-device") == 0 && i + 1 < argc)
            fb_device = argv[++i];
        else if(strcmp(argv[i], "--fb-format") == 0 && i + 1 < argc)
            fb_format = argv[++i];
        else if(argv[i][0] != '-' && krb_path == NULL)
            krb_path = argv[i];
        else {
            fprintf(stderr,
                    "usage: krb-run [--png out.png] [--record out.txt]"
                    " [--w W] [--h H] [--frames N]\n"
                    "                [--backend fb [--fb-device PATH]"
                    " [--fb-format xrgb8888|rgb565]] file.krb\n");
            return 2;
        }
    }
    if(krb_path == NULL) {
        fprintf(stderr, "krb-run: no cartridge given\n");
        return 2;
    }
#ifdef KRB_RUN_FB
    if(backend_fb) {
        FbTarget fb;
        const char *device = fb_device != NULL ? fb_device : getenv("FB_DEVICE");
        int frame;

        if(device == NULL)
            device = "/dev/fb0";
        if(fb_open(&fb, device, fb_format, w, h) != 0)
            return 1;
        w = fb.w;
        h = fb.h;
        if(KrbLoadFile(&img, krb_path) != 0) {
            fprintf(stderr, "krb-run: load failed: %s\n", krb_path);
            fb_close(&fb);
            return 1;
        }
        if(KrySwInit(&sw, NULL, w, h) != 0) {
            fprintf(stderr, "krb-run: rasterizer init failed\n");
            fb_close(&fb);
            return 1;
        }
        {
            const char *uis = getenv("KRB_RUN_UI_SCALE");

            if(uis != NULL) {
                int pm = (int)(atof(uis) * 1000.0f + 0.5f);

                if(pm > 0 && pm <= 8000)
                    sw.scale = pm;
            }
        }
        KrbAutoMount(&img);
        {
            const unsigned char *ad = NULL;
            unsigned al = 0;
            unsigned ak = 0;

            if(KrbAssetFind(&img, "@atlas", &ad, &al, &ak, NULL, NULL) == 0 &&
               ak == 1)
                KrySwSetAtlas(&sw, ad, al);
        }
        KryBackendSelect(KrySwBackend(&sw));
        signal(SIGINT, fb_sigint);
        fprintf(stderr, "krb-run: presenting %s on %s (%dx%d %d bpp)\n",
                krb_path, device, fb.w, fb.h, fb.bpp);
        for(frame = 0; !g_fb_done; frame++) {
            KrySwAdvance(&sw, 1.0f / 60.0f);
            KrbDraw(&img, 0, 0, w, h);
            fb_present(&fb, &sw);
            if(!fb.is_file && frames > 0 && frame + 1 >= frames)
                break;
            if(fb.is_file && frames > 0 && frame + 1 >= frames)
                break;
            usleep(16667);
        }
        if(png_path != NULL &&
           krb_png_write(png_path, sw.pixels, sw.w, sw.h) != 0) {
            fprintf(stderr, "krb-run: cannot write %s\n", png_path);
            fb_close(&fb);
            return 1;
        }
        fb_close(&fb);
        KrbFree(&img);
        KrySwFree(&sw);
        printf("krb-run ok %s fb %dx%d\n", krb_path, w, h);
        return 0;
    }
#else
    if(backend_fb) {
        fprintf(stderr,
                "krb-run: --backend fb is only built on Linux\n");
        return 1;
    }
#endif
    memset(&img, 0, sizeof(img));
    if(KrbLoadFile(&img, krb_path) != 0) {
        fprintf(stderr, "krb-run: load failed: %s\n", krb_path);
        return 1;
    }
    if(KrySwInit(&sw, NULL, w, h) != 0) {
        fprintf(stderr, "krb-run: rasterizer init failed\n");
        return 1;
    }
    {
        const char *uis = getenv("KRB_RUN_UI_SCALE");

        if(uis != NULL) {
            int pm = (int)(atof(uis) * 1000.0f + 0.5f);

            if(pm > 0 && pm <= 8000)
                sw.scale = pm;
        }
    }
    if(getenv("KRB_RUN_THEME_LIGHT") != NULL) {
        KrySwSetTheme(&sw, KRY_THEME_BACKGROUND, 0xffffffffu);
        KrySwSetTheme(&sw, KRY_THEME_SURFACE, 0xfbfbfbffu);
        KrySwSetTheme(&sw, KRY_THEME_ICON, 0xd3d3d3ffu);
        KrySwSetTheme(&sw, KRY_THEME_TEXT, 0x00000000u | 0xffu);
        KrySwSetTheme(&sw, KRY_THEME_BUTTON, 0xffffffffu);
    }
    if(0) {
        fprintf(stderr, "krb-run: rasterizer init failed\n");
        return 1;
    }
    KrbAutoMount(&img);
    if(rec_path != NULL) {
        recf = fopen(rec_path, "w");
        if(recf == NULL) {
            fprintf(stderr, "krb-run: cannot write %s\n", rec_path);
            return 1;
        }
        KryBackendSelect(KryBackendRecBackend(&rec, recf, KrySwBackend(&sw)));
    } else {
        {
        const unsigned char *ad = NULL;
        unsigned al = 0;
        unsigned ak = 0;

        if(KrbAssetFind(&img, "@atlas", &ad, &al, &ak, NULL, NULL) == 0 &&
           ak == 1)
            KrySwSetAtlas(&sw, ad, al);
    }
    KryBackendSelect(KrySwBackend(&sw));
    }
    {
        int f;

        for(f = 0; f < frames; f++) {
            KrySwAdvance(&sw, 1.0f / 60.0f);
            KrbDraw(&img, 0, 0, w, h);
        }
    }
    if(recf != NULL) {
        fclose(recf);
        calls = KryBackendRecCalls(&rec);
    }
    if(png_path != NULL && krb_png_write(png_path, sw.pixels, sw.w, sw.h) != 0) {
        fprintf(stderr, "krb-run: cannot write %s\n", png_path);
        return 1;
    }
    KrbFree(&img);
    KrySwFree(&sw);
    printf("krb-run ok %s %dx%d calls=%ld\n", krb_path, w, h, calls);
    return 0;
}
