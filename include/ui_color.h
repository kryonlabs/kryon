#ifndef KRYON_COLOR_H
#define KRYON_COLOR_H

#include "kryon.h"

// Lighten a color by increasing HSL lightness.
Color LightenColor(Color c, int amount);

// Darken a color by decreasing HSL lightness.
Color DarkenColor(Color c, int amount);

#endif // KRYON_COLOR_H
