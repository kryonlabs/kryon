#ifndef KRYON_UI_TREE_NODE_INTERNAL_H
#define KRYON_UI_TREE_NODE_INTERNAL_H

#include "ui_tree.h"

typedef struct {
    ButtonProps props;
    ControlStyle style;
    Style paint;
    Color hover_background;
    int style_resolved;
    Rectangle surface_bounds;
    int disclosure;
    int style_kind;
} ButtonSpec;

typedef union WidgetData {
    struct {
        unsigned long long words[16];
    } internal;
    struct {
        DragProps props;
        size_t format_offset;
    } drag;
    struct {
        SliderProps props;
        size_t format_offset;
    } slider;
    struct {
        int gap;
        int padding;
        int columns;
        int min_item_width;
        int max_columns;
    } layout;
    ParagraphSpec paragraph;
    ImageProps image;
    struct {
        int x1;
        int y1;
        int x2;
        int y2;
        int x3;
        int y3;
        int font;
        int font_token;
        int letter_spacing;
        int heading_level;
        int wrap;
        int align;
        int vertical_align;
        Color color;
        Color border;
        int styled;
        Style style;
    } primitive;
    ButtonSpec button;
    TextFieldProps text_field;
    TextAreaProps text_area;
    struct {
        int *value;
        int class_name;
        const char *off_label;
        const char *on_label;
    } toggle;
    struct {
        int *value;
        const char *label;
    } checkbox;
} WidgetData;

struct TreeNode {
    int id;
    KeyID key;
    int kind;
    Rectangle bounds;
    Rectangle declared_bounds;
    Rectangle input_clip;
    int has_input_clip;
    unsigned paint_capture;
    unsigned popup_input_capture;
    int font_token;
    int parent;
    int first_child;
    int next_sibling;
    const void *props;
    void *state;
    WidgetData data;
    unsigned flags;
    unsigned generation;
    char *owned_text;
};

#endif
