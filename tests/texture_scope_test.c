#include "kryon.h"
#include "kry_inject.h"
#include <stdio.h>
#include <string.h>
#include "../src/ui/ui_internal.h"
#include "../src/ui/ui_clip_internal.h"
#include "../src/ui/ui_blend_internal.h"
#include "../src/ui/ui_paint_layers_internal.h"
#if defined(__unix__)
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static int failures;
static int check_window_readback;
static int window_readbacks;
static const char *expected_dropdown_text;
static int full_dropdown_text_draws;

static void check_invalid_layer_end(UIPaintLayerToken token, const char *label)
{
#if defined(__unix__)
    pid_t child = fork();
    if(child == 0) {
        struct rlimit limit = {0,0};
        setrlimit(RLIMIT_CORE,&limit);
        alarm(2);
        ui_paint_layer_end(token);
        _exit(0);
    }
    int status = 0;
    if(child < 0 || waitpid(child,&status,0) != child ||
       !WIFSIGNALED(status) || WTERMSIG(status) != SIGABRT) {
        fprintf(stderr,"%s: invalid layer close was not rejected\n",label);
        failures++;
    }
#else
    (void)token;
    (void)label;
#endif
}

/* Backend-only experiment for the owned-layer prerequisite. This rlgl entry
 * point is deliberately not added to Kryon's public generated surface. */
