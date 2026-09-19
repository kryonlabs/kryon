import Base
import ./main.bend as Selection

# Selecting an item returns that item, whatever the previous selection.
law select_item_returns_item:
  for selected: Selection.Value
  for item: Selection.Value
  {Selection.single_selection(selected, item, Selection.SelectItem{}) == item : Selection.Value}

# Clearing the selection always yields -1.
law clear_selection_returns_none:
  for selected: Selection.Value
  for item: Selection.Value
  {Selection.single_selection(selected, item, Selection.ClearSelection{}) == Selection.MinusOne{} : Selection.Value}

# Deselecting the currently selected item yields -1 (ground instances; the
# equality helper scrutinizes its arguments, so quantified forms do not
# reduce and are not claimed).
law deselect_matching_minus:
  {Selection.single_selection(Selection.MinusOne{}, Selection.MinusOne{}, Selection.DeselectItem{}) == Selection.MinusOne{} : Selection.Value}

law deselect_matching_zero:
  {Selection.single_selection(Selection.Zero{}, Selection.Zero{}, Selection.DeselectItem{}) == Selection.MinusOne{} : Selection.Value}

law deselect_matching_one:
  {Selection.single_selection(Selection.One{}, Selection.One{}, Selection.DeselectItem{}) == Selection.MinusOne{} : Selection.Value}

# Deselecting a different item preserves the selection (ground instances).
law deselect_distinct_preserves_minus_zero:
  {Selection.single_selection(Selection.MinusOne{}, Selection.Zero{}, Selection.DeselectItem{}) == Selection.MinusOne{} : Selection.Value}

law deselect_distinct_preserves_zero_one:
  {Selection.single_selection(Selection.Zero{}, Selection.One{}, Selection.DeselectItem{}) == Selection.Zero{} : Selection.Value}

law deselect_distinct_preserves_one_minus:
  {Selection.single_selection(Selection.One{}, Selection.MinusOne{}, Selection.DeselectItem{}) == Selection.One{} : Selection.Value}

# Any other action preserves the selection.
law other_actions_preserve:
  for selected: Selection.Value
  for item: Selection.Value
  {Selection.single_selection(selected, item, Selection.Other{}) == selected : Selection.Value}
