import * as NativeListBox from "kryon/list_box";
import { Session } from "kryon/session";

export interface ListBoxProps {
    session: Session;
    props: NativeListBox.Props;
    items: NativeListBox.ListBoxItem[];
}

export function ListBox(widget: ListBoxProps): NativeListBox.ListBoxResult {
    return NativeListBox.Draw(widget.session, widget.props, widget.items);
}

export interface ListBoxMultiProps {
    session: Session;
    props: NativeListBox.ListBoxMultiProps;
    items: NativeListBox.ListBoxItem[];
    selected: boolean[];
}

export function ListBoxMulti(widget: ListBoxMultiProps): NativeListBox.ListBoxMultiResult {
    return NativeListBox.ListBoxMulti(widget.session, widget.props, widget.items, widget.selected);
}
