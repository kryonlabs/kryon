// {{name}}: a Kryon application. Frame runs every frame and composes the
// interface from the state in src/tasks.zi; Kryon keeps the widgets between
// frames and reports what the user did. Layout containers place their
// children and theme tokens size them, so no code here computes a position.
//
//   kryon run                     the desktop window (ziran run does the same)
//   kryon run tui                 the same interface in a terminal
//   kryon run web                 a Canvas2D browser page (needs Emscripten)
import * as App from "kryon/app";
import { ButtonEmphasis, ButtonTone, ControlSize } from "kryon/control_props";
import { Props as ButtonProps, ButtonMeasure } from "kryon/button";
import { CardVariant } from "kryon/card";
import { Gap, ColumnWidth } from "kryon/layout";
import { Session } from "kryon/session";
import * as Theme from "kryon/theme";
import { ThemeMetricRole } from "kryon/theme";
import { Button } from "kryon/tsx/button";
import { Card } from "kryon/tsx/card";
import { Checkbox } from "kryon/tsx/checkbox";
import { Column } from "kryon/tsx/layout";
import { TextField } from "kryon/tsx/text_field";
import { Text } from "kryon/tsx/text";
import { Toggle } from "kryon/tsx/toggle";
import { TitleBar } from "kryon/tsx/title_bar";
import * as Look from "look";
import * as Model from "tasks";

// One line of text in the open column, sized by a type-scale token.
interface LineProps { session: Session; text: string; role: ThemeMetricRole; }

function Line(line: LineProps): void {
    <Text session={line.session} props={{bounds: {height: LineHeight(line.role)},
        text: line.text, size: s32(Theme.CurrentThemeMetric(line.role))}}/>;
}

function LineHeight(role: ThemeMetricRole): float32 {
    return float32(Theme.CurrentThemeMetric(role) * 1.4);
}

// The app bar: the title, and a dark mode switch at its right.
interface AppBarProps { session: Session; }

function AppBar(bar: AppBarProps): void {
    const height = float32(Theme.CurrentThemeMetric(
        ThemeMetricRole.ThemeMetricControlHeightLarge) +
        Theme.CurrentThemeMetric(ThemeMetricRole.ThemeMetricSpace3));
    const title = TitleBar({session: bar.session, options: Model.MenuOptions(), props: {bounds: {height: height}, title: "{{name}}", strong_title: true,
            trailing_width: s32(120)}});
    const toggle = Toggle({session: bar.session, props: {bounds: title.trailing_bounds, label: "Dark", value: Look.Dark()}});
    if (toggle.changed) { Look.SetDark(toggle.value); }
}

// One task's checkbox and the model update its change reports.
interface TaskBoxProps { session: Session; index: s32; }

function TaskBox(task: TaskBoxProps): void {
    const box = Checkbox({session: task.session, props: {label: Model.TaskTitle(task.index), checked: Model.TaskDone(task.index),
            bounds: {height: Theme.CurrentThemeMetric(
                ThemeMetricRole.ThemeMetricControlHeightMedium)}}});
    if (box.changed) { Model.SetTaskDone(task.index, box.checked); }
}

// One task as a card: the checkbox row and a detail line. The card grows
// to its content.
interface TaskCardProps { session: Session; index: s32; }

function TaskCard(task: TaskCardProps): void {
    <Card session={task.session} props={{variant: CardVariant.CardOutlined}}>
        <TaskBox session={task.session} index={task.index}/>;
        <Line session={task.session} text={Model.TaskDetail(task.index)}
            role={ThemeMetricRole.ThemeMetricTextSM}/>;
    </Card>;
}

// Every task, drawn one after another so the cards keep their order.
interface TaskListProps { session: Session; }

function TaskList(list: TaskListProps): void {
    for (let index = s32(0); index < Model.TaskCount(); index = index + s32(1)) {
        <TaskCard session={list.session} index={index}/>;
    }
}

// The new-task row: the draft field, and the button that adds it. The
// button or the Enter key adds the draft. Value copies into the task, so
// Clear can empty the field in the same frame.
interface AddTaskProps { session: Session; }

function AddTask(row: AddTaskProps): void {
    const width = ColumnWidth(row.session);
    const field = TextField({session: row.session, state: Model.Draft(), props: {placeholder: "New task", label: "Task"}});
    const add: ButtonProps = {label: "Add task", size: ControlSize.ControlSizeLarge,
        tone: ButtonTone.ButtonToneAccent, emphasis: ButtonEmphasis.ButtonEmphasisFilled};
    add.bounds = {width: width, height: ButtonMeasure(add).height};
    const pressed = Button({session: row.session, props: add});
    if (pressed || field.committed) {
        if (Model.AddTask(Model.DraftText(), "Rename it in src/tasks.zi")) {
            Model.ClearDraft();
        }
    }
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    // First frame: start in the host's theme with the seed tasks.
    if (frame.first) {
        Look.SetDark(Theme.CurrentTheme().dark);
        Model.StartTasks();
    }
    Look.ApplyLook(Look.Dark());
    <Column session={session} props={{bounds: frame.viewport}}>
        <AppBar session={session}/>;
        <Column session={session} props={{gap: Gap(ThemeMetricRole.ThemeMetricSpace3),
            gap_override: true, padding: Gap(ThemeMetricRole.ThemeMetricSpace4),
            padding_override: true}}>
            <Line session={session} text={Model.Summary()}
                role={ThemeMetricRole.ThemeMetricHeading3}/>;
            <TaskList session={session}/>;
            <AddTask session={session}/>;
        </Column>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "{{name}}"}, Frame);
}
