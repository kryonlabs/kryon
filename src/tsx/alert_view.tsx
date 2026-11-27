import * as NativeAlert from "kryon/alert";
import { Session } from "kryon/session";

export interface AlertProps {
    session: Session;
    props: NativeAlert.Props;
    state: NativeRef<NativeAlert.AlertState>;
}

export function Alert(widget: AlertProps): NativeAlert.AlertResult {
    return NativeAlert.Draw(widget.session, widget.props, widget.state);
}
