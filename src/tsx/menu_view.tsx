import * as NativeMenu from "kryon/menu";
import { Session } from "kryon/session";

export interface MenuProps {
    session: Session;
    props: NativeMenu.Props;
    groups: NativeMenu.MenuGroup[];
    state: NativeRef<NativeMenu.MenuState>;
}

export function Menu(widget: MenuProps): NativeMenu.MenuResult {
    return NativeMenu.Draw(widget.session, widget.props, widget.groups, widget.state);
}
