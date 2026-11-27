import * as App from "kryon/app";
import { ColorFromPacked } from "kryon/drawing_props";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Column } from "kryon/tsx/layout";
import { SvgIcon } from "kryon/tsx/svg_icon";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "SVG icon", size: s32(28)}}/>
        <SvgIcon session={session} props={{bounds: {width: 96, height: 96},
            shape: "S1.7rr:M3 12L9 18L21 6", color: ColorFromPacked(u32(0x269A62FF))}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "svgicon"}, Frame);
}
