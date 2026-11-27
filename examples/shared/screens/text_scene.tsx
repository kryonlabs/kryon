import * as App from "kryon/app";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { TextSelectionEnable } from "kryon/text_selection";
import { ThemeMetricRole } from "kryon/theme";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    TextSelectionEnable();
    <Column session={session} props={{bounds: frame.viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Text", size: s32(28)}}/>
        <Text session={session} props={{disabled: disabled, bounds: {height: 56},
            text: "Make room for good ideas", size: s32(24)}}/>
        <Text session={session} props={{disabled: disabled, bounds: {height: 144},
            text: "Select and copy this text. Unicode works here too: café, Ω, é.", size: s32(18), selectable: true}}/>
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480)},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace5), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 44}, text: "Make room for good ideas",
            size: s32(24), disabled: disabled}}/>
        <Text session={session} props={{bounds: {height: 132}, size: s32(16), disabled: disabled,
            text: "Readable text, natural wrapping and familiar Unicode: café, Ω, é. Every little detail adds up."}}/>
    </Column>;
}

export function GallerySelectionFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    TextSelectionEnable();
    <Text session={session} props={{key: u64(10), bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480), height: 148},
        text: "Select a little text.\nKeep reading in comfort.", selectable: true, disabled: exampleDisabled}}/>;
}

export function main(): s32 {
    return App.Run({title: "text"}, Frame);
}
