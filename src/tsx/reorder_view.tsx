import * as NativeReorder from "kryon/reorder";
import { Session } from "kryon/session";

export interface ReorderProps {
    session: Session;
    props: NativeReorder.Props;
    items: NativeReorder.ReorderItem[];
}

export function Reorder(widget: ReorderProps): NativeReorder.ReorderListResult {
    return NativeReorder.Draw(widget.session, widget.props, widget.items);
}
