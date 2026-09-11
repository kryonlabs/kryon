#ifndef UI_COLOR_H
#define UI_COLOR_H

#include "kryon.h"

// Lighten a color by increasing HSL lightness.
Color LightenColor(Color c, int amount);
Color LightenUIColor(Color c, int amount);

// Darken a color by decreasing HSL lightness.
Color DarkenColor(Color c, int amount);
Color DarkenUIColor(Color c, int amount);

#endif // UI_COLOR_H
