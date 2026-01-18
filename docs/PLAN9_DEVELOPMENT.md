# Plan 9 Development Guide for Kryon

This guide covers developing Kryon's Plan 9/9front backend on Linux using plan9port.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Plan 9 C Dialect Differences](#plan-9-c-dialect-differences)
3. [mkfile Build System](#mkfile-build-system)
4. [libdraw Graphics API](#libdraw-graphics-api)
5. [Event Handling](#event-handling)
6. [Font System](#font-system)
7. [Common Issues and Solutions](#common-issues-and-solutions)
8. [Resources](#resources)

---

## Prerequisites

### Using Nix (Recommended)

The Kryon `shell.nix` includes plan9port automatically:

```bash
cd /path/to/kryon
nix-shell
# Plan 9 tools are now available
echo $PLAN9  # Should show plan9port installation path
mk --help    # Should display mk usage
```

### Manual Installation

If not using Nix, install plan9port:

**Arch Linux:**
```bash
yay -S plan9port
```

**Ubuntu/Debian:**
```bash
git clone https://github.com/9fans/plan9port.git /usr/local/plan9
cd /usr/local/plan9
./INSTALL
```

**macOS:**
```bash
brew install plan9port
```

Then set environment variables:
```bash
export PLAN9=/usr/local/plan9  # Adjust path as needed
export PATH=$PATH:$PLAN9/bin
```

---

## Plan 9 C Dialect Differences

Plan 9 C is based on pre-C99 ANSI C with some extensions. Key differences from modern C:

### 1. No Designated Initializers

**WRONG (C99):**
```c
IRComponent comp = {
    .id = 123,
    .type = "Button",
    .x = 10.0f
};
```

**CORRECT (Plan 9):**
```c
IRComponent comp;
memset(&comp, 0, sizeof(comp));
comp.id = 123;
comp.type = "Button";
comp.x = 10.0f;

/* OR use ordered initialization if all fields are known */
IRComponent comp = { 123, "Button", 10.0f, 0.0f, ... };
```

### 2. No Variable-Length Arrays (VLAs)

**WRONG:**
```c
int count = 10;
int values[count];  /* VLA not supported */
```

**CORRECT:**
```c
/* Fixed-size buffer */
int values[MAX_VALUES];

/* OR dynamic allocation */
int *values = malloc(count * sizeof(int));
/* ... use values ... */
free(values);
```

### 3. No inline Keyword

**WRONG:**
```c
inline int square(int x) { return x * x; }
```

**CORRECT:**
```c
/* Use static functions */
static int square(int x) { return x * x; }

/* OR use macros for simple cases */
#define square(x) ((x) * (x))
```

### 4. for Loop Variable Declarations

**WRONG:**
```c
for (int i = 0; i < count; i++) { }
```

**CORRECT:**
```c
int i;
for (i = 0; i < count; i++) { }
```

### 5. Comments

Plan 9 C supports both `/* */` and `//` comments, but `/* */` is preferred:

**PREFERRED:**
```c
/* This is a comment */
```

**ACCEPTABLE:**
```c
// This also works but is less idiomatic
```

### 6. Header Files

Plan 9 has its own header structure:

**WRONG (POSIX):**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

**CORRECT (Plan 9):**
```c
#include <u.h>      /* Universal header (types, macros) */
#include <libc.h>   /* Standard C library */
```

**For portability**, use conditional compilation:
```c
#ifdef PLAN9
    #include <u.h>
    #include <libc.h>
#else
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
#endif
```

### 7. String Functions

Plan 9 uses different string functions:

| POSIX        | Plan 9      | Notes                      |
|--------------|-------------|----------------------------|
| `sprintf`    | `sprint`    | No 'f' prefix              |
| `snprintf`   | `snprint`   | Returns chars written      |
| `strcpy`     | `strcpy`    | Same                       |
| `strlen`     | `strlen`    | Same                       |
| `strdup`     | `strdup`    | Same (use malloc/strcpy)   |

### 8. Memory Allocation

Plan 9 C provides malloc/free but also has:

```c
void *mallocz(ulong size, int clear);  /* Allocate and optionally clear */
void *realloc(void *ptr, ulong size);  /* Same as POSIX */
```

### 9. File I/O

Plan 9 uses different I/O:

**POSIX:**
```c
FILE *f = fopen("file.txt", "r");
fprintf(f, "Hello %s\n", name);
fclose(f);
```

**Plan 9:**
```c
int fd = open("file.txt", OREAD);
fprint(fd, "Hello %s\n", name);
close(fd);

/* OR use Bio for buffered I/O */
#include <bio.h>
Biobuf *b = Bopen("file.txt", OREAD);
Bprint(b, "Hello %s\n", name);
Bterm(b);
```

---

## mkfile Build System

Plan 9 uses `mk` instead of `make`. Syntax is similar but with differences.

### Basic mkfile Structure

```mk
# mkfile for Kryon IR library

</$objtype/mkfile  # Include architecture-specific settings

LIB=libir.a

OFILES=\
    ir_core.$O\
    ir_logic.$O\
    ir_executor.$O\
    ir_builder.$O\

HFILES=\
    ir_core.h\
    ir_logic.h\

</sys/src/cmd/mklib  # Include standard library build rules

# Custom rules
test:V:
    echo "Running tests..."
    ./test_runner

clean:V:
    rm -f *.$O *.a
```

### mkfile Syntax Differences

| Feature              | make                | mk                  |
|----------------------|---------------------|---------------------|
| Target marker        | `target:`           | `target:`           |
| Virtual target       | `.PHONY: target`    | `target:V:`         |
| Variables            | `VAR = value`       | `VAR=value`         |
| Variable reference   | `$(VAR)`            | `$VAR`              |
| Automatic variables  | `$@`, `$<`, `$^`    | `$target`, `$prereq`|
| Pattern rules        | `%.o: %.c`          | `%.$O: %.c`         |
| Include              | `include file.mk`   | `<file.mk`          |

### Object File Extensions

Plan 9 uses architecture-specific object file extensions:

| Architecture | `$objtype` | Object extension `$O` |
|--------------|------------|-----------------------|
| x86 (32-bit) | `386`      | `8`                   |
| x86-64       | `amd64`    | `6`                   |
| ARM          | `arm`      | `5`                   |

Example: `ir_core.c` compiles to `ir_core.6` on amd64.

### Compilation Commands

```mk
# Compile C source to object
%.$O: %.c
    $CC $CFLAGS $stem.c

# Link objects into library
$LIB: $OFILES
    $AR rvc $LIB $OFILES

# Link into executable
program: $OFILES
    $LD -o $target $OFILES
```

### Running mk

```bash
mk           # Build default target
mk clean     # Run clean target
mk all       # Build 'all' target
mk -n        # Dry run (show commands without executing)
mk -a        # Force rebuild all
```

---

## libdraw Graphics API

libdraw is Plan 9's graphics library. All drawing happens on `Image` structures.

### Key Data Structures

```c
#include <draw.h>

/* Display connection */
typedef struct Display Display;

/* Image (screen, window, or off-screen buffer) */
typedef struct Image Image;

/* Point (x, y coordinates) */
typedef struct Point Point;
struct Point {
    int x;
    int y;
};

/* Rectangle (defined by two points) */
typedef struct Rectangle Rectangle;
struct Rectangle {
    Point min;
    Point max;
};

/* Font */
typedef struct Font Font;
```

### Initialization

```c
#include <u.h>
#include <libc.h>
#include <draw.h>

/* Initialize display and create window */
int main(int argc, char *argv[]) {
    if (initdraw(nil, nil, "My Window") < 0) {
        fprint(2, "initdraw failed: %r\n");
        exits("initdraw");
    }

    /* Global variables are now set:
     *   display - Display connection
     *   screen  - Screen Image
     *   font    - Default Font
     */

    /* ... drawing code ... */

    /* Cleanup */
    closedisplay(display);
    exits(nil);
}
```

### Drawing Primitives

```c
/* Fill rectangle with solid color */
Rectangle r = Rect(x0, y0, x1, y1);
draw(screen, r, display->black, nil, ZP);

/* Draw line */
Point p0 = Pt(x0, y0);
Point p1 = Pt(x1, y1);
line(screen, p0, p1, 0, 0, thickness, display->white, ZP);

/* Draw text */
Point p = Pt(x, y);
string(screen, p, display->black, ZP, font, "Hello World");

/* Draw arc (for rounded corners) */
Point center = Pt(cx, cy);
fillarc(screen, center, radius, radius, color_img, ZP, start_angle, angle_span);

/* Draw ellipse */
ellipse(screen, center, radiusX, radiusY, thickness, color_img, ZP);

/* Draw polygon */
Point pts[] = { Pt(x0,y0), Pt(x1,y1), Pt(x2,y2) };
poly(screen, pts, nelem(pts), 0, 0, thickness, color_img, ZP);
fillpoly(screen, pts, nelem(pts), ~0, color_img, ZP);
```

### Creating Color Images

```c
/* Allocate 1x1 replicated image for solid color */
/* Color format: 0xRRGGBBAA (32-bit RGBA) */
ulong rgba = (255 << 24) | (0 << 16) | (0 << 8) | 255;  /* Red */
Image *color_img = allocimage(display, Rect(0,0,1,1), RGBA32, 1, rgba);

/* Use the color */
draw(screen, screen->r, color_img, nil, ZP);

/* Free when done */
freeimage(color_img);
```

### Compositing

The `draw()` function signature:
```c
void draw(Image *dst, Rectangle r, Image *src, Image *mask, Point p);
```

- `dst`: Destination image (usually `screen`)
- `r`: Rectangle in destination to draw to
- `src`: Source image (color, texture, etc.)
- `mask`: Mask image for compositing (or `nil` for opaque)
- `p`: Point in source image to start from (usually `ZP` for zero point)

### Flushing

```c
/* Flush drawing commands to display */
flushimage(display, 1);  /* 1 = visible flush */
```

---

## Event Handling

Plan 9 uses the event library for input handling.

### Initialization

```c
#include <event.h>

/* Enable mouse and keyboard events */
einit(Emouse | Ekeyboard);
```

### Event Loop

```c
Event e;
ulong key;

while (running) {
    key = event(&e);  /* Blocking wait for event */

    switch (key) {
    case Emouse:
        /* Mouse event */
        if (e.mouse.buttons & 1) {
            /* Left button clicked at e.mouse.xy */
            handle_click(e.mouse.xy.x, e.mouse.xy.y);
        }
        break;

    case Ekeyboard:
        /* Keyboard event */
        if (e.kbdc == 'q') {
            running = 0;  /* Quit */
        }
        handle_key(e.kbdc);
        break;
    }
}
```

### Non-Blocking Event Check

```c
/* Check if events are available without blocking */
if (ecanread(Emouse | Ekeyboard)) {
    key = event(&e);
    /* ... handle event ... */
}
```

### Mouse Structure

```c
typedef struct Mouse Mouse;
struct Mouse {
    int buttons;   /* Button bit flags: 1=left, 2=middle, 4=right */
    Point xy;      /* Cursor position */
    ulong msec;    /* Timestamp */
};
```

### Window Resize Handling

```c
/* Check for resize */
if (eresized) {
    if (getwindow(display, Refnone) < 0) {
        fprint(2, "getwindow failed: %r\n");
    }
    /* screen pointer is now updated */
    eresized = 0;
}
```

---

## Font System

Plan 9 fonts are bitmap fonts with full UTF-8 support.

### Loading Fonts

```c
/* Open a Plan 9 font file */
Font *f = openfont(display, "/lib/font/bit/lucsans/unicode.13.font");
if (!f) {
    fprint(2, "openfont failed: %r\n");
    /* Fall back to default font */
    f = font;
}
```

### Text Measurement

```c
/* Measure string width */
int width = stringwidth(font, "Hello World");

/* Get font height */
int height = font->height;
```

### Text Rendering

```c
/* Draw text string */
Point p = Pt(x, y);
string(screen, p, display->black, ZP, font, "Hello World");

/* With background color */
stringbg(screen, p, display->black, ZP, font, "Hello", display->white, ZP);
```

### Font Paths

Standard Plan 9 fonts are in `/lib/font/bit/`:

```
/lib/font/bit/lucsans/unicode.8.font
/lib/font/bit/lucsans/unicode.10.font
/lib/font/bit/lucsans/unicode.13.font
/lib/font/bit/lucm/unicode.9.font         # Monospace
/lib/font/bit/pelm/unicode.8.font         # Proportional
```

---

## Common Issues and Solutions

### Issue: `undeclared identifier: inline`

**Solution:** Remove `inline` keyword or use static functions.

### Issue: `syntax error near for`

**Cause:** Variable declaration in for loop.

**Solution:**
```c
int i;
for (i = 0; i < count; i++) { }
```

### Issue: `unknown type: FILE`

**Cause:** Using POSIX stdio.h.

**Solution:** Use Plan 9 file descriptors or Bio:
```c
int fd = open("file.txt", OREAD);
/* OR */
#include <bio.h>
Biobuf *b = Bopen("file.txt", OREAD);
```

### Issue: `mk: cannot open file`

**Cause:** mkfile syntax error or missing include.

**Solution:** Check mkfile syntax, ensure `</$objtype/mkfile` is included.

### Issue: Linking errors with cJSON

**Cause:** cJSON uses POSIX functions not available on Plan 9.

**Solution:** Port cJSON or use Plan 9-native JSON parser (see Phase 14).

---

## Resources

### Official Documentation

- **Plan 9 Manual:** http://man.cat-v.org/plan_9/
- **libdraw:** http://man.cat-v.org/plan_9/2/draw
- **event:** http://man.cat-v.org/plan_9/2/event
- **mk:** http://man.cat-v.org/plan_9/1/mk

### plan9port Documentation

- **plan9port GitHub:** https://github.com/9fans/plan9port
- **plan9port man pages:** `man 3 draw` (on system with plan9port installed)

### 9front Resources

- **9front Website:** http://9front.org/
- **9front Wiki:** http://wiki.9front.org/
- **9front Manual:** http://man.9front.org/

### Tutorials and Examples

- **Draw Examples:** http://doc.cat-v.org/plan_9/programming/
- **Graphics Programming Guide:** http://doc.cat-v.org/plan_9/4th_edition/papers/libdraw

### Community

- **9fans Mailing List:** https://9fans.topicbox.com/groups/9fans
- **#cat-v on Libera.Chat:** IRC channel for Plan 9 discussion

---

## Next Steps

1. Complete C code compatibility audit (Phase 2)
2. Create mkfiles for Kryon build system (Phase 4)
3. Implement Plan 9 backend using libdraw (Phases 5-10)
4. Test on plan9port (Phase 11)
5. Test on native 9front (Phase 13)

For implementation details, see the main implementation plan.
