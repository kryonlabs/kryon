#include "kryon.h"
#include "kry_inject.h"
#include "kryon_test.h"
#include "theme.h"
#include "ui_inspect.h"
#include "../src/ui/ui_internal.h"
#include "../src/ui/ui_numeric_input_internal.h"
#include "../src/ui/ui_tree_layout_internal.h"
#include "../src/ui/ui_disabled_internal.h"
#include "../src/ui/ui_input_clip_internal.h"
#include "../src/ui/ui_popup_input_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

static void
check_color(const char *name, Color got, Color want)
{
    if(got.r == want.r && got.g == want.g && got.b == want.b &&
       got.a == want.a)
        return;
    fprintf(stderr, "%s: got #%02X%02X%02X%02X want #%02X%02X%02X%02X\n",
            name, got.r, got.g, got.b, got.a,
            want.r, want.g, want.b, want.a);
    exit(1);
}

static void
test_theme_surface_helpers(void)
{
    Color mixed = MixThemeColor((Color){0, 0, 0, 255},
                                (Color){100, 50, 200, 127}, 0.5f);

    check_color("theme mix", mixed, (Color){50, 25, 100, 191});
    check_color("theme mix clamps low",
                MixThemeColor(BLACK, WHITE, -1.0f), BLACK);
    check_color("theme mix clamps high",
                MixThemeColor(BLACK, WHITE, 2.0f), WHITE);
    check_int("black luminance", GetThemeColorLuminance(BLACK), 0);
    check_int("white luminance", GetThemeColorLuminance(WHITE), 255);
    check_int("dark color", IsThemeColorDark((Color){12, 12, 12, 255}), 1);
    check_int("light color", IsThemeColorDark((Color){240, 240, 240, 255}), 0);
    check_color("readable on dark",
                GetThemeReadableText((Color){20, 20, 20, 255}), RAYWHITE);
    check_color("readable on light",
                GetThemeReadableText((Color){240, 240, 240, 255}), BLACK);

    SetThemeSource(THEME_SOURCE_APP);
    SetCurrentTheme(THEME_MONO, 0);
    check_int("surface alt alpha", GetThemeSurfaceAlt().a, 255);
    check_int("theme border alpha", GetThemeBorder().a, 255);
    check_int("muted text alpha", GetThemeMutedText().a, 255);
    check_int("selection alpha", GetThemeSelection().a, 255);
    check_color("button text", GetThemeButtonText(),
                GetThemeReadableText(GetThemeButton()));
}

static void
test_semantic_font_sizes_follow_ui_scale(void)
{
    BeginUIFrame(720, 1400, 1.75f);
    check_int("body font at 1.75x", GetFontSize(), 28);
    check_int("small font at 1.75x", GetSmallFontSize(), 25);
    check_int("title font at 1.75x", GetTitleFontSize("Title", 1000), 42);
    check_int("fitted font preserves body", FitFontSize("Day", 1000, Text16, Text8), 28);
    check_int("fitted caption token scales", FitFontSize("", 1000, Text12, Text8), 21);
    {
        int fitted = FitFontSize("Delete Habit", ScaleUIPx(64),
                                 GetFontSize(), Text8);
        check_int("button label fit stays inside content width",
                  TextWidth("Delete Habit", fitted) <= ScaleUIPx(64), 1);
    }
    EndUIFrame();
}

static void
test_reorder_uses_item_center_and_header_handle(void)
{
    UIReorderItem items[2] = {
        {1, {10, 100, 200, 100}, 0},
        {2, {10, 210, 200, 100}, 0}
    };
    UIReorderList list = {
        .id = 811, .bounds = {0, 0, 300, 500},
        .items = items, .item_count = 2,
        .handle_width = 200, .handle_height = 40,
        .drag_threshold = 5
    };
    UIReorderListResult result;

    InjectReset();
    InjectMousePosition(50, 170);
    InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
    InjectPump();
    BeginUIFrame(300, 500, 1.0f);
    result = UpdateUIReorderList(list);
    EndUIFrame();
    check_int("reorder ignores item body below handle", result.active, 0);
    InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
    InjectPump();

    InjectReset();
    InjectMousePosition(50, 120);
    InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
    InjectPump();
    BeginUIFrame(300, 500, 1.0f);
    result = UpdateUIReorderList(list);
    EndUIFrame();
    check_int("reorder captures header", result.active, 1);

    InjectMousePosition(50, 240);
    InjectPump();
    BeginUIFrame(300, 500, 1.0f);
    result = UpdateUIReorderList(list);
    EndUIFrame();
    check_int("reorder drag active", result.dragging, 1);
    check_int("reorder target follows lifted center", result.target_index, 1);

    InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
    InjectPump();
    BeginUIFrame(300, 500, 1.0f);
    result = UpdateUIReorderList(list);
    EndUIFrame();
}

static void
test_menu_bar_switches_while_popup_captures_input(void)
{
    static const MenuItem file_items[] = {
        {MenuCommand, "Open", "Ctrl+O", 101, 0, 0, NULL, 0}
    };
    static const MenuItem edit_items[] = {
        {MenuCommand, "Copy", "Ctrl+C", 201, 0, 0, NULL, 0}
    };
    static const Menu menus[] = {
        {{0, 0, 0, 0}, "File", file_items, 1},
        {{0, 0, 0, 0}, "Edit", edit_items, 1}
    };
    Rectangle bounds = {0, 0, 240, 28};
    int open_index = 0;
    int font;
    int edit_x;
    MenuBarResult result;

    InjectReset();
    BeginUIFrame(640, 480, 1.0f);
    font = GetFontSize();
    edit_x = ScaleUIPx(4) + TextWidth("File", font) + ScaleUIPx(24) +
             ScaleUIPx(2) + ScaleUIPx(8);
    EndUIFrame();

    InjectTap((float)edit_x, 14.0f);
    InjectPump();
    InjectPump();

    BeginUIFrame(640, 480, 1.0f);
    PushUIInputCapture((Rectangle){0, 28, 180, 64}, 1);
    result = MenuBar(700, bounds, menus, 2, &open_index);
    EndUIFrame();

    check_int("menu bar switches over popup capture", open_index, 1);
    check_int("menu bar result switches over popup capture",
              result.open_index, 1);
}

static void
test_circle_click_uses_ui_release_path(void)
{
    int hover = 0;
    int clicked = 0;
    int second_clicked = 0;

    InjectReset();
    InjectTap(100.0f, 100.0f);
    InjectPump();
    InjectPump();

    BeginUIFrame(640, 480, 1.0f);
    clicked = UIHandleCircleClick((Vector2){100.0f, 100.0f}, 32.0f, 0, &hover);
    second_clicked =
        UIHandleClick((Rectangle){80.0f, 80.0f, 40.0f, 40.0f}, 0, NULL);
    EndUIFrame();

    check_int("circle click inside", clicked, 1);
    check_int("circle click hover", hover, UIHoverEffectsEnabled() ? 1 : 0);
    check_int("circle click consumes release", second_clicked, 0);

    InjectReset();
    InjectTap(160.0f, 100.0f);
    InjectPump();
    InjectPump();

    BeginUIFrame(640, 480, 1.0f);
    clicked = UIHandleCircleClick((Vector2){100.0f, 100.0f}, 32.0f, 0, NULL);
    EndUIFrame();

    check_int("circle click outside", clicked, 0);
}

static void
test_nested_disabled_scope(void)
{
    ButtonProps button = {{10, 10, 80, 28}, "Blocked", ButtonStylePrimary,
                          14, 145, 0};
    Color text = GetThemeText();

    BeginDisabled(1);
    check_int("disabled scope dims", GetThemeText().a < text.a, 1);
    BeginDisabled(0);
    check_int("disabled false nested in true", GetThemeText().a < text.a, 1);
    EndDisabled();
    check_int("disabled outer remains", GetThemeText().a < text.a, 1);
    EndDisabled();
    check_color("disabled scope restores theme", GetThemeText(), text);

    InjectReset();
    InjectTap(30, 20);
    InjectPump();
    BeginUIFrame(220, 100, 1.0f);
    BeginDisabled(1);
    check_int("disabled button press frame", Button(button), 0);
    EndDisabled();
    EndUIFrame();
    InjectPump();
    BeginUIFrame(220, 100, 1.0f);
    BeginDisabled(1);
    check_int("disabled button release frame", Button(button), 0);
    EndDisabled();
    EndUIFrame();
}

static void
test_disabled_scalar_cancels_gesture(void)
{
    for(int slider = 0; slider < 2; slider++) {
        for(int scope = 0; scope < 2; scope++) {
            float value = 25.0f;
            float before = value;
            DragFloatProps drag = {0};
            SliderFloatProps slide = {0};
            drag.bounds = slide.bounds = (Rectangle){10, 10, 100, 24};
            drag.id = 982;
            slide.id = 981;
            drag.values = slide.values = &value;
            drag.value_count = slide.value_count = 1;
            drag.max = slide.max = 100;
            drag.speed = 1;
            InjectReset();
            for(int step = 0; step < 4; step++) {
                InjectMousePosition((float)(35 + step * 15), 20);
                if(step == 0)
                    InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
                if(step == 3)
                    InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
                InjectPump();
                BeginUIFrame(220, 100, 1.0f);
                if(scope)
                    BeginDisabled(step == 1);
                drag.disabled = slide.disabled = !scope && step == 1;
                if(slider)
                    (void)SliderFloat(slide);
                else
                    (void)DragFloat(drag);
                if(scope)
                    EndDisabled();
                EndUIFrame();
                if(step == 0)
                    before = value;
                else
                    check_int("disabled scalar cancels held gesture", value == before, 1);
            }
        }
    }
}

static void
test_deep_disabled_scopes(void)
{
    int keyboard = SetUIKeyboardInputEnabled(1);
    for(int outer = 0; outer < 2; outer++) {
        BeginDisabled(outer);
        for(int depth = 0; depth < 130; depth++)
            BeginDisabled(depth == 100);
        check_int("deep disabled keyboard", UIKeyboardInputEnabled(), 0);
        for(int depth = 129; depth >= 0; depth--) {
            EndDisabled();
            check_int("deep disabled unwind", UIKeyboardInputEnabled(),
                      !outer && depth <= 100);
        }
        EndDisabled();
        check_int("deep disabled restored", UIKeyboardInputEnabled(), 1);
    }
    SetUIKeyboardInputEnabled(keyboard);
}

static void
test_collapsible_composes_children(void)
{
    bool open = false;
    int actions = 0;
    CollapsibleProps section = {.bounds = {10, 10, 180, 200}, .label = "Details", .open = &open};
    ButtonProps child = {{10, 46, 100, 28}, "Child", ButtonStylePrimary, 14, 983, 0};
    int ys[] = {20, 50, 20, 50};

    InjectReset();
    for(int click = 0; click < 4; click++) {
        InjectTap(20, (float)ys[click]);
        for(int frame = 0; frame < 2; frame++) {
            InjectPump();
            BeginUIFrame(240, 300, 1.0f);
            (void)Collapsible(section);
            if(open && Button(child))
                actions++;
            EndUIFrame();
        }
        check_int("collapsible open state", open, click < 2);
        check_int("collapsible child actions", actions, click > 0);
    }
}

