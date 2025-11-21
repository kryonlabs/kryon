import ../../bindings/nim/ir_core
import ../../bindings/nim/ir_desktop

# Test overflow protection in space distribution
# Gray container: 100px tall
# Three boxes: 30, 40, 50 = 120px total (exceeds container!)
# Expected: boxes should be packed tightly inside, not overflow

proc main() =
  let root = ir_container()

  let style = ir_create_style()
  ir_set_width(style, IR_DIMENSION_PX, 800)
  ir_set_height(style, IR_DIMENSION_PX, 600)
  ir_set_background_color(style, 30, 30, 30, 255)
  ir_set_style(root, style)

  # Container with limited height (100px)
  let container = ir_container()
  let container_style = ir_create_style()
  ir_set_width(container_style, IR_DIMENSION_PX, 200)
  ir_set_height(container_style, IR_DIMENSION_PX, 100)  # Limited height
  ir_set_background_color(container_style, 100, 100, 100, 255)
  ir_set_padding(container_style, 5, 5, 5, 5)
  ir_set_style(container, container_style)

  # Column with space-evenly (should handle overflow gracefully)
  let col = ir_column()
  let col_layout = ir_create_layout()
  ir_set_justify_content(col_layout, IR_ALIGNMENT_SPACE_EVENLY)
  ir_set_layout(col, col_layout)

  # Box 1: 30px
  let box1 = ir_container()
  let style1 = ir_create_style()
  ir_set_width(style1, IR_DIMENSION_PX, 80)
  ir_set_height(style1, IR_DIMENSION_PX, 30)
  ir_set_background_color(style1, 255, 100, 100, 255)
  ir_set_style(box1, style1)

  # Box 2: 40px
  let box2 = ir_container()
  let style2 = ir_create_style()
  ir_set_width(style2, IR_DIMENSION_PX, 80)
  ir_set_height(style2, IR_DIMENSION_PX, 40)
  ir_set_background_color(style2, 100, 255, 100, 255)
  ir_set_style(box2, style2)

  # Box 3: 50px (total = 120px, exceeds 100px container!)
  let box3 = ir_container()
  let style3 = ir_create_style()
  ir_set_width(style3, IR_DIMENSION_PX, 80)
  ir_set_height(style3, IR_DIMENSION_PX, 50)
  ir_set_background_color(style3, 100, 100, 255, 255)
  ir_set_style(box3, style3)

  ir_add_child(col, box1)
  ir_add_child(col, box2)
  ir_add_child(col, box3)
  ir_add_child(container, col)
  ir_add_child(root, container)

  ir_desktop_run(root)

main()
