# Motion

Timed scalar tracks, stylesheet transitions, layout reflow motion, per-widget
springs and page view transforms. Read the section for the kind of movement you
want.

## Timed motion and keyframes

Import `kryon/motion` for a timed scalar transition or keyframe track.
The module takes time and state from the caller and depends only on scalar
math. Apply a sample to opacity, position, size, or several chart points.

- `Motion.TimingFor(.EaseOut)` creates a CSS timing preset. The choices are
  `.Linear`, `.Ease`, `.EaseIn`, `.EaseOut`, and `.EaseInOut`.
  `Motion.CubicBezier(x1, y1, x2, y2)` accepts CSS control points, including
  overshoot. Invalid control points yield linear timing. Use `Motion.Progress`
  to ease a unit interval directly.
- `Motion.BuildTrack(frames)` validates a borrowed slice of `Motion.Keyframe`
  records. Start at offset `0`, end at `1`, and place any other offsets in
  increasing order. A keyframe's timing governs the segment leaving it.
  Keep the slice alive and unchanged. `Motion.At(track, progress)` samples
  by normalized progress. `Motion.Sample(track, elapsed, duration, iterations,
  alternate, reduced_motion)` samples by seconds and reports `.value` and
  `.active`. Reduced motion settles at the same endpoint as ordinary playback.
- `Motion.TweenTo(*state, target, now, duration, timing, reduced_motion)`
  starts an implicit transition when the target changes. Give each value its
  own zero-initialized `Motion.TweenState`. The first call initializes at the
  supplied target. Changing targets mid-flight starts at the current value.
  Pass monotonic seconds and request motion frames while `.active` is true.

The [checked usage example](../src/ui/motion_api_example.zi) samples a whole
series with one easing calculation per frame and shows an interrupted hover
transition. Keep the chart's scale and the input series stable while they
interpolate. Pass the resulting values to your usual chart drawing code.

## Retained style and layout motion

`kryon/style_motion` installs typed transition rules and keyframes over the
normal widget style cascade. For CSS authoring, use `css/motion` to install
a parsed or packed sheet. Buttons use animated background, text and border
colors, opacity, corner radius, border width and content offset. Text and
links animate their color and opacity. Images animate the surface properties
the image already paints. Give each moving widget a stable key.

`kryon/layout_motion` moves keyed children through a reflow or reorder. Open a
layout scope and call `LayoutMotion.Children(session)` before drawing its
children. Set `.resize = true` to animate width and height changes and
`.enter = true` to grow newly mounted children from zero size at their placed
origin. Each option applies to direct keyed children of that scope.

Set `.seconds`, `.timing` and `.reduced_motion` to control the transition.
The layout reserves each child's target space while its visible size changes.
Paint and hit testing use the sampled bounds, and interrupted transitions
continue from the current sample. Moving a parent also moves its relative
children. Reduced motion and zero duration settle immediately.

```zi
Layout :: #import "kryon/layout";
LayoutMotion :: #import "kryon/layout_motion";
Button :: #import "kryon/button";
#import "kryon/tree"

row := Layout.Row(session, .{.key = 1, .gap = 12})
if row.opened {
    unused LayoutMotion.Children(session, .{.seconds = 0.24,
        .resize = true, .enter = true})
    unused Button.Draw(session, .{.key = 2, .label = "First"})
    unused Button.Draw(session, .{.key = 3, .label = "Second"})
    unused End(session)
}
```

For exit motion, keep the child’s data and a `LayoutMotion.PresenceState` in
application state. Call `LayoutMotion.Presence` each composed frame, including
while the child is absent. Its first sample is fully visible if present and
fully hidden if absent. Later changes animate from the current sample and
honor reduced motion. The result gives a `fraction` from 0 to 1. Submit the
child while `draw` is true. Use `interactive` to disable input as soon as
exit begins. Optional `PresenceOptions` control `.seconds`, `.timing` and
`.reduced_motion`.

```zi
LayoutMotion :: #import "kryon/layout_motion";
Button :: #import "kryon/button";
#import "kryon/session"
#import "kryon/tree"
#import "kryon/tree_input"
#import "kryon/widget_kind"

PrintAction :: (session: Session, presence: *LayoutMotion.PresenceState,
    available: bool) -> bool {
    sample := LayoutMotion.Presence(session, presence, available)
    if !sample.interactive {
        previous := TreePrevious(session, TreeCurrentParent(session), 7,
            cast(s32)WidgetKind.WidgetKindButton)
        TreeBlurAt(session, previous)
    }
    if !sample.draw { return false }
    // Keep a positive explicit height throughout the transition.
    return Button.Draw(session, .{.key = 7, .id = 7, .label = "Print",
        .bounds = .{0.0, 0.0, 88.0, 1.0 + 35.0 * sample.fraction},
        .fit_content = true, .disabled = !sample.interactive}) != 0
}
```

