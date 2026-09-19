#ifndef LIBDRAW_X11_INPUT_H
#define LIBDRAW_X11_INPUT_H

/* plan9port's rune stream does not carry key releases or modifier state.
 * The X11 host observes keyboard events on the same devdraw window. Text
 * composition still comes from plan9port. These are private backend hooks. */
#if defined(__linux__) && !defined(KRYON_NATIVE_PLAN9)
int X11InputOpen(const char *title);
void X11InputPoll(void);
void X11InputNextFrame(void);
void X11InputClose(void);
int X11InputReady(void);
int X11InputFocused(void);
void *X11InputWindow(void);
#endif

#endif
