/**
 * Plan 9 Backend - Core Renderer Implementation
 *
 * Implements the DesktopRendererOps interface for Plan 9/9front
 * Uses libdraw for graphics rendering
 */

#include "plan9_renderer.h"
#include "plan9_internal.h"
#include "../desktop/abstract/desktop_events.h"
#include "../desktop/desktop_internal.h"
#include "../desktop/ir_to_commands.h"
#include "../../core/include/kryon.h"
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
    kryon_cmd_buf_t cmd_buf;
    LayoutRect bounds;

    if (!renderer || !root)
        return 0;

    data = (Plan9RendererData*)renderer->ops->backend_data;
    if (!data)
        return 0;

    /* Generate command buffer from IR tree */
    kryon_cmd_buf_init(&cmd_buf);

    bounds.x = 0;
    bounds.y = 0;
    bounds.width = Dx(data->screen->r);
    bounds.height = Dy(data->screen->r);

    if (!ir_component_to_commands(root, &cmd_buf, &bounds, 1.0f, renderer, nil)) {
        kryon_cmd_buf_destroy(&cmd_buf);
        return 0;
    }

    /* Execute commands */
    plan9_execute_commands(data, &cmd_buf);

    kryon_cmd_buf_destroy(&cmd_buf);
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
 * Command Execution
 * ============================================================================ */

void
plan9_draw_rounded_rect(Plan9RendererData* data, int x, int y, int w, int h,
                       int radius, ulong color)
{
    Rectangle r;
    Image* color_img;
    Point corners[4];
    int i;

    if (!data || !data->screen)
        return;

    /* Clamp radius to max of half the smallest dimension */
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;

    color_img = plan9_get_color_image(data, color);

    /* If radius is 0, just draw regular rectangle */
    if (radius == 0) {
        r = Rect(x, y, x + w, y + h);
        draw(data->screen, r, color_img, nil, ZP);
        return;
    }

    /* Draw four corner arcs */
    corners[0] = Pt(x + radius, y + radius);           /* Top-left */
    corners[1] = Pt(x + w - radius, y + radius);       /* Top-right */
    corners[2] = Pt(x + w - radius, y + h - radius);   /* Bottom-right */
    corners[3] = Pt(x + radius, y + h - radius);       /* Bottom-left */

    fillarc(data->screen, corners[0], radius, radius, color_img, ZP, 90, 90);
    fillarc(data->screen, corners[1], radius, radius, color_img, ZP, 0, 90);
    fillarc(data->screen, corners[2], radius, radius, color_img, ZP, 270, 90);
    fillarc(data->screen, corners[3], radius, radius, color_img, ZP, 180, 90);

    /* Fill center rectangles to complete the shape */
    r = Rect(x + radius, y, x + w - radius, y + h);
    draw(data->screen, r, color_img, nil, ZP);

    r = Rect(x, y + radius, x + radius, y + h - radius);
    draw(data->screen, r, color_img, nil, ZP);

    r = Rect(x + w - radius, y + radius, x + w, y + h - radius);
    draw(data->screen, r, color_img, nil, ZP);
}

void
plan9_execute_commands(Plan9RendererData* data, kryon_cmd_buf_t* cmd_buf)
{
    kryon_command_t* cmd;
    Image* color_img;
    Rectangle r;
    Point p0, p1;

    if (!data || !cmd_buf)
        return;

    kryon_cmd_buf_rewind(cmd_buf);

    while ((cmd = kryon_cmd_buf_get_next(cmd_buf)) != nil) {
        switch (cmd->type) {
        case KRYON_CMD_DRAW_RECT:
            r = Rect(cmd->data.draw_rect.x,
                    cmd->data.draw_rect.y,
                    cmd->data.draw_rect.x + cmd->data.draw_rect.w,
                    cmd->data.draw_rect.y + cmd->data.draw_rect.h);
            color_img = plan9_get_color_image(data, cmd->data.draw_rect.color);
            draw(data->screen, r, color_img, nil, ZP);
            break;

        case KRYON_CMD_DRAW_TEXT:
            p0 = Pt(cmd->data.draw_text.x, cmd->data.draw_text.y);
            color_img = plan9_get_color_image(data, cmd->data.draw_text.color);
            string(data->screen, p0, color_img, ZP, data->default_font,
                   cmd->data.draw_text.text);
            break;

        case KRYON_CMD_DRAW_LINE:
            p0 = Pt(cmd->data.draw_line.x1, cmd->data.draw_line.y1);
            p1 = Pt(cmd->data.draw_line.x2, cmd->data.draw_line.y2);
            color_img = plan9_get_color_image(data, cmd->data.draw_line.color);
            line(data->screen, p0, p1, 0, 0, 1, color_img, ZP);
            break;

        case KRYON_CMD_DRAW_ROUNDED_RECT:
            plan9_draw_rounded_rect(data,
                cmd->data.draw_rounded_rect.x,
                cmd->data.draw_rounded_rect.y,
                cmd->data.draw_rounded_rect.w,
                cmd->data.draw_rounded_rect.h,
                cmd->data.draw_rounded_rect.radius,
                cmd->data.draw_rounded_rect.color);
            break;

        case KRYON_CMD_SET_CLIP:
            r = Rect(cmd->data.set_clip.x,
                    cmd->data.set_clip.y,
                    cmd->data.set_clip.x + cmd->data.set_clip.w,
                    cmd->data.set_clip.y + cmd->data.set_clip.h);
            replclipr(data->screen, 0, r);
            data->clip_rect = r;
            break;

        case KRYON_CMD_POP_CLIP:
            /* Restore to full screen clipping */
            replclipr(data->screen, 0, data->screen->r);
            data->clip_rect = data->screen->r;
            break;

        default:
            /* Unsupported command - ignore */
            break;
        }
    }
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
    desktop_register_backend(DESKTOP_BACKEND_PLAN9, ops);
    IR_LOG_INFO("PLAN9", "Backend registered");
}
