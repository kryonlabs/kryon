import * as NativeButtonGroup from "kryon/button_group";
import * as NativeButton from "kryon/button";
import { Session } from "kryon/session";

export interface ButtonGroupProps {
    session: Session;
    props: NativeButtonGroup.Props;
    items: NativeButton.Props[];
    state: NativeRef<NativeButtonGroup.ButtonGroupState>;
}

export function ButtonGroup(widget: ButtonGroupProps): NativeButtonGroup.ButtonGroupResult {
    return NativeButtonGroup.Draw(widget.session, widget.props, widget.items, widget.state);
}
