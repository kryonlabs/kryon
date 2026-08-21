#ifndef UI_TOAST_H
#define UI_TOAST_H

void ShowUIToast(const char *message);
void ShowUIToastFor(const char *message, double seconds);
void ClearUIToast(void);
void DrawUIToast(void);

#endif
