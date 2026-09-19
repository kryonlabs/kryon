#include "kryon.h"
#include "kry_inject.h"
#include "../src/platform/accessibility_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int
main(void)
{
    char text[64] = "Ae\xcc\x81Z";
    char password[32] = "secret";
    int cursor = 5, password_cursor = 6;
    int clicks = 0, announced = 0;
    struct timespec delay = {0, 1000000};
    int window = getenv("KRYON_ACCESSIBILITY_TEST_WINDOW") != NULL;
    int nested = getenv("KRYON_ACCESSIBILITY_TEST_TREE") != NULL;
    SetTraceLogLevel(LOG_NONE);
    if(window)
        InitWindow(200, 160, "Kryon Accessibility Test");
    else
        ui_accessibility_platform_start("Kryon Accessibility Test");
    for(int frame = 0; frame < 15000; frame++) {
        InjectPump();
        BeginInterfaceFrame(200, 160, 1);
        BeginTree(Key("accessibility-dbus-test"));
        if(nested)
            Group((ColumnProps){.key = 501, .bounds = {5, 2, 190, 150}});
        if(Button((ButtonProps){.id = 11, .label = "Run", .bounds = {10, 5, 60, 25}}))
            clicks++;
        if(nested)
            Group((ColumnProps){.key = 502, .bounds = {8, 30, 184, 32}});
        TextField((TextFieldProps){.focus_id = 12, .text = text, .text_size = sizeof(text),
            .cursor_position = &cursor, .bounds = {10, 35, 180, 25}});
        if(nested) {
            End();
            End();
        }
        TextField((TextFieldProps){.focus_id = 13, .text = password, .text_size = sizeof(password),
            .cursor_position = &password_cursor, .secure = 1, .bounds = {10, 65, 180, 25}});
        EndTree();
        EndInterfaceFrame();
        if(!announced && ui_accessibility_platform_active()) {
            puts("READY");
            fflush(stdout);
            announced = 1;
        }
        if(clicks == 1 && strcmp(text, "\xe7\x95\x8c") == 0 && cursor == 3) {
            puts("APPLIED");
            fflush(stdout);
            if(window)
                CloseWindow();
            else
                ui_accessibility_platform_close();
            return 0;
        }
        nanosleep(&delay, NULL);
    }
    if(window)
        CloseWindow();
    else
        ui_accessibility_platform_close();
    return 1;
}
