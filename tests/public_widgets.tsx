import * as NativeButton from "kryon/button";
import * as NativeCountdown from "kryon/countdown";
import * as NativeEmptyState from "kryon/empty_state";
import * as NativeImage from "kryon/image";
import * as NativeLoadingScreen from "kryon/loading_screen";
import * as NativeProgressRing from "kryon/progress_ring";
import * as NativeText from "kryon/text";
import * as NativeTextField from "kryon/text_field";
import * as NativeTimePicker from "kryon/time_picker";
import { TextAlign } from "kryon/text_align";
import { Wrap } from "kryon/text";
import { CompositionState } from "kryon/text_input";
import { Session } from "kryon/session";
import { Button } from "kryon/tsx/button";
import { Countdown } from "kryon/tsx/countdown";
import { EmptyState } from "kryon/tsx/empty_state";
import { Image } from "kryon/tsx/image";
import { LoadingScreen } from "kryon/tsx/loading_screen";
import { ProgressRing } from "kryon/tsx/progress_ring";
import { Text } from "kryon/tsx/text";
import { ControlledTextField, TextField } from "kryon/tsx/text_field";
import { TimePicker } from "kryon/tsx/time_picker";

export function PublicLoadingScreen(session: Session, props: NativeLoadingScreen.Props): void {
    <LoadingScreen session={session} props={props}/>;
}

export function PublicWidgets(session: Session, button: NativeButton.Props, image: NativeImage.Props): boolean {
    <Image session={session} props={image}/>;
    return Button({session: session, props: button});
}

export function PublicTextField(session: Session, props: NativeTextField.Props): CompositionState {
    const result = ControlledTextField({session: session, props: props});
    return result.composition;
}

export function PublicTextFieldState(session: Session, state: NativeRef<NativeTextField.State>, props: NativeTextField.Props): boolean {
    const result = TextField({session: session, state: state, props: props});
    return result.committed;
}

export function PublicTextAlignment(session: Session, props: NativeText.Props): void {
    props.align = TextAlign.TextAlignCenter;
    props.vertical_align = TextAlign.TextAlignEnd;
    props.wrap = Wrap.TextWrapNone;
    <Text session={session} props={props}/>;
}

export function PublicTimePicker(session: Session, props: NativeTimePicker.Props): s32 {
    const result = TimePicker({session: session, props: props});
    return result.value;
}

export function PublicCountdown(session: Session, props: NativeCountdown.Props, state: NativeRef<NativeCountdown.CountdownState>): NativeCountdown.CountdownResult {
    return Countdown({session: session, props: props, state: state});
}

export function PublicEmptyState(session: Session, props: NativeEmptyState.Props): NativeEmptyState.EmptyStateResult {
    return EmptyState({session: session, props: props});
}

export function PublicProgressRing(session: Session, props: NativeProgressRing.Props, state: NativeRef<NativeProgressRing.ProgressRingState>): NativeProgressRing.ProgressRingResult {
    return ProgressRing({session: session, props: props, state: state});
}
