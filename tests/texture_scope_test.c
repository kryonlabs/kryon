#include "kryon.h"
#include "kry_inject.h"
#include <stdio.h>
#include <string.h>
#include "../src/ui/ui_internal.h"
#include "../src/ui/ui_clip_internal.h"

static int failures;
static int check_window_readback;
static int window_readbacks;
static const char *expected_dropdown_text;
static int full_dropdown_text_draws;

void __real_DrawUIText(const char *text, int x, int y, int font, Color color);
void __wrap_DrawUIText(const char *text, int x, int y, int font, Color color)
{
    if(expected_dropdown_text != NULL && text != NULL &&
       strcmp(text, expected_dropdown_text) == 0)
        full_dropdown_text_draws++;
    __real_DrawUIText(text, x, y, font, color);
}

static void check_pixel(Image image, int x, int y, Color expected, const char *label)
{
    Color got = GetImageColor(image,x,y);
    if(got.r != expected.r || got.g != expected.g || got.b != expected.b || got.a != expected.a) {
        fprintf(stderr,"%s: (%d,%d) got %u,%u,%u,%u expected %u,%u,%u,%u\n",
                label,x,y,got.r,got.g,got.b,got.a,expected.r,expected.g,expected.b,expected.a);
        failures++;
    }
}

/* Observe the real X11 presenter's readback without exposing UIWindow internals.
 * Do not replace rendering or readback with a mock. */
