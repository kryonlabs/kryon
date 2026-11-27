import * as App from "kryon/app";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Avatar, AvatarGroup } from "kryon/tsx/avatar";
import { Column, Row } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    const gap = Gap(ThemeMetricRole.ThemeMetricSpace4);
    const team: [string, string, string, string] = [
        "Ada Lovelace", "Grace Hopper", "Katherine Johnson", "Alan Turing"
    ];
    const names = nativeSlice(team);
    <Column session={session} props={{bounds: frame.viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace5), gap: gap, gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Avatar", size: s32(28)}}/>
        <Row session={session} props={{gap: gap, gap_override: true}}>
            <Avatar session={session} props={{disabled: disabled, bounds: {width: 72, height: 72},
                name: "Ada Lovelace", size: float32(72)}}/>
            <Avatar session={session} props={{disabled: disabled, bounds: {width: 72, height: 72},
                name: "Grace Hopper", size: float32(72), square: true}}/>
        </Row>
        <AvatarGroup session={session} names={names}
            props={{bounds: {height: 56}, size: float32(48), max_visible: s32(3)}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "avatar"}, Frame);
}
