import * as Model from "model";
import * as S from "dashboard_state";
import * as Styles from "styles";
import * as Layout from "kryon/layout";
import { Session } from "kryon/session";
import { Rectangle } from "kryon/geometry";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { TreeSubmittedBounds } from "kryon/tree";
import { StyleKindColumn } from "kryon/style_sheet";
import { ButtonTone, ButtonEmphasis, ControlSize, ContentAlignment } from "kryon/control_props";
import { SemanticKind } from "kryon/semantic";
import * as NativeText from "kryon/text";
import * as NativeBadge from "kryon/badge";
import * as NativeAlert from "kryon/alert";
import * as NativeScroll from "kryon/scroll";
import * as NativeModal from "kryon/modal";
import { Text } from "kryon/tsx/text";
import { Badge } from "kryon/tsx/badge";
import { Button } from "kryon/tsx/button";
import { Checkbox } from "kryon/tsx/checkbox";
import { Toggle } from "kryon/tsx/toggle";
import { Progress } from "kryon/tsx/progress";
import { Separator } from "kryon/tsx/separator";
import { LoadingIndicator } from "kryon/tsx/loading_indicator";
import { Alert } from "kryon/tsx/alert";
import { EmptyState } from "kryon/tsx/empty_state";
import { TextField } from "kryon/tsx/text_field";
import { Scroll } from "kryon/tsx/scroll";
import { ResolvedModal } from "kryon/tsx/modal";
import { Column, Row, ResolvedColumn, ResolvedStack, MeasuredRow } from "kryon/tsx/layout";
import { Grid } from "kryon/tsx/grid";

