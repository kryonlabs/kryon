#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#include "../../ir/ir_core.h"
#include "../../ir/ir_builder.h"
#include "../../ir/ir_serialization.h"
#include "ir_desktop_renderer.h"
#include "canvas_sdl3.h"

// Platform-specific includes (conditional compilation)
#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#endif

// Desktop IR Renderer Context
struct DesktopIRRenderer {
    DesktopRendererConfig config;
    bool initialized;
    bool running;

    // SDL3-specific fields (only available when compiled with SDL3)
#ifdef ENABLE_SDL3
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* default_font;
    SDL_Cursor* cursor_default;
    SDL_Cursor* cursor_hand;
    SDL_Cursor* current_cursor;
#endif

    // Event handling
    DesktopEventCallback event_callback;
    void* event_user_data;

    // Performance tracking
    uint64_t frame_count;
    double last_frame_time;
    double fps;

    // Component rendering cache
    IRComponent* last_root;
    bool needs_relayout;
};

// Color conversion utilities (only when SDL3 is available)
static float ir_dimension_to_pixels(IRDimension dimension, float container_size) {
    switch (dimension.type) {
        case IR_DIMENSION_PX:
            return dimension.value;
        case IR_DIMENSION_PERCENT:
            return (dimension.value / 100.0f) * container_size;
        case IR_DIMENSION_AUTO:
            return 0.0f; // Let layout engine determine
        case IR_DIMENSION_FLEX:
            return dimension.value; // Treat flex as pixels for simplicity
        default:
            return 0.0f;
    }
}

#ifdef ENABLE_SDL3
static SDL_Color ir_color_to_sdl(IRColor color) {
    return (SDL_Color){
        .r = color.r,
        .g = color.g,
        .b = color.b,
        .a = color.a
    };
}
#endif

// Layout calculation helpers
typedef struct LayoutRect {
    float x, y, width, height;
} LayoutRect;

static LayoutRect calculate_component_layout(IRComponent* component, LayoutRect parent_rect) {
    LayoutRect rect = {0};

    if (!component) return rect;

    // Default to parent bounds for container
    rect.x = parent_rect.x;
    rect.y = parent_rect.y;
    rect.width = parent_rect.width;
    rect.height = parent_rect.height;

    // Apply component-specific dimensions
    if (component->style) {
        if (component->style->width.type != IR_DIMENSION_AUTO) {
            rect.width = ir_dimension_to_pixels(component->style->width, parent_rect.width);
        }
        if (component->style->height.type != IR_DIMENSION_AUTO) {
            rect.height = ir_dimension_to_pixels(component->style->height, parent_rect.height);
        }

        // Apply margins
        if (component->style) {
            if (component->style->margin.top > 0) rect.y += component->style->margin.top;
            if (component->style->margin.left > 0) rect.x += component->style->margin.left;
        }
    }

    return rect;
}

