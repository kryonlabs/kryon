#include "kryon.h"

#include <stdio.h>

void PushInspectSource(const char *path, int line)
{
    (void)path;
    (void)line;
}

void PopInspectSource(void) {}

#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(PLATFORM_WEB)
Vector2
GetWindowScaleDPI(void)
{
    return (Vector2){1.0f, 1.0f};
}
#endif

#if defined(__FreeBSD__) && !defined(PLATFORM_ANDROID) && \
    !defined(__ANDROID__) && !defined(PLATFORM_WEB)
bool
IsWindowReady(void)
{
    return false;
}

void
glDisable(unsigned int capability)
{
    (void)capability;
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

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "FAIL: %s got %d want %d\n", name, got, want);
    failures++;
}

int
main(void)
{
    InitDPI();
    UpdateDPI(720, 1400);
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    check_scale_hundredths("android startup viewport fallback", GetDPIScale(), 250);
    check_int("android startup logical width", GetLayoutWidth(), 288);
    check_int("android startup logical height", GetLayoutHeight(), 560);
#else
    check_scale_hundredths("desktop ignores viewport height", GetDPIScale(), 100);
    check_int("desktop layout width follows window", GetLayoutWidth(), 720);
    check_int("desktop layout height follows window", GetLayoutHeight(), 1400);
#endif

    SetDeviceDensity(1.75f);
    UpdateDPI(720, 1400);
    check_scale_hundredths("android density ignores tall aspect ratio", GetDPIScale(), 175);
    check_int("density logical width", GetLayoutWidth(), 411);
    check_int("density logical height", GetLayoutHeight(), 800);

    SetDeviceDensity(3.0f);
    UpdateDPI(320, 560);
    check_scale_hundredths("density protects small high-density viewport", GetDPIScale(), 300);
    check_int("small high-density logical width", GetLayoutWidth(), 107);
    check_int("small high-density logical height", GetLayoutHeight(), 187);

    return failures == 0 ? 0 : 1;
}
