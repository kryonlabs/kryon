# Reference contract for runtime/focus.kry: FocusTabDirectionFor.
# This is not a second production implementation.
import Base

type Direction is Data:
  Stay{}
  Forward{}
  Backward{}

def tab_direction(tab_pressed: Bool, shift_down: Bool) -> Direction:
  match tab_pressed:
    case False{}:
      Stay{}
    case True{}:
      match shift_down:
        case False{}:
          Forward{}
        case True{}:
          Backward{}
