import * as NativeToolbar from "kryon/toolbar";
import * as NativeDropdown from "kryon/dropdown";
import { Session } from "kryon/session";

export interface ToolbarProps {
    session: Session;
    props: NativeToolbar.Props;
    options: NativeDropdown.Option[];
    actions: NativeToolbar.ToolbarAction[];
}

export function Toolbar(widget: ToolbarProps): NativeToolbar.ToolbarResult {
    return NativeToolbar.Draw(widget.session, widget.props, widget.options, widget.actions);
}
