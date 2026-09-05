#include "kryon.h"

#include <stdio.h>

static int failures;

static void
check_scale_hundredths(const char *name, float got, int want)
{
    int scaled = (int)(got * 100.0f + 0.5f);

    if(scaled == want)
        return;
    fprintf(stderr, "FAIL: %s got %.3f want %.2f\n", name, got,
            (float)want / 100.0f);
    failures++;
}

int
main(void)
{
    InitUIDPI();
    UpdateUIDPI(720, 1400);
    check_scale_hundredths("viewport scale without density", GetUIDPIScale(), 250);

    SetUIDeviceDensity(1.75f);
    UpdateUIDPI(720, 1400);
    check_scale_hundredths("android density ignores tall aspect ratio", GetUIDPIScale(), 175);

    SetUIDeviceDensity(3.0f);
    UpdateUIDPI(320, 560);
    check_scale_hundredths("density protects small high-density viewport", GetUIDPIScale(), 300);

    return failures == 0 ? 0 : 1;
}
