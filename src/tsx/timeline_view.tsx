import * as NativeTimeline from "kryon/timeline";
import * as Widgets from "kryon/tsx/widget";

export function Timeline(widget: Widgets.WidgetProps<NativeTimeline.Props>): NativeTimeline.TimelineResult {
    return NativeTimeline.Draw(widget.session, widget.props);
}
