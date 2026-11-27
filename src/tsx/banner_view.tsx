import * as NativeBanner from "kryon/banner";
import { Session } from "kryon/session";

export interface BannerProps {
    session: Session;
    props: NativeBanner.Props;
    state: NativeRef<NativeBanner.BannerState>;
}

export function Banner(widget: BannerProps): NativeBanner.BannerResult {
    return NativeBanner.Draw(widget.session, widget.props, widget.state);
}
