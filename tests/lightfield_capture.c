#include "kryon.h"
#include "kry_inject.h"
#include "examples/02_buttons.h"
#include "../examples/kryon_example_font.h"
#include "../src/ui/ui_internal.h"
#include "../src/ui/ui_blend_internal.h"
#include "../src/ui/ui_style_internal.h"
#include "../src/ui/toolkit_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Link-time clock substitution is confined to this verification executable. */
static double capture_time;

double __wrap_GetTime(void)
{
    return capture_time;
}

float __wrap_GetFrameTime(void)
{
    return 1.0f / 60.0f;
}

static void check_explicit_button_motion(void)
{
    for(int state = ButtonStateNormal; state <= ButtonStateSelected; state++) {
        int id = 9800 + state;
        unsigned int key = (2166136261u ^ (unsigned int)id) * 16777619u;
        InjectReset();
        SetUIFocus(id);
        for(int activate = 0; activate <= 1; activate++) {
            InjectMousePosition(30, 30);
            if(activate)
                InjectKeyTap(KEY_SPACE);
            InjectPump();
            BeginDrawing();
            BeginUIFrame(768, 1024, 1.0f);
            BeginTree(Key("explicit-button-motion"));
            int clicked = Button((ButtonProps){.bounds = {20, 20, 120, 40},
                .label = "Run", .id = id, .state = state});
            EndTree();
            EndUIFrame();
            EndDrawing();
            InteractionMotion *motion = toolkit_button_motion(key);
            assert(motion->hover.value == (state == ButtonStateHover));
            assert(motion->press.value == (state == ButtonStatePressed));
            assert(motion->focus.value == (state == ButtonStateFocus));
            if(activate)
                assert(clicked == (state != ButtonStateDisabled && state != ButtonStateLoading));
        }
    }
    InjectReset();
    SetUIFocus(0);
}

static void check_button_blocking_motion(void)
{
    for(int mode = 0; mode < 4; mode++) {
        int id = 9700 + mode;
        unsigned int key = (2166136261u ^ (unsigned int)id) * 16777619u;
        InjectReset();
        SetUIFocus(id);
        for(int frame = 0; frame < 4; frame++) {
            int blocked = frame == 2;
            InjectMousePosition(30, 30);
            if(frame == 1 || frame == 2)
                InjectKeyTap(KEY_SPACE);
            InjectPump();
            BeginDrawing();
            BeginUIFrame(768, 1024, 1.0f);
            BeginDisabled(blocked && mode == 1);
            BeginTree(Key("button-blocking-motion"));
            ButtonProps props = {.bounds = {20, 20, 120, 40}, .label = "Run", .id = id,
                .disabled = blocked && mode == 0, .loading = blocked && mode == 3};
            if(blocked && mode == 2)
                props.state = ButtonStateDisabled;
            int clicked = Button(props);
            EndTree();
            EndDisabled();
            EndUIFrame();
            EndDrawing();
            InteractionMotion *motion = toolkit_button_motion(key);
            if(frame == 1) {
                assert(motion->hover.value > 0);
                assert(motion->press.value > 0);
                assert(motion->focus.value > 0);
            } else if(blocked) {
                assert(!clicked);
                assert(motion->hover.value == 0 && motion->press.value == 0 && motion->focus.value == 0);
            } else if(frame == 3) {
                assert(!clicked);
                assert(motion->hover.value > 0 && motion->hover.value < 1 && motion->press.value == 0);
            }
        }
    }
    InjectReset();
    SetUIFocus(0);
}

