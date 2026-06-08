# Flint UI Library Makefile

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -O2 -Iinclude -I../inbe/vendor/raylib/src
ARFLAGS = rcs

# Source files
SRCS = src/flint_color.c \
       src/flint_scaling.c \
       src/flint_layout.c \
       src/flint_icons.c

# Object files
OBJS = $(SRCS:.c=.o)

# Output library
LIB = libflint.a

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIB)
