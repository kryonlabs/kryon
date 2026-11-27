import * as App from "kryon/app";
import * as NativeListBox from "kryon/list_box";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Column } from "kryon/tsx/layout";
import { ListBox, ListBoxMulti } from "kryon/tsx/list_box";
import { Text } from "kryon/tsx/text";
import { SelectionBounds } from "selection_bounds";

let exampleDisabled: boolean = false;
let standaloneChoice: s32 = s32(0);
let galleryChoice: s32 = s32(2);
let standaloneSelected: [boolean, boolean, boolean] = [false, false, false];
let gallerySelected: [boolean, boolean, boolean, boolean, boolean] = [true, false, true, false, false];

let items: [NativeListBox.ListBoxItem, NativeListBox.ListBoxItem, NativeListBox.ListBoxItem,
    NativeListBox.ListBoxItem, NativeListBox.ListBoxItem] = [
    nativeInit<NativeListBox.ListBoxItem>({label: "Morning walk"}), nativeInit<NativeListBox.ListBoxItem>({label: "Read a chapter"}), nativeInit<NativeListBox.ListBoxItem>({label: "Make something"}),
    nativeInit<NativeListBox.ListBoxItem>({label: "Take a break"}), nativeInit<NativeListBox.ListBoxItem>({label: "Reflect"})];

function DrawStandaloneLists(session: Session): void {
    const standaloneItems = nativeSlice(items, s32(0), s32(3));
    const result = ListBox({session: session, props: {bounds: {height: 100}, row_height: s32(32),
        selected_index: standaloneChoice, disabled: exampleDisabled}, items: standaloneItems});
    standaloneChoice = result.selected_index;
    <ListBoxMulti session={session} props={{bounds: {height: 100}, row_height: s32(32),
        disabled: exampleDisabled}} items={standaloneItems} selected={nativeSlice(standaloneSelected)}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 40}, text: "List box", size: s32(28),
            disabled: exampleDisabled}}/>
        {DrawStandaloneLists(session)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const result = ListBox({session: session, props: {bounds: SelectionBounds(frame.viewport, 64, 184),
        row_height: s32(34), selected_index: galleryChoice, disabled: exampleDisabled}, items: nativeSlice(items)});
    galleryChoice = result.selected_index;
}

export function GalleryMultiFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <ListBoxMulti session={session} props={{bounds: SelectionBounds(frame.viewport, 64, 184),
        row_height: s32(34), disabled: exampleDisabled}} items={nativeSlice(items)} selected={nativeSlice(gallerySelected)}/>;
}

export function main(): s32 {
    return App.Run({title: "listbox"}, Frame);
}
