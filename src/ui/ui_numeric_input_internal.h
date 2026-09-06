#ifndef UI_NUMERIC_INPUT_INTERNAL_H
#define UI_NUMERIC_INPUT_INTERNAL_H

typedef struct UINumericInputState {
    int token;
    int kind;
    int widget_id;
    int component;
    char text[64];
    int cursor;
    int focused;
    struct UINumericInputState *next;
} UINumericInputState;

/* Editor addresses remain stable while additional controls are registered. */
UINumericInputState *ui_numeric_input_state(int kind, int widget_id, int component);

#endif
