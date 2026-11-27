import * as NativeShape from "kryon/shape";
import * as Widgets from "kryon/tsx/widget";

export function Shape(widget: Widgets.WidgetProps<NativeShape.Props>): s32 {
    return NativeShape.Draw(widget.session, widget.props);
}

export function Circle(widget: Widgets.WidgetProps<NativeShape.CircleProps>): s32 {
    return NativeShape.Circle(widget.session, widget.props);
}

export function Ring(widget: Widgets.WidgetProps<NativeShape.RingProps>): s32 {
    return NativeShape.Ring(widget.session, widget.props);
}

export function Triangle(widget: Widgets.WidgetProps<NativeShape.TriangleProps>): s32 {
    return NativeShape.Triangle(widget.session, widget.props);
}
