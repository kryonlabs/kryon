import * as App from "kryon/app";
import { Rectangle } from "kryon/geometry";
import { LayoutContainer } from "kryon/layout";
import * as Native from "kryon/reorder";
import { Session } from "kryon/session";
import { ResolvedStack } from "kryon/tsx/layout";
import { Reorder } from "kryon/tsx/reorder";
import { Text } from "kryon/tsx/text";
import { EditorExample } from "editor_example";

interface ReorderExampleState {
    order: [s32, s32, s32];
}

let standaloneState: ReorderExampleState = {order: [s32(0), s32(1), s32(2)]};
let galleryState: ReorderExampleState = {order: [s32(0), s32(1), s32(2)]};

function DrawList(session: Session, bounds: Rectangle, state: NativeRef<ReorderExampleState>): void {
    let order = state.order;
    const labels: [string, string, string] = ["Morning walk", "Read a chapter", "Take a break"];
    let items: [Native.ReorderItem, Native.ReorderItem, Native.ReorderItem] = [nativeInit<Native.ReorderItem>({}), nativeInit<Native.ReorderItem>({}), nativeInit<Native.ReorderItem>({})];
    for (let index = s32(0); index < s32(3); index += s32(1)) {
        const top = bounds.y + float32(index) * 56;
        const label = labels[order[index]];
        items[index] = nativeInit<Native.ReorderItem>({id: order[index] + s32(10),
            bounds: {x: bounds.x, y: top, width: bounds.width, height: 44}});
        <Text session={session} props={{bounds: {x: bounds.x + 40, y: top + 12, width: bounds.width - 72, height: 36},
            text: label}}/>;
    }
    const moved = Reorder({session: session, props: {id: s32(10), bounds: bounds, handle_width: s32(28)}, items: nativeSlice(items)});
    const from = moved.from_index;
    const to = moved.to_index;
    if (moved.committed !== s32(0) && from >= s32(0) && to >= s32(0) && from < s32(3) && to < s32(3)) {
        const saved = order[from];
        let index = from;
        while (index < to) {
            order[index] = order[index + s32(1)];
            index += s32(1);
        }
        while (index > to) {
            order[index] = order[index - s32(1)];
            index -= s32(1);
        }
        order[to] = saved;
        state.order = order;
    }
}

export function SetExampleOptions(_disabled: boolean, _reducedMotion: boolean): void {
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <EditorExample frame={frame} title="Reorder">
        <ResolvedStack session={session} props={{bounds: {height: 200}}} children={(area: LayoutContainer): void => {
            DrawList(session, area.bounds, nativeRef(standaloneState));
        }}/>
    </EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const view = frame.viewport;
    DrawList(session, {x: view.x + float32(24) * view.width / float32(480), y: view.y + 64,
        width: float32(432) * view.width / float32(480), height: 200}, nativeRef(galleryState));
}

export function GalleryCaptureFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    DrawList(session, {x: 24, y: 64, width: 432, height: 200}, nativeRef(galleryState));
}

export function main(): s32 {
    return App.Run({title: "reorder"}, Frame);
}
