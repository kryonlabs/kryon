import * as App from "kryon/app";
import * as Theme from "kryon/Theme";
import * as Transition from "kryon/transition";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { LayoutContainer, Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Button } from "kryon/tsx/button";
import { Column, ResolvedStack } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let disabled: boolean = false;
let reducedMotion: boolean = false;
let standalone: Transition.TransitionState = nativeInit<Transition.TransitionState>({});
let gallery: Transition.TransitionState = nativeInit<Transition.TransitionState>({});

function Fade(frame: App.Frame, inGallery: boolean): void {
    let state = inGallery ? gallery : standalone;
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const input = CaptureFrameInput(session);
    if (Button({session: session, props: {disabled: disabled, bounds: {width: 180, height: 44}, label: "Animate fade"}})) {
        state = Transition.TransitionStart(float32(0.6));
    }
    state = Transition.TransitionAdvance(state, reducedMotion || Theme.CurrentTheme().motion.reduced_motion
        ? float32(1) : float32(input.delta_ms) / float32(1000)).state;
    if (inGallery) { gallery = state; }
    else { standalone = state; }
    <ResolvedStack session={session} props={{bounds: {height: 128}}}
        children={(area: LayoutContainer): void => {
            <Text session={session} props={{disabled: disabled, bounds: area.bounds, text: "A transition over this content"}}/>;
            <Box session={session} props={{bounds: area.bounds, fill: Transition.TransitionFadeColor(state,
                {r: u8(16), g: u8(34), b: u8(45), a: u8(180)})}}/>;
        }}/>;
}

export function SetExampleOptions(isDisabled: boolean, isReducedMotion: boolean): void {
    disabled = isDisabled;
    reducedMotion = isReducedMotion;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Transition", size: s32(28)}}/>
        {Fade(frame, false)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Column session={session} props={{bounds: {x: frame.viewport.x + 24, y: frame.viewport.y + 64,
        width: frame.viewport.width - 48}, gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        {Fade(frame, true)}
    </Column>;
}

export function main(): s32 { return App.Run({title: "transition"}, Frame); }
