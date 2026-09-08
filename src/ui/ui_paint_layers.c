#include "ui_internal.h"
#include "ui_blend_internal.h"
#include "ui_clip_internal.h"
#include "ui_paint_layers_internal.h"
#include "ui_tree_layout_internal.h"
#include "ui_disabled_internal.h"
#include "dropdown_store.h"
#include "tab_bar_store.h"
#include "toolkit_store.h"
#include "ui_input_clip_internal.h"
#include <stdlib.h>
#include <limits.h>

#if defined(KRYON_BACKEND_RAYLIB)
int kryon_rl_framebuffer_save(void);
void kryon_rl_framebuffer_restore(int framebuffer);
#endif

/* Texture allocation and destruction bind their own framebuffer internally.
 * Preserve the active destination even before a new layer has been entered. */
static RenderTexture2D layer_texture_replace(RenderTexture2D old, int width, int height)
{
#if defined(KRYON_BACKEND_RAYLIB)
    int framebuffer = kryon_rl_framebuffer_save();
#endif
    if(old.id) UnloadRenderTexture(old);
    RenderTexture2D texture = width > 0 ? LoadRenderTexture(width,height) : (RenderTexture2D){0};
#if defined(KRYON_BACKEND_RAYLIB)
    kryon_rl_framebuffer_restore(framebuffer);
#endif
    return texture;
}

Image ui_paint_readback(Texture2D texture)
{
#if defined(KRYON_BACKEND_RAYLIB)
    int framebuffer = kryon_rl_framebuffer_save();
#endif
    Image image = LoadImageFromTexture(texture);
#if defined(KRYON_BACKEND_RAYLIB)
    kryon_rl_framebuffer_restore(framebuffer);
#endif
    return image;
}

typedef struct UIPaintLayer {
    RenderTexture2D texture, previous_target;
    UIBlendState previous_blend;
    UIClipState previous_clip;
    UIPaintLayerToken previous_scope;
    UITreeLayoutScope previous_layout;
    UIDisabledScope previous_disabled;
    UIInputClipScope previous_input_clip;
    int owner, parent, visible;
} UIPaintLayer;

struct UIPaintLayers {
    UIPaintLayer *items;
    int count, allocated, capacity, active;
    int width, height, finished;
    unsigned long frame;
    Matrix projection, modelview;
    UIPopupInput *input, *previous_input;
    DropdownStore *dropdowns, *previous_dropdowns;
    TabBarStore *tab_bars, *previous_tab_bars;
    ToolkitStore *toolkit, *previous_toolkit;
};

static UIPaintLayers *main_layers;
static int main_frame_open;
/* Input ownership is a runtime concern, not a renderer concern.  Injection,
 * generated parity, and other headless callers still need popup scopes even
 * when no texture-backed paint-layer host can exist. */
static UIPopupInput *headless_input, *headless_previous_input;
static DropdownStore *headless_dropdowns, *headless_previous_dropdowns;
static TabBarStore *headless_tab_bars, *headless_previous_tab_bars;
static ToolkitStore *headless_toolkit, *headless_previous_toolkit;
static int headless_input_open;
/* Graphics scopes form one stack even when their textures belong to separate
 * host contexts. Validate it before restoring any backend or borrowed state. */
static UIPaintLayerToken active_scope;
static unsigned long scope_generation;

void ui_frame_layers_begin(void)
{
    if(ui_window_frame_active()) return;
    if(IsWindowReady()) {
        if(!main_layers) main_layers = ui_paint_layers_create();
        ui_paint_layers_frame(main_layers,ui_view_width,ui_view_height);
    } else if(ui_popup_input_bound() == NULL) {
        if(!headless_input) headless_input = ui_popup_input_create();
        if(!headless_dropdowns)
            headless_dropdowns = dropdown_store_new();
        if(!headless_tab_bars)
            headless_tab_bars = tab_bar_store_new();
        if(!headless_toolkit)
            headless_toolkit = toolkit_store_new();
        ui_popup_input_frame(headless_input);
        headless_previous_input = ui_popup_input_bind(headless_input);
        headless_previous_dropdowns =
            dropdown_store_swap(headless_dropdowns);
        headless_previous_tab_bars =
            tab_bar_store_swap(headless_tab_bars);
        headless_previous_toolkit = toolkit_store_swap(headless_toolkit);
        headless_input_open = 1;
    }
    main_frame_open = 1;
}

UIPaintLayers *ui_frame_paint_layers(void)
{
    if(ui_window_frame_active()) return ui_window_paint_layers();
    if(!main_frame_open || !IsWindowReady()) return NULL;
    if(main_layers == NULL) {
        main_layers = ui_paint_layers_create();
        ui_paint_layers_frame(main_layers,ui_view_width,ui_view_height);
    }
    return main_layers;
}

