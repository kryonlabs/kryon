import * as NativeCountdown from "kryon/countdown";
import { Session } from "kryon/session";

export interface CountdownProps {
    session: Session;
    props: NativeCountdown.Props;
    state: NativeRef<NativeCountdown.CountdownState>;
}

export function Countdown(widget: CountdownProps): NativeCountdown.CountdownResult {
    return NativeCountdown.Draw(widget.session, widget.props, widget.state);
}
