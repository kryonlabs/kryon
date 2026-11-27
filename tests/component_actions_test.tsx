// Owned action behavior is shared with the source/saved lifecycle matrix.
import * as Model from "component_actions_model";
import * as Life from "component_lifecycle";
import * as Types from "component_types";
import * as Actions from "component_actions";
import * as Component from "component_runtime";
import * as Box from "box";
import { Session } from "session";
import { Owned, Scoped } from "std/callable";
import { Text, TextCopy } from "std/vec";

interface CounterProps { session: Session; initial: s32; row: number; key: Types.Key; }
function Counter(props: CounterProps, origin: Source_Code_Location = #caller_location): void {
    const body: Scoped<Component.Body> = (): void => {
        const view: Model.CounterView = Model.UseCounter(props.session, props.initial);
        Model.ObserveCounter(view, origin);
        Box.Draw(props.session, {key: u64(7), bounds: {
            x: float32(0), y: float32(props.row * 12),
            width: float32(20), height: float32(10)
        }});
    };
    Component.Compose({session: props.session, component: "Counter", key: props.key, children: body}, origin);
}
function SeparateSites(session: Session, first: boolean, second: boolean): void {
    const unkeyed: Types.Key = {specified: false, value: u64(0)};
    // Intentionally one line: the columns must distinguish these authored calls.
    if (first) { <Counter session={session} initial={s32(10)} row={0} key={unkeyed}/>; } if (second) { <Counter session={session} initial={s32(20)} row={1} key={unkeyed}/>; }
}
function KeyedScreen(session: Session, reverse: boolean, count: s32, duplicate: boolean): void {
    for (let index: s32 = s32(0); index < count; index += s32(1)) {
        const key: u64 = duplicate ? u64(3) : reverse ? u64(s32(2) - index) : u64(index + s32(1));
        <Counter session={session} initial={s32(key) * s32(10)} row={number(index)} key={{specified: true, value: key}}/>;
    }
}
function CounterGroup(props: CounterProps, origin: Source_Code_Location = #caller_location): void {
    const body: Scoped<Component.Body> = (): void => {
        <Counter session={props.session} initial={props.initial} row={props.row} key={{specified: false, value: u64(0)}}/>;
    };
    Component.Compose({session: props.session, component: "CounterGroup", key: props.key, children: body}, origin);
}
function NestedScreen(session: Session, reverse: boolean, count: s32): void {
    for (let index: s32 = s32(0); index < count; index += s32(1)) {
        const key: u64 = reverse ? u64(s32(2) - index) : u64(index + s32(1));
        <CounterGroup session={session} initial={s32(key) * s32(10)} row={number(index)} key={{specified: true, value: key}}/>;
    }
}
function CounterChecks(): s32 {
    const session: Session = Model.ScreenOpen();
    Model.ScreenStart(session);
    SeparateSites(session, true, true);
    if (!Model.ScreenFinish(session) || Model.CounterCount() !== s32(2)) { return s32(20); }
    const first: Model.CounterView = Model.CounterAt(s32(0));
    const second: Model.CounterView = Model.CounterAt(s32(1));
    const firstSite: Source_Code_Location = Model.CounterOrigin(s32(0));
    const secondSite: Source_Code_Location = Model.CounterOrigin(s32(1));
    if (firstSite.fully_pathed_filename !== secondSite.fully_pathed_filename ||
        firstSite.line_number !== secondSite.line_number || firstSite.column_number <= s64(0) ||
        secondSite.column_number <= firstSite.column_number) { return s32(35); }
    const firstNode: u64 = Model.ScreenIdentity(session, s32(1));
    const secondNode: u64 = Model.ScreenIdentity(session, s32(2));
    if (first.handle.token.owner.identity === second.handle.token.owner.identity || firstNode === secondNode ||
        !Model.CounterSet(first, s32(11))) { return s32(21); }
    Model.ScreenStart(session);
    SeparateSites(session, true, true);
    if (!Model.ScreenFinish(session) || Model.CounterAt(s32(0)).count !== s32(11) ||
        Model.CounterAt(s32(1)).count !== s32(20) || Model.ScreenIdentity(session, s32(1)) !== firstNode ||
        Model.ScreenIdentity(session, s32(2)) !== secondNode) { return s32(22); }
    Model.ScreenStart(session);
    SeparateSites(session, false, true);
    if (!Model.ScreenFinish(session) || Model.CounterCleanups() !== s32(1) ||
        Model.CounterSet(first, s32(99)) || Model.CounterAt(s32(0)).handle.token.owner.identity !== second.handle.token.owner.identity ||
        Model.ScreenIdentity(session, s32(1)) !== secondNode || !Model.ScreenClose(session) ||
        Model.CounterCleanups() !== s32(2) || Model.CounterSet(second, s32(99))) { return s32(23); }

    const keyed: Session = Model.ScreenOpen();
    Model.ScreenStart(keyed);
    KeyedScreen(keyed, false, s32(2), false);
    if (!Model.ScreenFinish(keyed)) { return s32(24); }
    const one: Model.CounterView = Model.CounterAt(s32(0));
    const two: Model.CounterView = Model.CounterAt(s32(1));
    const oneNode: u64 = Model.ScreenIdentity(keyed, s32(1));
    const twoNode: u64 = Model.ScreenIdentity(keyed, s32(2));
    if (!Model.CounterSet(one, s32(13)) || !Model.CounterSet(two, s32(27))) { return s32(25); }
    Model.ScreenStart(keyed);
    KeyedScreen(keyed, true, s32(2), false);
    if (!Model.ScreenFinish(keyed) || Model.CounterAt(s32(0)).count !== s32(27) ||
        Model.CounterAt(s32(1)).count !== s32(13) || Model.ScreenIdentity(keyed, s32(1)) !== twoNode ||
        Model.ScreenIdentity(keyed, s32(2)) !== oneNode) { return s32(26); }
    Model.ScreenStart(keyed);
    KeyedScreen(keyed, false, s32(2), true);
    if (Model.ScreenFinish(keyed) || Model.CounterCleanups() !== s32(3) ||
        Model.ScreenIdentity(keyed, s32(1)) !== twoNode || Model.ScreenIdentity(keyed, s32(2)) !== oneNode) { return s32(27); }
    Model.ScreenStart(keyed);
    KeyedScreen(keyed, true, s32(1), false);
    if (!Model.ScreenFinish(keyed) || Model.CounterAt(s32(0)).count !== s32(27) ||
        Model.CounterCleanups() !== s32(4) || Model.CounterSet(one, s32(99)) ||
        Model.ScreenIdentity(keyed, s32(1)) !== twoNode) { return s32(28); }
    Model.ScreenStart(keyed);
    KeyedScreen(keyed, false, s32(2), false);
    if (!Model.ScreenFinish(keyed) || Model.CounterAt(s32(0)).count !== s32(10) ||
        Model.CounterAt(s32(1)).count !== s32(27) || Model.CounterAt(s32(0)).handle.token.owner.identity === one.handle.token.owner.identity ||
        !Model.ScreenClose(keyed) || Model.CounterCleanups() !== s32(6) || !Model.CounterCleanupValid() ||
        Model.CounterSet(two, s32(99))) { return s32(29); }

    const nested: Session = Model.ScreenOpen();
    Model.ScreenStart(nested);
    NestedScreen(nested, false, s32(2));
    if (!Model.ScreenFinish(nested)) { return s32(30); }
    const childOne: Model.CounterView = Model.CounterAt(s32(0));
    const childTwo: Model.CounterView = Model.CounterAt(s32(1));
    const childOneNode: u64 = Model.ScreenIdentity(nested, s32(1));
    const childTwoNode: u64 = Model.ScreenIdentity(nested, s32(2));
    if (childOne.handle.token.owner.identity === childTwo.handle.token.owner.identity ||
        childOneNode === childTwoNode || !Model.CounterSet(childOne, s32(101)) ||
        !Model.CounterSet(childTwo, s32(202))) { return s32(31); }
    Model.ScreenStart(nested);
    NestedScreen(nested, true, s32(2));
    if (!Model.ScreenFinish(nested) || Model.CounterAt(s32(0)).count !== s32(202) ||
        Model.CounterAt(s32(1)).count !== s32(101) || Model.ScreenIdentity(nested, s32(1)) !== childTwoNode ||
        Model.ScreenIdentity(nested, s32(2)) !== childOneNode) { return s32(32); }
    Model.ScreenStart(nested);
    NestedScreen(nested, true, s32(1));
    if (!Model.ScreenFinish(nested) || Model.CounterCleanups() !== s32(7) ||
        Model.CounterSet(childOne, s32(99)) || Model.CounterAt(s32(0)).count !== s32(202) ||
        Model.ScreenIdentity(nested, s32(1)) !== childTwoNode) { return s32(33); }
    Model.ScreenStart(nested);
    NestedScreen(nested, false, s32(2));
    if (!Model.ScreenFinish(nested) || Model.CounterAt(s32(0)).count !== s32(10) ||
        Model.CounterAt(s32(1)).count !== s32(202) ||
        Model.CounterAt(s32(0)).handle.token.owner.identity === childOne.handle.token.owner.identity ||
        !Model.ScreenClose(nested) || Model.CounterCleanups() !== s32(9) ||
        !Model.CounterCleanupValid() || Model.CounterSet(childTwo, s32(99))) { return s32(34); }
    return s32(0);
}

