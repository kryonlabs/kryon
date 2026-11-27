import * as NativeSheet from "kryon/sheet";
import { Session } from "kryon/session";
import { Scoped } from "std/callable";

export interface SheetPanelProps {
    session: Session;
    props: NativeSheet.Props;
    state: NativeRef<NativeSheet.SheetState>;
    children: Scoped<(panel: NativeSheet.SheetResult) => void>;
}

export function SheetPanel(widget: SheetPanelProps): NativeSheet.SheetResult {
    const result = NativeSheet.SheetBegin(widget.session, widget.props, widget.state);
    if (result.content_node >= s32(0)) {
        widget.children(result);
        NativeSheet.SheetEnd(widget.session);
    }
    return result;
}
