import * as NativePanedView from "kryon/paned_view";
import * as Widgets from "kryon/tsx/widget";

export function PanedView(widget: Widgets.WidgetProps<NativePanedView.Props>): NativePanedView.PanedViewResult {
    return NativePanedView.Draw(widget.session, widget.props);
}