static void
test_tree_header_modes(void)
{
    bool open = false;
    CollapsibleProps p = {.bounds = {10,10,180,32}, .label = "Node", .open = &open,
                          .tree = 1, .depth = 2, .selected = 1, .id = 993};
    InjectReset();
    for(int mode = 0; mode < 4; mode++) {
        p.leaf = mode == 0;
        p.disabled = mode == 1;
        InjectTap(mode == 2 ? 20 : 60,20);
        for(int frame = 0; frame < 2; frame++) {
            InjectPump();
            BeginUIFrame(240,240,1.0f);
            Collapsible(p);
            EndUIFrame();
        }
        check_int("tree leaf/disabled/indent/open",open,mode == 3);
    }
}

static void
test_tree_header_keyboard_gates(void)
{
    bool open = false;
    CollapsibleProps p = {.bounds = {10,10,180,32}, .open = &open, .tree = 1, .id = 994};
    InjectReset();
    for(int mode = 0; mode < 4; mode++) {
        p.leaf = mode == 0;
        p.disabled = mode == 1;
        SetUIFocus(994);
        InjectKeyTap(KEY_RIGHT);
        for(int frame = 0; frame < 2; frame++) {
            InjectPump();
            BeginUIFrame(240,240,1.0f);
            BeginDisabled(mode == 2);
            Collapsible(p);
            EndDisabled();
            EndUIFrame();
        }
        check_int("tree keyboard gates",open,mode == 3);
    }
}

static void
test_combo_popup_lifecycle(void)
{
    const char *options[] = {"One", "Two"};
    int selected = 0;
    ComboboxProps p = {.bounds = {10,10,160,28}, .id = 996, .options = options, .option_count = 2, .selected_index = &selected};
    for(int mode = 0; mode < 3; mode++) {
        InjectReset();
        p.disabled = 0;
        InjectTap(20,20);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1.0f); Combobox(p); EndUIFrame();
        }
        check_int("combo opened capture",UIInputCapturesClick((Vector2){20,70}),1);
        int background_scroll = 0;
        InjectMousePosition(20,70);
        InjectWheel(-1);
        InjectPump();
        BeginUIFrame(240,240,1.0f);
        BeginScroll((Rectangle){10,40,180,120},400,&background_scroll);
        EndScroll();
        Combobox(p);
        EndUIFrame();
        check_int("popup owns wheel before owner declaration",background_scroll,0);
        p.disabled = mode == 0;
        InjectTap(20,75);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1.0f);
            BeginDisabled(mode == 1);
            if(mode != 2) Combobox(p);
            EndDisabled(); EndUIFrame();
        }
        check_int("combo lifecycle selection",selected,0);
        check_int("combo released capture",UIInputCapturesClick((Vector2){20,70}),0);
        p.disabled = 0;
        BeginUIFrame(240,240,1.0f); Combobox(p); EndUIFrame();
        check_int("combo stays closed",UIInputCapturesClick((Vector2){20,70}),0);
    }
}

static void
test_many_combo_identities(void)
{
    const char *options[] = {"One", "Two"};
    int selected[41] = {0};
    InjectReset();
    for(int phase = 0; phase < 4; phase++) {
        if(phase < 3) InjectTap(20, phase == 1 ? 75 : 20);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump();
            BeginUIFrame(240,240,1);
            for(int i = phase == 3 ? 1 : 0; i < 41; i++) {
                Combobox((ComboboxProps){.bounds = {i ? 300 : 10,10,160,28},
                    .id = 20000+i, .options = options, .option_count = 2,
                    .selected_index = &selected[i]});
            }
            EndUIFrame();
        }
        if(phase == 0 || phase == 2)
            check_int("41 combos keep first owner's capture",UIInputCapturesClick((Vector2){20,70}),1);
        if(phase == 3)
            check_int("missing first combo releases capture",UIInputCapturesClick((Vector2){20,70}),0);
        if(phase == 1)
            check_int("41 combos keep first owner's selection",selected[0],1);
        for(int i = 1; i < 41; i++)
            check_int("41 combos preserve independent selection",selected[i],0);
    }
    InjectReset();
    BeginUIFrame(240,240,1);
    EndUIFrame();
    check_int("retired combo releases capture",UIInputCapturesClick((Vector2){20,70}),0);
}

static void
test_large_combo_options(void)
{
    const char *options[131];
    for(int i = 0; i < 131; i++) options[i] = "item";
    int selected = 0;
    InjectReset();
    for(int phase = 0; phase < 2; phase++) {
        InjectTap(20, phase == 0 ? 20 : 46 + 130*28 + 10);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump();
            BeginUIFrame(240,6000,1);
            Combobox((ComboboxProps){.bounds = {10,10,160,28}, .id = 21000,
                .options = options, .option_count = 131, .selected_index = &selected});
            EndUIFrame();
        }
    }
    check_int("combo selects beyond old 128 option limit",selected,130);
    InjectReset();
    BeginUIFrame(240,240,1);
    EndUIFrame();
}

static void
test_combo_keyboard_navigation(void)
{
    const char *options[131];
    for(int i = 0; i < 131; i++) options[i] = "item";
    int selected = 0;
    const int keys[] = {0, KEY_END, KEY_UP, KEY_ENTER, 0, KEY_HOME, KEY_ESCAPE, 0, KEY_HOME, KEY_DOWN, KEY_ENTER};
    InjectReset();
    for(int step = 0; step < 11; step++) {
        if(keys[step] == 0) InjectTap(20,20);
        else InjectKeyTap(keys[step]);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump();
            BeginUIFrame(240,240,1);
            Combobox((ComboboxProps){.bounds = {10,10,160,28}, .id = 23000,
                .options = options, .option_count = 131, .selected_index = &selected});
            EndUIFrame();
        }
        check_int("combo keyboard commits only on Enter",selected,step < 3 ? 0 : step < 10 ? 129 : 1);
    }
    for(int step = 0; step < 3; step++) {
        if(step == 0) InjectTap(20,20);
        else if(step == 1) InjectKeyTap(KEY_END);
        else InjectTap(20,200);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump();
            BeginUIFrame(240,240,1);
            Combobox((ComboboxProps){.bounds = {10,10,160,28}, .id = 23000,
                .options = options, .option_count = 131, .selected_index = &selected});
            EndUIFrame();
        }
    }
    check_int("keyboard End reveals last option for pointer selection",selected,130);
    InjectReset();
}

static void
test_popup_preedit_cancellation(void)
{
    for(int cause = 0; cause < 3; cause++) {
        UIPopupInput *context = ui_popup_input_create();
        char text[32] = "a";
        int cursor = 1, focused = 1;
        InjectReset(); ClearTextComposition();
        for(int frame = 0; frame < 3; frame++) {
            if(frame == 0) SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,"ni",2,0);
            InjectPump(); BeginUIFrame(240,240,1);
            ui_popup_input_frame(context);
            UIPopupInput *previous = ui_popup_input_bind(context);
            BeginTree(Key("popup-preedit-cancellation"));
            UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
            if(frame == 1 && cause == 0) {
                UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){20,20,60,60});
                ui_popup_input_end(child);
            } else ui_popup_input_close(context,1);
            SetUIFocus(frame == 1 && cause == 2 ? 0 : 26010);
            TextField((TextFieldProps){.bounds={10,10,120,28},.text=text,.text_size=sizeof(text),
                .cursor_position=&cursor,.focused=&focused,.focus_id=26010,
                .read_only=frame == 1 && cause == 1});
            ui_popup_input_end(parent);
            UIEvent event;
            while(NextEvent(&event)) {}
            EndTree();
            int composition_events = 0;
            while(NextEvent(&event))
                if(event.kind == UI_EVENT_COMPOSITION_CHANGED) composition_events++;
            check_int("preedit starts then cancels without revival",composition_events,frame < 2 ? 1 : 0);
            check_int("preedit cancellation leaves committed text intact",strcmp(text,"a"),0);
            EndUIFrame();
            ui_popup_input_finish(context);
            ui_popup_input_bind(previous);
        }
        ui_popup_input_destroy(context);
    }
    ClearTextComposition(); InjectReset();
}

static void
test_popup_composition_dismissal_replay(void)
{
    for(int read_only = 0; read_only < 2; read_only++)
    for(int area = 0; area < 2; area++) {
        UIPopupInput *context = ui_popup_input_create();
        char text[32] = "a";
        int cursor = 1, focused = 1;
        InjectReset(); ClearTextComposition();
        for(int frame = 0; frame < 3; frame++) {
            if(frame != 1) {
                SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,"ni",2,0);
                SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT,"x",1,0);
            }
            InjectPump(); BeginUIFrame(240,240,1);
            ui_popup_input_frame(context);
            UIPopupInput *previous = ui_popup_input_bind(context);
            BeginTree(Key("popup-composition-replay"));
            UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
            if(frame == 0) {
                UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){20,20,60,60});
                ui_popup_input_end(child);
            } else ui_popup_input_close(context,1);
            SetUIFocus(26000); focused = 1;
            if(area)
                TextArea((TextAreaProps){.bounds={10,10,120,80},.text=text,.text_size=sizeof(text),
                    .cursor_position=&cursor,.focused=&focused,.focus_id=26000,.read_only=read_only});
            else
                TextField((TextFieldProps){.bounds={10,10,120,28},.text=text,.text_size=sizeof(text),
                    .cursor_position=&cursor,.focused=&focused,.focus_id=26000,.read_only=read_only});
            ui_popup_input_end(parent);
            EndTree();
            check_int("IME commit respects popup ownership and read-only state",
                strcmp(text,frame == 2 && !read_only ? "ax" : "a"),0);
            EndUIFrame();
            ui_popup_input_finish(context);
            ui_popup_input_bind(previous);
        }
        ui_popup_input_destroy(context);
    }
    ClearTextComposition(); InjectReset();
}

