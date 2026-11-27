import * as Sessions from "kryon/session";

// Supply the draw session alongside the native widget's checked props.
export interface WidgetProps<Native> {
    session: Sessions.Session;
    props: Native;
}
