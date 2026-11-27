import * as NativeGuidedTour from "kryon/guided_tour";
import { Session } from "kryon/session";

export interface GuidedTourProps {
    session: Session;
    props: NativeGuidedTour.Props;
    state: NativeRef<NativeGuidedTour.GuidedTourState>;
}

export function GuidedTour(widget: GuidedTourProps): NativeGuidedTour.GuidedTourResult {
    return NativeGuidedTour.Draw(widget.session, widget.props, widget.state);
}
