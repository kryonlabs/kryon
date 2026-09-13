#ifndef KRYON_UI_PAINT_LAYERS_INTERNAL_H
#define KRYON_UI_PAINT_LAYERS_INTERNAL_H
#include "kryon.h"
#include "ui_popup_input_internal.h"

/* Private paint ownership only, not a public popup/widget scope. The owner
 * must finish retained painting before compositing or destroying its layers.
 * One context belongs to one rendering host; no process-global layer registry. */
typedef struct PaintLayers PaintLayers;
typedef struct PaintLayerToken {
    PaintLayers *owner;
    unsigned long frame;
    int index;
} PaintLayerToken;

PaintLayers *ui_paint_layers_create(void);
void ui_paint_layers_destroy(PaintLayers *layers);
void ui_paint_layers_frame(PaintLayers *layers, int width, int height);
PaintLayerToken ui_paint_layer_begin(PaintLayers *layers, int owner);
void ui_paint_layer_end(PaintLayerToken token);
void ui_paint_layers_hide(PaintLayers *layers, int owner);
void ui_paint_layers_composite(PaintLayers *layers);
PopupInput *ui_paint_layers_input(PaintLayers *layers);
void ui_window_layers_begin(void);
/* Lazily obtain the active native NativeWindow's context. Its frame, composition
 * and destruction belong to the window, not to the caller. */
PaintLayers *ui_window_paint_layers(void);
int ui_window_frame_active(void);
PaintLayers *ui_frame_paint_layers(void);
void ui_frame_layers_begin(void);
void ui_frame_layers_end(void);
void ui_paint_layers_shutdown(void);
Image ui_paint_readback(Texture2D texture);
#endif