Image __real_LoadImageFromTexture(Texture2D texture);
Image __wrap_LoadImageFromTexture(Texture2D texture)
{
    Image image = __real_LoadImageFromTexture(texture);
    if(check_window_readback) {
        window_readbacks++;
        /* Render-texture readback has its origin at the bottom. */
        check_pixel(image,1,image.height-2,RED,"UIWindow retained background");
        check_pixel(image,9,image.height-10,GREEN,"UIWindow drawing after nested target");
        check_pixel(image,41,image.height-42,YELLOW,"UIWindow restored viewport");
    }
    return image;
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64,64,"nested paint targets");
    if(!IsWindowReady()) return 1;
    RenderTexture2D outer = LoadRenderTexture(64,64);
    RenderTexture2D inner = LoadRenderTexture(16,16);
    RenderTexture2D leaf = LoadRenderTexture(8,8);
    if(outer.id == 0 || inner.id == 0 || leaf.id == 0) return 1;

    BeginDrawing();
    ClearBackground(BLACK);
    BeginTextureMode(outer);
    ClearBackground(RED);
    BeginMode2D((Camera2D){.offset = {7,5}, .zoom = 1});
    Matrix projection = rlGetMatrixProjection(), modelview = rlGetMatrixModelview();
    BeginTextureMode(inner);
    ClearBackground(BLUE);
    BeginTextureMode(leaf);
    ClearBackground(MAGENTA);
    EndTextureMode();
    DrawRectangle(8,8,3,3,YELLOW);
    EndTextureMode();
    Matrix restored_projection = rlGetMatrixProjection(), restored_modelview = rlGetMatrixModelview();
    if(memcmp(&projection,&restored_projection,sizeof(projection)) != 0 ||
       memcmp(&modelview,&restored_modelview,sizeof(modelview)) != 0) {
        fprintf(stderr,"nested target did not restore parent matrices\n");
        failures++;
    }
    DrawRectangle(0,0,4,4,GREEN);
    EndMode2D();
    DrawRectangle(40,40,4,4,YELLOW);
    EndTextureMode();
    BeginMode2D((Camera2D){.zoom = 1});
    DrawRectangle(20,20,4,4,WHITE);
    EndMode2D();
    EndDrawing();
    Image screen = LoadImageFromScreen();
    check_pixel(screen,1,1,BLACK,"screen retained background");
    check_pixel(screen,21,21,WHITE,"screen drawing after outer scope");
    UnloadImage(screen);

    Image a = LoadImageFromTexture(outer.texture), b = LoadImageFromTexture(inner.texture), c = LoadImageFromTexture(leaf.texture);
    ImageFlipVertical(&a); ImageFlipVertical(&b); ImageFlipVertical(&c);
    check_pixel(a,1,1,RED,"outer retained background");
    check_pixel(a,8,6,GREEN,"outer translated drawing after nested scope");
    check_pixel(a,41,41,YELLOW,"outer viewport restored");
    check_pixel(b,1,1,BLUE,"inner retained background");
    check_pixel(b,9,9,YELLOW,"inner drawing after leaf scope");
    check_pixel(c,1,1,MAGENTA,"leaf independent target");
    UnloadImage(a); UnloadImage(b); UnloadImage(c);

    UIWindow *window = OpenUIWindow("nested UI paint target",0,0,64,64,
                                   UI_WINDOW_BORDERLESS,RED,1);
    if(window == NULL) {
        fprintf(stderr,"UIWindow integration requires a working desktop window backend\n");
        failures++;
    } else {
        BeginUIWindow(window);
        BeginTextureMode(inner);
        ClearBackground(BLUE);
        BeginTextureMode(leaf);
        ClearBackground(MAGENTA);
        EndTextureMode();
        EndTextureMode();
        DrawRectangle(8,8,4,4,GREEN);
        DrawRectangle(40,40,4,4,YELLOW);
        check_window_readback = 1;
        EndUIWindow();
        check_window_readback = 0;
        if(window_readbacks != 1) {
            fprintf(stderr,"expected one real UIWindow presenter readback, got %d\n",window_readbacks);
            failures++;
        }
        CloseUIWindow(window);
    }
    /* Mixed immediate/retained capture must survive later opaque content.
     * Repeat an unchanged tree after clearing its targets to catch invalidation. */
    for(int frame = 0; frame < 2; frame++) {
        BeginTextureMode(outer);
        ClearBackground(BLACK);
        BeginTree(Key("captured paint"));
        BeginTextureMode(inner);
        ClearBackground(BLUE);
        RenderTexture2D previous = ui_tree_set_paint_target(inner);
        DrawRectangle(1,1,3,3,GREEN);
        BeginUIClip(12,12,2,2);
        Rect(11,11,4,4,WHITE,BLANK);
        EndUIClip();
        Rect(8,8,3,3,YELLOW,BLANK);
        BeginMode2D((Camera2D){.offset = {4,3}, .zoom = 1});
        Rect(3,3,2,2,ORANGE,BLANK);
        BeginTextureMode(leaf);
        ClearBackground(MAGENTA);
        RenderTexture2D parent = ui_tree_set_paint_target(leaf);
        Rect(2,2,2,2,WHITE,BLANK);
        ui_tree_set_paint_target(parent);
        EndTextureMode();
        Rect(8,0,2,2,WHITE,BLANK);
        EndMode2D();
        ui_tree_set_paint_target(previous);
        EndTextureMode();
        Rect(1,1,64,64,RED,BLANK);
        BeginUIClip(0,0,4,4);
        EndTree();
        DrawRectangle(0,0,32,32,ORANGE);
        EndUIClip();
        DrawTextureRec(inner.texture,(Rectangle){0,0,16,-16},(Vector2){0,0},WHITE);
        EndTextureMode();
        Image captured = LoadImageFromTexture(outer.texture);
        ImageFlipVertical(&captured);
        check_pixel(captured,2,2,GREEN,"captured immediate paint");
        check_pixel(captured,9,9,YELLOW,"captured retained paint above later opaque content");
        check_pixel(captured,7,6,ORANGE,"captured retained camera translation");
        check_pixel(captured,12,3,WHITE,"captured camera restored after nested target");
        check_pixel(captured,12,12,WHITE,"captured drawing clip retained");
        check_pixel(captured,11,12,BLUE,"captured drawing clip excludes outside");
        check_pixel(captured,40,40,BLACK,"ordinary retained paint keeps active parent clip");
        check_pixel(captured,20,20,BLACK,"parent clip restored after captured nodes");
        UnloadImage(captured);
        captured = LoadImageFromTexture(leaf.texture);
        ImageFlipVertical(&captured);
        check_pixel(captured,2,2,WHITE,"nested retained paint destination");
        UnloadImage(captured);
    }
    BeginTextureMode(outer);
    ClearBackground(BLACK);
    BeginUIClip(0,0,4,4);
    UIClipState saved_clip = ui_clip_save();
    ResetUIClip();
    DrawRectangle(20,20,4,4,WHITE);
    ui_clip_restore(saved_clip);
    DrawRectangle(0,0,64,64,GREEN);
    EndUIClip();
    EndTextureMode();
    Image clip_image = LoadImageFromTexture(outer.texture);
    ImageFlipVertical(&clip_image);
    check_pixel(clip_image,21,21,WHITE,"reset disables actual scissor");
    check_pixel(clip_image,2,2,GREEN,"saved clip restored");
    check_pixel(clip_image,8,8,BLACK,"restored clip excludes outside");
    UnloadImage(clip_image);
    /* Sliders use the same declaration-order painter and layout as Rect/Button. */
    float float_value = 0;
    float angle_value = 0;
    int int_value = 0;
    char slider_label[] = "owned";
    char slider_format[] = "%.1f";
    BeginTextureMode(outer);
    ClearBackground(BLACK);
    BeginTree(Key("slider lifecycle"));
    Rect(1,1,63,63,RED,BLANK);
    Row((RowProps){.bounds = {10,10,40,12}});
    SliderInt((SliderIntProps){.bounds = {0,0,20,12}, .id = 911,
              .values = &int_value, .value_count = 1, .min = 0, .max = 10, .format = " "});
    VSliderFloat((SliderFloatProps){.bounds = {0,0,20,12}, .id = 912,
                 .values = &float_value, .value_count = 1, .min = 0, .max = 1,
                 .label = slider_label, .format = slider_format});
    End();
    Column((ColumnProps){.bounds = {10,30,40,12}});
    SliderAngle((SliderAngleProps){.bounds = {0,0,40,12}, .id = 913,
                .value = &angle_value, .min_degrees = 0, .max_degrees = 180,
                .format = " "});
    End();
    /* Paint must read the caller's radians, not a temporary declaration value. */
    angle_value = 1.5707963267948966f;
    slider_label[0] = 'X';
    slider_format[2] = '3';
    Rect(23,17,3,3,BLUE,BLANK);
    EndTree();
    EndTextureMode();
    Image sliders = LoadImageFromTexture(outer.texture);
    ImageFlipVertical(&sliders);
    check_pixel(sliders,25,12,c_button,"slider paints after earlier retained rectangle");
    check_pixel(sliders,31,12,c_button,"vertical slider uses resolved row bounds");
    check_pixel(sliders,24,18,BLUE,"later retained rectangle paints above slider");
    check_pixel(sliders,20,32,c_button_hover,"angle painter reads live radians");
    check_pixel(sliders,45,32,c_button,"angle slider uses resolved column bounds");
    if(angle_value != 1.5707963267948966f) {
        fprintf(stderr,"angle painting changed caller state\n");
        failures++;
    }
    UnloadImage(sliders);
    int node_count = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&node_count);
    for(int i = 0; i < node_count; i++) {
        if(nodes[i].id == 912 &&
           (nodes[i].bounds.x != 30 || nodes[i].bounds.y != 10 ||
            strcmp(nodes[i].owned_text,"owned") != 0 ||
            strcmp(nodes[i].owned_text+nodes[i].data.float_slider.format_offset,"%.1f") != 0)) {
            fprintf(stderr,"slider lost resolved layout or owned label/format\n");
            failures++;
        }
    }
    float drag_float = 1, range_min = 1, range_max = 2;
    int drag_int = 1, int_min = 1, int_max = 2;
    char drag_format[] = " ";
    BeginTextureMode(outer);
    ClearBackground(BLACK);
    BeginTree(Key("drag lifecycle"));
    Rect(1,1,63,63,RED,BLANK);
    Row((RowProps){.bounds = {10,10,40,12}});
    DragFloat((DragFloatProps){.bounds = {0,0,20,12}, .id = 920,
              .values = &drag_float, .value_count = 1, .format = drag_format});
    BeginDisabled(1);
    DragInt((DragIntProps){.bounds = {0,0,20,12}, .id = 921,
            .values = &drag_int, .value_count = 1, .format = drag_format});
    EndDisabled();
    End();
    Column((ColumnProps){.bounds = {10,30,40,28}, .gap = 4});
    DragFloatRange2((DragFloatRange2Props){.bounds = {0,0,40,12}, .id = 922,
        .current_min = &range_min, .current_max = &range_max,
        .format = drag_format, .format_max = drag_format});
    DragIntRange2((DragIntRange2Props){.bounds = {0,0,40,12}, .id = 923,
        .current_min = &int_min, .current_max = &int_max,
        .format = drag_format, .format_max = drag_format});
    End();
    drag_format[0] = 'X';
    EndTree();
    BeginDisabled(1);
    DrawRectangle(54,10,4,4,c_surface);
    EndDisabled();
    EndTextureMode();
    Image drags = LoadImageFromTexture(outer.texture);
    ImageFlipVertical(&drags);
    check_pixel(drags,12,12,c_button,"float drag retained order and layout");
    check_pixel(drags,32,12,GetImageColor(drags,55,12),"int drag retains disabled scope");
    check_pixel(drags,12,32,c_button,"float range low child layout");
    check_pixel(drags,32,32,c_button,"float range high child layout");
    check_pixel(drags,12,48,c_button,"int range low child layout");
    check_pixel(drags,32,48,c_button,"int range high child layout");
    UnloadImage(drags);
    nodes = GetTreeNodes(&node_count);
    int drag_nodes = 0;
    for(int i = 0; i < node_count; i++) {
        size_t offset;
        if(nodes[i].kind == UI_WIDGET_FLOAT_DRAG_NODE)
            offset = nodes[i].data.float_drag.format_offset;
        else if(nodes[i].kind == UI_WIDGET_INT_DRAG_NODE)
            offset = nodes[i].data.int_drag.format_offset;
        else
            continue;
        drag_nodes++;
        if(nodes[i].owned_text == NULL || strcmp(nodes[i].owned_text+offset," ") != 0) {
            fprintf(stderr,"drag retained a borrowed format string\n");
            failures++;
        }
    }
    if(drag_nodes != 6) {
        fprintf(stderr,"range controls did not compose six typed drag nodes\n");
        failures++;
    }
    BeginTextureMode(outer);
    ClearBackground(RED);
    DrawUITextInRect("wide", (Rectangle){10,10,20,20}, 16, WHITE);
    DrawRectangle(25,10,5,20,BLUE);
    EndTextureMode();
    Image text_reference = LoadImageFromTexture(outer.texture);
    char boxed_text[] = "wide";
    BeginTextureMode(outer);
    ClearBackground(BLACK);
    BeginTree(Key("boxed text lifecycle"));
    Rect(0,0,64,64,RED,BLANK);
    Row((RowProps){.bounds = {10,10,20,20}});
    TextInRect(boxed_text, (Rectangle){0,0,20,20}, 16, WHITE);
    End();
    memset(boxed_text,'X',4);
    Rect(25,10,5,20,BLUE,BLANK);
    EndTree();
    EndTextureMode();
    Image boxed = LoadImageFromTexture(outer.texture);
    int text_mismatches = 0;
    for(int y = 0; y < 64; y++) {
        for(int x = 0; x < 64; x++) {
            Color expected = GetImageColor(text_reference,x,y);
            Color actual = GetImageColor(boxed,x,y);
            if(memcmp(&expected,&actual,sizeof(Color)) != 0)
                text_mismatches++;
        }
    }
    if(text_mismatches != 0) {
        fprintf(stderr,"boxed retained text differs from reference at %d pixels\n",text_mismatches);
        failures++;
    }
    UnloadImage(text_reference);
    UnloadImage(boxed);
    for(int secure = 0; secure < 2; secure++) {
        char editor_text[] = "123";
        int cursor = 3, focused = 0;
        TextFieldProps editor = {.bounds = {4,8,56,24}, .text = editor_text,
            .text_size = sizeof(editor_text), .cursor_position = &cursor,
            .focused = &focused, .font = 16, .focus_id = 940 + secure,
            .secure = secure};
        BeginTextureMode(outer);
        ClearBackground(RED);
        RenderTextField(editor);
        DrawRectangle(45,8,15,24,BLUE);
        EndTextureMode();
        Image editor_reference = LoadImageFromTexture(outer.texture);
        BeginTextureMode(outer);
        ClearBackground(BLACK);
        BeginTree(Key("editor paint lifecycle"));
        Rect(0,0,64,64,RED,BLANK);
        RenderTextField(editor);
        memset(editor_text,'X',3);
        Rect(45,8,15,24,BLUE,BLANK);
        EndTree();
        EndTextureMode();
        Image editor_result = LoadImageFromTexture(outer.texture);
        int mismatches = 0;
        for(int y = 0; y < 64; y++) {
            for(int x = 0; x < 64; x++) {
                Color expected = GetImageColor(editor_reference,x,y);
                Color actual = GetImageColor(editor_result,x,y);
                if(memcmp(&expected,&actual,sizeof(Color)) != 0) mismatches++;
            }
        }
        if(mismatches != 0) {
            fprintf(stderr,"editor snapshot secure=%d differs at %d pixels\n",secure,mismatches);
            failures++;
        }
        nodes = GetTreeNodes(&node_count);
        int snapshots = 0;
        for(int i = 0; i < node_count; i++) {
            if(nodes[i].kind != UI_WIDGET_TEXT_INPUT_PAINT_NODE) continue;
            snapshots++;
            if(nodes[i].owned_text == NULL ||
               strcmp(nodes[i].owned_text,secure ? "***" : "123") != 0) {
                fprintf(stderr,"editor paint did not own its display text\n");
                failures++;
            }
        }
        if(snapshots != 1) failures++;
        UnloadImage(editor_reference);
        UnloadImage(editor_result);
    }
    char long_label[513], original_label[513];
    memset(original_label, 'q', sizeof(original_label)-1);
    original_label[sizeof(original_label)-1] = '\0';
    const char *long_options[] = {long_label};
    int long_selected = 0;
    InjectReset();
    InjectTap(20,20);
    for(int frame = 0; frame < 3; frame++) {
        memcpy(long_label, original_label, sizeof(long_label));
        InjectPump();
        BeginTextureMode(outer);
        BeginUIFrame(240,240,1);
        Combobox((ComboboxProps){.bounds = {10,10,44,28}, .id = 22000,
            .options = long_options, .option_count = 1, .selected_index = &long_selected});
        memset(long_label, 'X', sizeof(long_label)-1);
        expected_dropdown_text = original_label;
        EndUIFrame();
        expected_dropdown_text = NULL;
        EndTextureMode();
    }
    if(full_dropdown_text_draws == 0) {
        fprintf(stderr,"dropdown overlay did not draw the full owned 512-byte label\n");
        failures++;
    }
    InjectReset();
    BeginUIFrame(240,240,1);
    EndUIFrame();
    /* Thumb and rows must reflect the same scroll offset on the drag frame,
     * not settle into agreement one frame later. Compare real framebuffer
     * pixels while holding the pointer still after moving the thumb. */
    RenderTexture2D popup_target = LoadRenderTexture(240,240);
    if(popup_target.id == 0) return 1;
    const char *scroll_options[20];
    for(int i = 0; i < 20; i++) scroll_options[i] = i == 19 ? "Last row" : "Row";
    int scroll_selected = 0;
    Image drag_frame = {0};
    InjectReset();
    InjectKeyTap(KEY_SPACE);
    for(int frame = 0; frame < 6; frame++) {
        if(frame == 3) {
            InjectMousePosition(166,50);
            InjectMouseButton(MOUSE_BUTTON_LEFT,1);
        }
        /* A real window resumes its native cursor unless position is
         * injected for each frame; explicitly keep this drag stationary. */
        if(frame >= 4) InjectMousePosition(166,210);
        InjectPump();
        BeginTextureMode(popup_target);
        ClearBackground(BLACK);
        BeginUIFrame(240,240,1);
        SetUIFocus(22001);
        Combobox((ComboboxProps){.bounds={10,10,160,28},.id=22001,
            .options=scroll_options,.option_count=20,.selected_index=&scroll_selected});
        int previous_draws = full_dropdown_text_draws;
        if(frame == 4) expected_dropdown_text = "Last row";
        EndUIFrame();
        expected_dropdown_text = NULL;
        if(frame == 3 && g_ui_pointer_owner != UI_POINTER_OWNER_SCROLL) {
            fprintf(stderr,"rendered popup scrollbar did not acquire drag\n");
            failures++;
        }
        if(frame == 4 && full_dropdown_text_draws == previous_draws) {
            fprintf(stderr,"rendered popup did not reveal last row on drag frame\n");
            failures++;
        }
        EndTextureMode();
        if(frame == 4) drag_frame = LoadImageFromTexture(popup_target.texture);
        if(frame == 5) {
            Image settled = LoadImageFromTexture(popup_target.texture);
            int mismatches = 0;
            for(int y = 0; y < 240; y++)
                for(int x = 0; x < 240; x++) {
                    Color a = GetImageColor(drag_frame,x,y), b = GetImageColor(settled,x,y);
                    if(memcmp(&a,&b,sizeof(Color)) != 0) mismatches++;
                }
            if(mismatches) {
                fprintf(stderr,"popup drag paint lagged by one frame at %d pixels\n",mismatches);
                failures++;
            }
            UnloadImage(drag_frame);
            UnloadImage(settled);
        }
    }
    InjectReset(); BeginUIFrame(240,240,1); EndUIFrame();
    UnloadRenderTexture(popup_target);
    UnloadRenderTexture(leaf); UnloadRenderTexture(inner); UnloadRenderTexture(outer);
    CloseWindow();
    if(failures == 0) puts("nested texture scope pixels ok");
    return failures != 0;
}
