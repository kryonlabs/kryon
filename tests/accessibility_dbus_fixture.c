#include "kryon.h"
#include "kry_inject.h"
#include "../src/platform/accessibility_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

static void
large_list_snapshot(void)
{
    enum { item_count = 4096 };
    const char *items[item_count];
    int keys[item_count];
    int selected = -1;
    for(int i = 0; i < item_count; i++) {
        items[i] = "Item";
        keys[i] = i + 1;
    }
    InjectPump();
    BeginInterfaceFrame(200, 160, 1);
    BeginTree(Key("large-accessibility-list"));
    ListBox((ListBoxProps){.id = 23, .bounds = {10, 5, 180, 40},
        .items = items, .item_count = item_count, .item_keys = keys,
        .selected_index = &selected, .row_height = 20});
    EndTree();
    EndInterfaceFrame();
    int count = GetAccessibilitySnapshot(NULL, 0);
    AccessibilityNode *nodes = calloc((size_t)count, sizeof(*nodes));
    assert(nodes != NULL);
    assert(GetAccessibilitySnapshot(nodes, count) == count);
    int options = 0;
    for(int i = 0; i < count; i++) {
        if(strcmp(nodes[i].role, "option") != 0)
            continue;
        assert(nodes[i].item_index == options);
        assert(nodes[i].key == (uint64_t)options + 1);
        assert(nodes[i].parent > 0 && nodes[i].parent <= (unsigned)i);
        assert(nodes[nodes[i].parent - 1].focus_id == 23);
        options++;
    }
    assert(options == item_count);
    free(nodes);
}

static int
list_fixture(void)
{
    large_list_snapshot();
    const char *items[] = {"Alpha", "Beta", "Gamma"};
    int keys[] = {1, 2, 3};
    int selected = -1, scroll = 0, multi[] = {0, 0, 0}, count = 0;
    int announced = 0;
    struct timespec delay = {0, 1000000};
    ui_accessibility_platform_start("Kryon Accessibility List Test");
    for(int frame = 0; frame < 15000; frame++) {
        InjectPump();
        BeginInterfaceFrame(200, 160, 1);
        BeginTree(Key("accessibility-list-test"));
        ListBox((ListBoxProps){.id = 21, .bounds = {10, 5, 180, 40},
            .items = items, .item_count = 3, .item_keys = keys,
            .selected_index = &selected, .scroll_offset = &scroll, .row_height = 20});
        ListBox((ListBoxProps){.id = 22, .bounds = {10, 65, 180, 80},
            .items = items, .item_count = 3, .item_keys = keys,
            .selected = multi, .selected_count = &count, .row_height = 20});
        EndTree();
        EndInterfaceFrame();
        if(!announced && ui_accessibility_platform_active()) {
            puts("READY");
            fflush(stdout);
            announced = 1;
        }
        if(selected == 2 && scroll > 0 && count == 2 && multi[0] && multi[2]) {
            puts("APPLIED");
            fflush(stdout);
            ui_accessibility_platform_close();
            return 0;
        }
        nanosleep(&delay, NULL);
    }
    ui_accessibility_platform_close();
    return 1;
}

int
main(void)
{
    if(getenv("KRYON_ACCESSIBILITY_TEST_LIST") != NULL)
        return list_fixture();
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
