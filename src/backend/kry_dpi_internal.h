#ifndef KRY_DPI_INTERNAL_H
#define KRY_DPI_INTERNAL_H

#include "kryon.h"

int kry_dpi_platform_kind(void);
Vector2 kry_dpi_window_scale(void);
void FixDPIFramebufferColor(void);

#endif
