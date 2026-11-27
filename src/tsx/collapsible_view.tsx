import * as NativeCollapsible from "kryon/collapsible";
import { Session } from "kryon/session";

export interface CollapsibleProps {
    session: Session;
    props: NativeCollapsible.Props;
    headers: NativeCollapsible.CollapsibleHeader[];
}

export function Collapsible(widget: CollapsibleProps): NativeCollapsible.CollapsibleResult {
    return NativeCollapsible.Draw(widget.session, widget.props, widget.headers);
}
