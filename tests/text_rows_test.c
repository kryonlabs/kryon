#include "../src/ui/ui_internal.h"
#include "../src/ui/ui_text_rows.h"
#include <assert.h>

int TextWidth(const char *text, int font)
{
    (void)font;
    int width = 0;
    int length = (int)strlen(text);
    for (int offset = 0; offset < length; width++)
        offset = ui_grapheme_next_boundary(text, length, offset);
    return width;
}

int TextLineHeight(int font) { return font; }

int main(int argc, char **argv)
{
    assert(argc == 4);
    TextRowCursor cursor = ui_text_rows(argv[1], 16, 2, atoi(argv[2]), 1, atoi(argv[3]));
    TextMeasuredRow row;
    int count = 0;
    while (ui_text_row_next(&cursor, &row)) {
        assert(row.y >= 0 && row.height == row.font + 2);
        printf("%d %d %d\n", row.start, row.end, row.font);
        assert(++count < 1000);
    }
    return 0;
}
