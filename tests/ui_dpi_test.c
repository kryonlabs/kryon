#include "kryon.h"

#include <stdio.h>

#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(PLATFORM_WEB)
Vector2
GetWindowScaleDPI(void)
{
    return (Vector2){1.0f, 1.0f};
}
#endif

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
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    check_scale_hundredths("android startup viewport fallback", GetUIDPIScale(), 250);
#else
    check_scale_hundredths("desktop ignores viewport height", GetUIDPIScale(), 100);
#endif

    SetUIDeviceDensity(1.75f);
    UpdateUIDPI(720, 1400);
    check_scale_hundredths("android density ignores tall aspect ratio", GetUIDPIScale(), 175);

    SetUIDeviceDensity(3.0f);
    UpdateUIDPI(320, 560);
    check_scale_hundredths("density protects small high-density viewport", GetUIDPIScale(), 300);

    return failures == 0 ? 0 : 1;
}