static void
test_popup_text_dismissal_replay(void)
{
    for(int close_same_frame = 0; close_same_frame < 2; close_same_frame++)
    for(int area = 0; area < 2; area++)
    for(int queued = 0; queued < 3; queued++) {
        if(area && queued == 2) continue;
        UIPopupInput *context = ui_popup_input_create();
        char text[32] = "a";
        int cursor = 1, focused = 1, commit = 0;
        InjectReset(); ClearTextInputFocus();
        for(int frame = 0; frame < 3; frame++) {
            if(frame != 1) {
                if(queued == 2) {
                    QueueTextInputBackspace(); QueueTextInputEnter();
                } else if(queued) QueueTextInputCodepoint('x');
                else InjectText("x");
            }
            InjectPump(); BeginUIFrame(240,240,1);
            ui_popup_input_frame(context);
            UIPopupInput *previous = ui_popup_input_bind(context);
            UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
            if(frame == 0) {
                UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){20,20,60,60});
                ui_popup_input_end(child);
            } else ui_popup_input_close(context,1);
            SetUIFocus(25900); focused = 1;
            if(area)
                TextArea((TextAreaProps){.bounds={10,10,120,80},.text=text,.text_size=sizeof(text),
                    .cursor_position=&cursor,.focused=&focused,.focus_id=25900});
            else
                TextField((TextFieldProps){.bounds={10,10,120,28},.text=text,.text_size=sizeof(text),
                    .cursor_position=&cursor,.focused=&focused,.commit_pressed=&commit,.focus_id=25900});
            ui_popup_input_end(parent);
            check_int("dismissal does not replay blocked editor text",
                strcmp(text,frame == 2 ? (queued == 2 ? "" : "ax") : "a"),0);
            if(queued == 2) check_int("dismissal does not replay queued Enter",commit,frame == 2);
            if(close_same_frame && frame == 0) ui_popup_input_close(context,0);
            EndUIFrame();
            ui_popup_input_finish(context);
            ui_popup_input_bind(previous);
        }
        ClearTextInputFocus();
        ui_popup_input_destroy(context);
    }
    InjectReset();
}

static void
test_popup_tab_missing_owner(void)
{
    UIPopupInput *context = ui_popup_input_create();
    InjectReset();
    for(int frame = 0; frame < 3; frame++) {
        if(frame) InjectKeyTap(KEY_TAB);
        InjectPump(); BeginUIFrame(240,240,1);
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        RegisterUIFocus(25800,(Rectangle){0});
        if(frame < 2) {
            UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
            RegisterUIFocus(25810,(Rectangle){0});
            if(frame == 0) {
                UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){20,20,60,60});
                RegisterUIFocus(25820,(Rectangle){0});
                SetUIFocus(25820);
                ui_popup_input_end(child);
            }
            ui_popup_input_end(parent);
        }
        EndUIFocus();
        check_int("missing popup owner releases Tab in the same frame",GetUIFocus(),
            frame == 0 ? 25820 : frame == 1 ? 25810 : 25800);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        EndUIFrame();
        InjectPump();
    }
    ui_popup_input_destroy(context);
    InjectReset();
}

static void
test_popup_button_keyboard_ownership(void)
{
    const int keys[] = {KEY_ENTER,KEY_SPACE};
    for(int key = 0; key < 2; key++)
    for(int inside = 0; inside < 2; inside++) {
        InjectReset(); InjectKeyTap(keys[key]); InjectPump();
        BeginUIFrame(240,240,1);
        SetUIFocusTextInputActive(0);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        BeginTree(Key("popup-button-keyboard"));
        UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){180,180,40,40});
        UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){190,190,20,20});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(25700);
        int activated = Button((ButtonProps){.bounds={10,10,120,28},.label="Action",.id=25700});
        check_int("only top popup button activates from keyboard",activated,inside);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        UIEvent event;
        while(NextEvent(&event)) {}
        EndTree();
        int clicks = 0;
        while(NextEvent(&event)) if(event.kind == UI_EVENT_CLICK) clicks++;
        check_int("deferred keyboard clicks preserve popup ownership",clicks,inside);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_popup_tab_ownership(void)
{
    const int start[] = {25620,25621,25620,25600,25620,25620};
    const int want[] = {25621,25621,25621,25621,25611,25600};
    for(int mode = 0; mode < 6; mode++) {
        InjectReset();
        if(mode == 2 || mode == 3) InjectKey(KEY_LEFT_SHIFT,1);
        InjectKeyTap(KEY_TAB); InjectPump();
        BeginUIFrame(240,240,1);
        SetUIFocus(start[mode]);
        RegisterUIFocus(25600,(Rectangle){0});
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
        RegisterUIFocus(25610,(Rectangle){0});
        UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){20,20,60,60});
        RegisterUIFocus(25620,(Rectangle){0});
        RegisterUIFocus(25621,(Rectangle){0});
        RegisterUIFocus(25620,(Rectangle){0});
        ui_popup_input_end(child);
        RegisterUIFocus(25611,(Rectangle){0});
        ui_popup_input_end(parent);
        RegisterUIFocus(25601,(Rectangle){0});
        if(mode == 4) ui_popup_input_close(context,1);
        if(mode == 5) ui_popup_input_close(context,0);
        EndUIFocus();
        check_int("popup Tab ownership and wraparound",GetUIFocus(),want[mode]);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_popup_text_keyboard_ownership(void)
{
    for(int retained = 0; retained < 2; retained++)
    for(int area = 0; area < 2; area++)
    for(int inside = 0; inside < 2; inside++) {
        char text[32] = "a";
        int cursor = 1, focused = 1;
        InjectReset(); ClearTextInputFocus(); InjectText("x"); InjectPump();
        BeginUIFrame(240,240,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        if(retained) BeginTree(Key("popup-editor-keyboard"));
        UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){180,180,40,40});
        UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){190,190,20,20});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(25500);
        if(area)
            TextArea((TextAreaProps){.bounds={10,10,120,80},.text=text,.text_size=sizeof(text),
                .cursor_position=&cursor,.focused=&focused,.focus_id=25500});
        else
            TextField((TextFieldProps){.bounds={10,10,120,28},.text=text,.text_size=sizeof(text),
                .cursor_position=&cursor,.focused=&focused,.focus_id=25500});
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        if(retained) EndTree();
        check_int("only top popup editor receives typing",strcmp(text,inside ? "ax" : "a"),0);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        ClearTextInputFocus();
        EndUIFrame();
    }
    InjectReset();
}

static void
test_composed_combo_scope(void)
{
    bool open = true;
    char text[16] = "edit";
    int cursor = 4;
    BeginUIFrame(240,180,1);
    BeginTree(Key("composed combo ordinary children"));
    Button((ButtonProps){.bounds={10,10,80,24},.label="Background",.id=26999});
    check_int("open composed combo returns true",
        BeginCombo((ComboProps){.bounds={10,40,100,28},.popup_size={120,80},
            .preview="Choose",.id=27000,.open=&open,.flags=ComboPopupAlignLeft}),1);
    Column((ColumnProps){.bounds={12,72,100,60},.gap=3});
    Button((ButtonProps){.bounds={0,0,90,24},.label="Action",.id=27001});
    TextField((TextFieldProps){.bounds={0,0,90,24},.text=text,.text_size=sizeof(text),
        .cursor_position=&cursor,.focus_id=27002});
    End();
    EndCombo();
    Button((ButtonProps){.bounds={120,10,80,24},.label="After",.id=27003});
    EndTree();
    int count = 0, action = 0, field = 0, after = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    for(int i = 0; i < count; i++) {
        if(nodes[i].id == 27001) {
            action++;
            check_int("composed layout detached from owner",
                nodes[nodes[i].parent].parent,0);
            check_int("composed child x",(int)nodes[i].bounds.x,12);
        } else if(nodes[i].id == 27002) field++;
        else if(nodes[i].id == 27003) after++;
    }
    check_int("ordinary button retained in composed combo",action,1);
    check_int("ordinary field retained in composed combo",field,1);
    check_int("parent declarations resume after combo",after,1);
    EndUIFrame();

    BeginUIFrame(240,180,1);
    BeginTree(Key("composed combo explicit close"));
    check_int("composed combo reopens from caller state",
        BeginCombo((ComboProps){.bounds={10,40,100,28},.popup_size={120,80},
            .preview="Choose",.id=27000,.open=&open,
            .flags=ComboNoArrowButton|ComboWidthFitPreview|ComboHeightSmall}),1);
    CloseCombo();
    check_int("CloseCombo updates caller state",open,0);
    EndCombo();
    EndTree();
    EndUIFrame();
    BeginUIFrame(240,180,1);
    check_int("closed composed combo stays closed",
        BeginCombo((ComboProps){.bounds={10,40,100,28},.popup_size={120,80},
            .preview="Choose",.id=27000,.open=&open}),0);
    EndUIFrame();
}

static void
test_composed_popup_scope(void)
{
    bool open = true;
    BeginUIFrame(240,180,1);
    BeginTree(Key("composed popup ordinary children"));
    check_int("open composed popup returns true",
        BeginPopup((PopupProps){.bounds={20,30,140,100},.id=29000,.open=&open}),1);
    Column((ColumnProps){.bounds={28,40,120,70},.gap=4});
    Button((ButtonProps){.bounds={0,0,100,28},.label="Apply",.id=29001});
    End();
    EndPopup();
    Button((ButtonProps){.bounds={160,30,70,28},.label="After",.id=29002});
    EndTree();
    int count = 0, child = 0, after = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    for(int i = 0; i < count; i++) {
        if(nodes[i].id == 29001) {
            child++;
            check_int("composed popup child x",(int)nodes[i].bounds.x,28);
        } else if(nodes[i].id == 29002) after++;
    }
    check_int("ordinary button retained in composed popup",child,1);
    check_int("parent resumes after composed popup",after,1);
    EndUIFrame();

    BeginUIFrame(240,180,1);
    BeginTree(Key("composed popup explicit close"));
    check_int("composed popup reopens from caller state",
        BeginPopup((PopupProps){.bounds={20,30,140,100},.id=29000,.open=&open}),1);
    ClosePopup();
    check_int("ClosePopup updates caller state",open,0);
    EndPopup();
    EndTree();
    EndUIFrame();

    BeginUIFrame(240,180,1);
    check_int("invalid composed popup stays closed",
        BeginPopup((PopupProps){.bounds={20,30,0,100},.id=29000,.open=&open}),0);
    EndUIFrame();
}

static void
test_composed_tooltip_scope(void)
{
    InjectReset();
    InjectMousePosition(30,25);
    InjectPump();
    BeginUIFrame(240,180,1);
    BeginTree(Key("composed tooltip arbitrary children"));
    check_int("hovered tooltip popup opens",
        BeginPopup((PopupProps){.bounds={80,50,130,70},.id=29300,
            .trigger={20,20,80,30},.flags=PopupTooltip}),1);
    Column((ColumnProps){.bounds={88,58,114,54},.gap=4});
    Text((TextProps){.text="arbitrary tooltip",.font=Text14,
        .color=BLACK,.wrap=TextWrapNone});
    Button((ButtonProps){.bounds={0,0,90,24},.label="detail",.id=29301});
    End();
    check_int("tooltip does not capture popup input",
        ui_popup_input_current_captures((Vector2){90,60}),0);
    EndPopup();
    EndTree();
    int count = 0, child = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    for(int i = 0; i < count; i++) if(nodes[i].id == 29301) child++;
    check_int("ordinary child retained in tooltip",child,1);
    EndUIFrame();

    InjectMousePosition(230,170);
    InjectPump();
    BeginUIFrame(240,180,1);
    check_int("tooltip closes outside trigger",
        BeginPopup((PopupProps){.bounds={80,50,130,70},.id=29300,
            .trigger={20,20,80,30},.flags=PopupTooltip}),0);
    EndUIFrame();
}

