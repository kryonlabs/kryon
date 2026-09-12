#ifndef KRYON_SCALING_H
#define KRYON_SCALING_H

#include <stdint.h>

// Set the DPI scale factor (should be called once at startup)
void SetScale(float scale);

// Get the current DPI scale factor
float GetScale(void);

// Scale a pixel value by the DPI factor
int Scale(int px);

// Scale and clamp a pixel value between min and max
int ClampPx(int px, int min_px, int max_px);

#endif // KRYON_SCALING_H
