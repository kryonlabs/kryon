import * as NativeSheet from "kryon/sheet";
import { Session } from "kryon/session";

export interface SheetProps {
    session: Session;
    props: NativeSheet.Props;
    state: NativeRef<NativeSheet.SheetState>;
}

export function Sheet(widget: SheetProps): NativeSheet.SheetResult {
    return NativeSheet.Draw(widget.session, widget.props, widget.state);
}
