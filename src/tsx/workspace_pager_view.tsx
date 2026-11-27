import * as NativeWorkspacePager from "kryon/workspace_pager";
import { Session } from "kryon/session";

export interface WorkspacePagerProps {
    session: Session;
    props: NativeWorkspacePager.Props;
    windows: NativeWorkspacePager.Window[];
}

export function WorkspacePager(widget: WorkspacePagerProps): s32 {
    return NativeWorkspacePager.Draw(widget.session, widget.props, widget.windows);
}
