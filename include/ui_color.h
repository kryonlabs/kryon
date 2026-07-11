#ifndef UI_COLOR_H
#define UI_COLOR_H

#include "flint.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lighten a color by adding to each RGB component
Color LightenUIColor(Color c, int amount);

// Darken a color by subtracting from each RGB component
Color DarkenUIColor(Color c, int amount);

#ifdef __cplusplus
}
#endif

#endif // UI_COLOR_H
