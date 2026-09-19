# Reference contract for runtime/accessibility_policy.kry:
# AccessibilitySingleSelectionFor, modeled over the bounded subdomain
# selected, item in {-1, 0, 1} and four representative actions.
# This is not a second production implementation.
import Base

type Value is Data:
  MinusOne{}
  Zero{}
  One{}

type Action is Data:
  Other{}
  SelectItem{}
  DeselectItem{}
  ClearSelection{}

def deselect_from(selected: Value, item: Value) -> Value:
  match selected:
    case MinusOne{}:
      match item:
        case MinusOne{}: MinusOne{}
        case Zero{}: MinusOne{}
        case One{}: MinusOne{}
    case Zero{}:
      match item:
        case MinusOne{}: Zero{}
        case Zero{}: MinusOne{}
        case One{}: Zero{}
    case One{}:
      match item:
        case MinusOne{}: One{}
        case Zero{}: One{}
        case One{}: MinusOne{}

def single_selection(selected: Value, item: Value, action: Action) -> Value:
  match action:
    case SelectItem{}:
      item
    case ClearSelection{}:
      MinusOne{}
    case DeselectItem{}:
      deselect_from(selected, item)
    case Other{}:
      selected