// Platform-specific rendering implementations
#ifdef ENABLE_SDL3
static bool render_component_sdl3(DesktopIRRenderer* renderer, IRComponent* component, LayoutRect rect) {
    if (!renderer || !component || !renderer->renderer) return false;

    // Cache rendered bounds for hit testing
    ir_set_rendered_bounds(component, rect.x, rect.y, rect.width, rect.height);

    SDL_FRect sdl_rect = {
        .x = rect.x,
        .y = rect.y,
        .w = rect.width,
        .h = rect.height
    };

    // Render based on component type
    switch (component->type) {
        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
            // Only render background if component has a style (otherwise transparent)
            if (component->style) {
                SDL_Color bg_color = ir_color_to_sdl(component->style->background);
                SDL_SetRenderDrawColor(renderer->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
                SDL_RenderFillRect(renderer->renderer, &sdl_rect);
            }
            break;

        case IR_COMPONENT_CENTER:
            // Center is a layout-only component, completely transparent - don't render background
            break;

        case IR_COMPONENT_BUTTON:
            // Set button background color
            if (component->style) {
                SDL_Color bg_color = ir_color_to_sdl(component->style->background);
                SDL_SetRenderDrawColor(renderer->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
            } else {
                SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 200, 255); // Default blue
            }
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);

            // Draw button border
            SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
            SDL_RenderRect(renderer->renderer, &sdl_rect);

            // Render button text if present
            if (component->text_content && renderer->default_font) {
                SDL_Color text_color = component->style ?
                    ir_color_to_sdl(component->style->font.color) :
                    (SDL_Color){0, 0, 0, 255};

                SDL_Surface* surface = TTF_RenderText_Blended(renderer->default_font,
                                                              component->text_content,
                                                              strlen(component->text_content),
                                                              text_color);
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                    if (texture) {
                        // Center text in button
                        SDL_FRect text_rect = {
                            .x = rect.x + (rect.width - surface->w) / 2,
                            .y = rect.y + (rect.height - surface->h) / 2,
                            .w = (float)surface->w,
                            .h = (float)surface->h
                        };
                        SDL_RenderTexture(renderer->renderer, texture, NULL, &text_rect);
                        SDL_DestroyTexture(texture);
                    }
                    SDL_DestroySurface(surface);
                }
            }
            break;

        case IR_COMPONENT_TEXT:
            if (component->text_content && renderer->default_font) {
                SDL_Color text_color = component->style ?
                    ir_color_to_sdl(component->style->font.color) :
                    (SDL_Color){0, 0, 0, 255};

                SDL_Surface* surface = TTF_RenderText_Blended(renderer->default_font,
                                                              component->text_content,
                                                              strlen(component->text_content),
                                                              text_color);
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                    if (texture) {
                        SDL_FRect text_rect = {
                            .x = rect.x,
                            .y = rect.y + (rect.height - surface->h) / 2.0f,
                            .w = (float)surface->w,
                            .h = (float)surface->h
                        };
                        SDL_RenderTexture(renderer->renderer, texture, NULL, &text_rect);
                        SDL_DestroyTexture(texture);
                    }
                    SDL_DestroySurface(surface);
                }
            }
            break;

        case IR_COMPONENT_INPUT:
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);
            SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
            SDL_RenderRect(renderer->renderer, &sdl_rect);
            break;

        case IR_COMPONENT_CHECKBOX:
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);
            SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
            SDL_RenderRect(renderer->renderer, &sdl_rect);
            break;

        case IR_COMPONENT_CANVAS:
            // Set the SDL renderer context for canvas drawing
            canvas_sdl3_set_renderer(renderer->renderer);

            // Call the Nim canvas callback (declared in runtime.nim as exportc)
            extern void nimCanvasBridge(uint32_t componentId);
            nimCanvasBridge(component->id);

            // Clear renderer context
            canvas_sdl3_set_renderer(NULL);
            break;

        default:
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);
            break;
    }

    // Render children
    LayoutRect child_rect = rect;
    if (component->style) {
        child_rect.x += component->style->padding.left;
        child_rect.y += component->style->padding.top;
        child_rect.width -= (component->style->padding.left + component->style->padding.right);
        child_rect.height -= (component->style->padding.top + component->style->padding.bottom);
    }

    for (uint32_t i = 0; i < component->child_count; i++) {
        LayoutRect child_layout = calculate_component_layout(component->children[i], child_rect);

        // Special handling for Center component - center the child
        if (component->type == IR_COMPONENT_CENTER) {
            child_layout.x = child_rect.x + (child_rect.width - child_layout.width) / 2;
            child_layout.y = child_rect.y + (child_rect.height - child_layout.height) / 2;
        }

        render_component_sdl3(renderer, component->children[i], child_layout);

        // Simple vertical stacking for demo
        if (component->type == IR_COMPONENT_COLUMN) {
            child_rect.y += child_layout.height + 10; // 10px spacing
        }
    }

    return true;
}
#endif

// Event handling
#ifdef ENABLE_SDL3
static void handle_sdl3_events(DesktopIRRenderer* renderer) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        DesktopEvent desktop_event = {0};

#ifdef ENABLE_SDL3
        desktop_event.timestamp = SDL_GetTicks();
#else
        desktop_event.timestamp = 0;
