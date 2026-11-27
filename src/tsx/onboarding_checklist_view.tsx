import * as NativeOnboardingChecklist from "kryon/onboarding_checklist";
import { Session } from "kryon/session";

export interface OnboardingChecklistProps {
    session: Session;
    props: NativeOnboardingChecklist.Props;
}

export function OnboardingChecklist(widget: OnboardingChecklistProps): NativeOnboardingChecklist.OnboardingChecklistResult {
    return NativeOnboardingChecklist.Draw(widget.session, widget.props);
}
