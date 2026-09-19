import Base
import ./main.bend as Focus

# Without Tab, Shift alone never moves focus.
law no_tab_stays:
  for shift: Bool
  {Focus.tab_direction(False{}, shift) == Focus.Stay{} : Focus.Direction}

# Progress laws prevent an always-Stay implementation from passing.
law tab_moves_forward:
  {Focus.tab_direction(True{}, False{}) == Focus.Forward{} : Focus.Direction}

law shift_tab_moves_backward:
  {Focus.tab_direction(True{}, True{}) == Focus.Backward{} : Focus.Direction}