extern void rlSetBlendFactorsSeparate(int srcRGB, int dstRGB, int srcAlpha,
                                      int dstAlpha, int eqRGB, int eqAlpha);

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
        check_pixel(image,9,image.height-10,check_window_readback == 2 ? RED : GREEN,"UIWindow drawing after nested target");
        check_pixel(image,41,image.height-42,check_window_readback == 2 ? RED : YELLOW,"UIWindow restored viewport");
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

    /* Ordinary source-alpha blending also multiplies the alpha channel by
     * source alpha on this backend. Transparent layer capture instead needs
     * separate RGB/alpha factors, followed by premultiplied compositing. */
    rlSetBlendFactorsSeparate(0x0302,0x0303,1,0x0303,0x8006,0x8006);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    BeginTree(Key("transparent mixed layer"));
    BeginTextureMode(inner);
    ClearBackground(BLANK);
    RenderTexture2D transparent_previous = ui_tree_set_paint_target(inner);
    DrawRectangle(0,0,4,4,(Color){0,255,0,128});
    Rect(4,4,4,4,(Color){255,0,0,128},BLANK);
    ui_tree_set_paint_target(transparent_previous);
    EndTextureMode();
    /* The lexical capture scope ends before deferred painting. Neither its
     * active factors nor a later caller's custom configuration may leak. */
    EndBlendMode();
    rlSetBlendFactorsSeparate(1,1,1,1,0x8006,0x8006);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    rlSetBlendFactorsSeparate(1,0,1,0,0x8006,0x8006);
    UIBlendState caller_blend = ui_blend_save();
    EndTree();
    UIBlendState restored_blend = ui_blend_save();
    if(memcmp(&caller_blend,&restored_blend,sizeof(UIBlendState)) != 0) {
        fprintf(stderr,"retained layer did not restore active and pending blend state\n");
        failures++;
    }
    EndBlendMode();
    Image transparent = LoadImageFromTexture(inner.texture);
    ImageFlipVertical(&transparent);
    check_pixel(transparent,1,1,(Color){0,128,0,128},"transparent immediate capture");
    check_pixel(transparent,5,5,(Color){128,0,0,128},"transparent retained capture");
    UnloadImage(transparent);
    BeginTextureMode(outer);
    ClearBackground(BLUE);
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    DrawTextureRec(inner.texture,(Rectangle){0,0,16,-16},(Vector2){0,0},WHITE);
    EndBlendMode();
    EndTextureMode();
    Image composite = LoadImageFromTexture(outer.texture);
    BeginTextureMode(outer);
    ClearBackground(BLUE);
    rlSetBlendFactorsSeparate(0x0302,0x0303,1,0x0303,0x8006,0x8006);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    DrawRectangle(0,0,4,4,(Color){0,255,0,128});
    DrawRectangle(4,4,4,4,(Color){255,0,0,128});
    EndBlendMode();
    EndTextureMode();
    Image direct = LoadImageFromTexture(outer.texture);
    int alpha_mismatches = 0;
    for(int y = 0; y < 64; y++)
        for(int x = 0; x < 64; x++) {
            Color layer_pixel = GetImageColor(composite,x,y), direct_pixel = GetImageColor(direct,x,y);
            if(memcmp(&layer_pixel,&direct_pixel,sizeof(Color)) != 0) alpha_mismatches++;
        }
    if(alpha_mismatches) {
        fprintf(stderr,"transparent layer differs from direct source-over at %d pixels\n",alpha_mismatches);
        failures++;
    }
    UnloadImage(composite); UnloadImage(direct);

    UIPaintLayers *owned_layers = ui_paint_layers_create();
    UIPaintLayerToken stale_layer = {0};
    check_invalid_layer_end(stale_layer,"null layer token");
    for(int frame = 0; frame < 3; frame++) {
        BeginTextureMode(outer);
        ClearBackground(BLACK);
        BeginTree(Key("owned layers"));
        ui_paint_layers_frame(owned_layers,64,64);
        if(frame != 2) {
            UIPopupInputToken parent_input = ui_popup_input_begin(
                ui_paint_layers_input(owned_layers),1,(Rectangle){0,0,16,16});
            UIPaintLayerToken parent_layer = ui_paint_layer_begin(owned_layers,1);
            if(frame == 1) check_invalid_layer_end(stale_layer,"previous frame layer token");
            stale_layer = parent_layer;
            DrawRectangle(0,0,16,16,(Color){255,0,0,128});
            if(frame == 0) {
                UIPaintLayers *foreign = ui_paint_layers_create();
                ui_paint_layers_frame(foreign,64,64);
                BeginDisabled(1);
                UIPaintLayerToken foreign_layer = ui_paint_layer_begin(foreign,1);
                EndDisabled();
                if(!UIContentDisabled()) {
                    fprintf(stderr,"layer ended its parent's disabled scope\n");
                    failures++;
                }
                check_invalid_layer_end(parent_layer,"cross-host out-of-order close");
                ui_paint_layer_end(foreign_layer);
                if(!UIContentDisabled()) failures++;
                EndDisabled();
                if(UIContentDisabled()) failures++;
                ui_paint_layers_composite(foreign);
                ui_paint_layers_destroy(foreign);
            }
            Rect(20,0,4,4,YELLOW,BLANK);
            /* A descendant outside its parent's bounds must also lose capture
             * when the parent's paint branch is hidden. */
            UIPopupInputToken child_input = ui_popup_input_begin(
                ui_paint_layers_input(owned_layers),2,(Rectangle){20,20,8,8});
            UIPaintLayerToken child_layer = ui_paint_layer_begin(owned_layers,2);
            check_invalid_layer_end(parent_layer,"parent closed before child");
            Row((RowProps){.bounds={40,40,4,4}});
            check_invalid_layer_end(child_layer,"unclosed popup child layout");
            End();
            BeginDisabled(1);
            check_invalid_layer_end(child_layer,"unclosed popup disabled scope");
            EndDisabled();
            BeginScroll((Rectangle){0,0,4,4},20,NULL);
            check_invalid_layer_end(child_layer,"unclosed popup scroll scope");
            EndScroll();
            Rect(4,4,4,4,GREEN,BLANK);
            ui_paint_layer_end(child_layer);
            ui_popup_input_end(child_input);
            check_invalid_layer_end(child_layer,"layer closed twice");
            Rect(4,4,4,4,BLUE,BLANK);
            ui_paint_layer_end(parent_layer);
            ui_popup_input_end(parent_input);
            if(!ui_popup_input_current_captures((Vector2){1,1}) ||
               !ui_popup_input_current_captures((Vector2){21,21})) {
                fprintf(stderr,"owned popup branch did not capture background input\n");
                failures++;
            }
            if(frame == 1) {
                ui_paint_layers_hide(owned_layers,1);
                if(ui_popup_input_current_captures((Vector2){1,1}) ||
                   ui_popup_input_current_captures((Vector2){21,21})) {
                    fprintf(stderr,"hidden paint branch retained popup input capture\n");
                    failures++;
                }
            }
        }
        Rect(0,0,64,64,WHITE,BLANK);
        EndTree();
        BeginMode2D((Camera2D){.offset={7,5},.zoom=1});
        BeginUIClip(40,40,4,4);
        Matrix before_projection = rlGetMatrixProjection(), before_modelview = rlGetMatrixModelview();
        UIBlendState before_blend = ui_blend_save();
        ui_paint_layers_composite(owned_layers);
        Matrix after_projection = rlGetMatrixProjection(), after_modelview = rlGetMatrixModelview();
        UIBlendState after_blend = ui_blend_save();
        if(memcmp(&before_projection,&after_projection,sizeof(Matrix)) ||
           memcmp(&before_modelview,&after_modelview,sizeof(Matrix)) ||
           memcmp(&before_blend,&after_blend,sizeof(UIBlendState))) {
            fprintf(stderr,"owned layer composite did not restore parent drawing state\n");
            failures++;
        }
        DrawRectangle(0,0,64,64,ORANGE);
        EndUIClip(); EndMode2D();
        EndTextureMode();
        Image owned = LoadImageFromTexture(outer.texture);
        ImageFlipVertical(&owned);
        check_pixel(owned,1,1,frame == 0 ? (Color){191,127,127,191} : WHITE,"owned translucent layer");
        check_pixel(owned,5,5,frame == 0 ? GREEN : WHITE,"owned nested layer above later parent paint");
        check_pixel(owned,21,1,frame == 0 ? YELLOW : WHITE,"owned retained layer above later main paint");
        check_pixel(owned,41,41,ORANGE,"owned layer restores caller clip and destination");
        check_pixel(owned,39,39,WHITE,"owned layer caller clip excludes outside");
        UnloadImage(owned);
    }
    ui_paint_layers_destroy(owned_layers);
    check_invalid_layer_end(stale_layer,"destroyed host layer token");
    UIPaintLayers *replacement_host = ui_paint_layers_create();
    ui_paint_layers_frame(replacement_host,64,64);
    UIPaintLayerToken replacement_layer = ui_paint_layer_begin(replacement_host,1);
    check_invalid_layer_end(stale_layer,"destroyed host token after replacement allocation");
    ui_paint_layer_end(replacement_layer);
    ui_paint_layers_composite(replacement_host);
    ui_paint_layers_destroy(replacement_host);

    UIPaintLayers *host_a = ui_paint_layers_create();
    UIPaintLayers *host_b = ui_paint_layers_create();
    for(int frame = 0; frame < 2; frame++) {
        int size = frame == 0 ? 32 : 64;
        RenderTexture2D host_target = LoadRenderTexture(size,size);
        if(!host_target.id) return 1;
        BeginTree(Key("independent layer hosts"));
        BeginTextureMode(host_target);
        ClearBackground(BLACK);
        ui_paint_layers_frame(host_a,size,size);
        UIPaintLayerToken a_layer = ui_paint_layer_begin(host_a,1);
        DrawRectangle(0,0,size,size,RED);
        Rect(size-4,size-4,4,4,YELLOW,BLANK);
        ui_paint_layer_end(a_layer);
        EndTextureMode();
        BeginTextureMode(inner);
        ClearBackground(BLUE);
        ui_paint_layers_frame(host_b,16,16);
        UIPaintLayerToken b_layer = ui_paint_layer_begin(host_b,1);
        DrawRectangle(0,0,4,4,GREEN);
        Rect(8,8,4,4,MAGENTA,BLANK);
        ui_paint_layer_end(b_layer);
        EndTextureMode();
        EndTree();
        BeginTextureMode(inner);
        ui_paint_layers_composite(host_b);
        EndTextureMode();
        BeginTextureMode(host_target);
        ui_paint_layers_composite(host_a);
        if(frame == 1) ui_paint_layers_destroy(host_b);
        DrawRectangle(0,0,1,1,WHITE);
        EndTextureMode();
        Image host_image = LoadImageFromTexture(host_target.texture);
        ImageFlipVertical(&host_image);
        check_pixel(host_image,1,1,RED,"resized host owns its immediate layer");
        check_pixel(host_image,size-3,size-3,YELLOW,"resized host owns its retained layer");
        check_pixel(host_image,0,0,WHITE,"destroying another host preserves destination");
        UnloadImage(host_image);
        Image other_image = LoadImageFromTexture(inner.texture);
        ImageFlipVertical(&other_image);
        check_pixel(other_image,1,1,GREEN,"same owner ID remains host-local");
        check_pixel(other_image,9,9,MAGENTA,"interleaved host retained target");
        check_pixel(other_image,14,14,BLUE,"other host resize does not leak paint");
        UnloadImage(other_image);
        UnloadRenderTexture(host_target);
    }
    ui_paint_layers_destroy(host_a);

    UIWindow *window = OpenUIWindow("nested UI paint target",0,0,64,64,
                                   UI_WINDOW_BORDERLESS,RED,1);
    if(window == NULL) {
        fprintf(stderr,"UIWindow integration requires a working desktop window backend\n");
        failures++;
    } else {
        for(int frame = 0; frame < 4; frame++) {
            BeginUIWindow(window);
            BeginTextureMode(inner);
            ClearBackground(BLUE);
            BeginTextureMode(leaf);
            ClearBackground(MAGENTA);
            EndTextureMode();
            EndTextureMode();
            if(frame != 2) {
                UIPaintLayers *window_layers = ui_window_paint_layers();
                if(window_layers == NULL) return 1;
                UIPopupInputToken window_input = ui_popup_input_begin(ui_paint_layers_input(window_layers),1,(Rectangle){0,0,64,64});
                BeginTree(Key("UIWindow owned layers"));
                UIPaintLayerToken window_layer = ui_paint_layer_begin(window_layers,1);
                DrawRectangle(8,8,4,4,GREEN);
                Rect(40,40,4,4,YELLOW,BLANK);
                ui_paint_layer_end(window_layer);
                ui_popup_input_end(window_input);
                Rect(0,0,64,64,RED,BLANK);
                EndTree();
            }
            check_window_readback = frame == 2 ? 2 : 1;
            if(frame == 3) CloseUIWindow(window);
            else EndUIWindow();
            check_window_readback = 0;
            if(ui_window_paint_layers() != NULL) {
                fprintf(stderr,"ended UIWindow retained an active layer owner\n");
                failures++;
            }
        }
        if(window_readbacks != 4) {
            fprintf(stderr,"expected four real UIWindow presenter readbacks, got %d\n",window_readbacks);
            failures++;
        }
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
    BeginUIClip(10,10,20,20);
    DrawUIText("wide", 10 + (20 - TextWidth("wide",16)) / 2,
               TextBaselineY("wide",10,20,16),16,WHITE);
    EndUIClip();
    DrawRectangle(25,10,5,20,BLUE);
    EndTextureMode();
    Image text_reference = LoadImageFromTexture(outer.texture);
    char boxed_text[] = "wide";
    BeginTextureMode(outer);
    ClearBackground(BLACK);
    BeginTree(Key("boxed text lifecycle"));
    Rect(0,0,64,64,RED,BLANK);
    Row((RowProps){.bounds = {10,10,20,20}});
    Text((TextProps){.bounds=(Rectangle){0,0,20,20}, .text=boxed_text, .font=16, .color=WHITE, .wrap=TextWrapNone, .align=TextAlignCenter, .vertical_align=TextAlignCenter});
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
    UIWindow *auxiliary = OpenUIWindow("interleaved layer host",0,0,64,64,
                                       UI_WINDOW_BORDERLESS,BLUE,1);
    if(auxiliary == NULL) return 1;
    UIPopupInput *main_input = NULL;
    for(int frame = 0; frame < 2; frame++) {
        BeginTextureMode(outer);
        ClearBackground(BLACK);
        BeginUIFrame(64,64,1);
        if(frame == 1 && !ui_popup_input_current_captures((Vector2){9,9})) {
            fprintf(stderr,"host did not preserve popup capture before owner declaration\n");
            failures++;
        }
        BeginTree(Key("main host owned layers"));
        Rect(0,0,64,64,RED,BLANK);
        Row((RowProps){.bounds={10,20,44,8},.gap=2});
        Rect(0,0,8,8,BLUE,BLANK);
        if(frame == 0) {
            UIPaintLayers *main_layers = ui_frame_paint_layers();
            if(main_layers == NULL) return 1;
            BeginScroll((Rectangle){0,0,1,1},1,NULL);
            UIPaintLayerToken layer = ui_paint_layer_begin(main_layers,1);
            main_input = ui_paint_layers_input(main_layers);
            UIPopupInputToken input = ui_popup_input_begin(main_input,1,(Rectangle){0,0,64,64});
            Column((ColumnProps){.bounds={0,0,4,4}});
            Rect(0,0,4,4,MAGENTA,BLANK);
            End();
            DrawRectangle(8,8,4,4,GREEN);
            Rect(40,40,4,4,YELLOW,BLANK);
            ui_paint_layer_end(layer);
            ui_popup_input_end(input);
            EndScroll();
        }
        Rect(0,0,8,8,ORANGE,BLANK);
        End();
        EndTree();
        if(frame == 0) {
            UIPaintLayers *main_owner = ui_frame_paint_layers();
            BeginUIWindow(auxiliary);
            if(ui_popup_input_current_captures((Vector2){9,9})) {
                fprintf(stderr,"main popup capture leaked into auxiliary host\n");
                failures++;
            }
            UIPaintLayers *aux_owner = ui_frame_paint_layers();
            if(aux_owner == NULL || aux_owner == main_owner) return 1;
            UIPaintLayerToken aux_layer = ui_paint_layer_begin(aux_owner,1);
            DrawRectangle(0,0,4,4,GREEN);
            ui_paint_layer_end(aux_layer);
            EndUIWindow();
            if(!ui_popup_input_current_captures((Vector2){9,9})) failures++;
            if(ui_frame_paint_layers() != main_owner) {
                fprintf(stderr,"auxiliary frame consumed the main layer owner\n");
                failures++;
            }
        }
        EndUIFrame();
        if(ui_popup_input_bound() != NULL ||
           (frame == 1 && ui_popup_input_captures(main_input,(Vector2){9,9}))) {
            fprintf(stderr,"host retained stale popup binding or missing owner\n");
            failures++;
        }
        if(ui_frame_paint_layers() != NULL) {
            fprintf(stderr,"main layer owner remained available after frame end\n");
            failures++;
        }
        EndTextureMode();
        Image main_image = LoadImageFromTexture(outer.texture);
        ImageFlipVertical(&main_image);
        check_pixel(main_image,9,9,frame == 0 ? GREEN : RED,"main host immediate layer lifecycle");
        check_pixel(main_image,41,41,frame == 0 ? YELLOW : RED,"main host retained layer lifecycle");
        check_pixel(main_image,11,21,BLUE,"parent row before popup");
        check_pixel(main_image,21,21,ORANGE,"parent row resumes after popup");
        check_pixel(main_image,1,1,frame == 0 ? MAGENTA : RED,"popup column independent origin");
        UnloadImage(main_image);
    }
    bool composed_open = true;
    for(int frame = 0; frame < 2; frame++) {
        BeginTextureMode(outer);
        ClearBackground(BLACK);
        BeginUIFrame(64,64,1);
        BeginTree(Key("public composed combo paint"));
        if(BeginCombo((ComboProps){.bounds={0,0,16,8},
                .popup_size={64,56},.preview="",.id=28000,
                .open=&composed_open,.flags=ComboPopupAlignLeft|ComboNoArrowButton})) {
            DrawRectangle(0,8,8,8,GREEN);
            Rect(16,16,8,8,YELLOW,BLANK);
            if(frame == 1) CloseCombo();
            EndCombo();
        }
        Rect(0,0,64,64,RED,BLANK);
        EndTree();
        EndUIFrame();
        EndTextureMode();
        Image composed = LoadImageFromTexture(outer.texture);
        ImageFlipVertical(&composed);
        check_pixel(composed,2,10,frame == 0 ? GREEN : RED,
                    "public combo immediate paint layer");
        check_pixel(composed,18,18,frame == 0 ? YELLOW : RED,
                    "public combo retained paint layer");
        UnloadImage(composed);
    }
    if(composed_open) {
        fprintf(stderr,"public CloseCombo did not update caller state\n");
        failures++;
    }
    CloseUIWindow(auxiliary);
    UnloadRenderTexture(leaf); UnloadRenderTexture(inner); UnloadRenderTexture(outer);
    CloseWindow();
    if(ui_frame_paint_layers() != NULL) failures++;
    InitWindow(64,64,"reopened layer host");
    if(!IsWindowReady()) return 1;
    BeginDrawing();
    BeginUIFrame(64,64,1);
    UIPaintLayers *reopened_layers = ui_frame_paint_layers();
    if(reopened_layers == NULL) return 1;
    UIPaintLayerToken reopened_layer = ui_paint_layer_begin(reopened_layers,1);
    DrawRectangle(0,0,4,4,GREEN);
    ui_paint_layer_end(reopened_layer);
    EndUIFrame();
    EndDrawing();
    CloseWindow();
    if(failures == 0) puts("nested texture scope pixels ok");
    return failures != 0;
}
