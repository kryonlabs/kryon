import * as NativeSkeleton from "kryon/skeleton";
import * as Widgets from "kryon/tsx/widget";

export function Skeleton(widget: Widgets.WidgetProps<NativeSkeleton.Props>): void {
    NativeSkeleton.Draw(widget.session, widget.props);
}
