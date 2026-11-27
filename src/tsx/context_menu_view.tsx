import * as NativeContextMenu from "kryon/context_menu";
import { Session } from "kryon/session";

export interface ContextMenuProps {
    session: Session;
    props: NativeContextMenu.Props;
    items: NativeContextMenu.ContextMenuItem[];
}

export function ContextMenu(widget: ContextMenuProps): NativeContextMenu.ContextMenuResult {
    return NativeContextMenu.Draw(widget.session, widget.props, widget.items);
}
