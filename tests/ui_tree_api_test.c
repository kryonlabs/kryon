#include "kryon.h"
#include "kry_inject.h"
#include "theme.h"
#include <stdio.h>
#include <string.h>

static int failures;

static void
check_int(const char *name, int got, int want);

void __wrap_DrawRectangle(int posX, int posY, int width, int height,
                          Color color);
void __wrap_DrawRectangleRec(Rectangle rec, Color color);
void __wrap_DrawRectangleLinesEx(Rectangle rec, float lineThick,
                                 Color color);
void __wrap_DrawRectangleRounded(Rectangle rec, float roundness, int segments,
                                 Color color);
void __wrap_DrawLine(int startPosX, int startPosY, int endPosX, int endPosY,
                     Color color);
void __wrap_BeginScissorMode(int x, int y, int width, int height);
void __wrap_EndScissorMode(void);

typedef struct ScaffoldFixture {
    int seen_w;
    int closed;
} ScaffoldFixture;

static int
scaffold_height(int content_w, void *user_data)
{
    ScaffoldFixture *fixture = user_data;

    if(fixture != NULL)
        fixture->seen_w = content_w;
    return 180;
}

static int
scaffold_title(const char *title, int height, void *user_data)
{
    ScaffoldFixture *fixture = user_data;

    check_int("scaffold title text", strcmp(title, "Settings"), 0);
    check_int("scaffold title height", height, ScaleUIPx(36));
    return fixture != NULL ? fixture->closed : 0;
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
    LabelTextFieldProps field = {.field_h = 40};
    SectionLabelProps section = {0};
    CheckboxRowProps checkbox = {0};
    ButtonRowProps row = {.width = 240, .height = 40};
    UIForm form;
    Rectangle taken;
    UIScreenScaffold scaffold;
    BottomNavProps nav = {0};
    TabBarProps tabs = {0};
    const UIWidgetNode *nodes;
    const UIWidgetNode *node;
    NodeId group;
    NodeId nested;
    NodeId page;
    NodeId page_section;
    NodeId page_grid;
    NodeId grid_first;
    NodeId grid_second;
    KeyID stable_key;
    UIEvent event;
    int count = 0;
    ScaffoldFixture scaffold_fixture = {.closed = 1};

    SetThemeStyle(THEME_STYLE_RETRO);

    check_int("section label",
              GetNodeHeight(NodeSectionLabel(section, 0, 0)),
              ScaleUIPx(24));
    check_int("checkbox row",
              GetNodeHeight(NodeCheckboxRow(checkbox, 0, 0)),
              ScaleUIPx(42));
    check_int("label text field",
              GetNodeHeight(NodeLabelTextField(field, 0, 0, 240)),
              ScaleUIPx(22) + ScaleUIPx(40) + ScaleUIPx(24));
    check_int("button row",
              GetNodeHeight(NodeButtonRow(row)),
              ScaleUIPx(40));
    form = UIFormBegin(10, 20, 240);
    taken = UIFormTakeRect(&form, ScaleUIPx(18));
    check_int("form rect x", (int)taken.x, 10);
    check_int("form rect y", (int)taken.y, 20);
    check_int("form rect width", (int)taken.width, 240);
    check_int("form advances", UIFormY(&form), 20 + ScaleUIPx(18));
    BeginTree(6);
    UIFormSection(&form, "Account");
    EndTree();
    check_int("form section helper advances", UIFormY(&form),
              20 + ScaleUIPx(18) + ScaleUIPx(24));
    check_int("spinbox row height",
              GetUISpinboxRowHeight((SpinboxRowProps){0}),
              ScaleUIPx(54));
    check_int("bottom nav",
              GetNodeHeight(NodeBottomNav(nav)),
              ScaleUIPx(40));
    SetThemeStyle(THEME_STYLE_MATERIAL);
    check_int("material bottom nav",
              GetNodeHeight(NodeBottomNav(nav)),
              ScaleUIPx(64));
    SetThemeStyle(THEME_STYLE_RETRO);
    check_int("retro tab bar",
              GetNodeHeight(NodeTabBar(tabs)),
              ScaleUIPx(36));
    SetThemeStyle(THEME_STYLE_MATERIAL);
    check_int("material tab bar",
              GetNodeHeight(NodeTabBar(tabs)),
              ScaleUIPx(48));
    check_int("title bar custom",
              GetNodeHeight(NodeTitleBar(64)),
              64);

    BeginTree(7);
    group = Stack((ColumnProps){.bounds = {10, 10, 100, 80}, .key = 11});
    nested = Stack((ColumnProps){.bounds = {20, 20, 40, 30}, .key = 12});
    End();
    End();
    EndTree();

    nodes = GetTreeNodes(&count);
    check_int("tree count", count, 3);
    check_int("root parent", nodes[0].parent, -1);
    check_int("root first child", nodes[0].first_child, group);
    check_int("group parent", nodes[group].parent, 0);
    check_int("group first child", nodes[group].first_child, nested);
    check_int("nested parent", nodes[nested].parent, group);
    check_int("hit nested", HitTestNode((Vector2){25, 25}), nested);
    node = GetNode(group);
    check_int("get group", node != NULL ? node->id : -1, 11);

    stable_key = Key("settings/password");
    check_int("key is stable",
              stable_key == Key("settings/password"), 1);
    check_int("different keys differ",
              stable_key != Key("settings/username"), 1);

    /* The retained tree grows dynamically; the old implementation silently
     * stopped at 4096 declarations. */
    BeginTree(19);
    for(int i = 0; i < 5000; i++) {
        Stack((ColumnProps){.bounds = {0, 0, 1, 1},
                            .key = (KeyID)(1000 + i)});
        End();
    }
    EndTree();
    nodes = GetTreeNodes(&count);
    check_int("dynamic tree count", count, 5001);
    check_int("dynamic last id", nodes[5000].id, 5999);

    /* Reconciliation retains node-owned state by parent/key/type. */
    ((UIWidgetNode *)&nodes[2500])->state = (void *)0x1234;
    BeginTree(19);
    for(int i = 0; i < 5000; i++) {
        Stack((ColumnProps){.bounds = {0, 0, 2, 2},
                            .key = (KeyID)(1000 + i)});
        End();
    }
    EndTree();
    nodes = GetTreeNodes(&count);
    check_int("reconcile preserves state",
              nodes[2500].state == (void *)0x1234, 1);

    BeginTree(23);
    Stack((ColumnProps){.bounds = {0, 0, 10, 10}, .key = 1}); End();
    Stack((ColumnProps){.bounds = {0, 0, 10, 10}, .key = 2}); End();
    EndTree();
    nodes = GetTreeNodes(&count);
    ((UIWidgetNode *)&nodes[1])->state = (void *)0x1111;
    ((UIWidgetNode *)&nodes[2])->state = (void *)0x2222;
    BeginTree(23);
    Stack((ColumnProps){.bounds = {0, 0, 10, 10}, .key = 2}); End();
    Stack((ColumnProps){.bounds = {0, 0, 10, 10}, .key = 1}); End();
    EndTree();
    nodes = GetTreeNodes(&count);
    check_int("reconcile reordered first",
              nodes[1].state == (void *)0x2222, 1);
    check_int("reconcile reordered second",
              nodes[2].state == (void *)0x1111, 1);

    BeginTree(31);
    Column((ColumnProps){.bounds = {10, 20, 100, 200},
                         .gap = 5, .padding = 10, .key = 40});
    Stack((ColumnProps){.bounds = {0, 0, 0, 20}, .key = 41}); End();
    Stack((ColumnProps){.bounds = {0, 0, 0, 30}, .key = 42}); End();
    End();
    EndTree();
    nodes = GetTreeNodes(&count);
    check_int("column first x", (int)nodes[2].bounds.x, 20);
    check_int("column first y", (int)nodes[2].bounds.y, 30);
    check_int("column stretch width", (int)nodes[2].bounds.width, 80);
    check_int("column second y", (int)nodes[3].bounds.y, 55);
    BeginTree(31);
    Column((ColumnProps){.bounds = {10, 20, 100, 200},
                         .gap = 5, .padding = 10, .key = 40});
    Stack((ColumnProps){.bounds = {0, 0, 0, 20}, .key = 41}); End();
    Stack((ColumnProps){.bounds = {0, 0, 0, 30}, .key = 42}); End();
    End();
    EndTree();
    nodes = GetTreeNodes(&count);
    check_int("stable column keeps computed width", (int)nodes[2].bounds.width, 80);

    check_int("route fallback path", strcmp(GetRoutePath(), "/"), 0);
    check_int("route fallback hash", strcmp(GetRouteHash(), ""), 0);
    check_int("route fallback version", GetRouteVersion(), 0);

    SetUIViewSize(320, 240);
    SetThemeStyle(THEME_STYLE_RETRO);
    BeginTree(37);
    scaffold = BeginUIScreenScaffold((UIScreenScaffoldSpec){
        .title = "Settings",
        .bottom_reserved = 12,
        .max_content_width = 220,
        .min_content_width = 120,
        .content_height = scaffold_height,
        .user_data = &scaffold_fixture,
        .draw_title = scaffold_title
    });
    check_int("scaffold closed", scaffold.closed, 1);
    check_int("scaffold content y", scaffold.content_y, ScaleUIPx(36));
    check_int("scaffold content h", scaffold.content_h,
              240 - ScaleUIPx(36) - 12);
    check_int("scaffold content w", scaffold.content_w,
              scaffold_fixture.seen_w);
    EndUIScreenScaffold(scaffold);
    EndTree();

    BeginTree(38);
    page = Page((PageProps){.title = "Docs",
                            .bounds = {0, 0, 0, 0},
                            .gap = 6,
                            .padding = 8,
                            .key = 380});
    page_section = Section((SectionProps){.label = "Intro",
                                          .bounds = {0, 0, 0, 0},
                                          .gap = 2,
                                          .padding = 3,
                                          .key = 382});
    Flow((FlowProps){.bounds = {0, 0, 100, 24}, .gap = 4, .key = 383});
    Stack((ColumnProps){.bounds = {0, 0, 20, 12}, .key = 384}); End();
    Stack((ColumnProps){.bounds = {0, 0, 20, 12}, .key = 385}); End();
    End();
    End();
    page_grid = PageGrid((GridProps){.bounds = {0, 0, 100, 40},
                                     .columns = 2,
                                     .gap = 4,
                                     .padding = 4,
                                     .key = 386});
    Stack((ColumnProps){.bounds = {0, 0, 0, 10}, .key = 387}); End();
    Stack((ColumnProps){.bounds = {0, 0, 0, 12}, .key = 388}); End();
    End();
    End();
    EndTree();
    nodes = GetTreeNodes(&count);
    check_int("page fallback node kind", nodes[page].kind, UI_WIDGET_COLUMN_NODE);
    check_int("page fallback width", (int)nodes[page].bounds.width, 320);
    check_int("page fallback height", (int)nodes[page].bounds.height, 240);
    check_int("page section node kind", nodes[page_section].kind, UI_WIDGET_COLUMN_NODE);
    check_int("page grid kind", nodes[page_grid].kind, UI_WIDGET_GRID_NODE);
    grid_first = nodes[page_grid].first_child;
    grid_second = nodes[grid_first].next_sibling;
    check_int("page grid first x", (int)nodes[grid_first].bounds.x,
              (int)nodes[page_grid].bounds.x + 4);
    check_int("page grid first y", (int)nodes[grid_first].bounds.y,
              (int)nodes[page_grid].bounds.y + 4);
    check_int("page grid first width", (int)nodes[grid_first].bounds.width, 44);
    check_int("page grid second x", (int)nodes[grid_second].bounds.x,
              (int)nodes[page_grid].bounds.x + 52);
    check_int("page grid second y", (int)nodes[grid_second].bounds.y,
              (int)nodes[page_grid].bounds.y + 4);

    InjectReset();
    InjectTap(25, 25);
    InjectPump();
    BeginTree(44);
    Button((ButtonProps){.bounds = {10, 10, 100, 40},
                         .label = "Save", .id = 9001});
    ReconcileTree();
    LayoutTree();
    RouteInput();
    check_int("button queues event", NextEvent(&event), 1);
    check_int("button event kind", event.kind, UI_EVENT_CLICK);
    check_int("button event key", (int)event.key, 9001);
    check_int("event delivered once", NextEvent(&event), 0);

    {
        char password[32] = "secret";
        int cursor = 6;
        int focused = 0;
        KeyID password_key = 77;
        int saw_text = 0;
        int saw_selection = 0;

        InjectReset();
        InjectTap(25, 25);
        InjectPump();
        BeginTree(45);
        TextField((TextFieldProps){
            .bounds = {10, 10, 200, 40}, .text = password,
            .text_size = sizeof(password), .cursor_position = &cursor,
            .focused = &focused, .focus_id = (int)password_key, .secure = 1
        });
        ReconcileTree();
        LayoutTree();
        RouteInput();
        while(NextEvent(&event)) { }
        check_int("textfield focused", focused, 1);
        check_int("textfield selection set",
                  SetSelection(password_key, 0, 6), 1);
        while(NextEvent(&event)) { }
        InjectText("x");
        InjectPump();
        RouteInput();
        while(NextEvent(&event)) {
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

        BeginTree(46);
        TextField((TextFieldProps){
            .bounds = {10, 10, 220, 40}, .text = value,
            .text_size = sizeof(value), .cursor_position = &cursor,
            .focused = &focused, .focus_id = 78, .font = 16,
            .commit_pressed = &committed
        });
        ReconcileTree();
        LayoutTree();
        InjectReset();
        InjectMousePosition(20, 25);
        InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
        InjectPump();
        RouteInput();
        InjectMousePosition(220, 25);
        InjectPump();
        RouteInput();
        InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
        InjectPump();
        RouteInput();
        InjectText("z");
        InjectPump();
        RouteInput();
        check_int("mouse selection typing replaces text", strcmp(value, "z"), 0);
        check_int("mouse replacement cursor", cursor, 1);
        while(NextEvent(&event)) { }
        InjectKeyTap(KEY_ENTER);
        InjectPump();
        check_int("injected textarea enter is pressed", IsKeyPressed(KEY_ENTER), 1);
        RouteInput();
        while(NextEvent(&event)) {
            if(event.kind == UI_EVENT_TEXT_COMMIT && event.key == 78)
                saw_commit = 1;
        }
        check_int("textfield enter commits", committed, 1);
        check_int("textfield commit event", saw_commit, 1);
    }

    {
        char value[32] = "hello world";
        int cursor = 0;
        int focused = 0;
        int saw_selection = 0;

        BeginTree(47);
        TextField((TextFieldProps){
            .bounds = {10, 10, 220, 40}, .text = value,
            .text_size = sizeof(value), .cursor_position = &cursor,
            .focused = &focused, .focus_id = 79, .font = 16
        });
        ReconcileTree();
        LayoutTree();
        InjectReset();
        InjectTap(50, 25);
        InjectPump();
        RouteInput();
        InjectPump();
        RouteInput();
        while(NextEvent(&event)) { }
        InjectTap(50, 25);
        InjectPump();
        RouteInput();
        while(NextEvent(&event)) {
            if(event.kind == UI_EVENT_SELECTION_CHANGED &&
               event.data.selection.start == 0 &&
               event.data.selection.end == 11)
                saw_selection = 1;
        }
        check_int("double click selects text", saw_selection, 1);
        check_int("double click cursor at end", cursor, 11);
        InjectPump();
        RouteInput();
        InjectText("x");
        InjectPump();
        RouteInput();
        check_int("double click selection typing replaces text",
                  strcmp(value, "x"), 0);
        check_int("double click replacement cursor", cursor, 1);
    }

    /* Keyboard focus is a core traversal contract: every focusable widget
     * registers in visual declaration order, and Tab / Shift+Tab move through
     * that one shared order. */
    {
        Rectangle first = {10, 10, 100, 30};
        Rectangle second = {10, 50, 100, 30};
        Rectangle third = {10, 90, 100, 30};

        InjectReset();
        SetUIFocus(901);
        InjectKeyTap(KEY_TAB);
        InjectPump();
        BeginUIFocus();
        RegisterUIFocus(901, first);
        RegisterUIFocus(902, second);
        RegisterUIFocus(903, third);
        EndUIFocus();
        check_int("tab advances focus", IsUIFocusActive(902), 1);
        InjectPump();

        InjectKey(KEY_LEFT_SHIFT, 1);
        InjectKeyTap(KEY_TAB);
        InjectPump();
        BeginUIFocus();
        RegisterUIFocus(901, first);
        RegisterUIFocus(902, second);
        RegisterUIFocus(903, third);
        EndUIFocus();
        check_int("shift tab reverses focus", IsUIFocusActive(901), 1);
        InjectKey(KEY_LEFT_SHIFT, 0);
        InjectPump();
    }

    /* Retained declarations use the same traversal path as generated Kry
     * programs: after Tab, text must enter the next declared field. */
    {
        char first[16] = "one";
        char second[16] = "";
        int first_cursor = 3, second_cursor = 0;
        int first_focused = 0, second_focused = 0;

        InjectReset();
        InjectMousePosition(20, 20);
        InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
        InjectPump();
        BeginUIFocus();
        BeginTree(1000);
        TextField((TextFieldProps){ .bounds = {10, 10, 160, 30},
            .text = first, .text_size = sizeof(first),
            .cursor_position = &first_cursor, .focused = &first_focused,
            .focus_id = 1001, .font = 16 });
        TextField((TextFieldProps){ .bounds = {10, 50, 160, 30},
            .text = second, .text_size = sizeof(second),
            .cursor_position = &second_cursor, .focused = &second_focused,
            .focus_id = 1002, .font = 16 });
        EndTree();
        EndUIFocus();
        InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
        InjectPump();

        InjectKeyTap(KEY_TAB);
        InjectPump();
        BeginUIFocus();
        BeginTree(1000);
        TextField((TextFieldProps){ .bounds = {10, 10, 160, 30}, .text = first,
            .text_size = sizeof(first), .cursor_position = &first_cursor,
            .focused = &first_focused, .focus_id = 1001, .font = 16 });
        TextField((TextFieldProps){ .bounds = {10, 50, 160, 30}, .text = second,
            .text_size = sizeof(second), .cursor_position = &second_cursor,
            .focused = &second_focused, .focus_id = 1002, .font = 16 });
        EndTree();
        EndUIFocus();

        InjectPump();
        InjectText("x");
        InjectPump();
        BeginUIFocus();
        BeginTree(1000);
        TextField((TextFieldProps){ .bounds = {10, 10, 160, 30}, .text = first,
            .text_size = sizeof(first), .cursor_position = &first_cursor,
            .focused = &first_focused, .focus_id = 1001, .font = 16 });
        TextField((TextFieldProps){ .bounds = {10, 50, 160, 30}, .text = second,
            .text_size = sizeof(second), .cursor_position = &second_cursor,
            .focused = &second_focused, .focus_id = 1002, .font = 16 });
        EndTree();
        EndUIFocus();
        check_int("retained tab focuses next field", strcmp(second, "x"), 0);
    }

    /* Typing into a multiline text area keeps the newly reflowed caret line
     * in view instead of leaving the scroll position at the top. */
    {
        char value[256] =
            "one\ntwo\nthree\nfour\nfive\nsix\nseven\neight";
        int cursor = (int)strlen(value);
        int focused = 0;
        int scroll_y = 0;

        InjectReset();
        InjectTap(25, 25);
        InjectPump();
        BeginTree(1003);
        TextArea((TextAreaProps){
            .bounds = {10, 10, 90, 44}, .text = value,
            .text_size = sizeof(value), .cursor_position = &cursor,
            .focused = &focused, .scroll_y = &scroll_y,
            .focus_id = 1004, .font = 16, .line_gap = 4, .wrap = 1
        });
        ReconcileTree();
        LayoutTree();
        RouteInput();
        check_int("wrapped textarea selects end",
                  SetSelection(1004, (int)strlen(value),
                               (int)strlen(value)), 1);
        InjectText("x");
        InjectPump();
        RouteInput();
        check_int("multiline textarea appends text", value[cursor - 1], 'x');
        check_int("multiline textarea reveals caret", scroll_y > 0, 1);
        InjectKeyTap(KEY_ENTER);
        InjectPump();
        RouteInput();
        check_int("multiline textarea enter inserts newline",
                  value[cursor - 1], '\n');
    }

    /* Composition events preserve preedit separately and commit UTF-8 only
     * when the platform IME finalizes it. */
    {
        KryTextCompositionEvent event;

        ClearTextComposition();
        check_int("submit composition update",
                  SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,
                                        "nihon", 5, 0), 1);
        check_int("poll composition update", PollTextComposition(&event), 1);
        check_int("composition update phase", event.phase,
                  KRY_TEXT_COMPOSITION_UPDATE);
        check_int("composition update text", strcmp(event.text, "nihon"), 0);
        check_int("composition queue drained", PollTextComposition(&event), 0);
    }

    /* The retained tree exposes a backend-neutral accessibility snapshot. */
    {
        UIAccessibilityNode nodes[8];
        int count;
        int saw_main = 0;
        int saw_button = 0;

        BeginTree(1200);
        Button((ButtonProps){ .bounds = {10, 10, 100, 30},
            .label = "Save", .id = 1201 });
        EndTree();
        count = GetAccessibilitySnapshot(nodes, 8);
        for(int i = 0; i < count && i < 8; i++) {
            if(strcmp(nodes[i].role, "main") == 0)
                saw_main = 1;
            if(strcmp(nodes[i].role, "button") == 0 &&
               strcmp(nodes[i].label, "Save") == 0)
                saw_button = 1;
        }
        check_int("accessibility snapshot main", saw_main, 1);
        check_int("accessibility snapshot button", saw_button, 1);
    }

    return failures == 0 ? 0 : 1;
}

void
__wrap_DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    (void)posX;
    (void)posY;
    (void)width;
    (void)height;
    (void)color;
}

void
__wrap_DrawRectangleRec(Rectangle rec, Color color)
{
    (void)rec;
    (void)color;
}

void
__wrap_DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color)
{
    (void)rec;
    (void)lineThick;
    (void)color;
}

void
__wrap_DrawRectangleRounded(Rectangle rec, float roundness, int segments,
                            Color color)
{
    (void)rec;
    (void)roundness;
    (void)segments;
    (void)color;
}

void
__wrap_DrawLine(int startPosX, int startPosY, int endPosX, int endPosY,
                Color color)
{
    (void)startPosX;
    (void)startPosY;
    (void)endPosX;
    (void)endPosY;
    (void)color;
}

void
__wrap_BeginScissorMode(int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

void
__wrap_EndScissorMode(void)
{
}
