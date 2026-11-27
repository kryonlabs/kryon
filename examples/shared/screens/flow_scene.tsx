import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { FlowResult } from "kryon/flow";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Button } from "kryon/tsx/button";
import { Flow, ResolvedFlow } from "kryon/tsx/flow";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;

function DrawItems(session: Session, width: float32, disabled: boolean): void {
    <Button session={session} props={{bounds: {width: width, height: 44}, label: "First item", disabled: disabled}}/>;
    <Button session={session} props={{bounds: {width: width, height: 44}, label: "Second item", disabled: disabled}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    const width = float32(164) * viewport.width / float32(480);
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Flow", size: s32(23)}}/>
        <Flow session={session} props={{bounds: {height: 208}, gap: s32(12), padding: s32(8)}}>
            {DrawItems(session, width, disabled)}
        </Flow>
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <ResolvedFlow session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480), height: 208}, gap: s32(12), padding: s32(8)}}
        children={(flow: FlowResult): void => { DrawItems(session, flow.content.width * float32(0.4), disabled); }}/>;
}

export function main(): s32 {
    return App.Run({title: "flow"}, Frame);
}
