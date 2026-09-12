#include "kryon.h"
#include "kry_inject.h"
#include "runtime/navigation_bar.h"
#include "theme.h"
#include <stdio.h>
#include <string.h>

static int failures;
static int draw_rectangle_calls;

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

void ui_paint_text_area(TextAreaProps area, int cursor, int focused,
                        int selection_start, int selection_end);
void ui_paint_text_area_composition(TextAreaProps area, int cursor, int focused,
                                    int selection_start, int selection_end,
                                    int composition_start,
                                    int composition_end);
int ui_active_font_token(void);

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
    check_int("scaffold title height", height, Scale(36));
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
    CheckboxRowProps checkbox = {0};
    ButtonRowProps row = {.width = 240, .height = 40};
    Form form;
    Rectangle taken;
    GridMetrics grid_metrics;
    GridCursor grid_cursor;
    Rectangle grid_item;
    ScreenScaffold scaffold;
    NavigationBarProps nav = {0};
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

    SetThemeStyle(THEME_STYLE_CLASSIC);

    BeginTree(7000);
    BeginButton((ButtonProps){
        .bounds = {10, 10, 120, 40},
        .label = "Save",
        .id = 7001
    });
    End();
    BeginButton((ButtonProps){
        .bounds = {150, 10, 120, 40},
        .id = 7002
    });
    Text((TextProps){.text = "Save", .wrap = TextWrapNone});
    End();
    EndTree();
    nodes = GetTreeNodes(&count);
    {
        const UIWidgetNode *short_button = NULL;
        const UIWidgetNode *child_button = NULL;
        const UIWidgetNode *short_text = NULL;
        const UIWidgetNode *child_text = NULL;

        for(int i = 0; i < count; i++) {
            if(nodes[i].kind == UI_WIDGET_BUTTON_NODE && nodes[i].id == 7001)
                short_button = &nodes[i];
            else if(nodes[i].kind == UI_WIDGET_BUTTON_NODE && nodes[i].id == 7002)
                child_button = &nodes[i];
        }
        if(short_button != NULL && short_button->first_child >= 0)
            short_text = &nodes[short_button->first_child];
        if(child_button != NULL && child_button->first_child >= 0)
            child_text = &nodes[child_button->first_child];
        check_int("button shorthand has Text child",
                  short_text != NULL &&
                  short_text->kind == UI_WIDGET_TEXT_NODE, 1);
        check_int("button composition has Text child",
                  child_text != NULL &&
                  child_text->kind == UI_WIDGET_TEXT_NODE, 1);
        if(short_text != NULL && child_text != NULL) {
            check_int("button Text font parity",
                      short_text->data.primitive.font,
                      child_text->data.primitive.font);
            check_int("button Text color parity",
                      (int)ColorToInt(short_text->data.primitive.color),
                      (int)ColorToInt(child_text->data.primitive.color));
            check_int("button Text width parity",
                      (int)short_text->bounds.width,
                      (int)child_text->bounds.width);
            check_int("button Text height parity",
                      (int)short_text->bounds.height,
                      (int)child_text->bounds.height);
        }
    }

    check_int("label text field",
              GetLabelTextFieldHeight(field),
              Scale(22) + Scale(40) + Scale(24));
    check_int("button row",
              GetButtonRowHeight(row),
              Scale(40));
    form = FormBegin(10, 20, 240);
    taken = FormTakeRect(&form, Scale(18));
    check_int("form rect x", (int)taken.x, 10);
    check_int("form rect y", (int)taken.y, 20);
    check_int("form rect width", (int)taken.width, 240);
    check_int("form advances", FormY(&form), 20 + Scale(18));
    BeginTree(6);
    FormSection(&form, (SectionLabelProps){.label = "Account"});
    EndTree();
    check_int("form section helper advances", FormY(&form),
              20 + Scale(18) + Scale(24));
    FormCheckbox(&form, checkbox);
    check_int("form checkbox helper advances", FormY(&form),
              20 + Scale(18) + Scale(24) + Scale(42));
    check_int("spinbox row height",
              GetSpinboxRowHeight((SpinboxRowProps){0}),
              Scale(54));
    check_int("navigation bar",
              GetNodeHeight(NodeNavigationBar(nav)),
              Scale(40));
    SetThemeStyle(THEME_STYLE_DEFAULT);
    check_int("material navigation bar",
              GetNodeHeight(NodeNavigationBar(nav)),
              NavigationBarDefaultHeight(1.0f));
    SetThemeStyle(THEME_STYLE_CLASSIC);
    check_int("retro tab bar",
              GetNodeHeight(NodeTabBar(tabs)),
              Scale(36));
    SetThemeStyle(THEME_STYLE_DEFAULT);
    check_int("material tab bar",
              GetNodeHeight(NodeTabBar(tabs)),
              Scale(48));
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
    SetThemeStyle(THEME_STYLE_CLASSIC);
    BeginTree(37);
    scaffold = BeginScreenScaffold((ScreenScaffoldSpec){
        .title = "Settings",
        .bottom_reserved = 12,
        .max_content_width = 220,
        .min_content_width = 120,
        .content_height = scaffold_height,
        .user_data = &scaffold_fixture,
        .draw_title = scaffold_title
    });
    check_int("scaffold closed", scaffold.closed, 1);
    check_int("scaffold content y", scaffold.content_y, Scale(36));
    check_int("scaffold content h", scaffold.content_h,
              240 - Scale(36) - 12);
    check_int("scaffold content w", scaffold.content_w,
              scaffold_fixture.seen_w);
    EndScreenScaffold(scaffold);
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
    page_grid = Grid((GridProps){.bounds = {0, 0, 100, 40},
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

    grid_metrics = MeasureGrid((GridProps){.bounds = {0, 0, 660, 0},
                                           .min_item_width = 200,
                                           .gap = 12});
    check_int("responsive grid columns", grid_metrics.columns, 3);
    check_int("responsive grid cell width", grid_metrics.cell_width, 212);
    grid_cursor = BeginGridCursor((GridProps){.bounds = {10, 20, 660, 0},
                                           .min_item_width = 200,
                                           .gap = 12});
    grid_cursor = GridStep(grid_cursor, 40, 1);
    grid_item = grid_cursor.item;
    check_int("responsive grid item 0 x", (int)grid_item.x, 10);
    check_int("responsive grid item 0 width", (int)grid_item.width, 212);
    grid_cursor = GridStep(grid_cursor, 50, 2);
    grid_item = grid_cursor.item;
    check_int("responsive grid span x", (int)grid_item.x, 234);
    check_int("responsive grid span width", (int)grid_item.width, 436);
    grid_cursor = GridStep(grid_cursor, 30, 3);
    grid_item = grid_cursor.item;
    check_int("responsive grid full span x", (int)grid_item.x, 10);
    check_int("responsive grid full span y", (int)grid_item.y, 82);
    check_int("responsive grid full span width", (int)grid_item.width, 660);
    check_int("responsive grid cursor height", GridCursorHeight(grid_cursor), 92);

    /* Grid buttons must hit their visible cells during construction, before
       EndTree performs retained layout. Exercise both columns independently. */
    for(int column = 0; column < 2; column++) {
        UIFrameState saved = SaveUIFrameState();
        InjectReset();
        for(int frame = 0; frame < 2; frame++) {
            InjectMousePosition(120 + column * 100, 120);
            InjectMouseButton(MOUSE_BUTTON_LEFT, frame == 0);
            InjectPump();
            BeginUIFrame(640, 480, 1.0f);
            BeginTree(390 + column);
            Grid((GridProps){.bounds = {100, 100, 200, 60},
                              .columns = 2});
            check_int("grid first cell immediate click",
                      Button((ButtonProps){.bounds = {0, 0, 90, 40},
                                           .label = "A", .id = 3901}),
                      frame == 1 && column == 0);
            check_int("grid second cell immediate click",
                      Button((ButtonProps){.bounds = {0, 0, 90, 40},
                                           .label = "B", .id = 3902}),
                      frame == 1 && column == 1);
            End();
            EndTree();
            EndUIFrame();
            while(NextEvent(&event)) {}
        }
        InjectReset();
        RestoreUIFrameState(saved);
    }

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
        UIFrameState saved = SaveUIFrameState();
        int stopped = 0;

        InjectReset();
        while(NextEvent(&event)) {
        }
        BeginUIFrame(320, 240, 1.0f);
        BeginTree(441);
        Button((ButtonProps){.bounds = {10, 10, 100, 40},
                             .label = "First", .id = 4411});
        Button((ButtonProps){.bounds = {10, 60, 100, 40},
                             .label = "Second", .id = 4412});
        Button((ButtonProps){.bounds = {10, 110, 100, 40},
                             .label = "Third", .id = 4413});
        EndTree();
        EndUIFrame();
        nodes = GetTreeNodes(&count);
        check_int("atomic tree initial count", count, 4);

        InjectTap(25, 25);
        InjectPump();
        BeginUIFrame(320, 240, 1.0f);
        BeginTree(441);
        check_int("atomic tree press does not stop",
                  Button((ButtonProps){.bounds = {10, 10, 100, 40},
                                       .label = "First", .id = 4411}), 0);
        Button((ButtonProps){.bounds = {10, 60, 100, 40},
                             .label = "Second", .id = 4412});
        Button((ButtonProps){.bounds = {10, 110, 100, 40},
                             .label = "Third", .id = 4413});
        EndTree();
        EndUIFrame();

        InjectPump();
        BeginUIFrame(320, 240, 1.0f);
        BeginTree(441);
        if(Button((ButtonProps){.bounds = {10, 10, 100, 40},
                                .label = "First", .id = 4411})) {
            stopped = 1;
        }
        if(!stopped) {
            Button((ButtonProps){.bounds = {10, 60, 100, 40},
                                 .label = "Second", .id = 4412});
            Button((ButtonProps){.bounds = {10, 110, 100, 40},
                                 .label = "Third", .id = 4413});
        }
        EndTree();
        EndUIFrame();
        nodes = GetTreeNodes(&count);
        check_int("atomic tree click reached handler", stopped, 1);
        check_int("atomic tree kept complete count", count, 4);
        check_int("atomic tree kept second sibling", nodes[2].id, 4412);
        check_int("atomic tree kept third sibling", nodes[3].id, 4413);
        while(NextEvent(&event)) {
        }
        InjectReset();
        RestoreUIFrameState(saved);
    }

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

    {
        char value[512] = "";
        const char *payload =
            "line-00\nline-01\nline-02\nline-03\nline-04\nline-05\n"
            "line-06\nline-07\nline-08\nline-09\nline-10\nline-11";
        int cursor = 0;
        int focused = 1;
        int scroll_y = 0;

        InjectReset();
        SetUIClipboardTextValue(payload);
        SetUIFocus(1005);
        InjectKey(KEY_LEFT_CONTROL, 1);
        InjectKeyTap(KEY_V);
        InjectPump();
        BeginTree(1005);
        TextArea((TextAreaProps){
            .bounds = {10, 10, 160, 56}, .text = value,
            .text_size = sizeof(value), .cursor_position = &cursor,
            .focused = &focused, .scroll_y = &scroll_y,
            .max_codepoints = 511, .focus_id = 1005,
            .font = 16, .line_gap = 4
        });
        ReconcileTree();
        LayoutTree();
        RouteInput();
        InjectKey(KEY_LEFT_CONTROL, 0);
        InjectPump();
        check_int("retained textarea preserves pasted newlines",
                  strcmp(value, payload), 0);
        check_int("retained textarea bulk paste moves cursor",
                  cursor, (int)strlen(payload));
        check_int("retained textarea bulk paste reveals caret",
                  scroll_y > 0, 1);
    }

    {
        char value[64] = "textarea-probe";
        int cursor = 0;
        int focused = 0;
        int scroll_y = -400;

        InjectReset();
        ui_paint_text_area((TextAreaProps){
            .bounds = {20, 30, 180, 120}, .text = value,
            .text_size = sizeof(value), .cursor_position = &cursor,
            .focused = &focused, .scroll_y = &scroll_y,
            .focus_id = 1006, .font = 16, .line_gap = 4
        }, cursor, focused, cursor, cursor);
        check_int("retained textarea clamps negative scroll", scroll_y, 0);
    }

    {
        char value[64] = "a\xe6\x97\xa5\xe6\x9c\xacz";
        int cursor = 7;
        int scroll_y = 0;
        int ordinary_rectangles;

        draw_rectangle_calls = 0;
        ui_paint_text_area((TextAreaProps){
            .bounds = {20,30,180,80}, .text = value,
            .text_size = sizeof(value), .cursor_position = &cursor,
            .scroll_y = &scroll_y, .font = 16, .line_gap = 4
        }, cursor, 0, cursor, cursor);
        ordinary_rectangles = draw_rectangle_calls;
        draw_rectangle_calls = 0;
        ui_paint_text_area_composition((TextAreaProps){
            .bounds = {20,30,180,80}, .text = value,
            .text_size = sizeof(value), .cursor_position = &cursor,
            .scroll_y = &scroll_y, .font = 16, .line_gap = 4
        }, cursor, 0, cursor, cursor, 1, 7);
        check_int("textarea composition adds underline paint",
                  draw_rectangle_calls, ordinary_rectangles + 1);
    }

    /* Composition events preserve preedit separately and commit UTF-8 only
     * when the platform IME finalizes it. */
    {
        KryTextCompositionEvent event;
        char long_preedit[300];

        ClearTextComposition();
        check_int("submit composition update",
                  SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,
                                        "nihon", 5, 0), 1);
        check_int("poll composition update", PollTextComposition(&event), 1);
        check_int("composition update phase", event.phase,
                  KRY_TEXT_COMPOSITION_UPDATE);
        check_int("composition update text", strcmp(event.text, "nihon"), 0);
        check_int("composition queue drained", PollTextComposition(&event), 0);
        memset(long_preedit, 'x', 254);
        memcpy(long_preedit + 254, "\xe6\x97\xa5", 4);
        check_int("submit truncated UTF-8 composition",
                  SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,
                                        long_preedit, 254, 0), 1);
        check_int("poll truncated UTF-8 composition",
                  PollTextComposition(&event), 1);
        check_int("composition drops partial UTF-8 tail",
                  (int)strlen(event.text), 254);
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

    {
        char boxed[] = "owned";
        BeginTree(Key("headless boxed text"));
        Row((RowProps){.bounds = {11,12,80,24}});
        Text((TextProps){.bounds=(Rectangle){0,0,80,24}, .text=boxed, .font=16, .color=WHITE, .wrap=TextWrapNone, .align=TextAlignCenter, .vertical_align=TextAlignCenter});
        End();
        boxed[0] = 'X';
        EndTree();
        nodes = GetTreeNodes(&count);
        int boxed_count = 0;
        for(int i = 0; i < count; i++) {
            if(nodes[i].kind != UI_WIDGET_TEXT_NODE)
                continue;
            boxed_count++;
            check_int("boxed text owns string", strcmp(nodes[i].owned_text,"owned"), 0);
            check_int("boxed text row x", (int)nodes[i].bounds.x, 11);
            check_int("boxed text row y", (int)nodes[i].bounds.y, 12);
            check_int("boxed text font", nodes[i].data.primitive.font, 16);
        }
        check_int("boxed text typed node", boxed_count, 1);
    }
    {
        Font font = {0};
        Rectangle font_rec = {0};
        GlyphInfo font_glyph = {0};
        char field_text[8] = "a";
        char area_text[8] = "b";
        int field_cursor = 1;
        int area_cursor = 1;
        int scroll_y = 0;
        int previous;
        int text_inputs = 0;

        ClearUIFonts();
        font.baseSize = 16;
        font.glyphCount = 1;
        font.texture.id = 1;
        font.recs = &font_rec;
        font.glyphs = &font_glyph;
        check_int("register retained font base probe",
                  RegisterUIFont("retained-base", font), 1);
        check_int("register retained font snapshot probe",
                  RegisterUIFont("retained-probe", font), 1);
        previous = PushUIFont("retained-probe");
        check_int("font probe preserves prior font", previous, 0);
        BeginTree(1500);
        TextField((TextFieldProps){.bounds = {10,10,80,24},
            .text = field_text, .text_size = sizeof(field_text),
            .cursor_position = &field_cursor, .focus_id = 1501});
        TextArea((TextAreaProps){.bounds = {10,40,80,40},
            .text = area_text, .text_size = sizeof(area_text),
            .cursor_position = &area_cursor, .scroll_y = &scroll_y,
            .focus_id = 1502});
        EndTree();
        nodes = GetTreeNodes(&count);
        for(int i = 0; i < count; i++) {
            if(nodes[i].kind != UI_WIDGET_TEXT_FIELD_NODE &&
               nodes[i].kind != UI_WIDGET_TEXT_AREA_NODE)
                continue;
            text_inputs++;
            check_int("retained text captures declaration font",
                      nodes[i].font_token, 1);
        }
        check_int("both retained text widgets capture fonts", text_inputs, 2);
        PopUIFont(previous);
        check_int("font token restores prior font",
                  ui_active_font_token(), 0);
        ClearUIFonts();
    }
    {
        RouterRoute routes[] = {
            {1, -1, "home", "Home", "main"},
            {2, 1, "settings", "Settings", "main"}
        };
        RouterState state = {0};
        RouterResult result;

        BeginTree(4100);
        result = Router((RouterProps){
            .bounds = {0, 0, 120, 80},
            .key = Key("router-test"),
            .state = &state,
            .routes = routes,
            .route_count = 2,
            .initial_route = 1
        });
        EndTree();
        check_int("router initializes to initial route", result.route, 1);
        check_int("router initial frame is not changed", result.changed, 0);
        nodes = GetTreeNodes(&count);
        check_int("router node is retained",
                  count > 1 ? (int)nodes[1].kind : -1,
                  UI_WIDGET_ROUTER_NODE);

        RouterNavigate(&state, 2);
        BeginTree(4100);
        result = Router((RouterProps){
            .bounds = {0, 0, 120, 80},
            .key = Key("router-test"),
            .state = &state,
            .routes = routes,
            .route_count = 2,
            .initial_route = 1
        });
        EndTree();
        check_int("router applies queued route", result.route, 2);
        check_int("router reports queued route change", result.changed, 1);
        check_int("router tracks previous route", result.previous_route, 1);

        BeginTree(4100);
        result = Router((RouterProps){
            .bounds = {0, 0, 120, 80},
            .key = Key("router-test"),
            .state = &state,
            .routes = routes,
            .route_count = 2,
            .initial_route = 1
        });
        EndTree();
        check_int("router settles after change", result.changed, 0);
        check_int("router keeps current route", result.route, 2);
    }
    return failures == 0 ? 0 : 1;
}

void
__wrap_DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    draw_rectangle_calls++;
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
