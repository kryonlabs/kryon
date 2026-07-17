#ifndef UI_COLOR_H
#define UI_COLOR_H

#include "flint.h"

// Lighten a color by adding to each RGB component
Color LightenUIColor(Color c, int amount);

// Darken a color by subtracting from each RGB component
Color DarkenUIColor(Color c, int amount);

#endif // UI_COLOR_H