Hold one state per child and session. If your composition can cancel a frame,
sample into a copy of the state and keep the copy when the frame commits.

A custom retained host can set monotonic seconds with
`kryon/frame_motion.SetFrameMotionTime` before `BeginFrame` and use
`FrameMotionMoving` after commit to keep the frame loop active during motion.
Desktop, Android and Canvas frame hosts supply their own clocks. The web DOM
backend uses CSS motion through the browser.

## Widget motion

`Menu.Props`, `TabBar.Props`, `Navigation.Props`, and `Collapsible.Props`
accept the following motion props. Import each widget from its named package.
Retain one zero-initialized motion state per widget, with a stable widget key.

| Motion prop | Meaning |
| --- | --- |
| `motion: *TransitionMotionState` | Caller-owned spring state; the state must outlive every call and must not be shared between widgets. A null pointer keeps the widget immediate |
| `now: float64` | Monotonic seconds; time zero is valid, equal or backward timestamps do not advance the spring, and a long pause advances at most 0.1 seconds per submission |
| `reduced_motion: bool` | Snaps to the target and clears `animating` |

Motion runs in native widgets and VM-rendered widgets. The portable
`KRYON_HOSTED_UI` TabBar bridge supports immediate rendering with
`motion = null` only. A nonnull pointer payload is rejected before dispatch;
hosted motion needs an explicit state-value and write-back protocol. Guest
addresses are never accepted as native host state pointers.
Each result adds `animating: bool`. Request another application frame while
any submitted widget returns true, supplying the real clock on that frame.
Once every result is false, motion needs no further frames. The first
submission initializes at the current target; subsequent target changes
retain position and velocity, including reversals. Time zero is valid. Equal
or backward timestamps do not advance the spring; a long pause advances at
most 0.1 seconds per submission. Retained session and tree identities reset
state when the widget moves to another parent, remounts, or changes kind.

Spring parameters per widget:

| Widget | Springs |
| --- | --- |
| Menu | Fades, grows from 96 percent and translates six units into place; 0.28-second opening and 0.16-second closing |
| TabBar | Tab indicators stretch toward the selected tab: leading edge 0.26 seconds with no bounce, trailing edge 0.42 seconds with 0.14 bounce |
| Navigation | Selection bounds 0.42 seconds with 0.16 bounce in full and compact layouts |
| Collapsible | 0.40-second spring for the reveal and quarter-turn chevron | Continue submitting the
menu and its current groups during closing. Closing panels keep visual paint
but immediately release their scrim, rows, focus, and activation ownership.
The menu retains indices and a content signature, never borrowed item slices.

Hit targets and returned selection geometry follow the actual controls
independently of the paint.

For Collapsible, `content` is the animated reveal rectangle; the application draws and clips
its body inside that rectangle and owns body interaction while closing.
`open`, focus, keyboard navigation, and close results keep their logical
meaning throughout the transition.

Explicit `reduced_motion`, theme `SetReducedMotion(true)`, or a nonpositive
theme motion multiplier snaps to the target and clears `animating`. A
positive theme multiplier scales spring durations. These controls affect
actual widget paint and reveal geometry.

The four widgets use the shared `TransitionSpringAdvance(spring, target,
delta, duration, bounce, immediate) -> TransitionSpring` integrator.
`TransitionMotionBegin`, `TransitionMotionValue`, and
`TransitionMotionActive` expose its caller-owned, four-channel state for
compositions. `TransitionMotionBegin` takes the clock, reduced-motion flag,
session slot/epoch, parent identity, own identity, and widget kind; callers
using it directly must supply actual retained identities. Treat a widget's
state as opaque while that widget owns it.

## View transforms

`view_transform.ViewFit(viewport, page_width, page_height, maximum)` returns
the state for a page that fills its viewport at zoom 1. The pure mapping and
mutation helpers convert between page and screen coordinates, zoom at a point,
pan, resize while preserving the visible centre, and drive a two-finger pinch.
Every mutation keeps zoom in range and leaves no empty space around the page.
Hosts observe wheel, pointer, and touch input and pass those observations in;
`ViewWheelFactor(notches)` supplies the bounded 1.2-per-notch wheel factor.
