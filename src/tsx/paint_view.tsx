import * as NativePaint from "kryon/paint_queue";
import * as Widgets from "kryon/tsx/widget";

// The native frame queue controls paint order and owns the queued data.
export function CustomPaint(widget: Widgets.WidgetProps<NativePaint.QueuedPaint>): void {
    NativePaint.SubmitPaint(widget.session, widget.props);
}