void ui_frame_layers_end(void)
{
    if(ui_window_frame_active() || !main_frame_open) return;
    if(main_layers) {
        ui_paint_layers_composite(main_layers);
        /* Input routing happens before the next frame redeclares owners. */
        dropdown_store_swap(main_layers->dropdowns);
        tab_bar_store_swap(main_layers->tab_bars);
        toolkit_store_swap(main_layers->toolkit);
    }
    if(headless_input_open) {
        ui_popup_input_finish(headless_input);
        ui_popup_input_bind(headless_previous_input);
        dropdown_store_swap(headless_previous_dropdowns);
        tab_bar_store_swap(headless_previous_tab_bars);
        toolkit_store_swap(headless_previous_toolkit);
        headless_previous_input = NULL;
        headless_previous_dropdowns = NULL;
        headless_previous_tab_bars = NULL;
        headless_previous_toolkit = NULL;
        headless_input_open = 0;
        dropdown_store_swap(headless_dropdowns);
        tab_bar_store_swap(headless_tab_bars);
        toolkit_store_swap(headless_toolkit);
    }
    main_frame_open = 0;
}

void ui_paint_layers_shutdown(void)
{
    if(main_layers) {
        if(!main_layers->finished && main_layers->active == -1)
            ui_paint_layers_composite(main_layers);
        if(dropdown_store_current() == main_layers->dropdowns)
            dropdown_store_swap(NULL);
        if(tab_bar_store_current() == main_layers->tab_bars)
            tab_bar_store_swap(NULL);
        if(toolkit_store_current() == main_layers->toolkit)
            toolkit_store_swap(NULL);
        ui_paint_layers_destroy(main_layers);
        main_layers = NULL;
    }
    if(headless_input) {
        if(headless_input_open) {
            ui_popup_input_finish(headless_input);
            ui_popup_input_bind(headless_previous_input);
            dropdown_store_swap(headless_previous_dropdowns);
            tab_bar_store_swap(headless_previous_tab_bars);
            toolkit_store_swap(headless_previous_toolkit);
            headless_input_open = 0;
        }
        ui_popup_input_destroy(headless_input);
        headless_input = NULL;
        headless_previous_input = NULL;
        headless_previous_dropdowns = NULL;
        headless_previous_tab_bars = NULL;
        headless_previous_toolkit = NULL;
    }
    if(headless_dropdowns) {
        if(dropdown_store_current() == headless_dropdowns)
            dropdown_store_swap(NULL);
        dropdown_store_free(headless_dropdowns);
        headless_dropdowns = NULL;
    }
    if(headless_tab_bars) {
        if(tab_bar_store_current() == headless_tab_bars)
            tab_bar_store_swap(NULL);
        tab_bar_store_free(headless_tab_bars);
        headless_tab_bars = NULL;
    }
    if(headless_toolkit) {
        if(toolkit_store_current() == headless_toolkit)
            toolkit_store_swap(NULL);
        toolkit_store_free(headless_toolkit);
        headless_toolkit = NULL;
    }
    main_frame_open = 0;
}

UIPaintLayers *ui_paint_layers_create(void)
{
    UIPaintLayers *layers = calloc(1,sizeof(*layers));
    if(layers == NULL) abort();
    layers->active = -1;
    layers->finished = 1;
    layers->input = ui_popup_input_create();
    layers->dropdowns = dropdown_store_new();
    layers->tab_bars = tab_bar_store_new();
    layers->toolkit = toolkit_store_new();
    return layers;
}

UIPopupInput *ui_paint_layers_input(UIPaintLayers *layers)
{
    return layers ? layers->input : NULL;
}

void ui_paint_layers_destroy(UIPaintLayers *layers)
{
    if(layers == NULL) return;
    if(layers->active != -1 || !layers->finished) abort();
    for(int i = 0; i < layers->allocated; i++) layer_texture_replace(layers->items[i].texture,0,0);
    ui_popup_input_destroy(layers->input);
    dropdown_store_free(layers->dropdowns);
    tab_bar_store_free(layers->tab_bars);
    toolkit_store_free(layers->toolkit);
    free(layers->items);
    free(layers);
}

void ui_paint_layers_frame(UIPaintLayers *layers, int width, int height)
{
    if(layers == NULL || layers->active != -1 || !layers->finished || width <= 0 || height <= 0) abort();
    layers->count = 0;
    layers->width = width;
    layers->height = height;
    layers->finished = 0;
    /* Tokens must remain stale even if malloc later reuses a destroyed host's
     * address. Allocate generations across contexts, not from zero per host. */
    if(scope_generation == ULONG_MAX) abort();
    layers->frame = ++scope_generation;
    ui_popup_input_frame(layers->input);
    layers->previous_input = ui_popup_input_bind(layers->input);
    layers->previous_dropdowns = dropdown_store_swap(layers->dropdowns);
    layers->previous_tab_bars = tab_bar_store_swap(layers->tab_bars);
    layers->previous_toolkit = toolkit_store_swap(layers->toolkit);
}

