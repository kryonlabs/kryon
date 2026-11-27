import * as NativeSwipeRow from "kryon/swipe_row";
import * as Widgets from "kryon/tsx/widget";

export interface SwipeRowProps {
    widget: Widgets.WidgetProps<NativeSwipeRow.Props>;
    state: NativeSwipeRow.SwipeRowState;
}

export function SwipeRow(view: SwipeRowProps): NativeSwipeRow.SwipeRowResult {
    return NativeSwipeRow.Draw(view.widget.session, view.widget.props, view.state);
}