static void
test_composed_modal_scope(void)
{
    bool open = true;
    InjectReset();
    BeginUIFrame(240,180,1);
    BeginTree(Key("composed modal arbitrary children"));
    check_int("open composed modal returns true",
        BeginPopup((PopupProps){.bounds={40,30,120,90},.id=29400,
            .open=&open,.flags=PopupModal}),1);
    Column((ColumnProps){.bounds={48,38,104,70},.gap=4});
    Text((TextProps){.text="arbitrary modal",.font=Text14,
        .color=BLACK,.wrap=TextWrapNone});
    Button((ButtonProps){.bounds={0,0,90,24},.label="confirm",.id=29401});
    End();
    EndPopup();
    Button((ButtonProps){.bounds={190,145,45,30},.label="Behind",.id=29402});
    EndTree();
    int count = 0, child = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    for(int i = 0; i < count; i++) if(nodes[i].id == 29401) child++;
    check_int("ordinary child retained in modal",child,1);
    check_int("outside declaration leaves modal open",open,1);
    EndUIFrame();

    InjectKeyTap(KEY_ESCAPE);
    InjectPump();
    BeginUIFrame(240,180,1);
    check_int("Escape closes composed modal",
        BeginPopup((PopupProps){.bounds={40,30,120,90},.id=29400,
            .open=&open,.flags=PopupModal}),0);
    check_int("Escape updates modal caller state",open,0);
    EndUIFrame();
    InjectReset();
}

static void
test_composed_context_popup_scope(void)
{
    bool open = false;
    InjectReset();
    InjectMousePosition(30,25);
    InjectMouseButton(MOUSE_BUTTON_RIGHT,1); InjectPump();
    InjectMousePosition(30,25);
    InjectMouseButton(MOUSE_BUTTON_RIGHT,0); InjectPump();
    BeginUIFrame(240,180,1);
    BeginTree(Key("composed context popup arbitrary children"));
    check_int("right release opens composed context popup",
        BeginPopup((PopupProps){.bounds={80,50,130,70},.id=29500,
            .open=&open,.trigger={20,20,80,30},.flags=PopupContext}),1);
    Button((ButtonProps){.bounds={88,58,100,24},.label="context child",
        .id=29501});
    EndPopup();
    EndTree();
    check_int("context popup updates caller open state",open,1);
    int count = 0, child = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    for(int i = 0; i < count; i++)
        if(nodes[i].id == 29501) child++;
    check_int("ordinary child retained in context popup",child,1);
    EndUIFrame();

    InjectTap(220,160); InjectPump(); InjectPump();
    BeginUIFrame(240,180,1);
    check_int("outside release dismisses context popup",
        BeginPopup((PopupProps){.bounds={80,50,130,70},.id=29500,
            .open=&open,.trigger={20,20,80,30},.flags=PopupContext}),0);
    check_int("context popup dismissal updates caller",open,0);
    EndUIFrame();

    InjectReset();
    InjectMousePosition(30,25);
    InjectMouseButton(MOUSE_BUTTON_RIGHT,1); InjectPump();
    InjectMousePosition(30,25);
    InjectMouseButton(MOUSE_BUTTON_RIGHT,0); InjectPump();
    BeginUIFrame(240,180,1);
    check_int("disabled context popup stays closed",
        BeginPopup((PopupProps){.bounds={80,50,130,70},.id=29500,
            .open=&open,.trigger={20,20,80,30},.flags=PopupContext,
            .disabled=1}),0);
    EndUIFrame();
    InjectReset();
}

static void
test_composed_popup_focus_lifecycle(void)
{
    bool parent_open = true, child_open = false;
    UIPopupInput *context = ui_popup_input_create();
    UIPopupInput *previous;
    InjectReset();
    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    SetUIFocus(29600);
    BeginTree(Key("composed popup focus acquisition"));
    check_int("focus parent popup opens",
        BeginPopup((PopupProps){.bounds={20,20,180,130},.id=29610,
            .open=&parent_open}),1);
    BeginDisabled(1);
    Button((ButtonProps){.bounds={30,30,100,24},.label="Disabled",.id=29612});
    EndDisabled();
    Button((ButtonProps){.bounds={30,30,100,24},.label="Parent",.id=29611});
    EndPopup();
    EndTree();
    check_int("parent popup acquires first child focus",GetUIFocus(),29611);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    child_open = true;
    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    BeginTree(Key("composed nested popup focus"));
    BeginPopup((PopupProps){.bounds={20,20,180,130},.id=29610,
        .open=&parent_open});
    Button((ButtonProps){.bounds={30,30,100,24},.label="Parent",.id=29611});
    BeginPopup((PopupProps){.bounds={50,60,130,80},.id=29620,
        .open=&child_open});
    Button((ButtonProps){.bounds={60,70,100,24},.label="Child",.id=29621});
    EndPopup();
    EndPopup();
    EndTree();
    check_int("nested popup acquires first child focus",GetUIFocus(),29621);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    BeginTree(Key("composed nested popup focus restore"));
    BeginPopup((PopupProps){.bounds={20,20,180,130},.id=29610,
        .open=&parent_open});
    Button((ButtonProps){.bounds={30,30,100,24},.label="Parent",.id=29611});
    BeginPopup((PopupProps){.bounds={50,60,130,80},.id=29620,
        .open=&child_open});
    Button((ButtonProps){.bounds={60,70,100,24},.label="Child",.id=29621});
    ClosePopup();
    check_int("nested popup restores parent focus",GetUIFocus(),29611);
    EndPopup();
    EndPopup();
    EndTree();
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    BeginTree(Key("composed parent popup focus restore"));
    BeginPopup((PopupProps){.bounds={20,20,180,130},.id=29610,
        .open=&parent_open});
    Button((ButtonProps){.bounds={30,30,100,24},.label="Parent",.id=29611});
    ClosePopup();
    check_int("parent popup restores background focus",GetUIFocus(),29600);
    EndPopup();
    EndTree();
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    parent_open = true;
    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    SetUIFocus(29700);
    BeginTree(Key("composed missing popup owner focus"));
    BeginPopup((PopupProps){.bounds={20,20,120,80},.id=29710,
        .open=&parent_open});
    Button((ButtonProps){.bounds={30,30,90,24},.label="Popup",.id=29711});
    EndPopup();
    EndTree();
    check_int("popup before missing owner has child focus",GetUIFocus(),29711);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    BeginTree(Key("composed missing popup owner restore"));
    EndTree();
    ui_popup_input_finish(context);
    check_int("missing popup owner restores background focus",GetUIFocus(),29700);
    ui_popup_input_bind(previous);
    EndUIFrame();
    ui_popup_input_destroy(context);
    InjectReset();
}

static void
test_popup_active_drag_ownership(void)
{
    UIPopupInput *context = ui_popup_input_create();
    UIPopupInput *previous;
    UIPopupInputToken owner;
    float drag_value = 10.0f;
    float background_value = 10.0f;
    float slider_value = 0.0f;
    int split = 50;
    DragFloatProps drag = {.bounds={30,30,80,24},.id=401,
        .values=&drag_value,.value_count=1,.speed=1.0f,.min=0,.max=500};
    SliderFloatProps slider = {.bounds={30,30,80,24},.id=411,
        .values=&slider_value,.value_count=1,.min=0,.max=100};
    DragFloatProps background_drag = {.bounds={30,30,80,24},.id=391,
        .values=&background_value,.value_count=1,.speed=1.0f,.min=0,.max=500};
    const char *columns[] = {"A","B"};
    const char *cells[] = {"a","b"};
    TableRow rows[] = {{cells,2,NULL,NULL}};
    int widths[] = {70,70};
    TableViewProps table = {0};
    PanedViewProps panes = {{30,30,100,80},416,1,&split,20,20};

    table.bounds = (Rectangle){25,25,140,90};
    table.id = 421;
    table.columns = columns;
    table.column_count = 2;
    table.rows = rows;
    table.row_count = 1;
    table.column_widths = widths;
    table.resizable = 1;
    table.min_column_width = 32;

    InjectReset();
    InjectMousePosition(40,40);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    (void)DragFloat(background_drag);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    InjectMousePosition(80,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,390,(Rectangle){20,20,120,100});
    ui_popup_input_end(owner);
    check_int("new popup cancels background drag",DragFloat(background_drag),0);
    check_int("new popup blocks background drag mutation",(int)background_value,10);
    ui_popup_input_close(context,390);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();

    InjectReset();
    InjectMousePosition(40,40);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,400,(Rectangle){20,20,120,100});
    (void)DragFloat(drag);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    InjectMousePosition(200,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,400,(Rectangle){20,20,120,100});
    check_int("popup drag continues outside bounds",DragFloat(drag),1);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("popup drag outside value",(int)drag_value,170);

    InjectMousePosition(230,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    check_int("missing popup cancels active drag",DragFloat(drag),0);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("dismissed popup drag does not mutate background",(int)drag_value,170);

    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    InjectMousePosition(50,40);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,410,(Rectangle){20,20,120,100});
    (void)SliderFloat(slider);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    InjectMousePosition(90,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,410,(Rectangle){20,20,120,100});
    (void)SliderFloat(slider);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("popup slider follows owned drag",(int)slider_value,75);

    InjectMousePosition(30,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    (void)SliderFloat(slider);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("dismissed popup slider does not mutate background",(int)slider_value,75);

    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    InjectMousePosition(80,40);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,415,(Rectangle){20,20,120,100});
    (void)PanedView(panes);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    InjectMousePosition(110,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,415,(Rectangle){20,20,120,100});
    check_int("popup splitter follows owned drag",PanedView(panes),1);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("popup splitter value",split,80);

    InjectMousePosition(60,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    (void)PanedView(panes);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("dismissed popup splitter does not mutate background",split,80);

    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    InjectMousePosition(93,35);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,420,(Rectangle){20,20,160,110});
    (void)TableView(table);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    InjectMousePosition(123,35);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,420,(Rectangle){20,20,160,110});
    check_int("popup table resize follows owned drag",TableView(table),1);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("popup table resize width",widths[0],100);

    InjectMousePosition(153,35);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    (void)TableView(table);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("dismissed popup resize does not mutate background",widths[0],100);

    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    ui_popup_input_destroy(context);
    InjectReset();
}

