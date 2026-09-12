#ifndef KRYON_TOAST_H
#define KRYON_TOAST_H

typedef struct ToastProps {
    const char *message;
    double seconds;
} ToastProps;

void Toast(ToastProps props);

#endif
