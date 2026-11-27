import * as NativeScroll from "kryon/scroll";
import { Session } from "kryon/session";
import { End } from "kryon/tree";
import { Scoped } from "std/callable";

export interface ScrollProps {
    session: Session;
    props: NativeScroll.Props;
    children: Scoped<(view: NativeScroll.ScrollResult) => void>;
}

export function Scroll(widget: ScrollProps): NativeScroll.ScrollResult {
    const result = NativeScroll.Draw(widget.session, widget.props);
    if (result.opened) {
        widget.children(result);
        End(widget.session);
    }
    return result;
}
