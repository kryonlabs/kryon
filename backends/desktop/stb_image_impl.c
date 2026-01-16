// STB Image implementation for desktop backend
// This file provides the stb_image implementation for image loading and writing
//
// When using raylib (which includes its own stb_image), define KRYON_USE_EXTERNAL_STB
// before including this file to avoid multiple definition conflicts.
// Note: raylib only provides stb_image, not stb_image_write, so we always
// include STB_IMAGE_WRITE_IMPLEMENTATION.

#ifndef KRYON_USE_EXTERNAL_STB
#define STB_IMAGE_IMPLEMENTATION
#include "../../third_party/stb/stb_image.h"
#else
// Use external stb_image (e.g., from raylib) - just declare the interface
#include "../../third_party/stb/stb_image.h"
#endif

// Always include stb_image_write implementation since raylib doesn't provide it
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../third_party/stb/stb_image_write.h"
