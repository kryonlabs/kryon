import * as NativeRouter from "kryon/router";
import * as Routes from "kryon/router_props";
import { Session } from "kryon/session";

export interface RouterProps {
    session: Session;
    props: Routes.RouterProps;
    state: Routes.RouterState;
    routes: Routes.RouterRoute[];
    hash: string;
    basePath: string;
    routeVersion: s32;
}

export function Router(widget: RouterProps): Routes.RouterResult {
    return NativeRouter.Router(widget.session, widget.props, widget.state,
        widget.routes, widget.hash, widget.basePath, widget.routeVersion);
}