static void
test_popup_combo_keyboard_ownership(void)
{
    const char *options[] = {"One","Two"};
    for(int inside = 0; inside < 2; inside++) {
        InjectReset(); InjectKeyTap(KEY_SPACE); InjectPump();
        BeginUIFrame(240,240,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){180,180,40,40});
        UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){190,190,20,20});
        if(!inside) ui_popup_input_end(child);
        check_int("keyboard capture is independent of pointer bounds",ui_popup_input_keyboard_captures(),!inside);
        int selected = 0;
        SetUIFocus(25400);
        Combobox((ComboboxProps){.bounds={10,10,100,28},.id=25400,
            .options=options,.option_count=2,.selected_index=&selected});
        check_int("only top popup may open a focused combo",ui_dropdown_captures_click((Vector2){20,60}),inside);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_close(context,1);
        check_int("child dismissal restores parent keyboard",ui_popup_input_keyboard_captures(),0);
        ui_popup_input_end(parent);
        check_int("parent still captures background keyboard",ui_popup_input_keyboard_captures(),1);
        ui_popup_input_close(context,0);
        check_int("branch dismissal restores background keyboard",ui_popup_input_keyboard_captures(),0);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        ui_dropdown_close(25400);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_popup_accelerator_keyboard_ownership(void)
{
    Accelerator copy = {KEY_C,1,0,0,91};
    Accelerator commands[] = {{KEY_X,1,0,0,90}, {KEY_C,1,0,0,91}};
    InjectReset(); InjectKey(KEY_LEFT_CONTROL,1); InjectKeyTap(KEY_C); InjectPump();
    BeginUIFrame(240,240,1);
    UIPopupInput *context = ui_popup_input_create();
    ui_popup_input_frame(context);
    UIPopupInput *previous = ui_popup_input_bind(context);
    UIPopupInputToken parent = ui_popup_input_begin(context,26000,(Rectangle){10,10,120,100});
    UIPopupInputToken child = ui_popup_input_begin(context,26001,(Rectangle){20,20,80,60});

    ui_popup_input_end(child);
    check_int("parent accelerator blocked behind child",AcceleratorPressed(copy),0);
    child = ui_popup_input_begin(context,26001,(Rectangle){20,20,80,60});
    check_int("top popup accelerator dispatch",DispatchAccelerators(commands,2),91);
    ui_popup_input_end(child);
    ui_popup_input_close(context,26001);
    check_int("parent accelerator restored after child close",AcceleratorPressed(copy),91);
    ui_popup_input_end(parent);
    check_int("background accelerator blocked behind parent",AcceleratorPressed(copy),0);
    ui_popup_input_close(context,26000);
    check_int("background accelerator restored after popup close",AcceleratorPressed(copy),91);
    BeginDisabled(1);
    check_int("disabled accelerator blocked",AcceleratorPressed(copy),0);
    EndDisabled();

    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    ui_popup_input_destroy(context);
    EndUIFrame();
    InjectKey(KEY_LEFT_CONTROL,0);
    InjectReset();
}

static void
test_popup_collapsible_keyboard_ownership(void)
{
    for(int inside = 0; inside < 2; inside++) {
        bool open = false;
        InjectReset(); InjectKeyTap(KEY_RIGHT); InjectPump();
        BeginUIFrame(240,240,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(context,26100,(Rectangle){10,10,120,100});
        UIPopupInputToken child = ui_popup_input_begin(context,26101,(Rectangle){20,20,80,60});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(26110);
        Collapsible((CollapsibleProps){.bounds={20,20,80,28},.id=26110,
                    .label="Node",.open=&open,.tree=1});
        check_int("only top popup collapsible handles keyboard",open,inside);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_retained_popup_pointer_focus(void)
{
    for(int blocked = 0; blocked < 2; blocked++) {
        InjectReset(); InjectTap(60,60); InjectPump();
        BeginUIFrame(240,240,1);
        SetUIFocus(0);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        PushUIInputCapture((Rectangle){0,0,blocked ? 5 : 240,240},1);
        BeginTree(Key("retained-popup-pointer-focus"));
        UIPopupInputToken outer = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
        UIPopupInputToken inner = ui_popup_input_begin(context,1,(Rectangle){50,50,60,60});
        Row((RowProps){.bounds={50,50,80,24}});
        Button((ButtonProps){.bounds={0,0,40,24},.id=25301,.label="Child"});
        End();
        ui_popup_input_end(inner);
        Button((ButtonProps){.bounds={50,50,40,24},.id=25302,.label="Parent"});
        ui_popup_input_end(outer);
        Button((ButtonProps){.bounds={50,50,40,24},.id=25303,.label="Background"});
        check_int("immediate popup focus preserves modal blocking",GetUIFocus(),blocked ? 0 : 25301);
        /* Exercise the deferred focus pass independently of the immediate
         * button check; the popup scopes are already closed here. */
        SetUIFocus(0);
        UIEvent event;
        while(NextEvent(&event)) {}
        EndTree();
        check_int("deferred popup focus preserves modal blocking",GetUIFocus(),blocked ? 0 : 25301);
        int clicks = 0;
        while(NextEvent(&event)) {
            if(event.kind != UI_EVENT_CLICK) continue;
            check_int("deferred click belongs to popup child",(int)event.key,25301);
            clicks++;
        }
        check_int("modal blocker prevents deferred popup clicks",clicks,blocked ? 0 : 1);
        ClearUIInputCaptures();
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_retained_popup_input_ownership(void)
{
    InjectReset();
    BeginUIFrame(240,240,1);
    BeginTree(Key("retained-popup-input"));
    Button((ButtonProps){.bounds={50,50,40,24},.id=25200,.label="Before"});
    UIPopupInput *context = ui_popup_input_create();
    ui_popup_input_frame(context);
    UIPopupInput *previous = ui_popup_input_bind(context);
    UIPopupInputToken outer = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
    UIPopupInputToken inner = ui_popup_input_begin(context,1,(Rectangle){50,50,60,60});
    Button((ButtonProps){.bounds={50,50,40,24},.id=25201,.label="Child"});
    ui_popup_input_end(inner);
    Button((ButtonProps){.bounds={50,50,40,24},.id=25202,.label="Later parent"});
    ui_popup_input_end(outer);
    Button((ButtonProps){.bounds={50,50,40,24},.id=25203,.label="After"});
    EndTree();
    const UIWidgetNode *node = GetNode(HitTestNode((Vector2){60,60}));
    check_int("retained child beats later parent and background",node ? node->id : -1,25201);
    NodeId child_hit = HitTestNode((Vector2){60,60});
    BeginTree(Key("replacement-popup-tree"));
    check_int("pending declaration preserves committed popup ownership",
        HitTestNode((Vector2){60,60}),child_hit);
    check_int("deferred hit test does not reopen input scope",ui_popup_input_snapshot().order,0);
    ui_popup_input_close(context,1);
    node = GetNode(HitTestNode((Vector2){60,60}));
    check_int("retained parent receives input after child closes",node ? node->id : -1,25202);
    ui_popup_input_close(context,0);
    node = GetNode(HitTestNode((Vector2){60,60}));
    check_int("retained background receives input after branch closes",node ? node->id : -1,25203);
    ui_popup_input_finish(context);
    ui_popup_input_frame(context);
    check_int("previous-frame retained owners reject input",HitTestNode((Vector2){60,60}) <= 1,1);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    ui_popup_input_destroy(context);
    node = GetNode(HitTestNode((Vector2){60,60}));
    check_int("destroyed registry snapshots reject input safely",node ? node->id : -1,25200);
    EndTree();
    EndUIFrame();
}

static void
test_nested_popup_input_ownership(void)
{
    UIPopupInput *context = ui_popup_input_create();
    UIPopupInput *previous = ui_popup_input_bind(context);
    int background = 0, parent = 0, child = 0;
    InjectReset();
    for(int frame = 0; frame < 4; frame++) {
        if(frame == 1) InjectTap(60,60);
        InjectPump(); BeginUIFrame(240,240,1);
        ui_popup_input_frame(context);
        background += Button((ButtonProps){.bounds={50,50,40,24},.id=25100,.label="Before"});
        UIPopupInputToken outer = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
        parent += Button((ButtonProps){.bounds={50,50,40,24},.id=25101,.label="Parent"});
        UIPopupInputToken inner = ui_popup_input_begin(context,1,(Rectangle){50,50,60,60});
        child += Button((ButtonProps){.bounds={50,50,40,24},.id=25102,.label="Child"});
        ui_popup_input_end(inner);
        ui_popup_input_end(outer);
        background += Button((ButtonProps){.bounds={50,50,40,24},.id=25103,.label="After"});
        ui_popup_input_finish(context);
        EndUIFrame();
    }
    check_int("popup background controls blocked",background,0);
    check_int("popup parent cannot steal child input",parent,0);
    check_int("ordinary popup child button activates",child,1);
    ui_popup_input_frame(context);
    UIPopupInputToken outer = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
    check_int("previous-frame child remains above parent",ui_popup_input_captures(context,(Vector2){60,60}),1);
    ui_popup_input_close(context,1);
    check_int("closing child restores parent input",ui_popup_input_captures(context,(Vector2){60,60}),0);
    ui_popup_input_end(outer);
    ui_popup_input_finish(context);
    UIPopupInput *other = ui_popup_input_create();
    ui_popup_input_bind(other);
    check_int("popup capture remains context-local",ui_popup_input_current_captures((Vector2){60,60}),0);
    ui_popup_input_bind(context);
    check_int("rebinding restores popup capture",ui_popup_input_current_captures((Vector2){60,60}),1);
    ui_popup_input_frame(context);
    ui_popup_input_finish(context);
    check_int("missing popup owner retired",ui_popup_input_current_captures((Vector2){60,60}),0);
    ui_popup_input_frame(context);
    outer = ui_popup_input_begin(context,0,(Rectangle){0,0,100,100});
    UIPopupInputToken nested = ui_popup_input_begin(context,1,(Rectangle){0,0,100,100});
    ui_popup_input_end(nested); ui_popup_input_end(outer);
    UIPopupInputToken sibling = ui_popup_input_begin(context,2,(Rectangle){0,0,100,100});
    check_int("later sibling beats earlier nested branch",ui_popup_input_captures(context,(Vector2){60,60}),0);
    nested = ui_popup_input_begin(context,3,(Rectangle){0,0,100,100});
    ui_popup_input_end(nested); ui_popup_input_end(sibling);
    ui_popup_input_finish(context);
    ui_popup_input_frame(context);
    outer = ui_popup_input_begin(context,0,(Rectangle){0,0,100,100});
    ui_popup_input_close(context,1);
    check_int("reordered root beats other branch descendants",ui_popup_input_captures(context,(Vector2){60,60}),0);
    nested = ui_popup_input_begin(context,1,(Rectangle){0,0,100,100});
    ui_popup_input_close(context,0);
    check_int("closing parent disables active child",ui_popup_input_captures(context,(Vector2){60,60}),1);
    ui_popup_input_end(nested); ui_popup_input_end(outer);
    ui_popup_input_finish(context);
    check_int("closed and missing branches retired",ui_popup_input_current_captures((Vector2){60,60}),0);
    ui_popup_input_bind(previous);
    ui_popup_input_destroy(other);
    ui_popup_input_destroy(context);
    InjectReset();
}

static void
test_popup_input_clip_restoration(void)
{
    for(int blocked = 0; blocked < 2; blocked++) {
        int popup_actions = 0, parent_actions = 0;
        InjectReset(); InjectTap(20,20);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1);
            BeginTree(Key("popup input clip"));
            PushUIInputCapture((Rectangle){0,0,blocked ? 5 : 100,100},1);
            BeginScroll((Rectangle){0,0,1,1},100,NULL);
            UIInputClipScope scope = ui_input_clip_suspend();
            EndScroll(); /* Cannot pop the suspended owner's scroll scope. */
            popup_actions += Button((ButtonProps){.bounds={10,10,40,20},.id=25004,.label="Popup"});
            ui_input_clip_resume(scope);
            parent_actions += Button((ButtonProps){.bounds={10,10,40,20},.id=25005,.label="Clipped"});
            EndScroll();
            EndTree(); ClearUIInputCaptures(); EndUIFrame();
        }
        check_int("popup input escapes owner clip but respects capture",popup_actions,!blocked);
        check_int("owner input clip restored",parent_actions,0);
    }
    InjectReset();
}

static void
test_popup_disabled_restoration(void)
{
    for(int disabled = 0; disabled < 2; disabled++) {
        BeginDisabled(disabled);
        UIDisabledScope outer = ui_disabled_suspend();
        EndDisabled();
        check_int("popup cannot end parent disabled scope",UIContentDisabled(),disabled);
        BeginDisabled(1);
        UIDisabledScope inner = ui_disabled_suspend();
        BeginDisabled(0); EndDisabled();
        check_int("nested popup inherits disabled",UIContentDisabled(),1);
        ui_disabled_resume(inner);
        EndDisabled();
        check_int("popup child disabling restored",UIContentDisabled(),disabled);
        ui_disabled_resume(outer);
        check_int("popup parent disabling restored",UIContentDisabled(),disabled);
        EndDisabled();
        check_int("parent disabled scope remains balanced",UIContentDisabled(),0);
    }
}

static void
test_popup_layout_restoration(void)
{
    BeginTree(Key("popup layout restoration"));
    NodeId parent = Row((RowProps){.bounds={10,10,200,20},.gap=5});
    Rect(0,0,20,20,RED,BLANK);
    UITreeLayoutScope scope = ui_tree_layout_suspend();
    NodeId popup = Row((RowProps){.bounds={0,0,100,20},.gap=3});
    Rect(0,0,30,20,BLUE,BLANK);
    End();
    ui_tree_layout_resume(scope);
    Rect(0,0,20,20,GREEN,BLANK);
    End();
    EndTree();
    int count = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    check_int("isolated popup tree count",count,6);
    if(count != 6) return;
    check_int("popup attached to screen root",nodes[popup].parent,0);
    check_int("popup child layout origin",(int)nodes[4].bounds.x,0);
    check_int("parent child before popup",(int)nodes[2].bounds.x,10);
    check_int("parent child after popup",(int)nodes[5].bounds.x,35);
    check_int("parent relationship restored",nodes[5].parent,parent);
}

static void
test_combo_horizontal_viewport(void)
{
    const Rectangle bounds[] = {{-20,10,160,28},{200,10,160,28},{10,10,400,28}};
    const char *options[] = {"One","Two"};
    for(int i = 0; i < 3; i++) {
        int selected = 0;
        InjectReset(); InjectKeyTap(KEY_SPACE);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1); SetUIFocus(25002);
            Combobox((ComboboxProps){.bounds=bounds[i],.id=25002,.options=options,.option_count=2,.selected_index=&selected});
            EndUIFrame();
        }
        int x = i == 1 ? 92 : 12;
        check_int("shifted popup captures row",ui_dropdown_captures_click((Vector2){x,80}),1);
        check_int("popup left edge bounded",ui_dropdown_captures_click((Vector2){-1,80}),0);
        check_int("popup right edge bounded",ui_dropdown_captures_click((Vector2){241,80}),0);
        InjectTap(x,80);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1);
            Combobox((ComboboxProps){.bounds=bounds[i],.id=25002,.options=options,.option_count=2,.selected_index=&selected});
            EndUIFrame();
        }
        check_int("shifted popup selected second row",selected,1);
        InjectReset(); BeginUIFrame(240,240,1); EndUIFrame();
    }
}

