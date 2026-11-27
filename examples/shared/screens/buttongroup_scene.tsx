import * as App from "kryon/app";
import * as NativeButton from "kryon/button";
import { ButtonGroupState } from "kryon/button_group";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { ButtonGroup } from "kryon/tsx/button_group";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let state: ButtonGroupState = nativeInit<ButtonGroupState>({});
let active: s32 = s32(0);

function DrawButtons(session: Session, disabled: boolean): void {
    const items: [NativeButton.Props, NativeButton.Props, NativeButton.Props] = [
        nativeInit<NativeButton.Props>({label: "Left", disabled: disabled}),
        nativeInit<NativeButton.Props>({label: "Center", disabled: disabled}),
        nativeInit<NativeButton.Props>({label: "Right", disabled: disabled}),
    ];
    const result = ButtonGroup({session: session, props: {bounds: {height: 44}, full_width: true},
        items: nativeSlice(items), state: nativeRef(state)});
    if (result.activated_index >= s32(0)) {
        active = result.activated_index;
    }
    <Text session={session} props={{disabled: disabled, bounds: {height: 36}, text: items[active].label}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Button group", size: s32(28)}}/>
        {DrawButtons(session, disabled)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "buttongroup"}, Frame);
}