function Class(name: string): s32 { return Styles.ClassId(name); }
function Line(session: Session, text: string, className: string): void {
    <Text session={session} props={{text: text, class_name: Class(className), wrap: NativeText.Wrap.TextWrapEllipsis}}/>;
}
function Heading(session: Session, text: string, className: string, level: s32): void {
    <Text session={session} props={{text: text, class_name: Class(className), semantic_kind: SemanticKind.SemanticHeading,
            heading_level: level, wrap: NativeText.Wrap.TextWrapEllipsis}}/>;
}
function Chip(session: Session, label: string, selected: boolean): boolean {
    return Button({session: session, props: {class_name: Class("chip"), label: label, size: ControlSize.ControlSizeSmall,
            selected: selected, fit_content: true, emphasis: selected ? ButtonEmphasis.ButtonEmphasisSoft : ButtonEmphasis.ButtonEmphasisOutline,
            tone: selected ? ButtonTone.ButtonToneAccent : ButtonTone.ButtonToneNeutral}});
}
function Nav(session: Session, wide: boolean): void {
    for (let i: s32 = s32(0); i < s32(4); i += s32(1)) {
        const selected = s32(S.active_section) === i;
        if (Button({session: session, props: {class_name: Class(wide ? "nav-item" : "chip"), label: nativeAt(S.NAV_LABELS, i),
                selected: selected, content_alignment: ContentAlignment.ContentAlignmentStart,
                emphasis: selected ? ButtonEmphasis.ButtonEmphasisSoft : ButtonEmphasis.ButtonEmphasisGhost,
                tone: selected ? ButtonTone.ButtonToneAccent : ButtonTone.ButtonToneNeutral, full_width: true}})) { S.ChooseSection(i); }
    }
}
function SettingsButtons(session: Session, light: string, dark: string): void {
    if (Button({session: session, props: {class_name: Class("chip"), label: S.dark_theme ? light : dark,
            emphasis: ButtonEmphasis.ButtonEmphasisOutline, fit_content: true}})) { S.dark_theme = !S.dark_theme; }
    if (Button({session: session, props: {class_name: Class("chip"), label: "Settings",
            emphasis: ButtonEmphasis.ButtonEmphasisSoft, fit_content: true}})) { S.OpenSettingsModal(); }
}
function Sidebar(session: Session): void {
    <Column session={session} props={{class_name: Class("sidebar")}} children={(): void => {
        Heading(session, "Pulseboard", "brand", s32(1));
        Line(session, S.WorkspaceName(), "muted");
        Nav(session, true);
        SettingsButtons(session, "Light mode", "Dark mode");
    }}/>;
}
function Header(session: Session, wide: boolean): void {
    <Row session={session} props={{class_name: Class("header")}} children={(): void => {
        Heading(session, Model.SectionLabel(S.active_section), "title", s32(1));
        <Row session={session} props={{class_name: Class("header-actions")}} children={(): void => {
            if (wide) {
                const states: [Model.ViewState, Model.ViewState, Model.ViewState] = [Model.ViewState.ViewReady, Model.ViewState.ViewLoading, Model.ViewState.ViewError];
                for (let i: s32 = s32(0); i < s32(3); i += s32(1)) {
                    if (Chip(session, nativeAt(S.STATE_LABELS, i), S.view_state === nativeAt(states, i))) { S.view_state = nativeAt(states, i); S.alert_state.dismissed = false; }
                }
            } else { SettingsButtons(session, "Light", "Dark"); };
        }}/>;
    }}/>;
}
function KpiCard(session: Session, title: string, value: string, badge: string, tone: NativeBadge.BadgeTone, progress: s32, most: s32): void {
    <Column session={session} props={{class_name: Class("kpi")}} children={(): void => {
        <Row session={session} props={{class_name: Class("kpi-top")}} children={(): void => {
            Line(session, title, "muted");
            <Row session={session} props={{class_name: Class("badge-slot")}} children={(): void => {
                <Badge session={session} props={{text: badge, tone: tone, size: ControlSize.ControlSizeSmall}}/>;
            }}/>;
        }}/>;
        Heading(session, value, "kpi-value", s32(3));
        <Progress session={session} props={{bounds: {height: 6}, min: s32(0), max: most, value: progress}}/>;
    }}/>;
}
function Kpis(session: Session, wide: boolean): void {
    const text = S.PrepareKpis();
    const metrics = text.metrics;
    <Grid session={session} props={{class_name: Class("kpis")}} children={(): void => {
        if (wide) {
            KpiCard(session, "Healthy services", text.healthy, metrics.offline > s32(0) ? "1 offline" : "Nominal",
            metrics.offline > s32(0) ? NativeBadge.BadgeTone.BadgeToneWarning : NativeBadge.BadgeTone.BadgeToneSuccess, metrics.healthy, metrics.total);
            KpiCard(session, "Mean p95 latency", text.latency, "EU / US", NativeBadge.BadgeTone.BadgeToneInfo, s32(200) - metrics.avg_latency_ms, s32(200));
            KpiCard(session, "Availability SLA", text.uptime, "30d", NativeBadge.BadgeTone.BadgeToneAccent, metrics.avg_uptime_tenths, s32(1000));
        } else {
            KpiCard(session, "Healthy", text.healthy, "SLA", NativeBadge.BadgeTone.BadgeToneSuccess, metrics.healthy, metrics.total);
            KpiCard(session, "Latency", text.latency, "p95", NativeBadge.BadgeTone.BadgeToneInfo, s32(200) - metrics.avg_latency_ms, s32(200));
        };
    }}/>;
}
function Toolbar(session: Session): void {
    <TextField session={session} state={nativeRef(S.search_state)} props={{focus_id: S.SEARCH_FOCUS, class_name: Class("search-field"),
            label: "Search services", placeholder: "Search services, regions, owners...", clear_label: "Clear search"}}/>;
    const filters: [Model.StatusFilter, Model.StatusFilter, Model.StatusFilter] = [Model.StatusFilter.FilterAll, Model.StatusFilter.FilterHealthy, Model.StatusFilter.FilterAttention];
    <Row session={session} props={{class_name: Class("filters")}} children={(): void => {
        for (let i: s32 = s32(0); i < s32(3); i += s32(1)) {
            if (Chip(session, nativeAt(S.FILTER_LABELS, i), s32(S.active_filter) === i)) {
                S.active_filter = nativeAt(filters, i); S.view_state = Model.ViewState.ViewReady; S.results_scroll = s32(0);
            }
        };
    }}/>;
}
function LoadingState(session: Session, now: float64): void {
    <Column session={session} props={{class_name: Class("state-card")}} children={(): void => {
        <ResolvedStack session={session} props={{bounds: {height: 32}}} children={(area: Layout.LayoutContainer): void => {
            <LoadingIndicator session={session} props={{bounds: area.content, now: now, scale: 1}}/>;
        }}/>;
        Line(session, "Refreshing service telemetry...", "state-text");
        <Column session={session} props={{class_name: Class("centered-action")}} children={(): void => {
            if (Button({session: session, props: {label: "Cancel refresh", emphasis: ButtonEmphasis.ButtonEmphasisOutline}})) { S.view_state = Model.ViewState.ViewReady; };
        }}/>;
    }}/>;
}
function ErrorState(session: Session): void {
    <ResolvedStack session={session} props={{bounds: {height: 74}}} children={(area: Layout.LayoutContainer): void => {
        <Alert session={session} state={nativeRef(S.alert_state)} props={{bounds: area.content, title: "Telemetry sync failed",
                description: "Collector endpoint eu-west-1 returned HTTP 503 Service Unavailable.", tone: NativeAlert.AlertTone.AlertToneDanger, dismissible: false}}/>;
    }}/>;
    <Row session={session} props={{class_name: Class("actions")}} children={(): void => {
        if (Button({session: session, props: {class_name: Class("primary-btn"), label: "Retry sync", tone: ButtonTone.ButtonToneAccent,
                emphasis: ButtonEmphasis.ButtonEmphasisFilled}})) { S.view_state = Model.ViewState.ViewReady; };
        if (Button({session: session, props: {label: "Open settings", emphasis: ButtonEmphasis.ButtonEmphasisOutline}})) { S.OpenSettingsModal(); };
    }}/>;
}
function ResultRows(session: Session, count: s32): void {
    Line(session, S.CountLabel(count), "muted");
    <ResolvedColumn session={session} props={{class_name: Class("scroll-area")}} children={(area: Layout.LayoutContainer): void => {
        const scroll = Scroll({session: session, props: {bounds: area.content, content_height: S.results_height, scroll_offset: S.results_scroll}, children: (view: NativeScroll.ScrollResult): void => {
            const rows = Column({session: session, props: {class_name: Class("rows"), bounds: {x: view.content.x, y: view.content.y, width: view.viewport.width}}, children: (): void => {
                for (let i: s32 = s32(0); i < count; i += s32(1)) {
                    const index = nativeAt(S.filtered_indices, i);
                    const item = nativeAt(Model.SERVICES, index);
                    const selected = index === S.selected_service;
                    if (Button({session: session, props: {key: Layout.KeyHash(item.name), class_name: Class("result-row"), label: item.name,
                            detail: S.RowDescription(index), selected: selected,
                            emphasis: selected ? ButtonEmphasis.ButtonEmphasisSoft : ButtonEmphasis.ButtonEmphasisOutline,
                            tone: selected ? ButtonTone.ButtonToneAccent : ButtonTone.ButtonToneNeutral,
                            content_alignment: ContentAlignment.ContentAlignmentStart, full_width: true}})) { S.selected_service = index; }
                };
            }});
            if (rows.opened) { S.results_height = s32(TreeSubmittedBounds(session, rows.node).height); }
        }});
        S.results_scroll = scroll.scroll_offset;
    }}/>;
}
function Inspector(session: Session, bounds: Rectangle, item: Model.ServiceItem): void {
    const text = S.PrepareInspector(item);
    <Column session={session} props={{class_name: Class("inspector"), bounds: bounds}} children={(): void => {
        <Row session={session} props={{class_name: Class("inspector-top")}} children={(): void => {
            Heading(session, item.name, "inspector-title", s32(2));
            <Row session={session} props={{class_name: Class("badge-slot")}} children={(): void => {
                <Badge session={session} props={{text: Model.HealthLabel(item.health), tone: S.HealthBadgeTone(item.health), dot: true}}/>;
            }}/>;
        }}/>;
        Line(session, text.meta, "muted");
        <Separator session={session} props={{bounds: {height: 8}}}/>;
        <Text session={session} props={{text: item.summary, class_name: Class("summary"), wrap: NativeText.Wrap.TextWrapAuto, semantic_kind: SemanticKind.SemanticParagraph}}/>;
        <Grid session={session} props={{class_name: Class("stats")}} children={(): void => {
            <Column session={session} props={{class_name: Class("stat")}} children={(): void => {
                Line(session, "Latency", "stat-name");
                Line(session, text.latency, "stat-value");
            }}/>;
            <Column session={session} props={{class_name: Class("stat")}} children={(): void => {
                Line(session, "Uptime", "stat-name");
                Line(session, text.uptime, "stat-value");
            }}/>;
            <Column session={session} props={{class_name: Class("stat")}} children={(): void => {
                Line(session, "Throughput", "stat-name");
                Line(session, text.rps, "stat-value");
            }}/>;
        }}/>;
        Line(session, "Latest change", "stat-name");
        Line(session, item.audit_note, "muted");
    }}/>;
}
function Results(session: Session, count: s32): void {
    let visible: boolean = false;
    for (let i: s32 = s32(0); i < count; i += s32(1)) { if (nativeAt(S.filtered_indices, i) === S.selected_service) { visible = true; break; } }
    if (!visible) { S.selected_service = nativeAt(S.filtered_indices, s32(0)); }
    let items: [Layout.FlexItem, Layout.FlexItem] = [{kind: StyleKindColumn(), class_name: Class("list")}, {kind: StyleKindColumn(), class_name: Class("inspector")}];
    let slots: [Layout.FlexPlacement, Layout.FlexPlacement] = [{}, {}];
    const positions = nativeSlice(slots);
    <ResolvedColumn session={session} props={{class_name: Class("body")}} children={(area: Layout.LayoutContainer): void => {
        <MeasuredRow session={session} props={{class_name: Class("split"), bounds: area.content}} items={nativeSlice(items)} placements={positions}
        children={(_split: Layout.LayoutContainer): void => {
            <Column session={session} props={{class_name: Class("list"), bounds: nativeAt(positions, s32(0)).bounds}} children={(): void => {
                ResultRows(session, count);
            }}/>;
            Inspector(session, nativeAt(positions, s32(1)).bounds, nativeAt(Model.SERVICES, S.selected_service));
        }}/>;
    }}/>;
}
function Body(session: Session, now: float64): void {
    const count = S.FilterServices();
    if (S.view_state === Model.ViewState.ViewLoading) { LoadingState(session, now); return; }
    if (S.view_state === Model.ViewState.ViewError) { ErrorState(session); return; }
    if (S.view_state === Model.ViewState.ViewEmpty || count === s32(0)) {
        <ResolvedStack session={session} props={{bounds: {height: 220}}} children={(area: Layout.LayoutContainer): void => {
            const result = EmptyState({session: session, props: {title: "No matching services",
                    description: "Adjust your search query or reset filters to view all cluster services.", action: "Reset filters", bounds: area.content}});
            if (result.action) { S.ResetSearch(); S.active_filter = Model.StatusFilter.FilterAll; S.active_section = Model.NavSection.NavOverview; S.view_state = Model.ViewState.ViewReady; }
        }}/>;
        return;
    }
    Results(session, count);
}
function SettingsModal(session: Session): void {
    const modal = ResolvedModal({session: session, props: {title: "Dashboard settings", title_at_start: true, width: s32(460), height: s32(330),
            has_right_action: true, right_label: "Close", dismiss_on_outside: true}, children: (opened: NativeModal.ModalFrameResult): void => {
        if (opened.dismissed || opened.right_clicked) { return; }
        <Column session={session} props={{class_name: Class("settings"), bounds: opened.layout.content}} children={(): void => {
            Line(session, "Workspace name", "muted");
            const focusNow = S.focus_workspace_pending; S.focus_workspace_pending = false;
            const edited = TextField({session: session, state: nativeRef(S.draft_state), props: {focus_id: S.WORKSPACE_FOCUS, class_name: Class("search-field"),
                    label: "Workspace name", placeholder: "Enter workspace title", input: {focus: focusNow}}});
            <Row session={session} props={{class_name: Class("setting")}} children={(): void => {
                Line(session, "Color theme", "setting-name");
                <Row session={session} props={{class_name: Class("toggle-slot")}} children={(): void => {
                    const toggle = Toggle({session: session, props: {label: "Dark theme", off_label: "Light", on_label: "Dark", value: S.draft_dark}});
                    if (toggle.changed) { S.draft_dark = toggle.value; };
                }}/>;
            }}/>;
            const compact = Checkbox({session: session, props: {label: "Compact density", checked: S.draft_compact}});
            if (compact.changed) { S.draft_compact = compact.checked; }
            const refresh = Checkbox({session: session, props: {label: "Auto-refresh telemetry", checked: S.draft_auto_refresh}});
            if (refresh.changed) { S.draft_auto_refresh = refresh.checked; }
            <Grid session={session} props={{class_name: Class("modal-actions")}} children={(): void => {
                if (Button({session: session, props: {label: "Cancel", emphasis: ButtonEmphasis.ButtonEmphasisOutline}})) { S.settings_open = false; };
                if (Button({session: session, props: {class_name: Class("primary-btn"), label: "Apply", tone: ButtonTone.ButtonToneAccent,
                        emphasis: ButtonEmphasis.ButtonEmphasisFilled}}) || edited.committed) { S.CommitSettingsModal(); };
            }}/>;
        }}/>;
    }});
    if (modal.dismissed || modal.right_clicked) { S.settings_open = false; }
}
export function DashboardSetFixture(name: string): void { S.DashboardSetFixture(name); }
export function DashboardFrame(session: Session, viewport: Rectangle): void {
    S.EnsureInitialized();
    const snapshot = CaptureFrameInput(session);
    Styles.EnsureDashboardTheme(S.dark_theme, S.compact_density, false, viewport.width);
    const wide = viewport.width >= 760;
    <Row session={session} props={{class_name: Class("shell"), bounds: viewport}} children={(): void => {
        if (wide) { Sidebar(session); };
        <Column session={session} props={{class_name: Class("main")}} children={(): void => {
            Header(session, wide);
            if (!wide) { <Grid session={session} props={{class_name: Class("nav-strip")}} children={(): void => {
                    Nav(session, false);
                }}/>; };
            Kpis(session, wide);
            Toolbar(session);
            Body(session, float64(snapshot.elapsed_ms) / float64(1000));
        }}/>;
    }}/>;
    if (S.settings_open) { SettingsModal(session); }
}
