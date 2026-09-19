import Base
import ./main.bend as Activation

# Laws are stated so the checker can reduce them: every scrutinized position
# is a literal; quantified binders are only inputs the function never needs
# to inspect on that path. The exhaustive 128-row comparison against the
# generated C implementation lives in the native test, not here.

# An inactive control never activates, regardless of every other input.
law no_activation_when_inactive:
  for keyboard_enabled: Bool
  for content_disabled: Bool
  for captured: Bool
  for enter_pressed: Bool
  for space_pressed: Bool
  for text_input_active: Bool
  {Activation.activation(False{}, keyboard_enabled, content_disabled, captured, enter_pressed, space_pressed, text_input_active) == False{} : Bool}

# Keyboard-disabled or content-disabled controls never activate.
law no_activation_when_keyboard_disabled:
  for content_disabled: Bool
  for captured: Bool
  for enter_pressed: Bool
  for space_pressed: Bool
  for text_input_active: Bool
  {Activation.activation(True{}, False{}, content_disabled, captured, enter_pressed, space_pressed, text_input_active) == False{} : Bool}

law no_activation_when_disabled:
  for captured: Bool
  for enter_pressed: Bool
  for space_pressed: Bool
  for text_input_active: Bool
  {Activation.activation(True{}, True{}, True{}, captured, enter_pressed, space_pressed, text_input_active) == False{} : Bool}

# A captured control never activates.
law no_activation_when_captured:
  for enter_pressed: Bool
  for space_pressed: Bool
  for text_input_active: Bool
  {Activation.activation(True{}, True{}, False{}, True{}, enter_pressed, space_pressed, text_input_active) == False{} : Bool}

# Progress laws: an always-false implementation cannot satisfy these.
law enter_activates:
  {Activation.activation(True{}, True{}, False{}, False{}, True{}, False{}, False{}) == True{} : Bool}

law space_activates_without_text_input:
  {Activation.activation(True{}, True{}, False{}, False{}, False{}, True{}, False{}) == True{} : Bool}

# While text input is active, Space must not activate.
law text_input_blocks_space:
  for space_pressed: Bool
  {Activation.activation(True{}, True{}, False{}, False{}, False{}, space_pressed, True{}) == False{} : Bool}
