import * as NativeEmptyState from "kryon/empty_state";
import * as Widgets from "kryon/tsx/widget";

export function EmptyState(widget: Widgets.WidgetProps<NativeEmptyState.Props>): NativeEmptyState.EmptyStateResult {
    return NativeEmptyState.Draw(widget.session, widget.props);
}