static void check_button_motion_identity(int moving_id, int other_id)
{
    InjectReset();
    SetUIFocus(0);
    InteractionMotion expected = {0};
    for(int frame = 0; frame < 24; frame++) {
        ButtonProps moving = {.id = moving_id, .label = frame % 2 ? "Working" : "Run",
            .bounds = {20 + frame % 3, 20, 180 + frame % 5, 60}};
        ButtonProps other = {.id = other_id, .label = "Other", .bounds = {300, 20, 180, 60}};
        InjectMousePosition(80, 50);
        InjectPump();
        BeginDrawing();
        BeginUIFrame(768, 1024, 1.0f);
        BeginTree(Key("button-motion-identity"));
        if(frame % 2)
            Button(other);
        Button(moving);
        if(!(frame % 2))
            Button(other);
        EndTree();
        EndUIFrame();
        EndDrawing();
        ThemeMetrics metrics = GetThemeMetrics();
        expected = AdvanceInteractionMotion(expected, true, false, false,
            true, false, false, false, __wrap_GetFrameTime() * 1000.0f,
            metrics.transition_normal_ms, metrics.transition_fast_ms);
        unsigned int moving_key = (2166136261u ^ (unsigned int)moving_id) * 16777619u;
        unsigned int other_key = (2166136261u ^ (unsigned int)other_id) * 16777619u;
        assert(toolkit_button_motion(moving_key)->hover.value == expected.hover.value);
        assert(toolkit_button_motion(other_key)->hover.value == 0);
    }
}

static void check_scoped_button_style(void)
{
    RenderTexture2D target = LoadRenderTexture(240, 240);
    Color *direct = NULL;
    for(int scoped = 0; scoped <= 1; scoped++) {
        InjectReset();
        BeginDrawing();
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginUIFrame(240, 240, 1.0f);
        BeginDisabled(scoped);
        BeginTree(Key("scoped-button-style"));
        Button((ButtonProps){.bounds = {20, 20, 0, 0}, .label = "Measured",
            .id = 9501, .disabled = !scoped,
            .style.normal = {.fields = StyleFontSize | StylePaddingX, .font_size = 17, .padding_x = 8},
            .style.disabled = {.fields = StyleFontSize | StylePaddingX | StylePaddingY | StyleForeground,
                .font_size = 27, .padding_x = 19, .padding_y = 20, .foreground = {17, 34, 51, 128}}});
        BeginButton((ButtonProps){.bounds = {20, 120, 200, 100}, .id = 9502, .disabled = !scoped,
            .style.disabled = {.fields = StyleForeground, .foreground = {17, 34, 51, 128}}});
        Text((TextProps){.text = "Inherited"});
        Text((TextProps){.text = "Explicit", .color = {68, 85, 102, 128}});
        Text((TextProps){.text = "Disabled inherited", .disabled = 1});
        End();
        EndTree();
        int count = 0;
        const UIWidgetNode *nodes = GetTreeNodes(&count);
        int text_count = 0;
        for(int index = 0; index < count; index++) {
            if(nodes[index].kind != UI_WIDGET_TEXT_NODE)
                continue;
            unsigned int expected = strcmp(nodes[index].owned_text, "Explicit") == 0
                ? 0x44556639u : 0x11223380u;
            assert((unsigned int)ColorToInt(nodes[index].data.primitive.color) == expected);
            text_count++;
        }
        assert(text_count == 3);
        EndDisabled();
        EndUIFrame();
        EndTextureMode();
        EndDrawing();
        Image image = LoadImageFromTexture(target.texture);
        Color *pixels = LoadImageColors(image);
        assert(pixels != NULL);
        if(scoped) {
            assert(memcmp(direct, pixels, 240 * 240 * sizeof(Color)) == 0);
            UnloadImageColors(direct);
            UnloadImageColors(pixels);
        } else {
            direct = pixels;
        }
        UnloadImage(image);
    }
    UnloadRenderTexture(target);
}

