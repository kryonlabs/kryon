// {{name}}: a keyboard-driven task list for the terminal.
//
//   kryon run            in this terminal (Up/Down move, Space toggles,
//                        Ctrl+C quits)
//   kryon run desktop    the same interface in a window
//
// Frame runs for every key and composes the interface from the tasks in
// src/tasks.zi.
import * as App from "kryon/app";
import { Rectangle } from "kryon/geometry";
import { Props as ListBoxProps } from "kryon/list_box";
import { FontSize, NextRow } from "kryon/PageLayout";
import { Session } from "kryon/session";
import { KeyboardTake, TypedCodepointTake } from "kryon/tree_input";
import { ListBox } from "kryon/tsx/list_box";
import { Text } from "kryon/tsx/text";
import * as Model from "tasks";

// One row-sized line of text.
interface LabelProps { session: Session; text: string; bounds: Rectangle; size: s32; }

function Label(label: LabelProps): void {
    <Text session={label.session} props={{text: label.text, bounds: label.bounds,
        size: label.size}}/>;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    if (frame.first) { Model.Start(); }
    const cursor = Model.Page();
    // Keyboard first: Space toggles before this frame's rows are built.
    const move = Model.HandleKey(KeyboardTake(session), TypedCodepointTake(session));
    Model.StartPage(frame.viewport);
    Label({session: session, text: "{{name}}", bounds: NextRow(cursor, 36.0),
        size: FontSize(cursor, s32(28))});
    const rows = Model.Rows();
    Label({session: session, text: Model.Summary(), bounds: NextRow(cursor, 22.0),
        size: FontSize(cursor, s32(16))});
    const props: ListBoxProps = {id: s32(10), bounds: NextRow(cursor, 6.0 * 36.0),
        row_height: s32(36), selected_index: Model.Selection(), scroll_offset: Model.Scroll(),
        focused: true, navigation: move};
    if (cursor.cells) {
        // Six one-cell rows; Kryon measures the list in pixels, and a cell
        // is eight of them.
        props.bounds.height = 6.0;
        props.row_height = s32(8);
        props.scale = 0.125;
        cursor.y = cursor.y + 5.0;
    }
    const result = ListBox({session: session, props: props, items: rows});
    Model.SetSelection(result.selected_index);
    Model.SetScroll(result.scroll_offset);
    Label({session: session, text: "Up/Down move  Space toggles  Ctrl+C quits",
        bounds: NextRow(cursor, 22.0), size: FontSize(cursor, s32(14))});
}

export function main(): s32 {
    return App.Run({title: "{{name}}"}, Frame);
}
