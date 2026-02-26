/*
 * PPM Screenshot Tool
 * Converts raw BGRA pixel data to PPM image format
 * C89/C90 compliant
 *
 * Usage: save_ppm <input.raw> <width> <height> <output.ppm>
 *
 * PPM Format:
 * - Header: P6\n<width> <height>\n255\n
 * - Body: RGB triples (BGR from BGRA input, skip alpha)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    FILE *input_file;
    FILE *output_file;
    int width;
    int height;
    unsigned char *pixel_data;
    long file_size;
    long expected_size;
    int x, y;
    int i;

    /* Parse command line arguments */
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <input.raw> <width> <height> <output.ppm>\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Converts raw BGRA pixel data to PPM image format.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Example:\n");
        fprintf(stderr, "  %s /tmp/screen.raw 800 600 /tmp/screen.ppm\n", argv[0]);
        return 1;
    }

    /* Parse dimensions */
    width = atoi(argv[2]);
    height = atoi(argv[3]);

    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Error: Invalid dimensions %dx%d\n", width, height);
        return 1;
    }

    /* Calculate expected file size */
    expected_size = width * height * 4;  /* BGRA = 4 bytes per pixel */

    /* Open input file */
    input_file = fopen(argv[1], "rb");
    if (input_file == NULL) {
        fprintf(stderr, "Error: Failed to open input file '%s'\n", argv[1]);
        return 1;
    }

    /* Get file size */
    fseek(input_file, 0, SEEK_END);
    file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    /* Verify file size */
    if (file_size != expected_size) {
        fprintf(stderr, "Warning: File size (%ld bytes) doesn't match expected size (%ld bytes)\n",
                file_size, expected_size);
        fprintf(stderr, "         Proceeding anyway...\n");
    }

    /* Allocate buffer for pixel data */
    pixel_data = (unsigned char *)malloc(file_size);
    if (pixel_data == NULL) {
        fprintf(stderr, "Error: Failed to allocate %ld bytes for pixel data\n", file_size);
        fclose(input_file);
        return 1;
    }

    /* Read pixel data */
    if (fread(pixel_data, 1, file_size, input_file) != (size_t)file_size) {
        fprintf(stderr, "Error: Failed to read %ld bytes from input file\n", file_size);
        free(pixel_data);
        fclose(input_file);
        return 1;
    }

    fclose(input_file);

    /* Open output file */
    output_file = fopen(argv[4], "wb");
    if (output_file == NULL) {
        fprintf(stderr, "Error: Failed to create output file '%s'\n", argv[4]);
        free(pixel_data);
        return 1;
    }

    /* Write PPM header */
    fprintf(output_file, "P6\n%d %d\n255\n", width, height);

    /* Write pixel data (BGR from BGRA, skip alpha) */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            i = (y * width + x) * 4;
            /* Write B, G, R (skip A) */
            fputc(pixel_data[i + 0], output_file);     /* B */
            fputc(pixel_data[i + 1], output_file);     /* G */
            fputc(pixel_data[i + 2], output_file);     /* R */
        }
    }

    fclose(output_file);
    free(pixel_data);

    fprintf(stderr, "Converted %ld bytes from '%s' to '%s' (%dx%d)\n",
            file_size, argv[1], argv[4], width, height);

    return 0;
}
