#define _CRT_SECURE_NO_WARNINGS
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define DEFAULT_GLYPH_HEIGHT 16
#define DEFAULT_ATLAS_CELL_SIZE 32
#define ATLAS_WIDTH 512

typedef struct {
    int32_t value;      // Unicode codepoint
    int32_t x, y, w, h;  // Bounding box in PNG
    int32_t offsetX;     // Horizontal offset from cursor
    int32_t offsetY;     // Vertical offset from baseline
    int32_t advanceX;    // Pixels to advance after this glyph
} ChoppedGlyph;

typedef struct {
    int* codepoints;
    int count;
    int capacity;
} CodepointSet;

void init_codepoint_set(CodepointSet* set, int initial_capacity) {
    set->codepoints = calloc(initial_capacity, sizeof(int));
    set->count = 0;
    set->capacity = initial_capacity;
}

void free_codepoint_set(CodepointSet* set) {
    free(set->codepoints);
    set->codepoints = NULL;
    set->count = 0;
    set->capacity = 0;
}

void add_codepoint(CodepointSet* set, int codepoint) {
    if (set->count >= set->capacity) {
        set->capacity *= 2;
        int* new_codepoints = realloc(set->codepoints, set->capacity * sizeof(int));
        if (!new_codepoints) {
            fprintf(stderr, "Memory allocation failed\n");
            return;
        }
        set->codepoints = new_codepoints;
    }
    set->codepoints[set->count++] = codepoint;
}

void add_default_codepoints(CodepointSet* set, char* seen) {
    static const char *defaults =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

    for(const unsigned char *c = (const unsigned char *)defaults; *c != '\0'; c++) {
        int codepoint = (int)*c;
        if(!seen[codepoint]) {
            seen[codepoint] = 1;
            add_codepoint(set, codepoint);
        }
    }
}

