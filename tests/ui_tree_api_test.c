#include "kryon.h"
#include "kry_inject.h"
#include "theme.h"
#include <stdio.h>
#include <string.h>

static int failures;

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
    LabelTextFieldProps field = {.field_h = 40};
    SectionLabelProps section = {0};
    CheckboxRowProps checkbox = {0};
    ButtonRowProps row = {.width = 240, .height = 40};
    BottomNavProps nav = {0};
    TabBarProps tabs = {0};
    const UIWidgetNode *nodes;
    const UIWidgetNode *node;
    UINodeId group;
    UINodeId nested;
    UIKey stable_key;
    UIEvent event;
    int count = 0;

    SetThemeStyle(THEME_STYLE_RETRO);

    check_int("section label",
              UIGetNodeHeight(UINodeSectionLabel(section, 0, 0)),
              ScaleUIPx(24));
    check_int("checkbox row",
              UIGetNodeHeight(UINodeCheckboxRow(checkbox, 0, 0)),
              ScaleUIPx(42));
    check_int("label text field",
              UIGetNodeHeight(UINodeLabelTextField(field, 0, 0, 240)),
              ScaleUIPx(22) + ScaleUIPx(40) + ScaleUIPx(24));
    check_int("button row",
              UIGetNodeHeight(UINodeButtonRow(row)),
              ScaleUIPx(40));
    check_int("bottom nav",
              UIGetNodeHeight(UINodeBottomNav(nav)),
              ScaleUIPx(40));
    SetThemeStyle(THEME_STYLE_MATERIAL);
    check_int("material bottom nav",
              UIGetNodeHeight(UINodeBottomNav(nav)),
              ScaleUIPx(64));
    SetThemeStyle(THEME_STYLE_RETRO);
    check_int("retro tab bar",
              UIGetNodeHeight(UINodeTabBar(tabs)),
              ScaleUIPx(36));
    SetThemeStyle(THEME_STYLE_MATERIAL);
    check_int("material tab bar",
              UIGetNodeHeight(UINodeTabBar(tabs)),
              ScaleUIPx(48));
    check_int("title bar custom",
              UIGetNodeHeight(UINodeTitleBar(64)),
              64);

    BeginUI(7);
    group = Stack((ColumnProps){.bounds = {10, 10, 100, 80}, .key = 11});
    nested = Stack((ColumnProps){.bounds = {20, 20, 40, 30}, .key = 12});
    End();
    End();
    EndUI();

    nodes = UIGetTreeNodes(&count);
    check_int("tree count", count, 3);
    check_int("root parent", nodes[0].parent, -1);
    check_int("root first child", nodes[0].first_child, group);
    check_int("group parent", nodes[group].parent, 0);
    check_int("group first child", nodes[group].first_child, nested);
    check_int("nested parent", nodes[nested].parent, group);
    check_int("hit nested", UIHitTestNode((Vector2){25, 25}), nested);
    node = UIGetNode(group);
    check_int("get group", node != NULL ? node->id : -1, 11);

    stable_key = Key("settings/password");
    check_int("key is stable",
              stable_key == Key("settings/password"), 1);
    check_int("different keys differ",
              stable_key != Key("settings/username"), 1);

    /* The retained tree grows dynamically; the old implementation silently
     * stopped at 4096 declarations. */
    BeginUI(19);
    for(int i = 0; i < 5000; i++) {
        Stack((ColumnProps){.bounds = {0, 0, 1, 1},
                            .key = (UIKey)(1000 + i)});
        End();
    }
    EndUI();
    nodes = UIGetTreeNodes(&count);
    check_int("dynamic tree count", count, 5001);
    check_int("dynamic last id", nodes[5000].id, 5999);

    /* Reconciliation retains node-owned state by parent/key/type. */
    ((UIWidgetNode *)&nodes[2500])->state = (void *)0x1234;
    BeginUI(19);
    for(int i = 0; i < 5000; i++) {
        Stack((ColumnProps){.bounds = {0, 0, 2, 2},
                            .key = (UIKey)(1000 + i)});
        End();
    }
    EndUI();
    nodes = UIGetTreeNodes(&count);
    check_int("reconcile preserves state",
              nodes[2500].state == (void *)0x1234, 1);

    BeginUI(23);
    Stack((ColumnProps){.bounds = {0, 0, 10, 10}, .key = 1}); End();
    Stack((ColumnProps){.bounds = {0, 0, 10, 10}, .key = 2}); End();
    EndUI();
    nodes = UIGetTreeNodes(&count);
    ((UIWidgetNode *)&nodes[1])->state = (void *)0x1111;
    ((UIWidgetNode *)&nodes[2])->state = (void *)0x2222;
    BeginUI(23);
    Stack((ColumnProps){.bounds = {0, 0, 10, 10}, .key = 2}); End();
    Stack((ColumnProps){.bounds = {0, 0, 10, 10}, .key = 1}); End();
    EndUI();
    nodes = UIGetTreeNodes(&count);
    check_int("reconcile reordered first",
              nodes[1].state == (void *)0x2222, 1);
    check_int("reconcile reordered second",
              nodes[2].state == (void *)0x1111, 1);

    BeginUI(31);
    Column((ColumnProps){.bounds = {10, 20, 100, 200},
                         .gap = 5, .padding = 10, .key = 40});
    Stack((ColumnProps){.bounds = {0, 0, 0, 20}, .key = 41}); End();
    Stack((ColumnProps){.bounds = {0, 0, 0, 30}, .key = 42}); End();
    End();
    EndUI();
    nodes = UIGetTreeNodes(&count);
    check_int("column first x", (int)nodes[2].bounds.x, 20);
    check_int("column first y", (int)nodes[2].bounds.y, 30);
    check_int("column stretch width", (int)nodes[2].bounds.width, 80);
    check_int("column second y", (int)nodes[3].bounds.y, 55);

    KryonInjectReset();
    KryonInjectTap(25, 25);
    KryonInjectPump();
    BeginUI(44);
    Button((ButtonProps){.bounds = {10, 10, 100, 40},
                         .label = "Save", .id = 9001});
    UIReconcileTree();
    UILayoutTree();
    UIRouteInput();
    check_int("button queues event", NextUIEvent(&event), 1);
    check_int("button event kind", event.kind, UI_EVENT_CLICK);
    check_int("button event key", (int)event.key, 9001);
    check_int("event delivered once", NextUIEvent(&event), 0);

    {
        char password[32] = "secret";
        int cursor = 6;
        int focused = 0;
        UIKey password_key = 77;
        int saw_text = 0;
        int saw_selection = 0;

        KryonInjectReset();
        KryonInjectTap(25, 25);
        KryonInjectPump();
        BeginUI(45);
        TextField((TextFieldProps){
            .bounds = {10, 10, 200, 40}, .text = password,
            .text_size = sizeof(password), .cursor_position = &cursor,
            .focused = &focused, .focus_id = (int)password_key, .secure = 1
        });
        UIReconcileTree();
        UILayoutTree();
        UIRouteInput();
        while(NextUIEvent(&event)) { }
        check_int("textfield focused", focused, 1);
        check_int("textfield selection set",
                  SetSelection(password_key, 0, 6), 1);
        while(NextUIEvent(&event)) { }
        KryonInjectText("x");
        KryonInjectPump();
        UIRouteInput();
        while(NextUIEvent(&event)) {
            if(event.kind == UI_EVENT_TEXT_CHANGED)
                saw_text = 1;
            if(event.kind == UI_EVENT_SELECTION_CHANGED &&
               event.data.selection.start == 1 &&
               event.data.selection.end == 1)
                saw_selection = 1;
        }
        check_int("selection typing replaces password", strcmp(password, "x"), 0);
        check_int("replacement cursor", cursor, 1);
        check_int("replacement text event", saw_text, 1);
        check_int("replacement selection event", saw_selection, 1);
    }

    {
        char value[32] = "abcdef";
        int cursor = 6;
        int focused = 0;
        int committed = 0;
        int saw_commit = 0;

        BeginUI(46);
        TextField((TextFieldProps){
            .bounds = {10, 10, 220, 40}, .text = value,
            .text_size = sizeof(value), .cursor_position = &cursor,
            .focused = &focused, .focus_id = 78, .font = 16,
            .commit_pressed = &committed
        });
        UIReconcileTree();
        UILayoutTree();
        KryonInjectReset();
        KryonInjectMousePosition(20, 25);
        KryonInjectMouseButton(MOUSE_BUTTON_LEFT, 1);
        KryonInjectPump();
        UIRouteInput();
        KryonInjectMousePosition(220, 25);
        KryonInjectPump();
        UIRouteInput();
        KryonInjectMouseButton(MOUSE_BUTTON_LEFT, 0);
        KryonInjectPump();
        UIRouteInput();
        KryonInjectText("z");
        KryonInjectPump();
        UIRouteInput();
        check_int("mouse selection typing replaces text", strcmp(value, "z"), 0);
        check_int("mouse replacement cursor", cursor, 1);
        while(NextUIEvent(&event)) { }
        KryonInjectKeyTap(KEY_ENTER);
        KryonInjectPump();
        UIRouteInput();
        while(NextUIEvent(&event)) {
            if(event.kind == UI_EVENT_TEXT_COMMIT && event.key == 78)
                saw_commit = 1;
        }
        check_int("textfield enter commits", committed, 1);
        check_int("textfield commit event", saw_commit, 1);
    }

    /* Keyboard focus is a core traversal contract: every focusable widget
     * registers in visual declaration order, and Tab / Shift+Tab move through
     * that one shared order. */
    {
        Rectangle first = {10, 10, 100, 30};
        Rectangle second = {10, 50, 100, 30};
        Rectangle third = {10, 90, 100, 30};

        KryonInjectReset();
        SetUIFocus(901);
        KryonInjectKeyTap(KEY_TAB);
        KryonInjectPump();
        BeginUIFocus();
        RegisterUIFocus(901, first);
        RegisterUIFocus(902, second);
        RegisterUIFocus(903, third);
        EndUIFocus();
        check_int("tab advances focus", IsUIFocusActive(902), 1);
        KryonInjectPump();

        KryonInjectKey(KEY_LEFT_SHIFT, 1);
        KryonInjectKeyTap(KEY_TAB);
        KryonInjectPump();
        BeginUIFocus();
        RegisterUIFocus(901, first);
        RegisterUIFocus(902, second);
        RegisterUIFocus(903, third);
        EndUIFocus();
        check_int("shift tab reverses focus", IsUIFocusActive(901), 1);
        KryonInjectKey(KEY_LEFT_SHIFT, 0);
        KryonInjectPump();
    }

    /* Retained declarations use the same traversal path as generated Kry
     * programs: after Tab, text must enter the next declared field. */
    {
        char first[16] = "one";
        char second[16] = "";
        int first_cursor = 3, second_cursor = 0;
        int first_focused = 0, second_focused = 0;

        KryonInjectReset();
        KryonInjectMousePosition(20, 20);
        KryonInjectMouseButton(MOUSE_BUTTON_LEFT, 1);
        KryonInjectPump();
        BeginUIFocus();
        BeginUI(1000);
        TextField((TextFieldProps){ .bounds = {10, 10, 160, 30},
            .text = first, .text_size = sizeof(first),
            .cursor_position = &first_cursor, .focused = &first_focused,
            .focus_id = 1001, .font = 16 });
        TextField((TextFieldProps){ .bounds = {10, 50, 160, 30},
            .text = second, .text_size = sizeof(second),
            .cursor_position = &second_cursor, .focused = &second_focused,
            .focus_id = 1002, .font = 16 });
        EndUI();
        EndUIFocus();
        KryonInjectMouseButton(MOUSE_BUTTON_LEFT, 0);
        KryonInjectPump();

        KryonInjectKeyTap(KEY_TAB);
        KryonInjectPump();
        BeginUIFocus();
        BeginUI(1000);
        TextField((TextFieldProps){ .bounds = {10, 10, 160, 30}, .text = first,
            .text_size = sizeof(first), .cursor_position = &first_cursor,
            .focused = &first_focused, .focus_id = 1001, .font = 16 });
        TextField((TextFieldProps){ .bounds = {10, 50, 160, 30}, .text = second,
            .text_size = sizeof(second), .cursor_position = &second_cursor,
            .focused = &second_focused, .focus_id = 1002, .font = 16 });
        EndUI();
        EndUIFocus();

        KryonInjectPump();
        KryonInjectText("x");
        KryonInjectPump();
        BeginUIFocus();
        BeginUI(1000);
        TextField((TextFieldProps){ .bounds = {10, 10, 160, 30}, .text = first,
            .text_size = sizeof(first), .cursor_position = &first_cursor,
            .focused = &first_focused, .focus_id = 1001, .font = 16 });
        TextField((TextFieldProps){ .bounds = {10, 50, 160, 30}, .text = second,
            .text_size = sizeof(second), .cursor_position = &second_cursor,
            .focused = &second_focused, .focus_id = 1002, .font = 16 });
        EndUI();
        EndUIFocus();
        check_int("retained tab focuses next field", strcmp(second, "x"), 0);
    }

    return failures == 0 ? 0 : 1;
}
