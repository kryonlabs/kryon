/* System-theme refresh discipline regression test.
 *
 * The automatic theme queries (IsSystemThemeAvailable, SystemThemePrefersDark,
 * ...) answer per-frame widget questions. When desktop detection fails they
 * must NOT re-run the file-reading probe on every call — that re-ran the
 * whole GTK CSS scan per widget per frame and froze apps whose desktop theme
 * could not be parsed. The automatic path attempts detection at most once per
 * retry window; only the explicit RefreshSystemTheme() forces a re-run.
 *
 * The test points the CSS reader at a nonexistent HOME so detection keeps
 * failing, then asserts the refresh count stays flat across thousands of
 * queries. (When a GTK sampler is compiled in it may succeed once via the X
 * session; then the count also stays flat because the palette became
 * available. Both outcomes satisfy the no-storm assertion.) */

#define _POSIX_C_SOURCE 200809L

#include "theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void
check(int ok, const char *name)
{
    if(!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void
set_env(const char *name, const char *value)
{
    if(setenv(name, value, 1) != 0) {
        fprintf(stderr, "FAIL: setenv %s\n", name);
        failures++;
    }
}

int
main(void)
{
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *gtk_theme = getenv("GTK_THEME");
    long count_before, count_after, forced;
    int i;

    /* Detection must fail: no readable xsettings/settings.ini, no GTK_THEME. */
    set_env("GTK_THEME", "");
    set_env("XDG_CONFIG_HOME", "/nonexistent-kryon-system-theme-test");
    set_env("HOME", "/nonexistent-kryon-system-theme-test");

    count_before = SystemThemeRefreshCount();
    check(count_before == 0, "refresh count starts at zero");

    /* The storm scenario: thousands of per-frame queries. */
    for(i = 0; i < 5000; i++) {
        (void)SystemThemePrefersDark();
        (void)IsSystemThemeAvailable();
        (void)SystemThemeSupportsMode();
        (void)GetSystemThemeName();
    }
    count_after = SystemThemeRefreshCount();
    check(count_after - count_before <= 1,
          "automatic queries never re-run failed detection per call");

    /* The automatic path may attempt again only after the retry window;
       immediately repeated queries must not trigger another attempt. */
    for(i = 0; i < 5000; i++)
        (void)SystemThemePrefersDark();
    check(SystemThemeRefreshCount() == count_after,
          "rate-limited auto refresh holds within the retry window");

    /* Explicit refresh still forces a full re-detection. */
    (void)RefreshSystemTheme();
    forced = SystemThemeRefreshCount();
    check(forced > count_after, "RefreshSystemTheme forces a new attempt");
    (void)RefreshSystemTheme();
    check(SystemThemeRefreshCount() == forced + 1,
          "each explicit refresh bumps the count");

    if(home != NULL)
        set_env("HOME", home);
    else
        unsetenv("HOME");
    if(xdg != NULL)
        set_env("XDG_CONFIG_HOME", xdg);
    else
        unsetenv("XDG_CONFIG_HOME");
    if(gtk_theme != NULL)
        set_env("GTK_THEME", gtk_theme);
    else
        unsetenv("GTK_THEME");

    if(failures == 0)
        printf("system_theme_test: OK\n");
    return failures == 0 ? 0 : 1;
}