static void check_live_button_font(void)
{
    RenderTexture2D target = LoadRenderTexture(220, 100);
    for(int explicit_font = 0; explicit_font <= 13; explicit_font += 13) {
        Color *actual = NULL;
        for(int reference = 0; reference < 2; reference++) {
            ButtonProps props = {.bounds = {20, 20, 180, 60}, .label = "Measured",
                .id = 9200, .font = explicit_font,
                .style.normal = {.fields = StyleFontSize, .font_size = 17},
                .style.hover = {.fields = StyleFontSize, .font_size = 27}};
            if(reference) {
                props.state = ButtonStateHover;
                props.font = explicit_font > 0 ? explicit_font : 27;
            }
            InjectReset();
            SetUIFocus(0);
            for(int frame = 0; frame < 24; frame++) {
                capture_time += 1.0 / 60.0;
                InjectMousePosition(25, 25);
                InjectPump();
                BeginDrawing();
                BeginTextureMode(target);
                ClearBackground(BLACK);
                BeginUIFrame(220, 100, 1.0f);
                BeginTree(Key("live-button-font"));
                Button(props);
                EndTree();
                EndUIFrame();
                EndTextureMode();
                EndDrawing();
            }
            Image image = LoadImageFromTexture(target.texture);
            Color *pixels = LoadImageColors(image);
            assert(pixels != NULL);
            if(reference) {
                assert(memcmp(actual, pixels, 220 * 100 * sizeof(Color)) == 0);
                UnloadImageColors(actual);
                UnloadImageColors(pixels);
            } else {
                actual = pixels;
            }
            UnloadImage(image);
        }
    }
    UnloadRenderTexture(target);
    InjectReset();
}

static void check_surface_material_parity(void)
{
    InjectReset();
    SetUIFocus(0);
    RenderTexture2D target = LoadRenderTexture(120, 80);
    assert(target.id != 0);
    for(int material = MaterialLightfield; material <= MaterialFlat; material++) {
        for(int gradient = 0; gradient < 2; gradient++) {
            Color *reference = NULL;
            for(int button = 0; button < 2; button++) {
                Style style = {.fields = StyleMaterial | StyleBackground | StyleBorder | StyleRadius | StyleBorderWidth | StyleOpacity,
                    .material = material, .background = {18, 52, 86, 255}, .border = {80, 120, 200, 255},
                    .radius = 8, .border_width = 1, .opacity = 0.75f};
                if(gradient) {
                    style.fields |= StyleBackgroundEnd;
                    style.background_end = (Color){80, 20, 40, 128};
                }
                Rectangle bounds = {20, 20, 80, 40};
                BeginDrawing();
                BeginTextureMode(target);
                ClearBackground(BLACK);
                BeginUIFrame(120, 80, 1.0f);
                BeginTree(Key("surface-material-parity"));
                if(button)
                    Button((ButtonProps){.bounds = bounds, .id = 994, .state = ButtonStateNormal, .style.normal = style});
                else
                    Surface(bounds, style);
                EndTree();
                EndUIFrame();
                EndTextureMode();
                EndDrawing();
                Image capture = LoadImageFromTexture(target.texture);
                Color *pixels = LoadImageColors(capture);
                assert(pixels != NULL);
                if(button) {
                    assert(memcmp(reference, pixels, 120 * 80 * sizeof(Color)) == 0);
                    UnloadImageColors(reference);
                    UnloadImageColors(pixels);
                } else {
                    reference = pixels;
                }
                UnloadImage(capture);
            }
        }
    }
    UnloadRenderTexture(target);
}

static void check_flat_button_material(void)
{
    InjectReset();
    SetUIFocus(0);
    RenderTexture2D target = LoadRenderTexture(100, 60);
    assert(target.id != 0);
    for(int state = ButtonStateNormal; state <= ButtonStateFocus; state++) {
        ButtonProps props = {.bounds = {10, 10, 80, 40}, .id = 993, .state = state,
            .style.normal = {.fields = StyleMaterial | StyleBackground | StyleBorder | StyleFocus | StyleRadius,
                .material = MaterialFlat, .background = {18, 52, 86, 255}}};
        BeginDrawing();
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginUIFrame(100, 60, 1.0f);
        BeginTree(Key("flat-button-material"));
        Button(props);
        EndTree();
        EndUIFrame();
        EndTextureMode();
        EndDrawing();
        Image capture = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&capture);
        Color *pixels = LoadImageColors(capture);
        const int points[][2] = {{20, 15}, {50, 30}, {80, 45}};
        for(int index = 0; index < 3; index++) {
            Color pixel = pixels[points[index][1] * 100 + points[index][0]];
            assert(pixel.r == 18 && pixel.g == 52 && pixel.b == 86 && pixel.a == 255);
        }
        assert(ColorToInt(pixels[51 * 100 + 50]) == ColorToInt(BLACK));
        assert(ColorToInt(pixels[30 * 100 + 9]) == ColorToInt(BLACK));
        UnloadImageColors(pixels);
        UnloadImage(capture);
        props.style.hover = (Style){.fields = StyleMaterial, .material = MaterialLightfield};
        assert(ResolveButtonStyle(props, ButtonStateHover).material == MaterialLightfield);
    }
    UnloadRenderTexture(target);
}

