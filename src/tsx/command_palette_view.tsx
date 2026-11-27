import * as NativeCommandPalette from "kryon/command_palette";
import { Session } from "kryon/session";

export interface CommandPaletteProps {
    session: Session;
    props: NativeCommandPalette.Props;
    state: NativeRef<NativeCommandPalette.CommandPaletteState>;
}

export function CommandPalette(widget: CommandPaletteProps): NativeCommandPalette.CommandPaletteResult {
    return NativeCommandPalette.Draw(widget.session, widget.props, widget.state);
}