static void
test_combo_scrollbar_dismissal(void)
{
    const char *options[131];
    for(int i = 0; i < 131; i++) options[i] = "item";
    for(int mode = 0; mode < 3; mode++) {
        int selected = 0;
        InjectReset();
        InjectKeyTap(KEY_SPACE);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1);
            SetUIFocus(25001);
            Combobox((ComboboxProps){.bounds={10,10,160,28},.id=25001,
                .options=options,.option_count=131,.selected_index=&selected});
            EndUIFrame();
        }
        InjectMousePosition(166,50); InjectMouseButton(MOUSE_BUTTON_LEFT,1);
        InjectPump(); BeginUIFrame(240,240,1);
        Combobox((ComboboxProps){.bounds={10,10,160,28},.id=25001,
            .options=options,.option_count=131,.selected_index=&selected});
        EndUIFrame();
        check_int("combo scrollbar acquired drag",g_ui_pointer_owner,UI_POINTER_OWNER_SCROLL);
        if(mode == 0) InjectKeyTap(KEY_ESCAPE);
        InjectPump(); BeginUIFrame(240,240,1);
        if(mode != 2)
            Combobox((ComboboxProps){.bounds={10,10,160,28},.id=25001,
                .options=options,.option_count=131,.selected_index=&selected,.disabled=mode==1});
        EndUIFrame();
        check_int("dismissed combo scrollbar released drag",g_ui_pointer_owner,UI_POINTER_OWNER_NONE);
        check_int("dismissed combo scrollbar released capture",ui_dropdown_captures_click((Vector2){20,70}),0);
        InjectReset(); BeginUIFrame(240,240,1); EndUIFrame();
    }
}

static void
test_combo_keyboard_open(void)
{
    const char *options[] = {"One", "Two"};
    const int keys[] = {KEY_ENTER, KEY_KP_ENTER, KEY_SPACE, KEY_DOWN};
    for(int key = 0; key < 4; key++) {
        for(int mode = 0; mode < 3; mode++) {
            int selected = 1;
            InjectReset();
            InjectKeyTap(keys[key]);
            for(int frame = 0; frame < 3; frame++) {
                InjectPump();
                BeginUIFrame(240,240,1);
                SetUIFocus(24000);
                BeginDisabled(mode == 2);
                Combobox((ComboboxProps){.bounds = {10,10,160,28}, .id = 24000,
                    .options = options, .option_count = 2, .selected_index = &selected,
                    .disabled = mode == 1});
                EndDisabled();
                EndUIFrame();
            }
            check_int("focused combo keyboard opening",UIInputCapturesClick((Vector2){20,70}),mode == 0);
            check_int("opening key does not commit or move selection",selected,1);
            InjectReset();
            BeginUIFrame(240,240,1);
            EndUIFrame();
        }
    }
}

static void
test_custom_table_cell_scope(void)
{
    const char *columns[] = {"A","B"};
    TableRow rows[3] = {{0}};
    int order[] = {1,0}, scroll = 20, actions = 0;
    TableViewProps p = {.bounds = {10,10,200,90}, .columns = columns, .column_count = 2,
                       .rows = rows, .row_count = 3, .column_order = order, .row_height = 30,
                       .freeze_rows = 1, .scroll_offset = &scroll, .custom_cells = 1};
    InjectReset(); InjectTap(120,75);
    for(int frame = 0; frame < 2; frame++) {
        InjectPump(); BeginUIFrame(300,200,1.0f); TableView(p);
        Rectangle cell = BeginTableCell(p,1,0);
        check_int("custom cell reordered x",(int)cell.x,110);
        check_int("custom cell scrolling y",(int)cell.y,50);
        check_int("custom cell frozen clip",UIInputCapturesClick((Vector2){120,60}),1);
        if(Button((ButtonProps){.bounds = cell,.label = "Child",.id = 1000})) actions++;
        EndTableCell();
        p.disabled = 1; BeginTableCell(p,0,1);
        check_int("custom cell disabled",UIContentDisabled(),1);
        EndTableCell(); p.disabled = 0;
        check_int("custom cell disabled restored",UIContentDisabled(),0);
        EndUIFrame();
    }
    check_int("custom cell child action",actions,1);
}

static void
test_retained_scope_clip(void)
{
    InjectReset();
    BeginUIFrame(200,120,1.0f);
    BeginTree(Key("retained-scope-clip"));
    BeginScroll((Rectangle){10,10,50,30},30,NULL);
    Row((RowProps){.bounds = {10,10,100,30}});
    Button((ButtonProps){.bounds = {0,0,100,30},.label = "Clipped",.id = 1005});
    End(); EndScroll(); EndTree();
    NodeId inside = HitTestNode((Vector2){20,20});
    const UIWidgetNode *node = GetNode(inside);
    check_int("retained cell hit",node != NULL ? node->id : -1,1005);
    check_int("retained clip captured",node->has_input_clip,1);
    check_int("retained clip width",(int)node->input_clip.width,50);
    NodeId outside = HitTestNode((Vector2){80,20});
    check_int("retained clip rejects outside",outside == inside,0);
    EndUIFrame();
}

static void
test_list_box_scope(void)
{
    for(int disabled = 0; disabled < 2; disabled++) {
        int offset = 0;
        InjectReset(); InjectMousePosition(30,30); InjectWheel(-1); InjectPump();
        BeginUIFrame(200,150,1.0f);
        Rectangle content = BeginListBox((ListBoxProps){.bounds = {20,20,120,80}, .item_count = 4, .row_height = 25, .scroll_offset = &offset, .disabled = disabled});
        check_int("list scope scroll",offset,disabled ? 0 : 22);
        check_int("list scope content width",(int)content.width,108);
        check_int("list scope content y",(int)content.y,21-offset);
        check_int("list scope disabled",UIContentDisabled(),disabled);
        EndListBox();
        check_int("list scope restored",UIContentDisabled(),0);
        EndUIFrame();
    }
}

static void
test_scroll_scope(void)
{
    int offset = 0;
    Rectangle content;
    InjectReset();
    InjectMousePosition(30, 30);
    InjectWheel(-1);
    InjectPump();
    BeginUIFrame(220, 220, 1.0f);
    content = BeginScroll((Rectangle){10,10,100,60}, 200, &offset);
    check_int("scroll offset", offset, 42);
    check_int("scroll content y", (int)content.y, -32);
    check_int("scroll clipped input", UIInputCapturesClick((Vector2){20,90}), 1);
    (void)BeginScroll((Rectangle){20,30,100,60}, 100, NULL);
    check_int("nested scroll clips to parent", UIInputCapturesClick((Vector2){115,40}), 1);
    EndScroll();
    EndScroll();
    check_int("scroll restores input", UIInputCapturesClick((Vector2){20,90}), 0);
    EndUIFrame();
}

