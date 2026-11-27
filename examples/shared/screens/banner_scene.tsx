import * as App from "kryon/app";
import { BannerAdvance, BannerState, BannerStateFor } from "kryon/banner";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { CurrentTheme, ThemeMetricRole } from "kryon/theme";
import { Banner } from "kryon/tsx/banner";
import { Button } from "kryon/tsx/button";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;
let state: BannerState = nativeInit<BannerState>({});
let initialized: boolean = false;
let requested: boolean = false;

function DrawBannerControls(session: Session, disabled: boolean): void {
    if (!initialized) {
        state = BannerStateFor(true);
        initialized = true;
    }
    const delta = float32(CaptureFrameInput(session).delta_ms) / float32(1000);
    const reducedMotion = exampleReducedMotion || CurrentTheme().motion.reduced_motion;
    BannerAdvance(nativeRef(state), delta, reducedMotion);
    const result = Banner({session: session, state: nativeRef(state), props: {disabled: disabled, bounds: {height: 180},
        text: "A new version is available.", action: "Update", dismissible: true}});
    if (result.action) {
        requested = true;
        state.open = false;
    }
    if (requested) {
        <Text session={session} props={{disabled: disabled, bounds: {height: 180}, text: "Update requested"}}/>;
    }
    if (Button({session: session, props: {disabled: disabled, bounds: {width: 160, height: 40}, label: "Show banner"}})) {
        state.open = true;
        requested = false;
    }
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Banner", size: s32(28)}}/>
        {DrawBannerControls(session, disabled)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "banner"}, Frame);
}
