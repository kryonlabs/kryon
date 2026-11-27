import * as App from "kryon/app";
import * as NativeTooltip from "kryon/tooltip";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Rectangle } from "kryon/geometry";
import { PointInBounds } from "kryon/input_protocol";
import { Session } from "kryon/session";
import { OverlayPlacement } from "kryon/tooltip_common";
import { Tooltip } from "kryon/tsx/tooltip";
import { Text } from "kryon/tsx/text";
import { EditorExample } from "editor_example";

let disabled: boolean = false;
let reducedMotion: boolean = false;
let standalone: NativeTooltip.TooltipState = nativeInit<NativeTooltip.TooltipState>({});
let gallery: NativeTooltip.TooltipState = nativeInit<NativeTooltip.TooltipState>({});
let initialized: boolean = false;

function Hint(frame: App.Frame, inGallery: boolean): void {
    if (!initialized) {
        standalone = NativeTooltip.TooltipStateFor(u64(700));
        gallery = NativeTooltip.TooltipStateFor(u64(700));
        initialized = true;
    }
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const snapshot = CaptureFrameInput(session);
    const pointer = snapshot.pointer;
    const viewport = frame.viewport;
    const anchor: Rectangle = {x: viewport.x + float32(38) * viewport.width / float32(480),
        y: viewport.y + 128, width: float32(126) * viewport.width / float32(480), height: float32(34)};
    let props: NativeTooltip.Props = nativeInit<NativeTooltip.Props>({anchor: anchor, viewport: viewport,
        text: "Save the maintained Ziran source", shortcut: "Ctrl+S",
        placement: OverlayPlacement.OverlayPlaceBottom, gap: 8, max_width: 240,
        delay_ms: 400, passive_delay_ms: 800, warm_ms: 250, now_ms: snapshot.elapsed_ms,
        disabled: disabled, style: NativeTooltip.TooltipDefaultStyle(),
        pointer: {x: pointer.x, y: pointer.y, down: pointer.down, pressed: pointer.pressed,
            released: pointer.released, over_anchor: PointInBounds(pointer.x, pointer.y, anchor)}});
    props.style.motion_reduced = reducedMotion;
    <Text session={session} props={{bounds: anchor, text: "Save source", size: s32(17)}}/>;
    <Tooltip session={session} props={props} state={inGallery ? nativeRef(gallery) : nativeRef(standalone)}/>;
}

export function SetExampleOptions(isDisabled: boolean, isReducedMotion: boolean): void {
    disabled = isDisabled;
    reducedMotion = isReducedMotion;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="Tooltip">{Hint(frame, false)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void { Hint(frame, true); }

export function main(): s32 { return App.Run({title: "tooltip"}, Frame); }