static void check_automatic_split_button_ids(void)
{
    InjectReset();
    SetUIFocus(0);
    const int expected[] = {0x40000000, 0x40000001, 0x40000002, 0x40000003, 1, 0x40000004};
    for(int frame = 0; frame < 2; frame++) {
        BeginDrawing();
        BeginUIFrame(768, 1024, 1.0f);
        BeginTree(Key("automatic-split-button-ids"));
        for(int index = 0; index < 2; index++) {
            SplitButton((SplitButtonProps){.button = {
                .bounds = {10 + index * 150, 10, 120, 40}, .label = "Add"}});
        }
        Button((ButtonProps){.bounds = {310, 10, 120, 40}, .id = 1, .label = "Explicit"});
        Button((ButtonProps){.bounds = {460, 10, 120, 40}, .label = "Automatic"});
        EndTree();
        int count = 0;
        int button_index = 0;
        const UIWidgetNode *nodes = GetTreeNodes(&count);
        for(int index = 0; index < count; index++) {
            if(nodes[index].kind != UI_WIDGET_BUTTON_NODE)
                continue;
            assert(button_index < 6);
            assert(nodes[index].id == expected[button_index]);
            button_index++;
        }
        assert(button_index == 6);
        EndUIFrame();
        EndDrawing();
    }
}

static void check_button_font_measurement(void)
{
    /* Real font metrics are essential: a headless zero-width measurement can
       hide a disagreement between the font used for layout and painting. */
    assert(TextWidth("Measured", 13) > 0);
    assert(TextWidth("Measured", 13) != TextWidth("Measured", 27));
    for(int state = ButtonStateNormal; state <= ButtonStateSelected; state++) {
        Style state_style = {.fields = StyleFontSize | StylePaddingX,
            .font_size = 27, .padding_x = 19};
        ButtonProps props = {.label = "Measured", .id = 9100, .state = state,
            .font = 13,
            .style.normal = {.fields = StyleFontSize | StylePaddingX,
                .font_size = 17, .padding_x = 8}};
        props.style.hover = state_style;
        props.style.pressed = state_style;
        props.style.focused = state_style;
        props.style.disabled = state_style;
        props.style.loading = state_style;
        props.style.selected = state_style;
        BeginDrawing();
        BeginUIFrame(768, 1024, 1.0f);
        BeginTree(Key("button-font-measurement"));
        Button(props);
        EndTree();
        int count = 0;
        const UIWidgetNode *nodes = GetTreeNodes(&count);
        int padding = Scale(state == ButtonStateNormal ? 8 : 19);
        assert(count == 2 && nodes[1].kind == UI_WIDGET_BUTTON_NODE);
        assert(nodes[1].data.button.spec.font == 13);
        assert(nodes[1].declared_bounds.width == TextWidth(props.label, 13) + padding * 2);
        EndUIFrame();
        EndDrawing();
    }
}

