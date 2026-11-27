import * as NativeDropdown from "kryon/dropdown";
import { Session } from "kryon/session";

export interface DropdownProps {
    session: Session;
    props: NativeDropdown.Props;
    options: NativeDropdown.Option[];
}

export function Dropdown(widget: DropdownProps): NativeDropdown.Result {
    return NativeDropdown.Draw(widget.session, widget.props, widget.options);
}
