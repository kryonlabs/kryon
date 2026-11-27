import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Option } from "kryon/dropdown";
import { RecordGalleryClick } from "kryon/examples/gallery_clicks";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import * as NativeToolbar from "kryon/toolbar";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { Toolbar } from "kryon/tsx/toolbar";

let exampleDisabled: boolean = false;
let clicks: s32 = s32(0);

interface DemoToolbarProps {
    session: Session;
    props: NativeToolbar.Props;
    shareDisabled: boolean;
}

function DemoToolbar(widget: DemoToolbarProps): boolean {
    const options: [Option] = [nativeInit<Option>({})];
    const actions: [NativeToolbar.ToolbarAction, NativeToolbar.ToolbarAction] = [
        nativeInit<NativeToolbar.ToolbarAction>({label: "New note"}), nativeInit<NativeToolbar.ToolbarAction>({label: "Share", disabled: widget.shareDisabled}),
    ];
    const result = Toolbar({session: widget.session, props: widget.props,
        options: nativeSlice(options, s32(0), s32(0)), actions: nativeSlice(actions)});
    return result.clicked_action >= s32(0);
}

function DrawStandaloneToolbar(session: Session, disabled: boolean): void {
    if (DemoToolbar({session: session, props: nativeInit<NativeToolbar.Props>({height: s32(48)}), shareDisabled: disabled})) {
        clicks += s32(1);
    }
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Toolbar", size: s32(23)}}/>
        {DrawStandaloneToolbar(session, disabled)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    RecordGalleryClick(DemoToolbar({session: session, shareDisabled: exampleDisabled, props: nativeInit<NativeToolbar.Props>({
        x: s32(viewport.x + viewport.width * float32(24) / float32(480)), y: s32(viewport.y + 64),
        width: s32(float32(432) * (viewport.width / float32(480))), height: s32(48)})}));
}

export function main(): s32 {
    return App.Run({title: "toolbar"}, Frame);
}