export function main(): s32 {
    const owner: Types.Instance = Model.Open();
    const label: Text = TextCopy("owned");
    const run: Owned<Life.Action> = (instance: Types.Instance): void => {
        Model.Record(TextView(label).count);
        // Registration during invocation reallocates both resource tables.
        for (let item: s64 = s64(0); item < s64(64); item += s64(1)) {
            const extra: Owned<Life.Action> = (nested: Types.Instance): void => { Model.Record(item); };
            Actions.Callback(instance, extra);
        }
    };
    const token: Types.Token = Actions.Callback(owner, run);
    const cleanupLabel: Text = TextCopy("cleanup");
    const cleanup: Owned<Life.Action> = (instance: Types.Instance): void => { Model.Clean(TextView(cleanupLabel).count); };
    Actions.OnCleanup(owner, cleanup);
    for (let index: s64 = s64(0); index < s64(64); index += s64(1)) {
        const extra: Owned<Life.Action> = (instance: Types.Instance): void => { Model.Record(index); };
        Actions.Callback(owner, extra);
    }
    if (!Life.Queue(token)) { return s32(1); }
    Model.Pump();
    if (Model.Seen() !== s64(5) || !Life.Queue(token)) { return s32(2); }
    Model.Pump();
    if (Model.Seen() !== s64(10)) { return s32(3); }
    const selfLabel: Text = TextCopy("self");
    const selfAction: Owned<Life.Action> = (instance: Types.Instance): void => { Model.Record(TextView(selfLabel).count); Model.CancelSelf(); };
    const self: Types.Token = Actions.Callback(owner, selfAction);
    Model.SaveSelf(self);
    if (!Life.Queue(self)) { return s32(4); }
    Model.Pump();
    if (Model.Seen() !== s64(14) || Life.Queue(self)) { return s32(5); }
    if (!Model.Close() || Model.Cleaned() !== s64(7) || Life.Queue(token)) { return s32(6); }
    return CounterChecks();
}
