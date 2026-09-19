# Reference contract for runtime/focus.kry: FocusActivationFor.
# This is not a second production implementation.
import Base

def activation(active: Bool, keyboard_enabled: Bool, content_disabled: Bool, captured: Bool, enter_pressed: Bool, space_pressed: Bool, text_input_active: Bool) -> Bool:
  match active:
    case False{}:
      False{}
    case True{}:
      match keyboard_enabled:
        case False{}:
          False{}
        case True{}:
          match content_disabled:
            case True{}:
              False{}
            case False{}:
              match captured:
                case True{}:
                  False{}
                case False{}:
                  match enter_pressed:
                    case True{}:
                      True{}
                    case False{}:
                      match text_input_active:
                        case True{}:
                          False{}
                        case False{}:
                          space_pressed
