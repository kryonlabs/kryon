/**
 * Plan 9 Backend - Core Renderer Implementation
 *
 * Implements the DesktopRendererOps interface for Plan 9/9front
 * Uses libdraw for graphics rendering
 */

#include "plan9_renderer.h"
#include "plan9_internal.h"
#include "../desktop/abstract/desktop_events.h"
#include "../../ir/ir_log.h"

/* Forward declarations of backend operations */
static int plan9_initialize(DesktopIRRenderer* renderer);
static void plan9_shutdown(DesktopIRRenderer* renderer);
static void plan9_poll_events(DesktopIRRenderer* renderer);
static int plan9_begin_frame(DesktopIRRenderer* renderer);
static int plan9_render_component(DesktopIRRenderer* renderer, IRComponent* root);
static void plan9_end_frame(DesktopIRRenderer* renderer);

/* External font and effects operations (defined in other files) */
extern DesktopFontOps g_plan9_font_ops;
extern DesktopEffectsOps g_plan9_effects_ops;

/* External text measurement callback (from plan9_layout.c) */
extern void plan9_text_measure_callback(const char* text, const char* font_name,
                                        int font_size, float* out_width,
                                        float* out_height, void* user_data);

/* Global operations table */
static DesktopRendererOps g_plan9_ops;

/* ============================================================================
 * Color Cache Helper
 * ============================================================================ */

Image*
plan9_get_color_image(Plan9RendererData* data, ulong rgba)
{
    Image* img;
    int i;

    if (!data || !data->display)
        return nil;

    /* Check cache first */
    for (i = 0; i < data->color_cache_count; i++) {
        if (data->color_cache[i].rgba == rgba) {
            return data->color_cache[i].image;
        }
    }

    /* Create new 1x1 replicated image */
    img = allocimage(data->display, Rect(0, 0, 1, 1), RGBA32, 1, rgba);
    if (!img) {
        fprint(2, "plan9: allocimage failed: %r\n");
        return data->display->black;  /* Fallback to black */
    }

    /* Add to cache if space available */
    if (data->color_cache_count < PLAN9_MAX_COLOR_CACHE) {
        data->color_cache[data->color_cache_count].rgba = rgba;
        data->color_cache[data->color_cache_count].image = img;
        data->color_cache_count++;
    }

    return img;
}

void
plan9_free_color_cache(Plan9RendererData* data)
{
    int i;

    if (!data)
        return;

    for (i = 0; i < data->color_cache_count; i++) {
        if (data->color_cache[i].image) {
            freeimage(data->color_cache[i].image);
            data->color_cache[i].image = nil;
        }
    }

    data->color_cache_count = 0;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

static int
plan9_initialize(DesktopIRRenderer* renderer)
{
    Plan9RendererData* data;

    if (!renderer)
        return 0;

    /* Allocate backend data */
    data = mallocz(sizeof(Plan9RendererData), 1);
    if (!data) {
        fprint(2, "plan9: failed to allocate renderer data\n");
        return 0;
    }

    /* Initialize display connection */
    if (initdraw(nil, nil, "Kryon") < 0) {
        fprint(2, "plan9: initdraw failed: %r\n");
        free(data);
        return 0;
    }

    /* Store global variables from libdraw */
    data->display = display;
    data->screen = screen;
    data->default_font = font;

    /* Initialize state */
    data->running = 1;
    data->needs_redraw = 1;
    data->clip_rect = data->screen->r;
    data->color_cache_count = 0;
    data->font_cache_count = 0;
    data->frame_count = 0;
    data->last_frame_time = nsec();

    /* Enable events */
    einit(Emouse | Ekeyboard);

    /* Store in renderer */
    renderer->ops->backend_data = data;

    IR_LOG_INFO("PLAN9", "Backend initialized successfully");
    return 1;
}

static void
plan9_shutdown(DesktopIRRenderer* renderer)
{
    Plan9RendererData* data;

    if (!renderer || !renderer->ops)
        return;

    data = (Plan9RendererData*)renderer->ops->backend_data;
    if (!data)
        return;

    IR_LOG_INFO("PLAN9", "Shutting down backend");

    /* Free cached resources */
    plan9_free_color_cache(data);

    /* Note: display, screen, and font are freed by closedisplay() */
    if (data->display)
        closedisplay(data->display);

    /* Free backend data */
    free(data);
    renderer->ops->backend_data = nil;
}

/* ============================================================================
 * Frame Rendering Functions
 * ============================================================================ */

static int
plan9_begin_frame(DesktopIRRenderer* renderer)
{
    Plan9RendererData* data;
    Rectangle r;

    if (!renderer || !renderer->ops)
        return 0;

    data = (Plan9RendererData*)renderer->ops->backend_data;
    if (!data || !data->screen)
        return 0;

    /* Clear screen to white */
    r = data->screen->r;
    draw(data->screen, r, data->display->white, nil, ZP);

    return 1;
}

static int
plan9_render_component(DesktopIRRenderer* renderer, IRComponent* root)
{
    Plan9RendererData* data;
    /* TODO: Generate and execute command buffer */

    if (!renderer || !root)
        return 0;

    data = (Plan9RendererData*)renderer->ops->backend_data;
    if (!data)
        return 0;

    /* For now, this is a stub - full implementation in Phase 7 */
    IR_LOG_DEBUG("PLAN9", "render_component called (stub)");

    return 1;
}

static void
plan9_end_frame(DesktopIRRenderer* renderer)
{
    Plan9RendererData* data;

    if (!renderer || !renderer->ops)
        return;

    data = (Plan9RendererData*)renderer->ops->backend_data;
    if (!data || !data->display)
        return;

    /* Flush drawing to display */
    flushimage(data->display, 1);

    /* Update performance counters */
    data->frame_count++;
    data->last_frame_time = nsec();
}

/* ============================================================================
 * Operations Table Registration
 * ============================================================================ */

DesktopRendererOps*
desktop_plan9_get_ops(void)
{
    /* Initialize operations table */
    g_plan9_ops.initialize = plan9_initialize;
    g_plan9_ops.shutdown = plan9_shutdown;
    g_plan9_ops.poll_events = plan9_poll_events;
    g_plan9_ops.begin_frame = plan9_begin_frame;
    g_plan9_ops.render_component = plan9_render_component;
    g_plan9_ops.end_frame = plan9_end_frame;
    g_plan9_ops.font_ops = &g_plan9_font_ops;
    g_plan9_ops.effects_ops = &g_plan9_effects_ops;
    g_plan9_ops.text_measure = plan9_text_measure_callback;
    g_plan9_ops.backend_data = nil;

    return &g_plan9_ops;
}

void
plan9_backend_register(void)
{
    DesktopRendererOps* ops;

    ops = desktop_plan9_get_ops();
    /* TODO: Register with desktop platform backend registry */
    /* For now, this is a placeholder */
    IR_LOG_INFO("PLAN9", "Backend registered");
}
