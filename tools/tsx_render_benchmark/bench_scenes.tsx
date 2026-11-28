import { Session } from "kryon/session";
import { Row, Column } from "kryon/tsx/layout";
import { Button } from "kryon/tsx/button";
import { Text } from "kryon/tsx/text";
import { ControlledTextField } from "kryon/tsx/text_field";
import { ListBox } from "kryon/tsx/list_box";
import * as Types from "component_types";
import * as Model from "bench_model";
import * as Component from "component_runtime";
import { Scoped } from "std/callable";
import * as Starter from "web_starter";

interface FrameProps { session: Session; scenario: s32; frame: s32; }
interface RowProps { session: Session; index: s32; }
interface CounterProps { session: Session; id: s32; slot: s32; frame: s32; key: Types.Key; }

function Toolbar(session: Session): void {
    <Row session={session} props={Model.LayoutProps(u64(1), s32(8))}>
        <Button session={session} props={Model.ButtonProps(u64(2), "App icon")}/>
        <Button session={session} props={Model.ButtonProps(u64(3), "Harness selector")}/>
        <Text session={session} props={Model.TextProps(u64(4), "Harmony")}/>
        <Button session={session} props={Model.ButtonProps(u64(5), "New task")}/>
    </Row>;
}

function Counter(view: FrameProps): void {
    <Row session={view.session} props={Model.LayoutProps(u64(1), s32(8))}>
        {Model.CounterClick(view.scenario, Button({session: view.session, props: Model.ButtonProps(u64(2), "Add")}))}
        <Text session={view.session} props={Model.TextProps(u64(3), "Count:")}/>
        <Text session={view.session} props={Model.NumberProps(u64(4), Model.CounterValue(view.scenario, view.frame), s32(0))}/>
    </Row>;
}

function ListRow(row: RowProps): void {
    <Row session={row.session} props={Model.LayoutProps(Model.RowKey(row.index), s32(6))}>
        <Text session={row.session} props={Model.TextProps(u64(3), "Row")}/>
        <Text session={row.session} props={Model.NumberProps(u64(4), row.index + s32(1), row.index)}/>
        <Button session={row.session} props={Model.ButtonProps(u64(5), "Open")}/>
    </Row>;
}

function Rows(view: FrameProps): void {
    for (let index = s32(0); index < Model.RowCount(view.scenario); index += s32(1)) {
        <ListRow session={view.session} index={index}/>;
    }
}

function StatefulCounter(counter: CounterProps, origin: Source_Code_Location = nativeCallerLocation()): void {
    const body: Scoped<Component.Body> = (): void => {
        const clicked = Button({session: counter.session, props: Model.InstanceButton(counter.id, counter.slot)});
        const value = Model.InstanceState(counter.session, counter.id, counter.frame, clicked);
        <Text session={counter.session} props={Model.InstanceLabel(counter.id, counter.slot, value)}/>;
    };
    Component.Compose({session: counter.session, component: "StatefulCounter", key: counter.key, children: body}, origin);
}

function Instances(view: FrameProps): void {
    const count = Model.InstanceCount(view.scenario, view.frame);
    for (let slot = s32(0); slot < count; slot += s32(1)) {
        const id = Model.InstanceId(slot, count, view.scenario, view.frame);
        <StatefulCounter session={view.session} id={id} slot={slot} frame={view.frame} key={Model.InstanceKey(id)}/>;
    }
}

export function Compose(session: Session, scenario: s32, frame: s32): void {
    const view: FrameProps = {session: session, scenario: scenario, frame: frame};
    if (scenario === Model.Toolbar) { Toolbar(session); }
    else if (scenario === Model.ChangingCounter || scenario === Model.EventCounter) { Counter(view); }
    else if (scenario === Model.List128 || scenario === Model.LargeList) {
        <Column session={session} props={Model.LayoutProps(u64(1), s32(2))}>
            {Rows(view)}
        </Column>;
    } else if (scenario === Model.ControlledText) {
        const edit = ControlledTextField({session: session, props: Model.EditorProps(session)});
        Model.Edit(edit);
    } else if (scenario === Model.Independent || scenario === Model.KeyedChurn) { Instances(view); }
    else if (scenario === Model.VirtualList) {
        <ListBox session={session} props={Model.ListProps(frame)} items={Model.Items()}/>;
    } else {
        Starter.Frame({slot: session.slot, epoch: session.epoch, viewport: Model.Viewport, first: frame === s32(0)});
    }
}
