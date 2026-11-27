import * as NativeTitleBar from "kryon/title_bar";
import * as Dropdown from "kryon/dropdown";
import { Session } from "kryon/session";

export interface TitleBarProps {
    session: Session;
    props: NativeTitleBar.Props;
    options: Dropdown.Option[];
}

export function TitleBar(widget: TitleBarProps): NativeTitleBar.TitleBarResult {
    return NativeTitleBar.Draw(widget.session, widget.props, widget.options);
}
