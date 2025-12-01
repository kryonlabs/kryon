# Makefile for Kryon C Examples

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -g
INCLUDES = -I../../ir -I../../renderers/sdl3

# SDL3 configuration
SDL3_CFLAGS = $(shell pkg-config --cflags sdl3 2>/dev/null || echo "-I/usr/include/SDL3")
SDL3_LIBS = $(shell pkg-config --libs sdl3 2>/dev/null || echo "-lSDL3")

# Kryon libraries
IR_LIB = ../../build/libkryon_ir.a
SPRITE_BATCH_OBJ = ../../build/sdl3_sdl3_sprite_batch.o
SPRITE_BACKEND_OBJ = ../../build/sdl3_sdl3_sprite_backend.o
AUDIO_BACKEND_OBJ = ../../build/sdl3_audio.o

# STB Image needs to be available
CFLAGS += -DSTB_IMAGE_IMPLEMENTATION

# Output directory
BUILD_DIR = ../../build

# Targets
all: sprite_test particle_test sprite_stress_test asset_test audio_test physics_test entity_test

sprite_test: sprite_test.c $(IR_LIB) $(SPRITE_BATCH_OBJ) $(SPRITE_BACKEND_OBJ)
	@echo "Building sprite_test..."
	$(CC) $(CFLAGS) $(INCLUDES) $(SDL3_CFLAGS) \
		sprite_test.c \
		$(SPRITE_BATCH_OBJ) \
		$(SPRITE_BACKEND_OBJ) \
		$(IR_LIB) \
		$(SDL3_LIBS) -lm \
		-o $(BUILD_DIR)/sprite_test
	@echo "✓ Built $(BUILD_DIR)/sprite_test"

particle_test: particle_test.c $(IR_LIB)
	@echo "Building particle_test..."
	$(CC) $(CFLAGS) $(INCLUDES) $(SDL3_CFLAGS) \
		particle_test.c \
		$(IR_LIB) \
		$(SDL3_LIBS) -lm \
		-o $(BUILD_DIR)/particle_test
	@echo "✓ Built $(BUILD_DIR)/particle_test"

sprite_stress_test: sprite_stress_test.c $(IR_LIB) $(SPRITE_BATCH_OBJ) $(SPRITE_BACKEND_OBJ)
	@echo "Building sprite_stress_test..."
	$(CC) $(CFLAGS) $(INCLUDES) $(SDL3_CFLAGS) \
		sprite_stress_test.c \
		$(SPRITE_BATCH_OBJ) \
		$(SPRITE_BACKEND_OBJ) \
		$(IR_LIB) \
		$(SDL3_LIBS) -lm \
		-o $(BUILD_DIR)/sprite_stress_test
	@echo "✓ Built $(BUILD_DIR)/sprite_stress_test"

asset_test: asset_test.c $(IR_LIB) $(SPRITE_BATCH_OBJ) $(SPRITE_BACKEND_OBJ)
	@echo "Building asset_test..."
	$(CC) $(CFLAGS) $(INCLUDES) $(SDL3_CFLAGS) \
		asset_test.c \
		$(SPRITE_BATCH_OBJ) \
		$(SPRITE_BACKEND_OBJ) \
		$(IR_LIB) \
		$(SDL3_LIBS) -lm \
		-o $(BUILD_DIR)/asset_test
	@echo "✓ Built $(BUILD_DIR)/asset_test"

audio_test: audio_test.c $(IR_LIB) $(AUDIO_BACKEND_OBJ)
	@echo "Building audio_test..."
	$(CC) $(CFLAGS) $(INCLUDES) $(SDL3_CFLAGS) \
		audio_test.c \
		$(AUDIO_BACKEND_OBJ) \
		$(IR_LIB) \
		$(SDL3_LIBS) -lm \
		-o $(BUILD_DIR)/audio_test
	@echo "✓ Built $(BUILD_DIR)/audio_test"

physics_test: physics_test.c $(IR_LIB)
	@echo "Building physics_test..."
	$(CC) $(CFLAGS) $(INCLUDES) $(SDL3_CFLAGS) \
		physics_test.c \
		$(IR_LIB) \
		$(SDL3_LIBS) -lm \
		-o $(BUILD_DIR)/physics_test
	@echo "✓ Built $(BUILD_DIR)/physics_test"

entity_test: entity_test.c $(IR_LIB)
	@echo "Building entity_test..."
	$(CC) $(CFLAGS) $(INCLUDES) \
		entity_test.c \
		$(IR_LIB) \
		-lm \
		-o $(BUILD_DIR)/entity_test
	@echo "✓ Built $(BUILD_DIR)/entity_test"

clean:
	rm -f $(BUILD_DIR)/sprite_test $(BUILD_DIR)/particle_test $(BUILD_DIR)/sprite_stress_test $(BUILD_DIR)/asset_test $(BUILD_DIR)/audio_test $(BUILD_DIR)/physics_test $(BUILD_DIR)/entity_test

run-sprite: sprite_test
	@echo "Running sprite test..."
	LD_LIBRARY_PATH=$(BUILD_DIR):$$LD_LIBRARY_PATH $(BUILD_DIR)/sprite_test

run-particles: particle_test
	@echo "Running particle test..."
	LD_LIBRARY_PATH=$(BUILD_DIR):$$LD_LIBRARY_PATH $(BUILD_DIR)/particle_test

run: run-particles

.PHONY: all clean run run-sprite run-particles
