## Test IR System - Simple Button
## This tests if the IR system works end-to-end

import bindings/nim/ir_core
import bindings/nim/ir_desktop

proc main() =
  echo "Creating IR button component..."

  # Create button
  let button = ir_button("Click Me!")

  # Create style
  let style = ir_create_style()
  ir_set_width(style, IR_DIMENSION_PX, 150.0)
  ir_set_height(style, IR_DIMENSION_PX, 50.0)
  ir_set_background_color(style, 64, 64, 128, 255)  # Purple background
  ir_set_font(style, 16.0, "sans-serif", 255, 192, 203, 255, false, false)  # Pink text
  ir_set_style(button, style)

  # Create container
  let container = ir_create_component(IR_COMPONENT_CENTER)
  let containerStyle = ir_create_style()
  ir_set_background_color(containerStyle, 25, 25, 25, 255)  # Dark background
  ir_set_style(container, containerStyle)
  ir_add_child(container, button)

  # Create renderer config
  var config = desktop_renderer_config_sdl3(600, 400, "IR Button Test")
  config.resizable = true
  config.vsync_enabled = true

  # Render
  echo "Running IR desktop renderer..."
  if not desktop_render_ir_component(container, addr config):
    echo "ERROR: Failed to render IR component"
    quit(1)

  echo "Done!"

when isMainModule:
  main()
