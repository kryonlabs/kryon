// STB Image implementation for desktop backend
// This file provides the stb_image implementation for image loading and writing
//
// When using raylib (which includes both stb_image and stb_image_write),
// define KRYON_USE_EXTERNAL_STB before including this file to avoid
// multiple definition conflicts.

#ifndef KRYON_USE_EXTERNAL_STB
// Define implementations when NOT using external stb (raylib)
#define STB_IMAGE_IMPLEMENTATION
#include "../../third_party/stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../third_party/stb/stb_image_write.h"
#else
// Use external stb_image and stb_image_write (e.g., from raylib)
// Just declare the interface without defining implementations
#include "../../third_party/stb/stb_image.h"
#include "../../third_party/stb/stb_image_write.h"
#endif
