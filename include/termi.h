#ifndef TERMI_H
#define TERMI_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    TERMI_CELL_WIDTH = 8,
    TERMI_CELL_HEIGHT = 16,
    TERMI_ATTR_NONE = 0,
    TERMI_ATTR_BOLD = 1 << 0,
    TERMI_ATTR_DIM = 1 << 1,
    TERMI_ATTR_UNDERLINE = 1 << 2,
    TERMI_ATTR_REVERSE = 1 << 3
};

typedef struct TermiCell {
    char text[5];
    unsigned fg;
    unsigned bg;
    unsigned attr;
} TermiCell;

int termi_cols(void);
int termi_rows(void);
int termi_cell_width(void);
int termi_cell_height(void);
void termi_clear(unsigned bg_rgba);
void termi_rect(int x, int y, int w, int h, unsigned bg_rgba);
void termi_text(const char *text, int x, int y, unsigned fg_rgba,
                unsigned bg_rgba, unsigned attr);
void termi_present(void);
void termi_request_close(void);

int termi_font_height(unsigned id);
int termi_text_width(unsigned id, const char *text, int byte_len);
void termi_queue_text(unsigned font_id, const char *text, int byte_len,
                      int x, int y, int font_size, unsigned rgba);

#ifdef __cplusplus
}
#endif

#endif /* TERMI_H */
