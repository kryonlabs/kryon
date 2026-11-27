import * as NativeShortcutRecorder from "kryon/shortcut_recorder";
import * as Widgets from "kryon/tsx/widget";

export interface ShortcutRecorderProps {
    widget: Widgets.WidgetProps<NativeShortcutRecorder.Props>;
    state: NativeRef<NativeShortcutRecorder.ShortcutRecorderState>;
}

export function ShortcutRecorder(view: ShortcutRecorderProps): NativeShortcutRecorder.ShortcutRecorderResult {
    return NativeShortcutRecorder.Draw(view.widget.session, view.widget.props, view.state);
}
