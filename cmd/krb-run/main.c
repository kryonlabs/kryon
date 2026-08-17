/*
 * krb-run — headless KRB cartridge runner (plan 11, phase 1).
 *
 * Loads a .krb, renders it once with the kry_sw software rasterizer
 * (optionally wrapped in the recording backend), and writes the frame as
 * a PNG and/or the vtable call stream as text. Golden files for engine
 * conformance are produced with --png/--record and compared by
 * tests/krb_engine_test.sh.
 *
 * Usage: krb-run [--png out.png] [--record out.txt] [--w W] [--h H] file.krb
 */

#include "krb.h"
#include "kry_backend_rec.h"
#include "kry_sw.h"
#include "png_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv)
{
    const char *png_path = NULL;
    const char *rec_path = NULL;
    const char *krb_path = NULL;
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
        else if(argv[i][0] != '-' && krb_path == NULL)
            krb_path = argv[i];
        else {
            fprintf(stderr,
                    "usage: krb-run [--png out.png] [--record out.txt]"
                    " [--w W] [--h H] file.krb\n");
            return 2;
        }
    }
    if(krb_path == NULL) {
        fprintf(stderr, "krb-run: no cartridge given\n");
        return 2;
    }
    memset(&img, 0, sizeof(img));
    if(KrbLoadFile(&img, krb_path) != 0) {
        fprintf(stderr, "krb-run: load failed: %s\n", krb_path);
        return 1;
    }
    if(KrySwInit(&sw, NULL, w, h) != 0) {
        fprintf(stderr, "krb-run: rasterizer init failed\n");
        return 1;
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
