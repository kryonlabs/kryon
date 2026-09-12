#ifndef UI_NUMERIC_INTERNAL_H
#define UI_NUMERIC_INTERNAL_H

#include "kryon.h"

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    float *values;
    int value_count;
    float speed;
    float min;
    float max;
    const char *format;
    int disabled;
} UIFloatDragProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    int *values;
    int value_count;
    float speed;
    int min;
    int max;
    const char *format;
    int disabled;
} UIIntDragProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    float *current_min;
    float *current_max;
    float speed;
    float min;
    float max;
    const char *format;
    const char *format_max;
    int disabled;
} UIFloatDragRangeProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    int *current_min;
    int *current_max;
    float speed;
    int min;
    int max;
    const char *format;
    const char *format_max;
    int disabled;
} UIIntDragRangeProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    float *values;
    int value_count;
    float min;
    float max;
    const char *format;
    int disabled;
} UIFloatSliderProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    int *values;
    int value_count;
    int min;
    int max;
    const char *format;
    int disabled;
} UIIntSliderProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    float *value;
    float min_degrees;
    float max_degrees;
    const char *format;
    int disabled;
} UIAngleSliderProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    float *values;
    int value_count;
    float step;
    float step_fast;
    const char *format;
    int disabled;
} UIFloatInputProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    int *values;
    int value_count;
    int step;
    int step_fast;
    const char *format;
    int disabled;
} UIIntInputProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    double *values;
    int value_count;
    double step;
    double step_fast;
    const char *format;
    int disabled;
} UIDoubleInputProps;

#endif
