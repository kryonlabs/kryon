import * as NativeJoystick from "kryon/joystick";
import * as Widgets from "kryon/tsx/widget";

export function Joystick(widget: Widgets.WidgetProps<NativeJoystick.Props>): NativeJoystick.JoystickResult {
    return NativeJoystick.Draw(widget.session, widget.props);
}

export function SwipeArea(widget: Widgets.WidgetProps<NativeJoystick.SwipeAreaProps>): NativeJoystick.SwipeAreaResult {
    return NativeJoystick.SwipeArea(widget.session, widget.props);
}

export function SwipeFeedback(widget: Widgets.WidgetProps<NativeJoystick.SwipeFeedbackProps>): void {
    NativeJoystick.SwipeFeedback(widget.session, widget.props);
}
