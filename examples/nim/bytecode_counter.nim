## Bytecode Counter Example
## Demonstrates using bytecode for event handlers instead of Nim procs

import ../../bindings/nim/[ir_core, ir_serialization, bytecode]
import std/[strformat]

# ============================================================================
# Build Bytecode Functions
# ============================================================================

proc buildCounterBytecode(): ptr IRMetadata =
  let builder = newBytecodeBuilder()

  # Create reactive state for counter
  let counterId = builder.addStateInt("counter", 0)
  echo &"[Bytecode] Created state 'counter' (id={counterId})"

  # Function 1: Increment counter
  discard builder
    .beginFunction("handle_increment")
    .getState(counterId)
    .pushInt(1)
    .add()
    .setState(counterId)
    .halt()
    .endFunction()
  echo "[Bytecode] Created function 'handle_increment'"

  # Function 2: Decrement counter
  discard builder
    .beginFunction("handle_decrement")
    .getState(counterId)
    .pushInt(1)
    .sub()
    .setState(counterId)
    .halt()
    .endFunction()
  echo "[Bytecode] Created function 'handle_decrement'"

  # Function 3: Reset counter
  discard builder
    .beginFunction("handle_reset")
    .pushInt(0)
    .setState(counterId)
    .halt()
    .endFunction()
  echo "[Bytecode] Created function 'handle_reset'"

  # Function 4: Double counter
  discard builder
    .beginFunction("handle_double")
    .getState(counterId)
    .pushInt(2)
    .mul()
    .setState(counterId)
    .halt()
    .endFunction()
  echo "[Bytecode] Created function 'handle_double'"

  # Optional: Add a host function for logging
  let logId = builder.addHostFunction("console.log", "(string) -> void", required = false)
  echo &"[Bytecode] Created host function 'console.log' (id={logId})"

  result = builder.getMetadata()

# ============================================================================
# Build UI Component Tree
# ============================================================================

proc buildCounterUI(): ptr IRComponent =
  # Create root container
  result = ir_create_component(IR_COMPONENT_CONTAINER)
  result.tag = "counter-app".cstring

  # Set container style
  ir_set_width(result.style, IR_DIMENSION_PERCENT, 100.0)
  ir_set_height(result.style, IR_DIMENSION_AUTO, 0.0)
  ir_set_padding(result.style, 20, 20, 20, 20)

  # Title
  let title = ir_create_component(IR_COMPONENT_TEXT)
  title.text_content = "Bytecode Counter Example".cstring
  ir_set_font(title.style, 24, "Arial", 255, 255, 255, 255, true, false)
  ir_set_margin(title.style, 0, 0, 20, 0)
  ir_add_child(result, title)

  # Counter display
  let display = ir_create_component(IR_COMPONENT_TEXT)
  display.tag = "counter-display".cstring
  display.text_content = "Count: 0".cstring
  ir_set_font(display.style, 32, "Arial", 255, 255, 255, 255, true, false)
  ir_set_margin(display.style, 0, 0, 20, 0)
  ir_add_child(result, display)

  # Button row
  let buttonRow = ir_create_component(IR_COMPONENT_CONTAINER)
  ir_set_width(buttonRow.style, IR_DIMENSION_AUTO, 0.0)
  ir_set_height(buttonRow.style, IR_DIMENSION_AUTO, 0.0)
  ir_add_child(result, buttonRow)

  # Increment button
  let incrementBtn = ir_create_component(IR_COMPONENT_BUTTON)
  incrementBtn.tag = "increment-btn".cstring
  incrementBtn.text_content = "+1".cstring
  ir_set_width(incrementBtn.style, IR_DIMENSION_PX, 80.0)
  ir_set_height(incrementBtn.style, IR_DIMENSION_PX, 40.0)
  ir_set_margin(incrementBtn.style, 0, 10, 0, 0)
  ir_add_child(buttonRow, incrementBtn)

  # Decrement button
  let decrementBtn = ir_create_component(IR_COMPONENT_BUTTON)
  decrementBtn.tag = "decrement-btn".cstring
  decrementBtn.text_content = "-1".cstring
  ir_set_width(decrementBtn.style, IR_DIMENSION_PX, 80.0)
  ir_set_height(decrementBtn.style, IR_DIMENSION_PX, 40.0)
  ir_set_margin(decrementBtn.style, 0, 10, 0, 0)
  ir_add_child(buttonRow, decrementBtn)

  # Reset button
  let resetBtn = ir_create_component(IR_COMPONENT_BUTTON)
  resetBtn.tag = "reset-btn".cstring
  resetBtn.text_content = "Reset".cstring
  ir_set_width(resetBtn.style, IR_DIMENSION_PX, 80.0)
  ir_set_height(resetBtn.style, IR_DIMENSION_PX, 40.0)
  ir_set_margin(resetBtn.style, 0, 10, 0, 0)
  ir_add_child(buttonRow, resetBtn)

  # Double button
  let doubleBtn = ir_create_component(IR_COMPONENT_BUTTON)
  doubleBtn.tag = "double-btn".cstring
  doubleBtn.text_content = "×2".cstring
  ir_set_width(doubleBtn.style, IR_DIMENSION_PX, 80.0)
  ir_set_height(doubleBtn.style, IR_DIMENSION_PX, 40.0)
  ir_set_margin(doubleBtn.style, 0, 0, 0, 0)
  ir_add_child(buttonRow, doubleBtn)

  # Info text
  let info = ir_create_component(IR_COMPONENT_TEXT)
  info.text_content = "All event handlers are compiled to bytecode!".cstring
  ir_set_font(info.style, 14, "Arial", 150, 150, 150, 255, false, true)
  ir_set_margin(info.style, 20, 0, 0, 0)
  ir_add_child(result, info)

# ============================================================================
# Main
# ============================================================================

when isMainModule:
  echo "=== Kryon Bytecode Counter Example ==="
  echo ""

  # Build bytecode metadata
  echo "Building bytecode..."
  let metadata = buildCounterBytecode()
  echo ""

  # Build UI
  echo "Building UI..."
  let root = buildCounterUI()
  echo "  ✓ Created component tree"
  echo ""

  # Serialize to .kir file
  echo "Serializing to .kir file..."
  let filename = "bytecode_counter.kir"
  let success = ir_write_json_file_with_metadata(root, metadata, filename.cstring)

  if success:
    echo &"  ✓ Written to: {filename}"
    echo ""
    echo "Success! The .kir file contains:"
    echo "  • UI component tree"
    echo "  • Bytecode functions for all event handlers"
    echo "  • Reactive state definitions"
    echo "  • Host function declarations"
    echo ""
    echo "This .kir file can be:"
    echo "  • Executed by any Kryon runtime (desktop/web/mobile)"
    echo "  • Loaded without needing the original Nim source"
    echo "  • Inspected with: kryon inspect-detailed bytecode_counter.kir"
    echo "  • Visualized with: kryon tree bytecode_counter.kir"
  else:
    echo "  ✗ Failed to write .kir file"

  # Cleanup
  ir_metadata_destroy(metadata)
  ir_destroy_component(root)

  echo ""
  echo "=== Example Complete ==="
