/* Event-driven frame pacing owned by Kryon, not by the raylib backend.
 *
 * The upstream SDL backend records EnableEventWaiting() but continues to
 * poll and swap uncapped.  That leaves a declarative UI burning a core when
 * nothing changes, starving text input and eventually making the desktop
 * appear frozen.  Kryon keeps input polling at the backend boundary and
 * applies a short, deterministic yield after each completed frame.
 */

#include "kryon.h"

static int g_kry_event_waiting = 0;

void EnableEventWaiting(void)
{
    g_kry_event_waiting = 1;
}

void DisableEventWaiting(void)
{
    g_kry_event_waiting = 0;
}

void
kry_event_wait_after_frame(void)
{
    if(g_kry_event_waiting)
        WaitTime(1.0/120.0);
}
