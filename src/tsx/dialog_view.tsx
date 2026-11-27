import * as NativeDialog from "kryon/Dialog";
import * as NativeListBox from "kryon/list_box";
import { Session } from "kryon/session";

export interface DialogProps {
    session: Session;
    props: NativeDialog.DialogProps;
    items: NativeListBox.ListBoxItem[];
}

export function Dialog(widget: DialogProps): NativeDialog.DialogResult {
    return NativeDialog.Dialog(widget.session, widget.props, widget.items);
}
