import * as NativeBadge from "kryon/badge";
import * as Widgets from "kryon/tsx/widget";

export function Badge(widget: Widgets.WidgetProps<NativeBadge.Props>): NativeBadge.BadgeResult {
    return NativeBadge.Draw(widget.session, widget.props);
}
