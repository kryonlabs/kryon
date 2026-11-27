import * as NativeTextField from "kryon/text_field";
import { Session } from "kryon/session";
import * as Widgets from "kryon/tsx/widget";

export interface TextFieldProps {
    session: Session;
    props: NativeTextField.Props;
    state: NativeRef<NativeTextField.State>;
}

export function TextField(widget: TextFieldProps, location: Source_Code_Location = nativeCallerLocation()): NativeTextField.BindingResult {
    return NativeTextField.Bind(widget.session, widget.state, widget.props, true, location);
}

export function ControlledTextField(widget: Widgets.WidgetProps<NativeTextField.Props>, location: Source_Code_Location = nativeCallerLocation()): NativeTextField.Result {
    return NativeTextField.Draw(widget.session, widget.props, location);
}
