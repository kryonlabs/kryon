# Flint Examples Makefile

CC ?= gcc
CFLAGS_BASE ?= -Wall -Wextra -O2
CFLAGS = $(CFLAGS_BASE) -I../include
LDFLAGS = -L.. -lflint

# Raylib - use system library from nix-shell
# Check if we're in nix-shell and use its flags
ifneq ($(NIX_CFLAGS_COMPILE),)
    CFLAGS += $(NIX_CFLAGS_COMPILE)
endif

LDFLAGS += -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

EXAMPLES = 01_file_dialog 02_buttons 03_theme

.PHONY: all clean run list

all: ../libflint.a $(EXAMPLES)

../libflint.a:
	@$(MAKE) -C ..

%: %.c
	@echo "Building $@..."
	@$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

clean:
	rm -f $(EXAMPLES)

list:
	@echo "Flint Examples:"
	@for i in $(EXAMPLES); do \
		echo "  $$i"; \
		done

run:
	@echo "Flint Examples"
	@echo "================"
	@echo ""
	@echo "Available examples:"
	@echo "  1) file_dialog"
	@echo "  2) buttons"
	@echo "  3) theme"
	@echo ""
	@read -p "Enter example number: " num; \
	case "$$num" in \
		1) example="01_file_dialog";; \
		2) example="02_buttons";; \
		3) example="03_theme";; \
		*) example=;; \
	esac; \
	if [ -n "$$example" ]; then \
		$(MAKE) $$example 2>/dev/null; \
		if [ -f "$$example" ]; then \
			echo ""; \
			echo "Running $$example..."; \
			./$$example; \
		else \
			echo "Failed to build $$example"; \
		fi; \
	else \
		echo "Invalid example number"; \
	fi
