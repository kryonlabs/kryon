#ifndef NUMERIC_INPUT_INTERNAL_H
#define NUMERIC_INPUT_INTERNAL_H

typedef struct NumericInputState {
    int token;
    int kind;
    int widget_id;
    int component;
    char text[64];
    int cursor;
    int focused;
    struct NumericInputState *next;
} NumericInputState;

/* Editor addresses remain stable while additional controls are registered. */
NumericInputState *ui_numeric_input_state(int kind, int widget_id, int component);

#endif