static void check_composed_text_animation(void)
{
    for(int dark = 0; dark < 2; dark++) {
        SetThemeMode(dark ? THEME_MODE_DARK : THEME_MODE_LIGHT);
        SetUIFocus(0);
        InjectReset();
        ButtonProps props = {.bounds = {20, 20, 200, 100}, .id = 9000 + dark,
            .style.normal = {.fields = StyleForeground | StyleOpacity,
                .foreground = {200, 40, 20, 0}, .opacity = 0.5f},
            .style.hover = {.fields = StyleForeground | StyleOpacity,
                .foreground = {20, 100, 240, 255}, .opacity = 1.0f}};
        InteractionMotion motion = {0};
        for(int frame = 0; frame < 24; frame++) {
            int hovered = frame >= 2 && frame < 13;
            InjectMousePosition(hovered ? 80 : -100, hovered ? 50 : -100);
            InjectPump();
            BeginDrawing();
            BeginUIFrame(768, 1024, 1.0f);
            BeginTree(Key("composed-text-animation"));
            BeginButton(props);
            Text((TextProps){.text = "Direct"});
            Column((ColumnProps){.bounds = {0, 0, 180, 60}});
            Text((TextProps){.text = "Nested"});
            Text((TextProps){.text = "Explicit", .color = {17, 34, 51, 255}});
            Text((TextProps){.text = "Transparent", .style = {
                .fields = StyleForeground, .foreground = {0, 0, 0, 0}}});
            Text((TextProps){.text = "Half", .style = {
                .fields = StyleOpacity, .opacity = 0.5f}});
            Text((TextProps){.text = "Hidden", .style = {.fields = StyleOpacity}});
            End();
            End();
            EndTree();
            ThemeMetrics metrics = GetThemeMetrics();
            motion = AdvanceInteractionMotion(motion, hovered, false, false,
                true, false, false, false, __wrap_GetFrameTime() * 1000.0f,
                metrics.transition_normal_ms, metrics.transition_fast_ms);
            Style normal = ResolveButtonStyle(props, ButtonStateNormal);
            Style hover = ResolveButtonStyle(props, ButtonStateHover);
            FillStates fill;
            Style expected = ui_style_transition(hovered ? hover : normal,
                normal, hover, normal, normal, motion.hover.value, 0, 0, &fill);
            unsigned int color = Opacity(ColorToInt(expected.foreground), expected.opacity);
            int count = 0;
            const UIWidgetNode *nodes = GetTreeNodes(&count);
            int texts = 0;
            for(int i = 0; i < count; i++) {
                if(nodes[i].kind != UI_WIDGET_TEXT_NODE)
                    continue;
                unsigned int want = strcmp(nodes[i].owned_text, "Explicit") == 0
                    ? 0x112233ffu : color;
                if(strcmp(nodes[i].owned_text, "Transparent") == 0)
                    want = 0;
                if(strcmp(nodes[i].owned_text, "Half") == 0)
                    want = Opacity(color, 0.5f);
                if(strcmp(nodes[i].owned_text, "Hidden") == 0)
                    want = Opacity(color, 0);
                assert((unsigned int)ColorToInt(nodes[i].data.primitive.color) == want);
                texts++;
            }
            assert(texts == 6);
            if(frame == 6)
                assert((color & 255) > 0 && (color & 255) < 255);
            if(frame == 23)
                assert((color & 255) == 0);
            EndUIFrame();
            EndDrawing();
        }
    }
    InjectReset();
}

static int save_capture(RenderTexture2D target, const char *directory,
                        const char *theme, const char *stage)
{
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s%s.png", directory, theme, stage);
    int saved = ExportImage(image, path);
    UnloadImage(image);
    return saved;
}

