import * as App from "kryon/app";
import { BreadcrumbItem } from "kryon/breadcrumb";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Breadcrumb } from "kryon/tsx/breadcrumb";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let active: s32 = s32(2);

function DrawTrail(session: Session, disabled: boolean): void {
    const items: [BreadcrumbItem, BreadcrumbItem, BreadcrumbItem] = [
        nativeInit<BreadcrumbItem>({label: "Home"}), nativeInit<BreadcrumbItem>({label: "Projects"}), nativeInit<BreadcrumbItem>({label: "Kryon"}),
    ];
    const result = Breadcrumb({session: session, props: {disabled: disabled,
        bounds: {height: 44}, active_index: active}, items: nativeSlice(items)});
    if (result.changed) {
        active = result.activated_index;
    }
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
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Breadcrumb", size: s32(28)}}/>
        {DrawTrail(session, disabled)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "breadcrumb"}, Frame);
}
