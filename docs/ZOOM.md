# Zoom

`src/ui/zoom.zi` zooms a whole application the way a browser page zoom does.
Ctrl and the mouse wheel step one factor for the session; Ctrl with plus,
minus, or zero steps or resets it. The factor scales the entire application,
not one container, so it is a single number the host or the application
applies once.

Kryon makes the decisions and the host supplies raw observations. Each frame
the host passes the wheel movement and the held modifiers:

```zi
consumed := ZoomWheel(session, platform, wheel, KeyboardModifiers(session))
if consumed { wheel = 0.0 }   // Ctrl+wheel is zoom, not scrolling
```

`ZoomWheel` returns true when the wheel belongs to zoom, so the application
must not also scroll with it. Positive movement zooms in. Whole notches step
the factor, and the small movements of a touchpad add up until they reach a
notch. `ZoomKey(session, platform, key, modifiers)` does the same for the
keyboard shortcuts. Read the result with `ZoomFactor(session)` (`1.0` is
100%).

## The prop

`ZoomProps` configures a session with `ZoomConfigure(session, props)`. Its zero
value is the default, so an application that never configures zoom gets it:

| Field | Default | Meaning |
| --- | --- | --- |
| `mode` | `ZoomModeAuto` | `Auto` zooms on desktop windows only. `On` zooms on every platform. `Off` never zooms. |
| `factor` | 1.0 | Starting factor. |
| `minimum` | 0.5 | Lowest factor. |
| `maximum` | 3.0 | Highest factor. |
| `step` | 0.1 | One notch. The factor always lands on a whole number of steps. |

Automatic mode leaves a browser alone, because the page already zooms with the
same keys, and leaves touch screens alone, because they have no wheel. Pass
the platform as the `DPIPlatform` the host already reports for `ResolveDPI`.

`ZoomSet(session, factor)` restores a saved preference and returns the factor
after clamping and snapping; `ZoomReset(session)` returns to the starting
factor.

## Applying the factor

The factor scales the whole application, widget text included, so a host
applies it as a render scale rather than asking each screen to scale itself.
A host that lays out in logical units passes the factor to the DPI state:
`ZoomDPI(state, factor)` makes the render scale the display density times the
zoom, and the layout is the view divided by it. A zoom below 100% lays out a
larger area and a zoom above it a smaller one, and `ResolveDPI` keeps the zoom
across resizes and density changes. Inbe does this: it stores the factor as
its UI scale setting, so the wheel and the Appearance slider drive one value,
and hands that setting to `ZoomDPI` each frame.

A host that draws an application unchanged applies the factor as a render
scale and divides pointer positions and the viewport by it. Kryon's raylib
runner does this for every application it runs, with no application code:
`ZoomWheel` and `ZoomKey` run before each frame, the wheel and the shortcut
keys they consume never reach the application, and `RaylibSetZoom` scales the
frame, so layout reflows to the smaller viewport as a browser page does and
text is rebaked at its scaled size to stay sharp. `KRYON_ZOOM=1.5` starts a
window at 150%.

An application imports the module as `Zoom` (and `DPI` for the platform enum)
from the Kryon package. To opt out, or to force zoom on every platform, call
`ZoomConfigure(session, props)` with `mode` set to `ZoomModeOff` or
`ZoomModeOn`; the default `ZoomProps` is automatic.
