/**
 * Plan 9 Backend - Input Event Handling
 *
 * Handles mouse and keyboard events using Plan 9's event library
 * Converts Plan 9 events to DesktopEvent format
 */

#include "plan9_internal.h"
#include "../desktop/abstract/desktop_events.h"
#include "../../ir/include/ir_log.h"

/* Convert Plan 9 mouse buttons to DesktopEvent button codes */
static uint8_t
convert_mouse_button(int buttons)
{
    /* Plan 9 mouse buttons: 1=left, 2=middle, 4=right */
    if (buttons & 1) return 0;  /* Left */
    if (buttons & 2) return 1;  /* Middle */
    if (buttons & 4) return 2;  /* Right */
    return 0;
}

/* Find component at point (recursive hit testing) */
static IRComponent*
find_component_at_point(IRComponent* root, float x, float y)
{
    IRComponent* child;
    IRComponent* result;
    int i;

    if (!root)
        return nil;

    /* Check if point is in this component's bounds */
    if (x < root->x || x >= root->x + root->width ||
        y < root->y || y >= root->y + root->height) {
        return nil;
    }

    /* Check children in reverse order (top-most first) */
    for (i = root->child_count - 1; i >= 0; i--) {
        child = root->children[i];
        result = find_component_at_point(child, x, y);
        if (result)
            return result;
    }

    /* If no child matched, return this component */
    return root;
}

/* Poll events and dispatch to callback */
void
plan9_poll_events(DesktopIRRenderer* renderer)
{
    Plan9RendererData* data;
    Event e;
    DesktopEvent devent;
    ulong key;
    int got_event;

    if (!renderer || !renderer->event_callback)
        return;

    data = (Plan9RendererData*)renderer->ops->backend_data;
    if (!data)
        return;

    /* Non-blocking event check loop */
    while (ecanread(Emouse | Ekeyboard)) {
        got_event = 0;

        /* Get next event */
        key = event(&e);

        switch (key) {
        case Emouse:
            /* Mouse event */
            got_event = 1;

            /* Check for button press/release */
            if (e.mouse.buttons) {
                /* Mouse button pressed or held */
                devent.type = DESKTOP_EVENT_MOUSE_CLICK;
                devent.timestamp = e.mouse.msec;
                devent.data.mouse.x = (float)e.mouse.xy.x;
                devent.data.mouse.y = (float)e.mouse.xy.y;
                devent.data.mouse.button = convert_mouse_button(e.mouse.buttons);
                devent.data.mouse.pressed = 1;

                renderer->event_callback(&devent, renderer->event_user_data);
            } else {
                /* Mouse movement (no buttons) */
                devent.type = DESKTOP_EVENT_MOUSE_MOVE;
                devent.timestamp = e.mouse.msec;
                devent.data.mouse.x = (float)e.mouse.xy.x;
                devent.data.mouse.y = (float)e.mouse.xy.y;
                devent.data.mouse.button = 0;
                devent.data.mouse.pressed = 0;

                renderer->event_callback(&devent, renderer->event_user_data);
            }
            break;

        case Ekeyboard:
            /* Keyboard event */
            got_event = 1;

            /* Key press event */
            devent.type = DESKTOP_EVENT_KEY_PRESS;
            devent.timestamp = nsec() / 1000000;  /* Convert to milliseconds */
            devent.data.keyboard.key_code = e.kbdc;
            devent.data.keyboard.shift = 0;  /* Plan 9 sends shifted runes directly */
            devent.data.keyboard.ctrl = 0;
            devent.data.keyboard.alt = 0;

            /* Check for special keys */
            if (e.kbdc == 0x7F || e.kbdc == '\033') {  /* DEL or ESC */
                /* Could be quit signal */
                if (e.kbdc == 0x7F) {
                    DesktopEvent quit_event;
                    quit_event.type = DESKTOP_EVENT_QUIT;
                    quit_event.timestamp = nsec() / 1000000;
                    renderer->event_callback(&quit_event, renderer->event_user_data);
                    data->running = 0;
                    break;
                }
            }

            renderer->event_callback(&devent, renderer->event_user_data);

            /* For printable characters, also send text input event */
            if (e.kbdc >= 32 && e.kbdc < 127) {  /* Printable ASCII */
                devent.type = DESKTOP_EVENT_TEXT_INPUT;
                snprint(devent.data.text_input.text,
                       sizeof(devent.data.text_input.text),
                       "%c", (char)e.kbdc);
                renderer->event_callback(&devent, renderer->event_user_data);
            } else if (e.kbdc >= 0x80) {  /* UTF-8 rune */
                devent.type = DESKTOP_EVENT_TEXT_INPUT;
                snprint(devent.data.text_input.text,
                       sizeof(devent.data.text_input.text),
                       "%C", e.kbdc);
                renderer->event_callback(&devent, renderer->event_user_data);
            }
            break;
        }
    }

    /* Check for window resize */
    if (eresized) {
        if (getwindow(data->display, Refnone) < 0) {
            fprint(2, "plan9: getwindow failed: %r\n");
            return;
        }

        /* Update screen pointer */
        data->screen = screen;
        data->clip_rect = data->screen->r;
        data->needs_redraw = 1;

        /* Send resize event */
        devent.type = DESKTOP_EVENT_WINDOW_RESIZE;
        devent.timestamp = nsec() / 1000000;
        devent.data.resize.width = Dx(screen->r);
        devent.data.resize.height = Dy(screen->r);

        renderer->event_callback(&devent, renderer->event_user_data);

        /* Clear eresized flag */
        eresized = 0;
    }
}
