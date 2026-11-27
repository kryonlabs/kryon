import {ButtonEmphasis} from "kryon/control_props";
import {ColorFromPacked} from "kryon/drawing_props";
import {Rectangle} from "kryon/geometry";
import {ImageFit} from "kryon/image";
import {OpenUri} from "kryon/open_uri";
import {Session} from "kryon/session";
import {SetReducedMotion} from "kryon/theme";
import {TransitionSmoothProgress} from "kryon/transition";
import {Box} from "kryon/tsx/box";
import {Button} from "kryon/tsx/button";
import {Image} from "kryon/tsx/image";
import {Toggle} from "kryon/tsx/toggle";
import {InstallGalleryPalette} from "catalog";
import * as Scenes from "scenes";
import {SiteApps} from "site_apps";
import {SiteComponents} from "site_components";
import {SiteOverview, SiteReading} from "site_landing";
import * as State from "site_state";
import {SiteAction, SitePanel} from "site_widgets";

interface SiteHeaderProps {
    session: Session;
    viewport: Rectangle;
    height: float32;
    wide: boolean;
}

function SiteHeader(header: SiteHeaderProps): void {
    const session = header.session;
    const viewport = header.viewport;
    const height = header.height;
    const wide = header.wide;
    nativeSet(nativeRef(State.site_modal_active), false);
    <Box session={session} props={{key: u64(10001n), bounds: {width: viewport.width, height: height},
        fill: State.SiteBackground()}}/>;
    <Box session={session} props={{key: u64(10320n), bounds: {y: height - 1.0, width: viewport.width, height: 1},
        fill: State.SiteBorder()}}/>;
    <Image session={session} props={{key: u64(10004n), asset_path: "assets/brand/mascot-dragon.png",
        alt_text: "Kryon dragon", bounds: {x: 16, y: 10, width: 34, height: 34},
        fit: ImageFit.ImageFitContain, smooth: true}}/>;
    if (Button({session: session, props: {key: u64(10002n), bounds: {x: 50, y: 8, width: 68, height: 38},
        label: "Kryon", class_name: s32(11010), emphasis: ButtonEmphasis.ButtonEmphasisGhost,
        disabled: State.site_modal_active}})) { State.SiteNavigate(s32(0)); }
    const toggle = Toggle({session: session, props: {key: u64(10003n),
        bounds: {x: viewport.width - 108.0, y: 10, width: 92, height: 36}, label: "Dark",
        value: Scenes.dark, disabled: State.site_modal_active}});
    nativeSet(nativeRef(Scenes.dark), toggle.value);
    const tabWidth: float32 = wide ? 96.0 : (viewport.width - 32.0) / 3.0;
    const navX: float32 = wide ? 124.0 : 16.0;
    const navY: float32 = wide ? 10.0 : 56.0;
    if (wide) {
        SiteAction(session, u64(10010n), {x: navX, y: navY, width: tabWidth, height: 36}, "Overview", s32(0));
        SiteAction(session, u64(10011n), {x: navX + tabWidth, y: navY, width: tabWidth, height: 36}, "Guide", s32(1));
        SiteAction(session, u64(10012n), {x: navX + tabWidth * 2.0, y: navY, width: tabWidth, height: 36}, "Components", s32(2));
        SiteAction(session, u64(10014n), {x: navX + tabWidth * 3.0, y: navY, width: tabWidth, height: 36}, "Modules", s32(6));
    } else {
        SiteAction(session, u64(10014n), {x: navX, y: navY, width: tabWidth, height: 36}, "Modules", s32(6));
        SiteAction(session, u64(10012n), {x: navX + tabWidth, y: navY, width: tabWidth, height: 36}, "Components", s32(2));
        SiteAction(session, u64(10020n), {x: navX + tabWidth * 2.0, y: navY, width: tabWidth, height: 36}, "Apps", s32(3));
    }
    if (viewport.width >= 1000.0) {
        SiteAction(session, u64(10020n), {x: 508, y: 10, width: 72, height: 36}, "Apps", s32(3));
        SiteAction(session, u64(10021n), {x: 588, y: 10, width: 72, height: 36}, "Docs", s32(4));
        if (Button({session: session, props: {key: u64(10322n),
            bounds: {x: viewport.width - 208.0, y: 10, width: 88, height: 36}, label: "Source  ↗",
            class_name: s32(11010), emphasis: ButtonEmphasis.ButtonEmphasisGhost}})) {
            OpenUri("https://git.waozi.xyz/kryonlabs/kryon");
        }
    }
    let activeTab = s32(-1);
    if (wide && State.site_page < s32(3)) { activeTab = State.site_page; }
    if (State.site_page === s32(6)) { activeTab = wide ? s32(3) : s32(0); }
    if (!wide && State.site_page === s32(2)) { activeTab = s32(1); }
    if (!wide && State.site_page === s32(3)) { activeTab = s32(2); }
    const target = float32(activeTab >= s32(0) ? activeTab : s32(0));
    if (!State.site_marker_ready) {
        nativeSet(nativeRef(State.site_marker), target);
        nativeSet(nativeRef(State.site_marker_from), target);
        nativeSet(nativeRef(State.site_marker_target), target);
        nativeSet(nativeRef(State.site_marker_ready), true);
    }
    if (target !== State.site_marker_target) {
        nativeSet(nativeRef(State.site_marker_from), State.site_marker);
        nativeSet(nativeRef(State.site_marker_target), target);
        nativeSet(nativeRef(State.site_marker_started), State.site_now);
    }
    const progress = TransitionSmoothProgress((State.site_now - State.site_marker_started) / 0.22);
    nativeSet(nativeRef(State.site_marker), State.site_marker_from + (State.site_marker_target - State.site_marker_from) * progress);
    if (activeTab >= s32(0)) {
        <Box session={session} props={{key: u64(10013n),
            bounds: {x: navX + State.site_marker * tabWidth + 18.0, y: height - 2.0, width: tabWidth - 36.0, height: 2},
            fill: ColorFromPacked(Scenes.dark ? u32(0xf2f2f2ff) : u32(0x171717ff))}}/>;
    }
    const resourcesX: float32 = viewport.width >= 1000.0 ? 678.0 : viewport.width - 190.0;
    if (Button({session: session, props: {key: u64(10520n), bounds: {x: resourcesX, y: 10, width: 78, height: 36},
        label: "More", class_name: s32(11010), emphasis: ButtonEmphasis.ButtonEmphasisGhost,
        disabled: State.site_modal_active}})) { nativeSet(nativeRef(State.site_resources_open), !State.site_resources_open); }
    if (State.site_resources_open) {
        const x = viewport.width - 230.0;
        SitePanel(session, u64(10521n), {x: x, y: 54, width: 214, height: 202});
        const labels: [string, string, string, string] = ["Apps", "Documentation", "Platforms", "Guide"];
        for (let i = s32(0); i < s32(4); i += s32(1)) {
            const page = i === s32(3) ? s32(1) : i + s32(3);
            SiteAction(session, u64(s32(10522) + i),
                {x: x + 10.0, y: 66.0 + float32(i) * 44.0, width: 194, height: 38}, labels[i], page);
        }
    }
}

export function SiteFrame(session: Session, viewport: Rectangle): s32 {
    nativeSet(nativeRef(State.site_preview_visible), false);
    nativeSet(nativeRef(State.site_native_visible), false);
    InstallGalleryPalette(Scenes.dark);
    State.SiteStyle();
    SetReducedMotion(State.site_reduced_motion);
    <Box session={session} props={{key: u64(10000n), bounds: viewport, fill: State.SiteBackground()}}/>;
    const wide = viewport.width >= 760.0;
    const height: float32 = wide ? 56.0 : 104.0;
    const view: Rectangle = nativeInit<Rectangle>({y: height, width: viewport.width, height: viewport.height - height});
    nativeSet(nativeRef(State.site_preview_clip), view);
    if (State.site_page === s32(0)) { SiteOverview(session, view); }
    else if (State.site_page === s32(2)) { SiteComponents(session, view); }
    else if (State.site_page === s32(3)) { SiteApps(session, view); }
    else { SiteReading(session, view); }
    <SiteHeader session={session} viewport={viewport} height={height} wide={wide}/>;
    return s32(0);
}
