## Gradient Demo - Testing IR Gradient Support
## This example demonstrates linear, radial, and conic gradients

import ../../bindings/nim/ir_core

proc createGradientDemo(): ptr IRComponent =
  # Create root container
  var root = ir_create_component(IR_COMPONENT_COLUMN)
  var rootStyle = ir_create_style()
  ir_set_width(rootStyle, IR_DIMENSION_PX, 800)
  ir_set_height(rootStyle, IR_DIMENSION_PX, 600)
  ir_set_background_color(rootStyle, 25, 25, 25, 255)
  ir_set_padding(rootStyle, 20, 20, 20, 20)
  ir_set_style(root, rootStyle)

  var rootLayout = ir_create_layout()
  ir_set_flexbox(rootLayout, false, 20, IR_ALIGNMENT_START, IR_ALIGNMENT_START)
  ir_set_layout(root, rootLayout)

  # Title
  var title = ir_create_component(IR_COMPONENT_TEXT)
  ir_set_text_content(title, "Gradient Demo - Testing All Gradient Types")
  var titleStyle = ir_create_style()
  ir_set_font(titleStyle, 24, "sans-serif", 255, 255, 255, 255, true, false)
  ir_set_style(title, titleStyle)
  ir_add_child(root, title)

  # Container for gradient boxes
  var gradientRow = ir_create_component(IR_COMPONENT_ROW)
  var rowStyle = ir_create_style()
  ir_set_width(rowStyle, IR_DIMENSION_PERCENT, 100)
  ir_set_height(rowStyle, IR_DIMENSION_PX, 200)
  ir_set_style(gradientRow, rowStyle)

  var rowLayout = ir_create_layout()
  ir_set_flexbox(rowLayout, false, 20, IR_ALIGNMENT_SPACE_BETWEEN, IR_ALIGNMENT_START)
  ir_set_layout(gradientRow, rowLayout)
  ir_add_child(root, gradientRow)

  # Linear Gradient Box
  block:
    var container = ir_create_component(IR_COMPONENT_CONTAINER)
    var containerStyle = ir_create_style()
    ir_set_width(containerStyle, IR_DIMENSION_PX, 240)
    ir_set_height(containerStyle, IR_DIMENSION_PX, 200)

    # Create linear gradient (red to blue, 45 degrees)
    var gradient = ir_gradient_create(IR_GRADIENT_LINEAR)
    ir_gradient_set_angle(gradient, 45.0)
    ir_gradient_add_stop(gradient, 0.0, 255, 0, 0, 255)     # Red at start
    ir_gradient_add_stop(gradient, 0.5, 255, 255, 0, 255)   # Yellow at middle
    ir_gradient_add_stop(gradient, 1.0, 0, 0, 255, 255)     # Blue at end
    ir_set_background_gradient(containerStyle, gradient)

    ir_set_style(container, containerStyle)
    ir_add_child(gradientRow, container)

    # Label
    var label = ir_create_component(IR_COMPONENT_TEXT)
    ir_set_text_content(label, "Linear Gradient")
    var labelStyle = ir_create_style()
    ir_set_font(labelStyle, 16, "sans-serif", 255, 255, 255, 255, true, false)
    ir_set_padding(labelStyle, 10, 10, 10, 10)
    ir_set_style(label, labelStyle)
    ir_add_child(container, label)

  # Radial Gradient Box
  block:
    var container = ir_create_component(IR_COMPONENT_CONTAINER)
    var containerStyle = ir_create_style()
    ir_set_width(containerStyle, IR_DIMENSION_PX, 240)
    ir_set_height(containerStyle, IR_DIMENSION_PX, 200)

    # Create radial gradient (green to purple from center)
    var gradient = ir_gradient_create(IR_GRADIENT_RADIAL)
    ir_gradient_set_center(gradient, 0.5, 0.5)
    ir_gradient_add_stop(gradient, 0.0, 0, 255, 0, 255)     # Green at center
    ir_gradient_add_stop(gradient, 0.5, 0, 255, 255, 255)   # Cyan
    ir_gradient_add_stop(gradient, 1.0, 128, 0, 128, 255)   # Purple at edge
    ir_set_background_gradient(containerStyle, gradient)

    ir_set_style(container, containerStyle)
    ir_add_child(gradientRow, container)

    # Label
    var label = ir_create_component(IR_COMPONENT_TEXT)
    ir_set_text_content(label, "Radial Gradient")
    var labelStyle = ir_create_style()
    ir_set_font(labelStyle, 16, "sans-serif", 255, 255, 255, 255, true, false)
    ir_set_padding(labelStyle, 10, 10, 10, 10)
    ir_set_style(label, labelStyle)
    ir_add_child(container, label)

  # Conic Gradient Box
  block:
    var container = ir_create_component(IR_COMPONENT_CONTAINER)
    var containerStyle = ir_create_style()
    ir_set_width(containerStyle, IR_DIMENSION_PX, 240)
    ir_set_height(containerStyle, IR_DIMENSION_PX, 200)

    # Create conic gradient (rainbow wheel)
    var gradient = ir_gradient_create(IR_GRADIENT_CONIC)
    ir_gradient_set_center(gradient, 0.5, 0.5)
    ir_gradient_add_stop(gradient, 0.0, 255, 0, 0, 255)     # Red
    ir_gradient_add_stop(gradient, 0.25, 255, 255, 0, 255)  # Yellow
    ir_gradient_add_stop(gradient, 0.5, 0, 255, 0, 255)     # Green
    ir_gradient_add_stop(gradient, 0.75, 0, 0, 255, 255)    # Blue
    ir_gradient_add_stop(gradient, 1.0, 255, 0, 0, 255)     # Red (loop back)
    ir_set_background_gradient(containerStyle, gradient)

    ir_set_style(container, containerStyle)
    ir_add_child(gradientRow, container)

    # Label
    var label = ir_create_component(IR_COMPONENT_TEXT)
    ir_set_text_content(label, "Conic Gradient")
    var labelStyle = ir_create_style()
    ir_set_font(labelStyle, 16, "sans-serif", 255, 255, 255, 255, true, false)
    ir_set_padding(labelStyle, 10, 10, 10, 10)
    ir_set_style(label, labelStyle)
    ir_add_child(container, label)

  # Instructions
  var instructions = ir_create_component(IR_COMPONENT_TEXT)
  ir_set_text_content(instructions, "Testing gradients across renderers: SDL3, Web, and Terminal")
  var instrStyle = ir_create_style()
  ir_set_font(instrStyle, 14, "sans-serif", 180, 180, 180, 255, false, true)
  ir_set_style(instructions, instrStyle)
  ir_add_child(root, instructions)

  return root

# Main entry point
when isMainModule:
  # Create IR context and tree
  var context = ir_create_context()
  ir_set_context(context)

  var root = createGradientDemo()
  ir_set_root(root)

  echo "✓ Gradient demo IR tree created"
  echo "  - Linear gradient: red → yellow → blue (45°)"
  echo "  - Radial gradient: green → cyan → purple (center)"
  echo "  - Conic gradient: rainbow wheel (360°)"