#endif

        switch (event.type) {
            case SDL_EVENT_QUIT:
                desktop_event.type = DESKTOP_EVENT_QUIT;
                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                renderer->running = false;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                desktop_event.type = DESKTOP_EVENT_MOUSE_CLICK;
                desktop_event.data.mouse.x = event.button.x;
                desktop_event.data.mouse.y = event.button.y;
                desktop_event.data.mouse.button = event.button.button;

                // Dispatch click event to IR component at mouse position
                if (renderer->last_root) {
                    IRComponent* clicked = ir_find_component_at_point(
                        renderer->last_root,
                        (float)event.button.x,
                        (float)event.button.y
                    );

                    if (clicked) {
                        // Find and trigger IR_EVENT_CLICK handler
                        IREvent* ir_event = ir_find_event(clicked, IR_EVENT_CLICK);
                        if (ir_event && ir_event->logic_id) {
                            printf("🖱️  Click on component ID %u (logic: %s)\n",
                                   clicked->id, ir_event->logic_id);

                            // Check if this is a Nim button handler
                            if (strncmp(ir_event->logic_id, "nim_button_", 11) == 0) {
                                // Declare external Nim callback
                                extern void nimButtonBridge(uint32_t componentId);
                                // Invoke Nim handler
                                nimButtonBridge(clicked->id);
                            }
                        }
                    }
                }

                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                desktop_event.type = DESKTOP_EVENT_MOUSE_MOVE;
                desktop_event.data.mouse.x = event.motion.x;
                desktop_event.data.mouse.y = event.motion.y;

                // Update cursor based on what component is under the mouse
                if (renderer->last_root) {
                    IRComponent* hovered = ir_find_component_at_point(
                        renderer->last_root,
                        (float)event.motion.x,
                        (float)event.motion.y
                    );

                    // Set cursor to hand for clickable components
                    SDL_Cursor* desired_cursor;
                    if (hovered && (hovered->type == IR_COMPONENT_BUTTON ||
                                    hovered->type == IR_COMPONENT_INPUT ||
                                    hovered->type == IR_COMPONENT_CHECKBOX)) {
                        desired_cursor = renderer->cursor_hand;
                    } else {
                        desired_cursor = renderer->cursor_default;
                    }

                    // Only update if cursor changed
                    if (desired_cursor != renderer->current_cursor) {
                        SDL_SetCursor(desired_cursor);
                        renderer->current_cursor = desired_cursor;
                    }
                }

                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            case SDL_EVENT_KEY_DOWN:
                // ESC key closes window by default
                if (event.key.key == SDLK_ESCAPE) {
                    renderer->running = false;
                    break;
                }

                desktop_event.type = DESKTOP_EVENT_KEY_PRESS;
                desktop_event.data.keyboard.key_code = event.key.key;
                desktop_event.data.keyboard.shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
                desktop_event.data.keyboard.ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
                desktop_event.data.keyboard.alt = (event.key.mod & SDL_KMOD_ALT) != 0;
                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                desktop_event.type = DESKTOP_EVENT_WINDOW_RESIZE;
                desktop_event.data.resize.width = event.window.data1;
                desktop_event.data.resize.height = event.window.data2;
                renderer->needs_relayout = true;
                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            default:
                break;
        }
    }
}
#endif

// Public API implementation
DesktopIRRenderer* desktop_ir_renderer_create(const DesktopRendererConfig* config) {
    if (!config) return NULL;

    DesktopIRRenderer* renderer = malloc(sizeof(DesktopIRRenderer));
    if (!renderer) return NULL;

    memset(renderer, 0, sizeof(DesktopIRRenderer));
    renderer->config = *config;
    renderer->last_frame_time = 0; // Will be set when SDL3 initializes

    printf("🖥️ Creating desktop IR renderer with backend type: %d\n", config->backend_type);
    return renderer;
}

void desktop_ir_renderer_destroy(DesktopIRRenderer* renderer) {
    if (!renderer) return;

#ifdef ENABLE_SDL3
    if (renderer->cursor_hand) {
        SDL_DestroyCursor(renderer->cursor_hand);
    }
    if (renderer->cursor_default) {
        SDL_DestroyCursor(renderer->cursor_default);
    }
    if (renderer->default_font) {
        TTF_CloseFont(renderer->default_font);
    }
    if (renderer->renderer) {
        SDL_DestroyRenderer(renderer->renderer);
    }
    if (renderer->window) {
        SDL_DestroyWindow(renderer->window);
    }
#endif

    free(renderer);
}