static void check_transparent_icon(void)
{
    RenderTexture2D target = LoadRenderTexture(32, 32);
    Color background = {20, 40, 60, 255};
    BeginTextureMode(target);
    ClearBackground(background);
    DrawIcon(UI_ICON_TYPE_X, (Rectangle){4, 4, 24, 24}, (Color){123, 45, 67, 0});
    EndTextureMode();
    Image image = LoadImageFromTexture(target.texture);
    Color *pixels = LoadImageColors(image);
    assert(pixels != NULL);
    for(int i = 0; i < 32 * 32; i++) {
        assert(pixels[i].r == background.r && pixels[i].g == background.g &&
               pixels[i].b == background.b && pixels[i].a == background.a);
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    UnloadRenderTexture(target);
}

/* Capture the real transpiled example, not a separately drawn approximation. */
int main(int argc, char **argv)
{
    if(argc != 2) {
        fprintf(stderr, "usage: lightfield_capture OUTPUT_DIRECTORY\n");
        return 2;
    }
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(768, 1024, "Lightfield verification");
    if(!IsWindowReady()) return 1;
    SetTargetFPS(60);
    /* Match the generated app's `font examples` and UI initialization. */
    LoadExampleUIFont();
    InitUI(768, 1024, GetUIScale());
    check_transparent_icon();
    check_button_font_measurement();
    check_automatic_split_button_ids();
    check_flat_button_material();
    check_surface_material_parity();
    check_scoped_button_style();
    check_live_button_font();
    check_explicit_button_motion();
    check_button_blocking_motion();
    check_button_motion_identity(9400, 9401);
    /* These distinct IDs map to keys zero and one. Neither key is reserved. */
    check_button_motion_identity(-2128831035, -1266624162);
    check_composed_text_animation();
    int default_typeface = ui_active_font_token();
    assert(UseUIFont("semibold"));
    assert(ui_active_font_token() != default_typeface);
    PopUIFont(default_typeface);
    int unspaced_width = TextWidth("AéB", 18);
    ParagraphSpec paragraph = {.text = "AA BB", .font = 18,
        .width = TextWidth("AA BB", 18)};
    int unspaced_height = ui_paragraph_height(paragraph);
    int previous_spacing = ui_set_text_letter_spacing(3);
    assert(TextWidth("AéB", 18) == unspaced_width + 6);
    assert(TextWidth("", 18) == 0);
    assert(ui_paragraph_height(paragraph) > unspaced_height);
    ui_set_text_letter_spacing(previous_spacing);
    const int font_sizes[] = {12, 13, 14, 15, 16, 17, 18, 19, 20, 24, 25, 26, 27, 28};
    for(unsigned int i = 0; i < sizeof(font_sizes) / sizeof(font_sizes[0]); i++) {
        Font font = GetUIFontForCodepoint('M', font_sizes[i]);
        if(font.texture.id == 0 || font.baseSize != Scale(font_sizes[i])) {
            fprintf(stderr, "Font size %d is not rasterized at its display size\n",
                    font_sizes[i]);
            UnloadExampleUIFont();
            CloseWindow();
            return 1;
        }
    }
    for(int dark = 0; dark < 2; dark++) {
        InjectReset();
        SetUIFocus(0);
        SetThemeMode(dark ? THEME_MODE_DARK : THEME_MODE_LIGHT);
        RenderTexture2D target = LoadRenderTexture(768, 1024);
        for(int frame = 0; frame <= 142; frame++) {
            capture_time = (double)frame / 60.0;
            /* Repeat the input sequence on the wide automatic-state button. */
            int wide = frame >= 83;
            int sequence_frame = wide ? frame - 60 : frame;
            if(sequence_frame >= 24 && sequence_frame < 51)
                InjectMousePosition(wide ? 384 : 58, wide ? 918 : 768);
            else
                InjectMousePosition(-100, -100);
            if(sequence_frame == 34) InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
            if(sequence_frame == 41) InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
            InjectPump();
            BeginDrawing();
            UIBlendState previous_blend = ui_blend_save();
            BeginTextureMode(target);
            /* Screen blending does not accumulate framebuffer alpha. Use
             * the same source-over factors as Kryon's offscreen UI layers. */
            ui_blend_capture();
            BeginUIFrame(768, 1024, 1.0f);
            if(sequence_frame == 63) SetUIFocus(0);
            if(sequence_frame == 73) SetUIFocus(wide ? 5000 : 3000);
            ButtonsExample((Rectangle){0, 0, 768, 1024});
            assert(ui_get_text_letter_spacing() == 0);
            assert(ui_active_font_token() == default_typeface);
            EndUIFrame();
            EndTextureMode();
            ui_blend_restore(previous_blend);
            EndDrawing();
            const char *stage = NULL;
            if(sequence_frame == 23) stage = "";
            if(sequence_frame == 24) stage = "-hover-start";
            if(sequence_frame == 28) stage = "-hover-middle";
            if(sequence_frame == 33) stage = "-hover-end";
            if(sequence_frame == 34) stage = "-press-start";
            if(sequence_frame == 40) stage = "-press-end";
            if(sequence_frame == 50) stage = "-release-end";
            if(sequence_frame == 51) stage = "-exit-start";
            if(sequence_frame == 62) stage = "-exit-end";
            if(sequence_frame == 63) stage = "-focus-exit-start";
            if(sequence_frame == 72) stage = "-focus-exit-end";
            if(sequence_frame == 73) stage = "-focus-start";
            if(sequence_frame == 77) stage = "-focus-middle";
            if(sequence_frame == 82) stage = "-focus-end";
            const char *name = dark ? "dark" : "light";
            if(wide) name = dark ? "dark-wide" : "light-wide";
            if(stage != NULL && !save_capture(target, argv[1], name, stage))
                return 1;
        }
        UnloadRenderTexture(target);
    }
    InjectReset();
    SetThemeMode(THEME_MODE_SYSTEM);
    SetUIFocus(0);
    SetWindowSize(1536, 1024);
    RenderTexture2D split_target = LoadRenderTexture(1536, 1024);
    for(int frame = 0; frame < 24; frame++) {
        capture_time = (double)frame / 60.0;
        InjectMousePosition(-100, -100);
        InjectPump();
        BeginDrawing();
        UIBlendState previous_blend = ui_blend_save();
        BeginTextureMode(split_target);
        ui_blend_capture();
        BeginUIFrame(1536, 1024, 1.0f);
        ButtonsExample((Rectangle){0, 0, 1536, 1024});
        assert(GetThemeMode() == THEME_MODE_SYSTEM);
        EndUIFrame();
        EndTextureMode();
        ui_blend_restore(previous_blend);
        EndDrawing();
    }
    if(!save_capture(split_target, argv[1], "split", ""))
        return 1;
    /* Exercise both panel instances through the real native input path.
     * Shared style policy must not mean shared per-instance motion state. */
    for(int panel = 0; panel < 2; panel++) {
        int active_id = panel ? 13000 : 3000;
        int other_id = panel ? 3000 : 13000;
        unsigned int active_key = (2166136261u ^ (unsigned int)active_id) * 16777619u;
        unsigned int other_key = (2166136261u ^ (unsigned int)other_id) * 16777619u;
        for(int stage = 0; stage < 5; stage++) {
            InjectReset();
            SetUIFocus(stage == 3 ? active_id : 0);
            if(stage == 2)
                InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
            for(int frame = 0; frame < 24; frame++) {
                capture_time += 1.0 / 60.0;
                InjectMousePosition(stage == 1 || stage == 2 ? 58 + panel * 768 : -100,
                                    stage == 1 || stage == 2 ? 768 : -100);
                InjectPump();
                BeginDrawing();
                UIBlendState previous_blend = ui_blend_save();
                BeginTextureMode(split_target);
                ui_blend_capture();
                BeginUIFrame(1536, 1024, 1.0f);
                ButtonsExample((Rectangle){0, 0, 1536, 1024});
                InteractionMotion *other = toolkit_button_motion(other_key);
                assert(other->hover.value == 0 && other->press.value == 0 && other->focus.value == 0);
                if(frame == 23) {
                    InteractionMotion *active = toolkit_button_motion(active_key);
                    assert(active->hover.value == (stage == 1 || stage == 2));
                    assert(active->press.value == (stage == 2));
                    assert(active->focus.value == (stage == 2 || stage == 3));
                }
                EndUIFrame();
                EndTextureMode();
                ui_blend_restore(previous_blend);
                EndDrawing();
            }
        }
    }
    InjectReset();
    SetUIFocus(0);
    SetThemeMode(THEME_MODE_LIGHT);
    for(int frame = 0; frame < 24; frame++) {
        capture_time = (double)frame / 60.0;
        BeginDrawing();
        UIBlendState previous_blend = ui_blend_save();
        BeginTextureMode(split_target);
        ui_blend_capture();
        BeginUIFrame(1536, 1024, 1.0f);
        ClearBackground(ThemeDefaultDark().colors.background);
        ButtonsPanel((Rectangle){768, 0, 768, 1024}, 768, 10000, 2, false);
        EndUIFrame();
        EndTextureMode();
        ui_blend_restore(previous_blend);
        EndDrawing();
    }
    if(!save_capture(split_target, argv[1], "light-offset", ""))
        return 1;
    UnloadRenderTexture(split_target);
    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
