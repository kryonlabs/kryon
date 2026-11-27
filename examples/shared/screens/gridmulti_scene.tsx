import * as App from "kryon/app";
import { GridMultiItem } from "kryon/grid_multi";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { GridMulti } from "kryon/tsx/grid_multi";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let selected: [boolean, boolean, boolean, boolean, boolean, boolean] = [false, false, false, false, false, false];
let cursor: s32 = s32(0);
let anchor: s32 = s32(0);

function DrawChoices(session: Session, disabled: boolean): void {
    const items: [GridMultiItem, GridMultiItem, GridMultiItem, GridMultiItem, GridMultiItem, GridMultiItem] = [
        nativeInit<GridMultiItem>({label: "Documents"}), nativeInit<GridMultiItem>({label: "Pictures"}), nativeInit<GridMultiItem>({label: "Music"}),
        nativeInit<GridMultiItem>({label: "Videos"}), nativeInit<GridMultiItem>({label: "Projects"}), nativeInit<GridMultiItem>({label: "Downloads"}),
    ];
    const result = GridMulti({session: session, props: {disabled: disabled, bounds: {height: 224}, cell_width: s32(110),
        cell_height: s32(64), gap: s32(12), cursor: cursor, anchor: anchor}, items: nativeSlice(items), selected: nativeSlice(selected)});
    cursor = result.cursor;
    anchor = result.anchor;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Grid multi", size: s32(28)}}/>
        {DrawChoices(session, disabled)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "gridmulti"}, Frame);
}
