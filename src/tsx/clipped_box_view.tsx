import * as NativeBox from "kryon/box";
import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";
import { TreePopScope, TreePushScope, TreeSetChildClip } from "kryon/tree";
import { Scoped } from "std/callable";

export interface ClippedBoxProps {
    session: Session;
    props: NativeBox.Props;
    clip: Rectangle;
    children: Scoped<() => void>;
}

export function ClippedBox(widget: ClippedBoxProps): s32 {
    const node = NativeBox.Draw(widget.session, widget.props);
    if (node >= s32(0)) {
        TreeSetChildClip(widget.session, node, widget.clip);
        if (TreePushScope(widget.session, node)) {
            widget.children();
            TreePopScope(widget.session);
        }
    }
    return node;
}
