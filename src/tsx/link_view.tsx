import * as NativeLink from "kryon/link";
import * as Widgets from "kryon/tsx/widget";

export function Link(widget: Widgets.WidgetProps<NativeLink.Props>): NativeLink.LinkResult {
    return NativeLink.Draw(widget.session, widget.props);
}