UIPaintLayerToken ui_paint_layer_begin(UIPaintLayers *layers, int owner)
{
    if(layers == NULL || layers->finished || !IsWindowReady()) abort();
    for(int i = 0; i < layers->count; i++)
        if(layers->items[i].owner == owner) abort();
    if(layers->count == layers->capacity) {
        if(layers->capacity > INT_MAX/2) abort();
        int capacity = layers->capacity ? layers->capacity * 2 : 4;
        if((size_t)capacity > SIZE_MAX/sizeof(UIPaintLayer)) abort();
        UIPaintLayer *items = realloc(layers->items,(size_t)capacity*sizeof(*items));
        if(items == NULL) abort();
        layers->items = items;
        layers->capacity = capacity;
    }
    int index = layers->count++;
    UIPaintLayer *layer = &layers->items[index];
    if(index >= layers->allocated) {
        *layer = (UIPaintLayer){0};
        layers->allocated++;
    }
    if(layer->texture.id && (layer->texture.texture.width != layers->width || layer->texture.texture.height != layers->height)) {
        layer->texture = layer_texture_replace(layer->texture,layers->width,layers->height);
    }
    if(!layer->texture.id) layer->texture = layer_texture_replace(layer->texture,layers->width,layers->height);
    if(!layer->texture.id) abort();
    layer->owner = owner;
    layer->parent = layers->active;
    layer->visible = 1;
    layer->previous_clip = ui_clip_save();
    layer->previous_blend = ui_blend_save();
    layer->previous_scope = active_scope;
    layer->previous_layout = ui_tree_layout_suspend();
    layer->previous_disabled = ui_disabled_suspend();
    layer->previous_input_clip = ui_input_clip_suspend();
    BeginTextureMode(layer->texture);
    layers->projection = rlGetMatrixProjection();
    layers->modelview = rlGetMatrixModelview();
    ResetUIClip();
    ClearBackground(BLANK);
    ui_blend_capture();
    layer->previous_target = ui_tree_set_paint_target(layer->texture);
    layers->active = index;
    active_scope = (UIPaintLayerToken){layers,layers->frame,index};
    return active_scope;
}

void ui_paint_layer_end(UIPaintLayerToken token)
{
    UIPaintLayers *layers = token.owner;
    if(layers == NULL || token.owner != active_scope.owner ||
       token.frame != active_scope.frame || token.index != active_scope.index) abort();
    if(token.frame != layers->frame || layers->active != token.index || token.index < 0) abort();
    UIPaintLayer *layer = &layers->items[token.index];
    ui_input_clip_resume(layer->previous_input_clip);
    ui_disabled_resume(layer->previous_disabled);
    ui_tree_layout_resume(layer->previous_layout);
    ui_tree_set_paint_target(layer->previous_target);
    EndTextureMode();
    ui_blend_restore(layer->previous_blend);
    ui_clip_restore(layer->previous_clip);
    layers->active = layer->parent;
    active_scope = layer->previous_scope;
}

void ui_paint_layers_hide(UIPaintLayers *layers, int owner)
{
    if(layers == NULL || layers->finished) abort();
    ui_popup_input_close(layers->input,owner);
    for(int i = 0; i < layers->count; i++)
        if(layers->items[i].owner == owner) layers->items[i].visible = 0;
}

void ui_paint_layers_composite(UIPaintLayers *layers)
{
    if(layers == NULL || layers->active != -1 || layers->finished) abort();
    if(ui_popup_input_bound() != layers->input) abort();
    ui_popup_input_finish(layers->input);
    UIBlendState blend = ui_blend_save();
    UIClipState clip = ui_clip_save();
    Matrix projection = rlGetMatrixProjection(), modelview = rlGetMatrixModelview();
    /* Restoring the snapshot also flushes pending parent drawing before the
     * projection changes, without adding a public low-level drawing API. */
    ui_blend_restore(blend);
    if(layers->count) {
        rlSetMatrixProjection(layers->projection);
        rlSetMatrixModelview(layers->modelview);
    }
    ResetUIClip();
    /* Keep paint-layer compositing on the same straight-alpha path as normal
     * UI drawing. Font atlases are straight-alpha textures; treating captured
     * layers as premultiplied makes glyph quads render as visible boxes. */
    BeginBlendMode(BLEND_ALPHA);
    for(int i = 0; i < layers->count; i++) {
        UIPaintLayer *layer = &layers->items[i];
        if(layer->parent >= 0 && !layers->items[layer->parent].visible) layer->visible = 0;
        if(layer->visible)
            DrawTextureRec(layer->texture.texture,
                           (Rectangle){0,0,(float)layers->width,-(float)layers->height},
                           (Vector2){0,0},WHITE);
    }
    ui_blend_restore(blend);
    rlSetMatrixProjection(projection);
    rlSetMatrixModelview(modelview);
    ui_clip_restore(clip);
    for(int i = layers->count; i < layers->allocated; i++) layer_texture_replace(layers->items[i].texture,0,0);
    layers->allocated = layers->count;
    layers->finished = 1;
    ui_popup_input_bind(layers->previous_input);
    dropdown_store_swap(layers->previous_dropdowns);
    tab_bar_store_swap(layers->previous_tab_bars);
    toolkit_store_swap(layers->previous_toolkit);
}
