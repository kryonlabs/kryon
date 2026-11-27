import * as App from "kryon/app";
import { CountdownAdvance, CountdownPartsFor, CountdownState } from "kryon/countdown";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { CurrentTheme, ThemeMetricRole } from "kryon/theme";
import { Button } from "kryon/tsx/button";
import { Countdown } from "kryon/tsx/countdown";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;
let state: CountdownState = nativeInit<CountdownState>({});
let endsAt: float64 = float64(0);

function DrawTimer(session: Session, disabled: boolean, reducedMotion: boolean): void {
    const input = CaptureFrameInput(session);
    const now = float64(input.elapsed_ms) / float64(1000);
    if (endsAt === float64(0)) {
        endsAt = now + float64(90);
    }
    const remaining = endsAt > now ? endsAt - now : float64(0);
    CountdownAdvance(nativeRef(state), CountdownPartsFor(remaining, true), float32(input.delta_ms) / float32(1000), reducedMotion);
    <Countdown session={session} state={nativeRef(state)} props={{bounds: {height: 180}, seconds_left: remaining, hide_days: true}}/>;
    const restart = Button({session: session, props: {disabled: disabled, bounds: {width: 160, height: 40}, label: "Restart"}});
    if (restart) {
        endsAt = now + float64(90);
    }
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    const reducedMotion = exampleReducedMotion || CurrentTheme().motion.reduced_motion;
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Countdown", size: s32(28)}}/>
        {DrawTimer(session, disabled, reducedMotion)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "countdown"}, Frame);
}