int compare_ints(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

int parse_utf8_file(const char* filename, CodepointSet* set, char* seen) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        return 0;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer with extra space for null terminator
    unsigned char* buffer = malloc(file_size + 4);
    if (!buffer) {
        fclose(file);
        return 0;
    }

    // Read file content
    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    buffer[bytes_read + 1] = '\0';
    buffer[bytes_read + 2] = '\0';
    buffer[bytes_read + 3] = '\0'; // Extra nulls for safety
    fclose(file);

    size_t i = 0;
    while (i < bytes_read) {
        unsigned char c = buffer[i];
        int codepoint = 0;
        int bytes = 0;

        // Determine sequence length and extract initial bits
        if (c < 0x80) {
            codepoint = c;
            bytes = 1;
        } else if ((c & 0xE0) == 0xC0) {
            codepoint = c & 0x1F;
            bytes = 2;
        } else if ((c & 0xF0) == 0xE0) {
            codepoint = c & 0x0F;
            bytes = 3;
        } else if ((c & 0xF8) == 0xF0) {
            codepoint = c & 0x07;
            bytes = 4;
        } else {
            i++; // Invalid UTF-8, skip
            continue;
        }

        // Check if we have enough bytes (inclusive check)
        if (i + bytes > bytes_read) {
            break; // Not enough bytes for complete sequence
        }

        // Process continuation bytes with bounds checking
        int valid = 1;
        for (int j = 1; j < bytes; j++) {
            if (i + j >= bytes_read) {
                valid = 0;
                break;
            }
            unsigned char next = buffer[i + j];
            if ((next & 0xC0) != 0x80) {
                valid = 0;
                break;
            }
            codepoint = (codepoint << 6) | (next & 0x3F);
        }

        if (!valid) {
            i++; // Skip invalid sequence byte by byte
            continue;
        }

        i += bytes;

        // Validate and add codepoint
        if (codepoint > 0 && codepoint < 0x110000 &&
            !(codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            if (!seen[codepoint]) {
                seen[codepoint] = 1;
                add_codepoint(set, codepoint);
            }
        }
    }

    free(buffer);

    return set->count;
}

unsigned char* read_file(const char* filename, long* out_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open font file '%s'\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* buffer = malloc(size);
    fread(buffer, 1, size, file);
    fclose(file);

    *out_size = size;
    return buffer;
}

int main(int argc, char* argv[]) {
    int first_input_arg = 2;
    const int glyph_height = DEFAULT_GLYPH_HEIGHT;
    const int atlas_cell_size = DEFAULT_ATLAS_CELL_SIZE;

    if (argc - first_input_arg < 2) {
        fprintf(stderr, "Usage: %s <font.otf> <text1.txt> [more text files...] <output_prefix>\n", argv[0]);
        fprintf(stderr, "Example: %s unifont.otf locales/en.txt locales/es.txt assets/fonts/locales\n", argv[0]);
        return 1;
    }

    const char* font_path = argv[first_input_arg - 1];
    const char* output_prefix = argv[argc - 1];

    // Parse text file and extract unique codepoints
    CodepointSet set;
    init_codepoint_set(&set, 256);
    char* seen = calloc(0x110000, sizeof(char));
    if (!seen) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free_codepoint_set(&set);
        return 1;
    }

    add_default_codepoints(&set, seen);

    for (int i = first_input_arg; i < argc - 1; i++) {
        printf("Parsing %s...\n", argv[i]);
        if (parse_utf8_file(argv[i], &set, seen) == 0) {
            free(seen);
            free_codepoint_set(&set);
            return 1;
        }
    }

    free(seen);

    // Sort codepoints
    qsort(set.codepoints, set.count, sizeof(int), compare_ints);

    int unique_count = set.count;
    printf("Found %d unique codepoints\n", unique_count);

    if (unique_count == 0) {
        fprintf(stderr, "Error: No valid codepoints found in text file\n");
        free_codepoint_set(&set);
        return 1;
    }

    // Load font
    long font_size;
    unsigned char* font_buffer = read_file(font_path, &font_size);
    if (!font_buffer) {
        free_codepoint_set(&set);
        return 1;
    }

    stbtt_fontinfo font;
    int font_offset = stbtt_GetFontOffsetForIndex(font_buffer, 0);
    if (font_offset < 0 || !stbtt_InitFont(&font, font_buffer, font_offset)) {
        fprintf(stderr, "Error: Failed to initialize font\n");
        free(font_buffer);
        free_codepoint_set(&set);
        return 1;
    }

    // Generate Unifont at its canonical 16px grid. Larger UI text should be
    // rendered by integer-scaling this atlas with point filtering.
    float scale = stbtt_ScaleForPixelHeight(&font, (float)glyph_height);

    // Get font metrics
    int ascent;
    stbtt_GetFontVMetrics(&font, &ascent, NULL, NULL);
    int ascent_px = (int)((float)ascent * scale + 0.5f);

    // Allocate glyph info array
    ChoppedGlyph* glyphs = malloc(unique_count * sizeof(ChoppedGlyph));
    unsigned char** glyph_bitmaps = malloc(unique_count * sizeof(unsigned char*));

    printf("Rasterizing glyphs...\n");

    // Calculate atlas dimensions
    int glyphs_per_row = ATLAS_WIDTH / atlas_cell_size;
    int rows = (unique_count + glyphs_per_row - 1) / glyphs_per_row;
    int atlas_height = rows * atlas_cell_size;

    // Allocate atlas buffer (RGBA)
    unsigned char* atlas = malloc(ATLAS_WIDTH * atlas_height * 4);
    memset(atlas, 0, ATLAS_WIDTH * atlas_height * 4);

    // Rasterize each glyph and pack into atlas
    for (int i = 0; i < unique_count; i++) {
        int codepoint = set.codepoints[i];

        // Get glyph metrics
        int advance;
        stbtt_GetCodepointHMetrics(&font, codepoint, &advance, NULL);

        int ix0, iy0, ix1, iy1;
        stbtt_GetCodepointBitmapBox(&font, codepoint, scale, scale, &ix0, &iy0, &ix1, &iy1);

        int gw = ix1 - ix0;
        int gh = iy1 - iy0;
        if (gw <= 0)
            gw = 1;
        if (gh <= 0)
            gh = 1;

        // Allocate and render bitmap
        unsigned char* bitmap = calloc((size_t)gw * (size_t)gh, 1);
        if (!bitmap) {
            fprintf(stderr, "Error: Bitmap allocation failed\n");
            free(glyph_bitmaps);
            free(glyphs);
            free(atlas);
            free(font_buffer);
            free_codepoint_set(&set);
            return 1;
        }
        if (ix1 - ix0 > 0 && iy1 - iy0 > 0) {
            stbtt_MakeCodepointBitmap(&font, bitmap, gw, gh, gw, scale, scale, codepoint);
        }

        glyph_bitmaps[i] = bitmap;

        // Calculate position in atlas
        int row = i / glyphs_per_row;
        int col = i % glyphs_per_row;
        int atlas_x = col * atlas_cell_size;
        int atlas_y = row * atlas_cell_size;

        for (int y = 0; y < gh; y++) {
            for (int x = 0; x < gw; x++) {
                int atlas_idx = ((atlas_y + y) * ATLAS_WIDTH + (atlas_x + x)) * 4;
                unsigned char alpha = bitmap[y * gw + x];
                atlas[atlas_idx + 0] = 255; // R
                atlas[atlas_idx + 1] = 255; // G
                atlas[atlas_idx + 2] = 255; // B
                atlas[atlas_idx + 3] = alpha; // A
            }
        }

        // Store glyph metadata
        glyphs[i].value = codepoint;
        glyphs[i].x = atlas_x;
        glyphs[i].y = atlas_y;
        glyphs[i].w = gw;
        glyphs[i].h = gh;
        glyphs[i].offsetX = ix0;
        glyphs[i].offsetY = iy0 + ascent_px;
        glyphs[i].advanceX = (int)(advance * scale);
    }

    // Write PNG
    char png_path[512];
    snprintf(png_path, sizeof(png_path), "%s.png", output_prefix);
    printf("Writing %s...\n", png_path);
    stbi_write_png(png_path, ATLAS_WIDTH, atlas_height, 4, atlas, ATLAS_WIDTH * 4);

    // Write DAT file
    char dat_path[512];
    snprintf(dat_path, sizeof(dat_path), "%s.dat", output_prefix);
    printf("Writing %s...\n", dat_path);

    FILE* dat_file = fopen(dat_path, "wb");
    if (dat_file) {
        int32_t count = unique_count;
        fwrite(&count, sizeof(int32_t), 1, dat_file);
        fwrite(glyphs, sizeof(ChoppedGlyph), unique_count, dat_file);
        fclose(dat_file);
    }

    // Cleanup
    for (int i = 0; i < unique_count; i++) {
        free(glyph_bitmaps[i]);
    }
    free(glyph_bitmaps);
    free(glyphs);
    free(atlas);
    free(font_buffer);
    free_codepoint_set(&set);

    printf("Done! Generated %s and %s\n", png_path, dat_path);
    printf("Atlas size: %dx%d, Glyphs: %d\n", ATLAS_WIDTH, atlas_height, unique_count);

    return 0;
}
