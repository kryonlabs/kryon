#ifndef NUMERIC_INTERNAL_H
#define NUMERIC_INTERNAL_H

#include "kryon.h"

typedef struct {
    Rectangle bounds;
    int id;
    int class_name;
    const char *label;
    float *values;
    int value_count;
    float speed;
    float min;
    float max;
    const char *format;
    int disabled;
} DragContinuousProps;

typedef struct {
    Rectangle bounds;
    int id;
    int class_name;
    const char *label;
    int *values;
    int value_count;
    float speed;
    int min;
    int max;
    const char *format;
    int disabled;
} DragDiscreteProps;

typedef struct {
    Rectangle bounds;
    int id;
    int class_name;
    const char *label;
    float *current_min;
    float *current_max;
    float speed;
    float min;
    float max;
    const char *format;
    const char *format_max;
    int disabled;
} DragContinuousRangeProps;

typedef struct {
    Rectangle bounds;
    int id;
    int class_name;
    const char *label;
    int *current_min;
    int *current_max;
    float speed;
    int min;
    int max;
    const char *format;
    const char *format_max;
    int disabled;
} DragDiscreteRangeProps;

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
    int class_name;
} SliderContinuousProps;

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
    int class_name;
} SliderDiscreteProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    float *value;
    float min_degrees;
    float max_degrees;
    const char *format;
    int disabled;
    int class_name;
} SliderAngleProps;

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
} InputContinuousProps;

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
} InputDiscreteProps;

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
} InputPreciseProps;

#endif
