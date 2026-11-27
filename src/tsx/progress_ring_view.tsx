import * as NativeProgressRing from "kryon/progress_ring";
import { Session } from "kryon/session";

export interface ProgressRingProps {
    session: Session;
    props: NativeProgressRing.Props;
    state: NativeRef<NativeProgressRing.ProgressRingState>;
}

export function ProgressRing(widget: ProgressRingProps): NativeProgressRing.ProgressRingResult {
    return NativeProgressRing.Draw(widget.session, widget.props, widget.state);
}