bool desktop_ir_renderer_initialize(DesktopIRRenderer* renderer) {
    if (!renderer) return false;

    printf("🚀 Initializing desktop renderer...\n");

#ifdef ENABLE_SDL3
    if (renderer->config.backend_type != DESKTOP_BACKEND_SDL3) {
        printf("❌ Only SDL3 backend is currently implemented\n");
        return false;
    }

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("❌ Failed to initialize SDL3: %s\n", SDL_GetError());
        return false;
    }

    // Initialize SDL_ttf
    if (!TTF_Init()) {
        printf("❌ Failed to initialize SDL_ttf: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    // Create window
    Uint32 window_flags = SDL_WINDOW_OPENGL;
    if (renderer->config.resizable) window_flags |= SDL_WINDOW_RESIZABLE;
    if (renderer->config.fullscreen) window_flags |= SDL_WINDOW_FULLSCREEN;

    renderer->window = SDL_CreateWindow(
        renderer->config.window_title,
        renderer->config.window_width,
        renderer->config.window_height,
        window_flags
    );

    if (!renderer->window) {
        printf("❌ Failed to create window: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Create renderer
    renderer->renderer = SDL_CreateRenderer(renderer->window, NULL);
    if (!renderer->renderer) {
        printf("❌ Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(renderer->window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Set vsync
    if (renderer->config.vsync_enabled) {
        SDL_SetRenderVSync(renderer->renderer, 1);
    }

    // Load default font using fontconfig to find system fonts
    FILE* fc = popen("fc-match sans:style=Regular -f \"%{file}\"", "r");
    char fontconfig_path[512] = {0};
    if (fc) {
        if (fgets(fontconfig_path, sizeof(fontconfig_path), fc)) {
            // Remove newline
            size_t len = strlen(fontconfig_path);
            if (len > 0 && fontconfig_path[len-1] == '\n') {
                fontconfig_path[len-1] = '\0';
            }
            renderer->default_font = TTF_OpenFont(fontconfig_path, 16);
            if (renderer->default_font) {
                printf("✅ Loaded font via fontconfig: %s\n", fontconfig_path);
            }
        }
        pclose(fc);
    }

    // Fallback to hardcoded paths if fontconfig fails
    if (!renderer->default_font) {
        const char* font_paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            "/System/Library/Fonts/Arial.ttf",
            "C:/Windows/Fonts/arial.ttf",
            NULL
        };

        for (int i = 0; font_paths[i]; i++) {
            renderer->default_font = TTF_OpenFont(font_paths[i], 16);
            if (renderer->default_font) {
                printf("✅ Loaded font: %s\n", font_paths[i]);
                break;
            }
        }
    }

    if (!renderer->default_font) {
        printf("⚠️ Could not load any system font, text rendering will be limited\n");
    }

    // Initialize cursors
    renderer->cursor_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    renderer->cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    renderer->current_cursor = renderer->cursor_default;
    SDL_SetCursor(renderer->cursor_default);

    renderer->initialized = true;
    printf("✅ Desktop renderer initialized successfully\n");
    return true;

#else
    printf("❌ SDL3 support not enabled at compile time\n");
    return false;
#endif
}

bool desktop_ir_renderer_render_frame(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root || !renderer->initialized) return false;

#ifdef ENABLE_SDL3
    // Clear screen with root component's background color if available
    if (root->style) {
        SDL_Color bg = ir_color_to_sdl(root->style->background);
        SDL_SetRenderDrawColor(renderer->renderer, bg.r, bg.g, bg.b, bg.a);
    } else {
        SDL_SetRenderDrawColor(renderer->renderer, 240, 240, 240, 255);
    }
    SDL_RenderClear(renderer->renderer);

    // Get window size
    int window_width = renderer->config.window_width;
    int window_height = renderer->config.window_height;
#ifdef ENABLE_SDL3
    SDL_GetWindowSize(renderer->window, &window_width, &window_height);
#endif

    // Calculate root layout
    LayoutRect root_rect = {
        .x = 0, .y = 0,
        .width = (float)window_width,
        .height = (float)window_height
    };

    // Render component tree
    if (!render_component_sdl3(renderer, root, root_rect)) {
        printf("❌ Failed to render component\n");
        return false;
    }

    // Present frame
    SDL_RenderPresent(renderer->renderer);

    // Update performance stats
    renderer->frame_count++;
    double current_time = SDL_GetTicks() / 1000.0;
    double delta_time = current_time - renderer->last_frame_time;
    if (delta_time > 0.1) { // Update FPS every 100ms
        renderer->fps = renderer->frame_count / delta_time;
        renderer->frame_count = 0;
        renderer->last_frame_time = current_time;
    }

    return true;

#else
    return false;
#endif
}

bool desktop_ir_renderer_run_main_loop(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    if (!renderer->initialized && !desktop_ir_renderer_initialize(renderer)) {
        return false;
    }

    printf("🎮 Starting desktop main loop...\n");
    renderer->running = true;
    renderer->last_root = root;

#ifdef ENABLE_SDL3
    while (renderer->running) {
        handle_sdl3_events(renderer);

        if (!desktop_ir_renderer_render_frame(renderer, root)) {
            printf("❌ Frame rendering failed\n");
            break;
        }

        // Frame rate limiting
        if (renderer->config.target_fps > 0) {
#ifdef ENABLE_SDL3
            SDL_Delay(1000 / renderer->config.target_fps);
#else
            // Simple sleep fallback when SDL3 is not available
            struct timespec ts = {0, (1000000000L / renderer->config.target_fps)};
            nanosleep(&ts, NULL);
#endif
        }
    }
#endif

    printf("🛑 Desktop main loop ended\n");
    return true;
}

void desktop_ir_renderer_stop(DesktopIRRenderer* renderer) {
    if (renderer) {
        renderer->running = false;
    }
}

void desktop_ir_renderer_set_event_callback(DesktopIRRenderer* renderer,
                                          DesktopEventCallback callback,
                                          void* user_data) {
    if (renderer) {
        renderer->event_callback = callback;
        renderer->event_user_data = user_data;
    }
}

bool desktop_ir_renderer_load_font(DesktopIRRenderer* renderer, const char* font_path, float size) {
#ifdef ENABLE_SDL3
    if (!renderer || !font_path) return false;

    if (renderer->default_font) {
        TTF_CloseFont(renderer->default_font);
    }

    renderer->default_font = TTF_OpenFont(font_path, (int)size);
    return renderer->default_font != NULL;

#else
    return false;
#endif
}

bool desktop_ir_renderer_load_image(DesktopIRRenderer* renderer, const char* image_path) {
    // Image loading would be implemented here
    printf("📷 Image loading not yet implemented: %s\n", image_path);
    return false;
}

void desktop_ir_renderer_clear_resources(DesktopIRRenderer* renderer) {
    printf("🧹 Clearing desktop renderer resources\n");
    // Resource cleanup would be implemented here
}

bool desktop_ir_renderer_validate_ir(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    printf("🔍 Validating IR for desktop rendering...\n");
    return ir_validate_component(root);
}

void desktop_ir_renderer_print_tree_info(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    printf("🌳 Desktop IR Component Tree Information:\n");
    ir_print_component_info(root, 0);
}

void desktop_ir_renderer_print_performance_stats(DesktopIRRenderer* renderer) {
    if (!renderer) return;

    printf("📊 Desktop Renderer Performance Stats:\n");
    printf("   FPS: %.1f\n", renderer->fps);
    printf("   Backend type: %d\n", renderer->config.backend_type);
    printf("   Window size: %dx%d\n", renderer->config.window_width, renderer->config.window_height);
    printf("   Running: %s\n", renderer->running ? "Yes" : "No");
}

DesktopRendererConfig desktop_renderer_config_default(void) {
    DesktopRendererConfig config = {0};
    config.backend_type = DESKTOP_BACKEND_SDL3;
    config.window_width = 800;
    config.window_height = 600;
    config.window_title = "Kryon Desktop Application";
    config.resizable = true;
    config.fullscreen = false;
    config.vsync_enabled = true;
    config.target_fps = 60;
    return config;
}

DesktopRendererConfig desktop_renderer_config_sdl3(int width, int height, const char* title) {
    DesktopRendererConfig config = desktop_renderer_config_default();
    config.window_width = width;
    config.window_height = height;
    config.window_title = title ? title : "Kryon Desktop Application";
    return config;
}

bool desktop_render_ir_component(IRComponent* root, const DesktopRendererConfig* config) {
    DesktopRendererConfig default_config = desktop_renderer_config_default();
    if (!config) config = &default_config;

    DesktopIRRenderer* renderer = desktop_ir_renderer_create(config);
    if (!renderer) return false;

    bool success = desktop_ir_renderer_run_main_loop(renderer, root);
    desktop_ir_renderer_destroy(renderer);
    return success;
}