static void
test_scroll_thumb_drag(void)
{
    int offset = 0;
    InjectReset();
    for(int frame = 0; frame < 4; frame++) {
        InjectMousePosition(105, frame == 0 || frame == 3 ? 20 : 110);
        if(frame == 0) InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
        if(frame == 2) InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
        InjectPump();
        BeginUIFrame(220,220,1.0f);
        Rectangle content = BeginScroll((Rectangle){10,10,100,60},200,&offset);
        check_int("scrollbar reserves width", (int)content.width, 90);
        check_int("scrollbar excludes child input", UIInputCapturesClick((Vector2){105,20}), 1);
        EndScroll();
        EndUIFrame();
        check_int("scroll thumb drag and release", offset, frame == 0 ? 0 : 140);
    }
}

static void
test_table_column_resize(void)
{
    const char *columns[] = {"A", "B", "C"};
    const char *cells[] = {"a", "b", "c"};
    TableRow rows[] = {{cells, 3, NULL, NULL}};
    int widths[] = {100, 100, 100};
    TableViewProps table = {0};

    table.bounds = (Rectangle){10, 10, 300, 110};
    table.id = 143;
    table.columns = columns;
    table.column_count = 3;
    table.rows = rows;
    table.row_count = 1;
    table.column_widths = widths;
    table.row_height = 24;
    table.resizable = 1;
    table.min_column_width = 48;

    InjectReset();
    InjectMousePosition(108, 20);
    InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
    InjectPump();
    BeginUIFrame(340, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();

    InjectMousePosition(138, 20);
    InjectPump();
    BeginUIFrame(340, 180, 1.0f);
    check_int("table resize changed", TableView(table), 1);
    EndUIFrame();
    check_int("table resized width", widths[0], 130);

    InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
    InjectPump();
    BeginUIFrame(340, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();
}

static void
test_table_frozen_rows_hit_testing(void)
{
    const char *columns[] = {"Name"};
    const char *cells0[] = {"row 0"};
    const char *cells1[] = {"row 1"};
    const char *cells2[] = {"row 2"};
    const char *cells3[] = {"row 3"};
    const char *cells4[] = {"row 4"};
    TableRow rows[] = {
        {cells0, 1, NULL, NULL}, {cells1, 1, NULL, NULL},
        {cells2, 1, NULL, NULL}, {cells3, 1, NULL, NULL},
        {cells4, 1, NULL, NULL}
    };
    int selected_row = -1;
    int selected_column = -1;
    int scroll = 36;
    TableViewProps table = {0};

    table.bounds = (Rectangle){10, 10, 180, 102};
    table.id = 144;
    table.columns = columns;
    table.column_count = 1;
    table.rows = rows;
    table.row_count = 5;
    table.selected_row = &selected_row;
    table.selected_column = &selected_column;
    table.scroll_offset = &scroll;
    table.row_height = 24;
    table.freeze_rows = 1;

    InjectReset();
    InjectTap(30, 45);
    InjectPump();
    BeginUIFrame(300, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();
    InjectPump();
    BeginUIFrame(300, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();
    check_int("table frozen row hit", selected_row, 0);

    InjectTap(30, 70);
    InjectPump();
    BeginUIFrame(300, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();
    InjectPump();
    BeginUIFrame(300, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();
    check_int("table scrolled row hit", selected_row, 2);

    InjectTap(30, 115);
    for(int frame = 0; frame < 2; frame++) {
        InjectPump();
        BeginUIFrame(300, 180, 1.0f);
        (void)TableView(table);
        EndUIFrame();
    }
    check_int("table clipped row ignores click below body", selected_row, 2);
}

static void
test_table_keyboard_navigation(void)
{
    const char *columns[] = {"A", "B", "C"};
    const char *cells[] = {"a", "b", "c"};
    TableRow rows[] = {
        {cells,3,NULL,NULL}, {cells,3,NULL,NULL}, {cells,3,NULL,NULL},
        {cells,3,NULL,NULL}, {cells,3,NULL,NULL}, {cells,3,NULL,NULL}
    };
    int order[] = {2,0,1};
    int selected_row = 0, selected_column = 2;
    int activated_row = -1, activated_column = -1, scroll = 0;
    TableViewProps table = {
        .bounds={10,10,180,70}, .id=145, .columns=columns, .column_count=3,
        .rows=rows, .row_count=6, .selected_row=&selected_row,
        .selected_column=&selected_column, .activated_row=&activated_row,
        .activated_column=&activated_column, .scroll_offset=&scroll,
        .row_height=20, .column_order=order
    };

    InjectReset(); InjectKey(KEY_RIGHT,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145);
    int changed = TableView(table); EndUIFrame();
    InjectKey(KEY_RIGHT,0); InjectPump();
    check_int("table keyboard right changed",changed,1);
    check_int("table keyboard follows display order",selected_column,0);

    InjectKey(KEY_TAB,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_TAB,0); InjectPump();
    check_int("table tab advances within row",selected_column,1);
    check_int("table tab retains table focus",GetUIFocus(),145);

    InjectKey(KEY_LEFT_SHIFT,1); InjectKey(KEY_TAB,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_TAB,0); InjectKey(KEY_LEFT_SHIFT,0); InjectPump();
    check_int("table shift tab reverses within row",selected_column,0);
    check_int("table shift tab retains table focus",GetUIFocus(),145);

    InjectKey(KEY_DOWN,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_DOWN,0); InjectPump();
    check_int("table keyboard down",selected_row,1);

    InjectKey(KEY_F2,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    check_int("table keyboard activated row",activated_row,1);
    check_int("table keyboard activated column",activated_column,0);
    InjectKey(KEY_F2,0); InjectPump();

    for(int row = 2; row < 6; row++) {
        InjectKey(KEY_DOWN,1); InjectPump();
        BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
        InjectKey(KEY_DOWN,0); InjectPump();
        check_int("table keyboard advances each row",selected_row,row);
    }
    check_int("table keyboard reaches final row",selected_row,5);
    check_int("table keyboard scrolls selection",scroll,80);

    table.disabled = 1;
    InjectKey(KEY_UP,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_UP,0); InjectPump();
    check_int("disabled table blocks keyboard",selected_row,5);
    table.disabled = 0;

    InjectKey(KEY_ESCAPE,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_ESCAPE,0); InjectPump();
    check_int("table escape clears row",selected_row,-1);
    check_int("table escape clears column",selected_column,-1);
    InjectReset();
}

static void
test_popup_table_keyboard_ownership(void)
{
    const char *columns[] = {"A"};
    const char *cells[] = {"a"};
    TableRow rows[] = {{cells,1,NULL,NULL},{cells,1,NULL,NULL}};

    for(int inside = 0; inside < 2; inside++) {
        int selected_row = 0, selected_column = 0;
        TableViewProps table = {
            .bounds={20,20,100,80}, .id=26120, .columns=columns,
            .column_count=1, .rows=rows, .row_count=2,
            .selected_row=&selected_row, .selected_column=&selected_column
        };
        InjectReset(); InjectKeyTap(KEY_DOWN); InjectPump();
        BeginUIFrame(240,160,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(context,26100,(Rectangle){10,10,140,120});
        UIPopupInputToken child = ui_popup_input_begin(context,26101,(Rectangle){15,15,120,100});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(26120);
        TableView(table);
        check_int("only top popup table handles keyboard",selected_row,inside ? 1 : 0);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_paned_drag_outside_handle(void)
{
    UIFrameState saved = SaveUIFrameState();
    int split = 90;
    PanedViewProps panes = {{10,10,240,80}, 9450, 1, &split, 40, 40};
    InjectReset();
    for(int frame = 0; frame < 3; frame++) {
        InjectMousePosition(frame == 0 ? 100 : 190, 30);
        if(frame == 0) InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
        if(frame == 2) InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
        InjectPump();
        BeginUIFrame(640,480,1);
        PanedView(panes);
        EndUIFrame();
    }
    check_int("paned drag follows pointer outside original handle", split, 180);
    InjectReset();
    RestoreUIFrameState(saved);
}

static void
test_drag_drop_accepts_dragged_release(void)
{
    UIFrameState saved = SaveUIFrameState();
    int payload = 42, output = 0, accepted = 0;
    DragDropSourceProps source = {{10,10,80,40}, 9401, "integer", &payload, sizeof(payload), 0};
    DragDropTargetProps target = {{150,10,80,40}, 9402, "integer", &output, sizeof(output), &accepted, 0};

    InjectReset();
    for(int frame = 0; frame < 3; frame++) {
        InjectMousePosition(frame == 0 ? 30 : 180, 30);
        if(frame == 0) InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
        if(frame == 2) InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
        InjectPump();
        BeginUIFrame(640, 480, 1.0f);
        if(frame == 2) g_ui_pointer_dragging = 1;
        DragDropSource(source);
        DragDropTarget(target);
        EndUIFrame();
    }
    check_int("dragged release copies payload", output, payload);
    check_int("dragged release reports copied size", accepted, sizeof(payload));
    InjectReset();
    RestoreUIFrameState(saved);
}

int
main(void)
{
    {
        int value = 10;
        BeginUIFrame(220,120,1);
        BeginTree(Key("numeric origin layout"));
        Row((RowProps){.bounds = {0,0,220,24}});
        InputInt((InputIntProps){.bounds = {0,0,120,24}, .id = 872,
            .values = &value, .value_count = 1, .step = 1});
        Button((ButtonProps){.bounds = {0,0,40,24}, .id = 873, .label = "next"});
        End();
        EndTree();
        EndUIFrame();
        int count = 0, paints = 0, next = 0;
        const UIWidgetNode *nodes = GetTreeNodes(&count);
        for(int i = 0; i < count; i++) {
            if(nodes[i].kind == UI_WIDGET_TEXT_INPUT_PAINT_NODE) {
                paints++;
                check_int("numeric origin paint x", (int)nodes[i].bounds.x, 0);
                check_int("numeric origin parent", nodes[nodes[i].parent].id, 872);
            }
            if(nodes[i].id == 873) {
                next++;
                check_int("numeric consumes one row slot", (int)nodes[i].bounds.x, 120);
            }
        }
        check_int("numeric origin paint count", paints, 1);
        check_int("numeric next sibling count", next, 1);
    }
    {
        int value = 10;
        InjectReset();
        for(int frame = 0; frame < 4; frame++) {
            InjectMousePosition(100, 40);
            if(frame == 0) InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
            if(frame == 1) {
                InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
                InjectText("5");
            }
            if(frame == 2) InjectText("9");
            InjectPump();
            BeginUIFrame(220,120,1);
            BeginTree(Key("numeric typing"));
            Row((RowProps){.bounds = {20,30,120,24}});
            BeginDisabled(frame == 2);
            int changed = InputInt((InputIntProps){.bounds = {0,0,120,24}, .id = 871,
                .values = &value, .value_count = 1});
            check_int("numeric typing returns during declaration", changed, frame == 1);
            EndDisabled();
            End();
            EndTree();
            EndUIFrame();
            check_int("headless numeric typing and disabled discard", value, frame == 0 ? 10 : 105);
        }
        InjectReset();
    }
    {
        int value = 10;
        const int expected[] = {12, 10, 15, 15};
        for(int scenario = 0; scenario < 4; scenario++) {
            InjectReset();
            for(int frame = 0; frame < 2; frame++) {
                InjectMousePosition(scenario == 1 ? 104 : 128, 40);
                InjectMouseButton(MOUSE_BUTTON_LEFT, frame == 0);
                InjectKey(KEY_LEFT_SHIFT, scenario == 2);
                InjectPump();
                BeginUIFrame(220,120,1);
                BeginTree(Key("numeric steps"));
                Row((RowProps){.bounds = {20,30,120,24}});
                BeginDisabled(scenario == 3);
                InputInt((InputIntProps){.bounds = {0,0,120,24}, .id = 870,
                    .values = &value, .value_count = 1, .step = 2, .step_fast = 5});
                EndDisabled();
                End();
                EndTree();
                EndUIFrame();
            }
            check_int("headless numeric step lifecycle", value, expected[scenario]);
        }
        InjectReset();
    }
    UINumericInputState *editors[129];
    {
        int identities[396];
        int identity_count = 0;
        for(int kind = 0; kind < 3; kind++) {
            for(int id = -1; id <= 2; id++) {
                for(int component = 0; component < 33; component++) {
                    UINumericInputState *state = ui_numeric_input_state(kind, id, component);
                    check_int("numeric exact identity", state == ui_numeric_input_state(kind,id,component), 1);
                    for(int previous = 0; previous < identity_count; previous++) {
                        int delta = state->token-identities[previous];
                        check_int("numeric field/step IDs disjoint", delta >= 3 || delta <= -3, 1);
                    }
                    identities[identity_count++] = state->token;
                }
            }
        }
        check_int("numeric identities tested", identity_count, 396);
    }
    for(int i = 0; i < 129; i++) {
        editors[i] = ui_numeric_input_state(0, 1 + i*128, 0);
        snprintf(editors[i]->text, sizeof(editors[i]->text), "edit-%d", i);
        editors[i]->cursor = i % 8;
        editors[i]->focused = 1;
    }
    for(int i = 0; i < 129; i++) {
        char expected[64];
        UINumericInputState *state = ui_numeric_input_state(0, 1 + i*128, 0);
        snprintf(expected, sizeof(expected), "edit-%d", i);
        check_int("numeric editor stable address", state == editors[i], 1);
        check_int("numeric editor text isolation", strcmp(state->text, expected), 0);
        check_int("numeric editor cursor isolation", state->cursor, i % 8);
        check_int("numeric editor focus isolation", state->focused, 1);
    }
    test_paned_drag_outside_handle();
    test_drag_drop_accepts_dragged_release();
    Rectangle parent = {10, 20, 200, 120};
    FrameBox frame;
    Grid grid;
    Rectangle r;
    Rectangle hits[3] = {
        {0, 0, 20, 20},
        {10, 10, 20, 20},
        {100, 100, 10, 10}
    };

    SetUIScale(1.0f);
    test_theme_surface_helpers();
    test_semantic_font_sizes_follow_ui_scale();
    test_circle_click_uses_ui_release_path();

    SetThemeStyle(THEME_STYLE_RETRO);
    check_int("retro style", GetThemeStyle(), THEME_STYLE_RETRO);
    check_int("retro effective style", GetEffectiveThemeStyle(), THEME_STYLE_RETRO);
    check_int("retro bevel", GetUIStyleTokens().bevel_enabled, 1);

    /* Theme-section locale keys must resolve to real strings (the
     * settings picker wires these as fallbacks). */
    {
        static const char *keys[] = {
            "theme_style_label", "theme_style_system", "theme_style_retro",
            "theme_style_material", "theme_label",
            "theme_app", "theme_system", "theme_mode_label",
            "theme_follow_device", "theme_light", "theme_dark",
            "theme_color_label", "theme_picker_title"
        };
        size_t i;

        for(i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            const char *text = GetLocaleText(keys[i]);

            if(text == NULL || text[0] == '\0' || strcmp(text, keys[i]) == 0) {
                fprintf(stderr, "locale key unresolved: %s\n", keys[i]);
                return 1;
            }
        }
    }

    SetThemeStyle(THEME_STYLE_MATERIAL);
    check_int("material style", GetThemeStyle(), THEME_STYLE_MATERIAL);
    check_int("material effective style", GetEffectiveThemeStyle(), THEME_STYLE_MATERIAL);
    check_int("material bevel", GetUIStyleTokens().bevel_enabled, 0);
    check_int("material touch target", GetUIStyleTokens().touch_target_min, 48);

    SetThemeStyle((ThemeStyle)3);
    check_int("out-of-range style clamps", GetThemeStyle(), THEME_STYLE_SYSTEM);
    check_int("out-of-range effective style", GetEffectiveThemeStyle(),
              GetDefaultPlatformThemeStyle());
    check_int("theme count", THEME_COUNT, THEME_SWEET + 1);
    check_int("out-of-range theme normalizes", NormalizeTheme(THEME_COUNT),
              THEME_MONO);
    /* Theme-section locale keys must resolve to real strings (the
     * settings picker wires these as fallbacks). */
    {
        static const char *keys[] = {
            "theme_style_label", "theme_style_system", "theme_style_retro",
            "theme_style_material", "theme_label",
            "theme_app", "theme_system", "theme_mode_label",
            "theme_follow_device", "theme_light", "theme_dark",
            "theme_color_label", "theme_picker_title"
        };
        size_t i;

        for(i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            const char *text = GetLocaleText(keys[i]);

            if(text == NULL || text[0] == '\0' || strcmp(text, keys[i]) == 0) {
                fprintf(stderr, "locale key unresolved: %s\n", keys[i]);
                return 1;
            }
        }
    }

    SetThemeStyle(THEME_STYLE_MATERIAL);

    SetThemeStyle((ThemeStyle)99);
    check_int("invalid style clamps", GetThemeStyle(), THEME_STYLE_SYSTEM);
    SetThemeStyle(THEME_STYLE_SYSTEM);
#if defined(ANDROID_BUILD) && ANDROID_BUILD
    check_int("android default style", GetEffectiveThemeStyle(), THEME_STYLE_MATERIAL);
#elif defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    check_int("android default style", GetEffectiveThemeStyle(), THEME_STYLE_MATERIAL);
#else
    check_int("host default style", GetEffectiveThemeStyle(), THEME_STYLE_SYSTEM);
#endif

    frame = BeginFrameBox(parent, 10, 10, 4);
    r = FramePack(&frame, SideTop, 30);
    check_int("pack x", (int)r.x, 20);
    check_int("pack y", (int)r.y, 30);
    check_int("pack width", (int)r.width, 180);
    check_int("pack height", (int)r.height, 30);

    grid = (Grid){parent, 2, 2, 10, 10, 0, 0};
    r = GridCell(grid, 1, 1, 1, 1);
    check_int("grid x", (int)r.x, 115);
    check_int("grid y", (int)r.y, 85);
    check_int("grid width", (int)r.width, 95);
    check_int("grid height", (int)r.height, 55);

    check_int("topmost hit", CanvasHitTest((Vector2){15, 15}, hits, 3), 1);
    check_int("miss", CanvasHitTest((Vector2){80, 80}, hits, 3), -1);
    test_menu_bar_switches_while_popup_captures_input();
    test_nested_disabled_scope();
    test_disabled_scalar_cancels_gesture();
    test_deep_disabled_scopes();
    test_collapsible_composes_children();
    test_tree_header_modes();
    test_tree_header_keyboard_gates();
    test_combo_popup_lifecycle();
    test_many_combo_identities();
    test_large_combo_options();
    test_combo_keyboard_navigation();
    test_combo_keyboard_open();
    test_combo_scrollbar_dismissal();
    test_combo_horizontal_viewport();
    test_popup_layout_restoration();
    test_popup_disabled_restoration();
    test_popup_input_clip_restoration();
    test_nested_popup_input_ownership();
    test_retained_popup_input_ownership();
    test_retained_popup_pointer_focus();
    test_popup_combo_keyboard_ownership();
    test_popup_accelerator_keyboard_ownership();
    test_popup_collapsible_keyboard_ownership();
    test_composed_combo_scope();
    test_composed_popup_scope();
    test_composed_tooltip_scope();
    test_composed_modal_scope();
    test_composed_context_popup_scope();
    test_composed_popup_focus_lifecycle();
    test_popup_active_drag_ownership();
    test_popup_text_keyboard_ownership();
    test_popup_tab_ownership();
    test_popup_button_keyboard_ownership();
    test_popup_tab_missing_owner();
    test_popup_text_dismissal_replay();
    test_popup_composition_dismissal_replay();
    test_popup_preedit_cancellation();
    test_custom_table_cell_scope();
    test_retained_scope_clip();
    test_list_box_scope();
    test_scroll_scope();
    test_scroll_thumb_drag();
    test_table_frozen_rows_hit_testing();
    test_table_column_resize();
    test_table_keyboard_navigation();
    test_popup_table_keyboard_ownership();

    {
        int sx = 10;
        int sy = 20;
        float zoom = 2.0f;
        Canvas canvas = {{40, 50, 200, 100}, &sx, &sy, &zoom};
        Vector2 p = CanvasToScreen(canvas, (Vector2){50, 70});
        Rectangle rr = CanvasRectToScreen(canvas, (Rectangle){50, 70, 20, 10});
        check_int("canvas screen x", (int)p.x, 40);
        check_int("canvas screen y", (int)p.y, 50);
        check_int("canvas rect w", (int)rr.width, 40);
        check_int("canvas rect h", (int)rr.height, 20);
    }

    {
        Camera2D camera = {0};
        UIWidget widget;
        UIInspectNode node;
        UIInspectSelection selection;
        int token;

        SetUIInspectEnabled(1);
        BeginUIInspectFrame(".");
        SetUIInspectCanvasBounds((Rectangle){40, 50, 200, 120});
        camera.offset = (Vector2){40, 50};
        camera.zoom = 2.0f;
        token = PushUIInspectTransform(camera);
        BeginUIInspectFrame(NULL);
        widget = BeginUIWidget("test", "inspect-transform",
                               (Rectangle){10, 20, 30, 15}, 0);
        EndUIWidget(&widget);
        check_int("inspect transformed count", UIInspectWidgetCount(), 1);
        check_int("inspect node count", UIInspectNodeCount(), 1);
        check_int("inspect find @name",
                  UIInspectFindNode("@inspect-transform", &node), 1);
        check_int("inspect find role", KryTFind("role=test", &node), 1);
        check_int("inspect node line default", node.source_line, 0);
        check_int("inspect transformed hit",
                  UIInspectSelectAt((Vector2){65, 95}), 1);
        selection = UIInspectGetSelection();
        check_int("inspect selected x", (int)selection.bounds.x, 10);
        check_int("inspect transformed miss",
                  UIInspectSelectAt((Vector2){20, 20}), 0);
        PopUIInspectTransform(token);
    }

    test_reorder_uses_item_center_and_header_handle();
    return 0;
}
