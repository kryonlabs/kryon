import * as NativeSegmentedControl from "kryon/segmented_control";
import { Session } from "kryon/session";

export interface SegmentedControlProps {
    session: Session;
    props: NativeSegmentedControl.Props;
    options: NativeSegmentedControl.SegmentOption[];
}

export function SegmentedControl(widget: SegmentedControlProps): NativeSegmentedControl.SegmentedControlResult {
    return NativeSegmentedControl.Draw(widget.session, widget.props, widget.options);
}
